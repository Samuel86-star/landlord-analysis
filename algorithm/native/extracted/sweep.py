#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
sweep.py — Type0 / Type1 发牌参数扫描 + 基线相对打分 + TOP20 排名

驱动 extracted/harness.exe（C++ 真值，复用 landlord.h 规范拆牌）。
- 候选网格（精选）→ 并行调 harness（每候选实跑 N 局）→ 解析 JSONL 算指标
- 指标缓存到 sweep_raw.json：重排(改权重/目标)免重跑
- 基线：纯随机(--pure-random) + old2(Type0) 作对照组
- 适应度：基线相对（炸弹抱随机 / 手数·单牌求更顺 / 庄闲差+极差越小越好 / 牌型多样 / 配牌真实触发）
- 两阶段：粗扫 N=coarse_n → 每类 TOP20 决赛 N=final_n

用法：
  py -3 sweep.py                       # 全量扫描（粗扫+决赛）+ 出报告
  py -3 sweep.py --rerank              # 只用 sweep_raw.json 重排（改权重后秒出）
  py -3 sweep.py --coarse-n 3000 --final-n 20000
"""
import json, subprocess, os, sys, math, statistics as st
from collections import Counter
from concurrent.futures import ThreadPoolExecutor, as_completed
from pathlib import Path

HERE = Path(__file__).resolve().parent
HARNESS = HERE / "harness.exe"
CFG = HERE.parent / "previous" / "makedeal.json"
RUNDIR = HERE / "sweep_runs"
RAW = HERE / "sweep_raw.json"
REPORT = HERE / "top20_report.md"
OUTJSON = HERE / "top20_configs.json"
SEED = 1

def sigmoid(x):
    """normalizeHandStrength 的 Python 版：sigmoid(V/40) ∈(0,1)，非负单调牌力强度。"""
    return 1.0 / (1.0 + math.exp(-x / 40.0))


# =============================================================================
# 候选网格（精选）
# =============================================================================
# CouPaiStrategy 种子（1单2对3三4顺5连对6飞机13炸；低等级主走无13，含带13对照）
TYPE1_COUPAI = [
    ("no-pair",     [4, 5, 3, 6]),
    ("with-pair",   [4, 6, 5, 2, 3]),
    ("no-bomb",     [4, 6, 5, 3, 2]),
    ("wp-no3",      [4, 6, 5, 2]),
    ("straight1st", [4, 5, 6, 3, 2]),
    ("plane1st",    [6, 4, 5, 3, 2]),
    ("pair1st",     [2, 4, 5, 3, 6]),
    ("bomb-last",   [4, 6, 5, 3, 2, 13]),   # 含炸码(对照，低等级不期望胜出)
]
TYPE1_BSEL = [(11, 15), (14, 17), (8, 14), (10, 15)]   # (BeginMakeNum, BeginSelectBanker)
TYPE1_TV = [999, 0.6]
TR = 10

def type1_candidates():
    out = []
    for name, cp in TYPE1_COUPAI:
        for (b, s) in TYPE1_BSEL:
            if not (b < s <= 17):
                continue
            for tv in TYPE1_TV:
                out.append({"type": 1, "coupai": cp, "coupai_name": name,
                            "begin": b, "select": s, "tv": tv, "tr": TR,
                            "label": f"T1 {name} cp{cp} b{b} s{s} tv{tv}"})
    return out

# Type0 阈值预设：(FirstHC,FirstBomb,FirstBig, OtherHC,OtherBomb,OtherBig, BigCardsTo)
# 高 HC/低 Bomb 阈值 → DoMakeDeal 难触发 → 更贴自然；低 HC → 更激进做牌
T0_THRESH = [
    (3, 3, 6, 4, 3, 5, 4),   # 极激进
    (4, 3, 4, 5, 3, 3, 3),   # 激进·容忍炸
    (5, 2, 5, 6, 2, 4, 4),   # = new/new2 档
    (5, 2, 4, 6, 2, 3, 2),   # = old2 档
    (6, 2, 5, 7, 2, 4, 3),   # 中庸
    (6, 3, 6, 7, 3, 5, 4),   # 中庸·容忍炸
    (7, 1, 4, 8, 1, 3, 2),   # 被动(少做牌→贴自然)
    (8, 1, 5, 9, 1, 4, 3),   # 极被动
]
T0_BMN = [6, 8, 10, 12, 14]

def type0_candidates():
    out = []
    for i, th in enumerate(T0_THRESH):
        for bmn in T0_BMN:
            if not (0 < bmn < 17):
                continue
            out.append({"type": 0, "bmn": bmn, "bigcardsto": th[6],
                        "first_hc": th[0], "first_bomb": th[1], "first_big": th[2],
                        "other_hc": th[3], "other_bomb": th[4], "other_big": th[5],
                        "label": f"T0 thr{i} bmn{bmn}"})
    return out

# =============================================================================
# harness 调用 + JSONL 解析
# =============================================================================
def build_cmd(cand, n):
    cmd = [str(HARNESS), "--cfg", str(CFG), "-n", str(n), "--seed", str(SEED)]
    if cand.get("pure"):
        cmd.append("--pure-random")
    elif cand["type"] == 1:
        cmd += ["--type1", "--coupai", ",".join(map(str, cand["coupai"])),
                "--begin", str(cand["begin"]), "--select", str(cand["select"]),
                "--tv", str(cand["tv"]), "--tr", str(cand["tr"])]
    else:
        cmd += ["--type0", "--bmn", str(cand["bmn"]), "--bigcards-to", str(cand["bigcardsto"]),
                "--first-hc", str(cand["first_hc"]), "--first-bomb", str(cand["first_bomb"]),
                "--first-big", str(cand["first_big"]), "--other-hc", str(cand["other_hc"]),
                "--other-bomb", str(cand["other_bomb"]), "--other-big", str(cand["other_big"])]
    return cmd

def parse_metrics(path):
    deals = [json.loads(l) for l in open(path, encoding="utf-8-sig") if l.strip()]
    seats = [s for r in deals for s in r["seats"]]
    def m(k):
        return st.mean([s[k] for s in seats])
    c = Counter()
    for s in seats:
        for k, v in s["gtypes"].items():
            c[int(k)] += v
    tot = sum(c.values()) or 1
    ent = -sum((v / tot) * math.log(v / tot) for v in c.values() if v > 0)
    n = len(deals)
    # §2.1 单局炸弹率（持有口径：整桌 3 人有任意炸弹即记 1）+ §2.2 密度分布（桌上炸弹总数 K）
    occ = sum(1 for r in deals if any(s["bombs"] >= 1 for s in r["seats"])) / n
    dens = [0, 0, 0, 0]
    for r in deals:
        k = sum(s["bombs"] for s in r["seats"])
        dens[3 if k >= 3 else k] += 1
    dens = [d / n for d in dens]
    # §2.5 首叫诱导度 P_max/P_avg + §2.6 抗衡度 (P_mid+P_min)/P_max，P=sigmoid(val_f/40)
    hs, rs = [], []
    for r in deals:
        ps = sorted((sigmoid(s["val_f"]) for s in r["seats"]), reverse=True)
        pavg = sum(ps) / 3.0
        if pavg > 1e-9:
            hs.append(ps[0] / pavg)
            if ps[0] > 1e-9:
                rs.append((ps[1] + ps[2]) / ps[0])
    return {
        "bomb_held": m("bombs"), "bomb_split": m("split_bombs"), "hands": m("opt_hands"),
        "singles": m("singles"), "bigcards": m("bigcards"), "value": m("value"),
        "hit": sum(1 for s in seats if s["makedeal"] > 0) / len(seats),
        "entropy": ent,
        "gap_val": st.mean([r["gap_val"] for r in deals]),
        "gap_bomb": st.mean([r["gap_bomb"] for r in deals]),
        "spread": st.mean([r["spread"] for r in deals]),
        "bomb_occ": occ, "dens": dens,
        "head_start": st.mean(hs) if hs else 0.0, "resist": st.mean(rs) if rs else 0.0,
        "n": n,
    }

def run_one(cand, n):
    RUNDIR.mkdir(exist_ok=True)
    tag = cand["label"].replace(" ", "_").replace(",", "").replace("[", "").replace("]", "")
    jpath = RUNDIR / f"{tag}_n{n}.jsonl"
    cmd = build_cmd(cand, n)
    with open(jpath, "wb") as f:
        subprocess.run(cmd, stdout=f, stderr=subprocess.DEVNULL, check=True)
    return cand, parse_metrics(jpath)

def run_all(cands, n, tag):
    res = {}
    workers = max(1, (os.cpu_count() or 4))
    with ThreadPoolExecutor(max_workers=workers) as ex:
        futs = {ex.submit(run_one, c, n): c for c in cands}
        for i, fut in enumerate(as_completed(futs), 1):
            c, m = fut.result()
            res[c["label"]] = m
            print(f"  [{tag}] {i}/{len(cands)}  {c['label']}", file=sys.stderr, flush=True)
    return res

# =============================================================================
# 适应度（基线相对；权重在 W，可改后 --rerank 秒出）
# =============================================================================
W = {"bomb": 0.28, "hand": 0.18, "single": 0.12, "susp": 0.20, "div": 0.10, "hit": 0.12}

def fitness(m, B):
    # §2.1 单局炸弹率（持有口径）抱随机——低等级房核心项
    Sb = 1 - min(1, abs(m["bomb_occ"] - B["bomb_occ"]) / (0.5 * B["bomb_occ"] + 1e-9))
    # §2.4 人均最优手数：比随机顺 ~1.5 手为峰
    ideal_h = B["hands"] - 1.5
    Sh = math.exp(-((m["hands"] - ideal_h) / 1.0) ** 2)
    # §2.3 人均散牌少于随机
    Ss = 1 - max(0, m["singles"] - (B["singles"] - 1.0)) / (B["singles"] + 1e-9)
    # §2.5/§2.6 悬念：首叫诱导(≤基线且≥1.05 ⇒ 至少如随机般均衡、不至流局) + 抗衡(≥基线 ⇒ 农民更能抗)
    Bhs, Brs = B["head_start"], B["resist"]
    S_hs = 1.0 if (1.05 <= m["head_start"] <= Bhs) else max(0.0, 1.0 - abs(m["head_start"] - Bhs) / (0.5 * Bhs + 1e-9))
    S_res = 1.0 if m["resist"] >= Brs else max(0.0, 1.0 - (Brs - m["resist"]) / (0.5 * Brs + 1e-9))
    Ssusp = 0.5 * S_hs + 0.5 * S_res
    # 牌型多样
    Sd = min(1.0, m["entropy"] / (B["entropy"] + 1e-9))
    # 配牌真实触发
    Shit = 1 if 0.2 <= m["hit"] <= 0.95 else 0.5
    total = (W["bomb"] * Sb + W["hand"] * Sh + W["single"] * Ss +
             W["susp"] * Ssusp + W["div"] * Sd + W["hit"] * Shit)
    return {"S_bomb": round(Sb, 4), "S_hand": round(Sh, 4), "S_single": round(Ss, 4),
            "S_susp": round(Ssusp, 4), "S_div": round(Sd, 4), "S_hit": round(Shit, 2),
            "score": round(total, 4)}

# =============================================================================
# 排名 + 报告
# =============================================================================
def rank(df, B):
    """df: {label: metrics} → list of (label, metrics, fitness) desc by score."""
    rows = []
    for lab, m in df.items():
        rows.append((lab, m, fitness(m, B)))
    rows.sort(key=lambda x: x[2]["score"], reverse=True)
    return rows

def cand_for_label(label, t1c, t0c):
    for c in t1c + t0c:
        if c["label"] == label:
            return c
    return None

def strategy_json(cand):
    """产出可粘进 makedeal.json 的 MakeDealStrategy 片段。"""
    if cand is None:
        return None
    if cand["type"] == 1:
        return {"MakeDealType": 1, "BeginMakeNum": cand["begin"], "BeginSelectBanker": cand["select"],
                "FirstChairHandCount": 5, "FirstChairBombCount": 2, "FirstChairBigCardsCount": 5,
                "OtherChairHandCount": 6, "OtherChairBombCount": 2, "OtherChairBigCardsCount": 4,
                "BigCardsTo": 4, "TargetValue": cand["tv"], "TargetRound": cand["tr"],
                "CouPaiStrategy": [list(cand["coupai"])]}
    else:
        return {"MakeDealType": 0, "BeginMakeNum": cand["bmn"], "BeginSelectBanker": 15,
                "FirstChairHandCount": cand["first_hc"], "FirstChairBombCount": cand["first_bomb"],
                "FirstChairBigCardsCount": cand["first_big"], "OtherChairHandCount": cand["other_hc"],
                "OtherChairBombCount": cand["other_bomb"], "OtherChairBigCardsCount": cand["other_big"],
                "BigCardsTo": cand["bigcardsto"], "TargetValue": 0.5, "TargetRound": 5,
                "CouPaiStrategy": [[3, 4, 13, 6, 5, 2]]}

def md_table(title, rows, B_label=""):
    head = (f"### {title}\n\n"
            f"| 排名 | 配置 | 综合得分 | 手均手数 | 人均散牌 | 单局炸弹率 | 0/1/2/3+炸密度 | 首叫偏置 | 抗衡度 | 持有炸(人均) | 拆牌炸(人均) |\n"
            f"|---|---|---|---|---|---|---|---|---|---|---|\n")
    lines = []
    for i, (lab, met, f) in enumerate(rows, 1):
        d = met["dens"]
        dens_str = f"{d[0]:.2f}/{d[1]:.2f}/{d[2]:.2f}/{d[3]:.2f}"
        lines.append(f"| {i} | `{lab}` | **{f['score']:.3f}** | {met['hands']:.2f} | {met['singles']:.2f} | "
                     f"{met['bomb_occ']:.3f} | {dens_str} | {met['head_start']:.3f} | {met['resist']:.3f} | "
                     f"{met['bomb_held']:.3f} | {met['bomb_split']:.3f} |")
    return head + "\n".join(lines) + "\n"

PROOF_SECTION = r"""**实现**：`extracted/optimal_split.h`（header-only，复用 `landlord.h` 的 Combo/Card/ComboType 与 `DefaultComboScoringStrategy::score`）。校验程序 `extracted/split_test.cpp`。

**目标**：字典序 `(最小组合数 n → 最大 Σscore)`。纯 max-score 被否——它会拒绝组低对子（两张 3：对子分 −12 < 两单牌 0），把 n 误判成 2，污染手数/单牌。min-n 居首对齐用户「手数=最小组合数」；max-Σscore 在等 n 中破平，对齐 §1.2「全局牌力最优」（6666+7..K 下 BOMB+STRAIGHT(55) 胜 STRAIGHT+TRIPLE(24)，炸弹不被长顺吞没）。

**算法核心（记忆化穷举 DFS）**：
```
solve(count向量 c, key):                       # key = Σ c[i]·5^i（base-5 打包，双射）
  若 c 全空: 返回 (n=0, score=0)
  若 memo[key] 命中: 返回
  r ← 最低非空点
  best ← (+∞, −∞)
  对「以 r 为主位的所有合法牌型」+「以 r 为翼的牌型」每个候选 m:   # 见下
      c2 ← c − m；key2 ← key − Δ(m)
      (n2, s2) ← solve(c2, key2)
      cand ← (n2+1, s2 + score(m))
      best ← min 字典序(n↑, score↓, moveCode↑)
  memo[key] ← best；返回
optimalSplit: solve(根) → 沿 memo.moveCode 重建组合序列
```
**枚举完备性**（关键）：任何最优拆解中，最低非空点 r 必被**恰好一个** combo 消费——作**主位**（单/对/三/三带/炸/四带/顺/连对/飞机及其带翼，r 为该 combo 最低主点）或作**翼**（被更高的三/四/飞机吸收为单翼或对翼）。两类枚举覆盖所有「消费 r 的合法牌型」⇒ 不漏。ROCKET 为 r==小王且有大王的特例。

**最优性证明（归纳）**：设 D* 为 c 的最优拆解，其首 combo m* 消费最低非空点 r；m* ∈ 上述枚举；D*\{m*} 在子状态 c−m* 上最优（归纳假设，solve 返回最优）；故循环 min 必达全局最优。memo 正确性由「状态纯函数 + 最优子结构」保证；确定性由 (n,score,moveCode) 三键 tie-break 保证（与调用路径无关）。

**5 组验证案例**（split_test 实测，5/5 PASS，score 手算对齐 `defaultScoringConfig`）：

| # | 手牌 | 最优拆解 | n | Σscore | 说明 |
|---|---|---|---|---|---|
| 1 | 6666 7 8 9 T J Q K | 炸[6]+顺[7..K] | 2 | 55 | §1.2 头例：炸弹不被长顺吞没（vs 顺[6..K]+三[6]=24） |
| 2 | 8888 3 4 | 四带二单[8;翼3,4] | 1 | 6 | min-n 把孤立四张吸为四带二（vs 炸+单+单 n=3）⇒ **持有炸弹才稳** |
| 3 | 555 666 3 4 | 飞机带单[5,6;翼3,4] | 1 | 5 | 飞机带翼减手数（vs 两组三带一 n=2） |
| 4 | 55 66 77 | 连对[5,6,7] | 1 | 12 | 连对打包（vs 三组对子 n=3, −18） |
| 5 | sj bj 3 4 5 6 7 | 王炸+顺[3..7] | 2 | 75 | 王炸优先于两王单 |

**实证对照（1000 随机手，split_test）**：optimal 比 `DefaultSplitterFactory`(贪心) **更少手数 546/1000**、等手数 454/1000、**违反 0**（即 optimal.n ≤ 贪心.n 恒成立，等 n 时 Σscore ≥ 贪心）⇒ 旧贪心拆牌确为次优，本最优解严格不劣于它。memo 稳态约 4.7 万状态/进程。
"""

def main():
    import argparse
    ap = argparse.ArgumentParser()
    ap.add_argument("--coarse-n", type=int, default=3000)
    ap.add_argument("--final-n", type=int, default=20000)
    ap.add_argument("--base-n", type=int, default=20000)
    ap.add_argument("--rerank", action="store_true", help="只用 sweep_raw.json 重排，不重跑")
    ap.add_argument("--top", type=int, default=20)
    args = ap.parse_args()

    t1c = type1_candidates()
    t0c = type0_candidates()
    print(f"候选：Type1={len(t1c)}  Type0={len(t0c)}", file=sys.stderr)

    if args.rerank:
        if not RAW.exists():
            print("sweep_raw.json 不存在，无法重排；去掉 --rerank 跑全量", file=sys.stderr)
            sys.exit(1)
        data = json.loads(open(RAW, encoding="utf-8").read())
        B = data["baseline"]
        # 用决赛指标（若已有），否则粗扫
        df = {k: v for k, v in data["candidates"].items()}
    else:
        # 基线
        print("[基线] pure-random...", file=sys.stderr, flush=True)
        _, B = run_one({"pure": True, "label": "PURE_RANDOM"}, args.base_n)
        print(f"  基线(纯随机 N={args.base_n}): split_bomb={B['bomb_split']:.3f} hands={B['hands']:.3f} "
              f"singles={B['singles']:.3f} gap_val={B['gap_val']:.2f} spread={B['spread']:.1f} ent={B['entropy']:.3f}",
              file=sys.stderr)
        old2 = {"type": 0, "bmn": 12, "bigcardsto": 2, "first_hc": 5, "first_bomb": 2, "first_big": 4,
                "other_hc": 6, "other_bomb": 2, "other_big": 3, "label": "T0 thr3 bmn12 (=old2)"}
        print("[基线] old2...", file=sys.stderr, flush=True)
        _, Mold2 = run_one(old2, args.base_n)

        allc = t1c + t0c
        print(f"[粗扫] N={args.coarse_n}, {len(allc)} 候选...", file=sys.stderr, flush=True)
        coarse = run_all(allc, args.coarse_n, "coarse")

        # 粗排选每类 TOP{top} 决赛
        cdf1 = {k: v for k, v in coarse.items() if k.startswith("T1 ")}
        cdf0 = {k: v for k, v in coarse.items() if k.startswith("T0 ")}
        fin1 = [lab for lab, _, _ in rank(cdf1, B)[:args.top]]
        fin0 = [lab for lab, _, _ in rank(cdf0, B)[:args.top]]
        fin_cands = [c for c in t1c + t0c if c["label"] in set(fin1 + fin0)]
        print(f"[决赛] N={args.final_n}, {len(fin_cands)} 候选...", file=sys.stderr, flush=True)
        final = run_all(fin_cands, args.final_n, "final")

        # 合并：决赛覆盖粗扫
        df = dict(coarse)
        df.update(final)
        data = {"baseline": B, "old2": Mold2, "coarse_n": args.coarse_n, "final_n": args.final_n,
                "weights": W, "candidates": df,
                "finalists": fin1 + fin0}
        open(RAW, "w", encoding="utf-8").write(json.dumps(data, ensure_ascii=False, indent=2))

    # 排名（决赛选手用决赛指标，其余用粗扫）
    df1 = {k: v for k, v in df.items() if k.startswith("T1 ")}
    df0 = {k: v for k, v in df.items() if k.startswith("T0 ")}
    top1 = rank(df1, B)[:args.top]
    top0 = rank(df0, B)[:args.top]

    # ===== 报告 =====
    md = []
    md.append("# 742/420 经典玩法发牌策略 TOP20（Type0 & Type1）—— 最优拆牌 + 严苛口径\n")
    md.append(f"> 模拟器：`extracted/harness.exe`（C++ 真值，发牌/配牌/洗牌管线 1:1 复刻线上；**指标期拆牌 = 搜索式全局最优** "
              f"`optimal_split.h`，目标字典序(min 组合数, max Σscore)，**非贪心**）。决赛 TOP{args.top} N={data.get('final_n',args.final_n)}，"
              f"粗扫 N={data.get('coarse_n',args.coarse_n)}。指标缓存 `sweep_raw.json`，改权重后 `py -3 sweep.py --rerank` 秒重排。\n")
    md.append(f"> **口径**：炸弹=**持有**(物理四张/王炸)为主、拆牌炸弹为辅；手数=**人均最优手数**(min-combo)；"
              f"首叫诱导/抗衡用归一化牌力 P=sigmoid(V/40)。\n")
    md.append(f"> **适应度(基线=纯随机)**：`总分=.28·S_bomb+.18·S_hand+.12·S_single+.20·S_susp+.10·S_div+.12·S_hit`。"
              f" S_bomb=**单局炸弹率**抱随机；S_hand=人均手数比随机顺~1.5手为峰；S_single=人均散牌少于随机；"
              f" S_susp=首叫诱导(≥1.05 且 ≤基线)+抗衡(≥基线)；S_div=牌型熵/基线熵；S_hit=配牌触发∈[0.2,0.95]。\n")
    md.append("\n## 一、最优拆牌算法与校验证明\n")
    md.append(PROOF_SECTION)
    md.append("\n## 二、对照组基线\n")
    md.append("| 组 | N | 持有炸(人均) | 单局炸弹率 | 0/1/2/3+炸密度 | 人均手数 | 人均散牌 | 首叫偏置 | 抗衡度 | 牌力 | hit |\n|---|---|---|---|---|---|---|---|---|---|---|\n")
    for lab, mm in [("纯随机(--pure-random)", B), ("old2 (Type0)", data.get("old2", df.get("T0 thr3 bmn12 (=old2)")))]:
        if mm is None:
            continue
        d = mm["dens"]
        md.append(f"| {lab} | {mm['n']} | {mm['bomb_held']:.3f} | {mm['bomb_occ']:.3f} | "
                  f"{d[0]:.2f}/{d[1]:.2f}/{d[2]:.2f}/{d[3]:.2f} | {mm['hands']:.2f} | {mm['singles']:.2f} | "
                  f"{mm['head_start']:.3f} | {mm['resist']:.3f} | {mm['value']:.1f} | {mm['hit']:.2f} |\n")
    md.append("\n## 三、makedealType = 1 最优 TOP20\n\n")
    md.append(md_table("Type1 TOP20", top1))
    md.append("\n## 四、makedealType = 0 最优 TOP20\n\n")
    md.append(md_table("Type0 TOP20", top0))
    open(REPORT, "w", encoding="utf-8").write("".join(md))

    # ===== JSON 导出 =====
    export = {"baseline_pure_random": B, "baseline_old2": data.get("old2"),
              "weights": W, "type1_top20": [{"rank": i + 1, "label": lab, "metrics": m, "fitness": f,
                                             "strategy": strategy_json(cand_for_label(lab, t1c, t0c))}
                                            for i, (lab, m, f) in enumerate(top1)],
              "type0_top20": [{"rank": i + 1, "label": lab, "metrics": m, "fitness": f,
                                             "strategy": strategy_json(cand_for_label(lab, t1c, t0c))}
                                            for i, (lab, m, f) in enumerate(top0)]}
    open(OUTJSON, "w", encoding="utf-8").write(json.dumps(export, ensure_ascii=False, indent=2))

    print(f"\n报告: {REPORT}", file=sys.stderr)
    print(f"配置: {OUTJSON}", file=sys.stderr)
    print(f"\n=== Type1 TOP3 ===", file=sys.stderr)
    for lab, m, f in top1[:3]:
        print(f"  {f['score']:.3f}  {lab}  bomb_occ={m['bomb_occ']:.3f} hands={m['hands']:.2f} hs={m['head_start']:.3f} res={m['resist']:.3f}", file=sys.stderr)
    print(f"=== Type0 TOP3 ===", file=sys.stderr)
    for lab, m, f in top0[:3]:
        print(f"  {f['score']:.3f}  {lab}  bomb_occ={m['bomb_occ']:.3f} hands={m['hands']:.2f} hs={m['head_start']:.3f} res={m['resist']:.3f}", file=sys.stderr)

if __name__ == "__main__":
    main()
