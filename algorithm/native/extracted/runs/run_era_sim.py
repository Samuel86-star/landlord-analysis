#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
时代模拟：pre-9.1 还原配置 vs post-9.1 现行配置，harness.exe（原版代码），--reals 1（1真人+2机器人）。
目的：检验 9.1 配置包里的机器人路由变化（通用 robot[b14温和] → robot4484[b13恒强]）
      能否复现数仓观察到的剪刀差（真人牌力↑ + 机器人牌力↓）。

pre-9.1 配置 = git HEAD 版 + {4484,12074}→new4 + new4 块（HEAD 里没有 new4，从现行版拷贝）
post-9.1 配置 = previous/makedeal.json 现行版（含 robot4484/robot12074）
"""
import subprocess, sys, os, json
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from pathlib import Path

HERE = Path(__file__).resolve().parent
CUR = HERE.parent.parent / "previous" / "makedeal.json"
PRE = HERE.parent / "results" / "makedeal_pre91.json"
N = int(sys.argv[1]) if len(sys.argv) > 1 else 20000
SEED = 42

# ---- 构建 pre-9.1 配置 ----
import subprocess as sp
head_txt = sp.run(["git", "-C", str(HERE.parents[3]), "show",
                   "HEAD:algorithm/native/previous/makedeal.json"],
                  capture_output=True, text=True, check=True).stdout
cur = json.loads(CUR.read_text(encoding="utf-8"))
new4_block = cur["MakeDealStrategy"]["new4"]
pre = json.loads(head_txt)
pre["MakeDeal"]["4484"] = "new4"
pre["MakeDeal"]["12074"] = "new4"
pre["MakeDealStrategy"]["new4"] = new4_block
assert "robot4484" not in pre["MakeDealStrategy"], "pre-9.1 不应有 robot4484"
PRE.write_text(json.dumps(pre, ensure_ascii=False, indent=2), encoding="utf-8")
print(f"pre-9.1 配置已构建: {PRE}")
print(f"  pre  robot 通用: bmn={pre['MakeDealStrategy']['robot']['BeginMakeNum']}, "
      f"tv={pre['MakeDealStrategy']['robot']['TargetValue']}, tr={pre['MakeDealStrategy']['robot']['TargetRound']}")
print(f"  post robot4484 : bmn={cur['MakeDealStrategy']['robot4484']['BeginMakeNum']}, "
      f"tv={cur['MakeDealStrategy']['robot4484']['TargetValue']}, tr={cur['MakeDealStrategy']['robot4484']['TargetRound']}")
print()

def run(cfg, tag):
    jl = HERE / f"era_{tag}.jsonl"
    cmd = [str(HERE.parent / "harness.exe"), "--cfg", str(cfg), "--room", "4484", "--reals", "1",
           "-n", str(N), "--seed", str(SEED)]
    with open(HERE / f"era_{tag}.log", "wb") as f:
        subprocess.run(cmd, stdout=f, stderr=subprocess.DEVNULL, check=True)
    with open(HERE / f"era_{tag}.log", "rb") as fin, open(jl, "wb") as fout:
        for line in fin:
            if line.strip().startswith(b"{"):
                fout.write(line)
    return jl

def seat_stats(jl):
    """按真人/机器人汇总 val_f / split_bombs / bombs / handcount"""
    acc = {"human": [], "robot": []}
    b = {"human": [], "robot": []}
    hb = {"human": [], "robot": []}
    hcn = {"human": [], "robot": []}
    sing = {"human": [], "robot": []}
    for line in open(jl, encoding="utf-8"):
        line = line.strip()
        if not line.startswith("{"): continue
        g = json.loads(line)
        for s in g["seats"]:
            k = "robot" if s["is_robot"] else "human"
            acc[k].append(s["val_f"]); b[k].append(s["split_bombs"])
            hb[k].append(s["bombs"]); hcn[k].append(s["handcount"]); sing[k].append(s["singles"])
    def avg(x): return sum(x) / len(x) if x else 0.0
    return {k: {"n": len(acc[k]), "val_f": avg(acc[k]), "split_bombs": avg(b[k]),
                "bombs": avg(hb[k]), "handcount": avg(hcn[k]), "singles": avg(sing[k])}
            for k in ("human", "robot")}

print(f"{'='*84}")
print(f"时代模拟 | harness.exe 原版代码 | room4484, 1真人+2机器人, N={N}, seed={SEED}")
print(f"{'='*84}")
res = {}
for cfg, tag in [(PRE, "pre91"), (CUR, "post91")]:
    st = seat_stats(run(cfg, tag))
    res[tag] = st
    print(f"\n[{tag}]")
    print(f"{'':12}{'n':>8}{'val_f(牌力)':>13}{'split_bombs':>13}{'bombs(held)':>13}{'handcount':>11}{'singles':>9}")
    for k in ("human", "robot"):
        s = st[k]
        print(f"{k:<12}{s['n']:>8}{s['val_f']:>13.2f}{s['split_bombs']:>13.4f}{s['bombs']:>13.4f}{s['handcount']:>11.3f}{s['singles']:>9.3f}")

h0, r0 = res["pre91"]["human"], res["pre91"]["robot"]
h1, r1 = res["post91"]["human"], res["post91"]["robot"]
print(f"\n{'='*84}")
print(f"时代差（post - pre）:")
print(f"  真人  val_f: {h0['val_f']:>8.2f} → {h1['val_f']:>8.2f}  ({h1['val_f']-h0['val_f']:+.2f})")
print(f"  机器人 val_f: {r0['val_f']:>8.2f} → {r1['val_f']:>8.2f}  ({r1['val_f']-r0['val_f']:+.2f})")
print(f"  牌力差(真人-机器人): {h0['val_f']-r0['val_f']:>8.2f} → {h1['val_f']-r1['val_f']:>8.2f}  ({(h1['val_f']-r1['val_f'])-(h0['val_f']-r0['val_f']):+.2f})")
print(f"  真人  split_bombs: {h0['split_bombs']:.4f} → {h1['split_bombs']:.4f}  ({h1['split_bombs']-h0['split_bombs']:+.4f})")
print(f"  机器人 split_bombs: {r0['split_bombs']:.4f} → {r1['split_bombs']:.4f}  ({r1['split_bombs']-r0['split_bombs']:+.4f})")
print(f"\n数仓参照（1h2r 局, card_power）: 真人 31.25→41.98 (+10.7), 机器人 7.52→0.59 (-6.9)")
