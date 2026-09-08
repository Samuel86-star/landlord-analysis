#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
new4 理论数据：原版 harness.exe vs 研发优化版 harness_optv2.exe 各自跑。
  1) 房间路由：--room 4484（读 makedeal.json new4，3 条 CouPaiStrategy 每局随机）× seeds {42, 2026}
  2) 单策略注入：--type1 --coupai 4,5,3,6（nopair） / 4,5,3,6,13（+bombcode）
每轮统计 abort / no-progress 打印次数；最后逐行对比同 seed 双版本输出。
"""
import subprocess, sys, os
sys.path.insert(0, os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "tools"))
from sweep import parse_metrics
from pathlib import Path

HERE = Path(__file__).resolve().parent
CFG = HERE.parent.parent / "previous" / "makedeal.json"
N = int(sys.argv[1]) if len(sys.argv) > 1 else 20000

def run(exe, tag, extra, seed):
    log = HERE / f"nb_{tag}.log"
    cmd = [str(HERE.parent / exe), "--cfg", str(CFG), "-n", str(N), "--seed", str(seed)] + extra
    with open(log, "wb") as f:
        subprocess.run(cmd, stdout=f, stderr=subprocess.DEVNULL, check=True)
    n_json, n_abort, n_noprog = 0, 0, 0
    jl = HERE / f"nb_{tag}.jsonl"
    with open(log, "rb") as fin, open(jl, "wb") as fout:
        for line in fin:
            s = line.strip()
            if s.startswith(b"{"): fout.write(line); n_json += 1
            elif b"SpliteCard aborted" in s: n_abort += 1
            elif b"no progress" in s: n_noprog += 1
    return jl, n_json, n_abort, n_noprog

KS = ["hands", "singles", "bigcards", "bomb_held", "bomb_occ", "occ_real",
      "table_3x17", "table_real", "landlord_bomb20", "head_start", "resist",
      "delta_ratio", "hit"]

SCENARIOS = [
    # (标签, harness额外参数, optv2额外参数, seed)
    ("room4484_s42",   ["--room", "4484", "--reals", "3"], ["--room", "4484", "--reals", "3"], 42),
    ("room4484_s2026", ["--room", "4484", "--reals", "3"], ["--room", "4484", "--reals", "3"], 2026),
    ("nopair",   ["--type1", "--coupai", "4,5,3,6",     "--begin", "13", "--select", "17", "--tv", "999", "--tr", "10"], None, 7),
    ("bombcode", ["--type1", "--coupai", "4,5,3,6,13",  "--begin", "13", "--select", "17", "--tv", "999", "--tr", "10"], None, 7),
]

print(f"{'='*96}")
print(f"new4 理论数据双版本对比 | N={N}/轮")
print(f"{'='*96}")

all_metrics = {}
for tag, ha, oa, seed in SCENARIOS:
    # 场景参数两版本一致（oa=None 表示同 ha）
    extra = ha if oa is None else ha
    jl_o, njo, ao, po = run("harness.exe",     f"{tag}_orig", extra, seed)
    jl_n, njn, an, pn = run("harness_optv2.exe", f"{tag}_opt",  extra, seed)
    mo = parse_metrics(jl_o); mn = parse_metrics(jl_n)
    all_metrics[tag] = (mo, mn)
    # 逐行一致性
    same = sum(1 for la, lb in zip(open(jl_o, "rb"), open(jl_n, "rb")) if la == lb)
    ntot = sum(1 for _ in open(jl_o, "rb"))
    ident = "逐行一致" if same == ntot and ntot == njn else f"分歧({ntot-same}/{ntot})"
    print(f"\n[{tag}]  原版: json={njo} abort={ao} noprog={po} | 优化版: json={njn} abort={an} noprog={pn} | {ident}")
    print(f"{'指标':<17}{'原版':>13}{'优化版':>13}{'差':>11}")
    for k in KS:
        d = mn[k] - mo[k]
        print(f"{k:<17}{mo[k]:>13.4f}{mn[k]:>13.4f}{d:>+11.4f}")
    print(f"炸分布(0/1/2/3+)  原版 {[round(x,4) for x in mo['dens']]}")
    print(f"                  优化 {[round(x,4) for x in mn['dens']]}")

# 2:1 加权混合（对齐此前 new4 报告口径）
if "nopair" in all_metrics and "bombcode" in all_metrics:
    def mix(a, b, wa=2, wb=1):
        return {k: ([(wa*x+wb*y)/(wa+wb) for x, y in zip(a[k], b[k])] if isinstance(a[k], list)
                  else (wa*a[k]+wb*b[k])/(wa+wb)) for k in a}
    for which, i in (("原版", 0), ("优化版", 1)):
        m = mix(all_metrics["nopair"][i], all_metrics["bombcode"][i])
        print(f"\n2:1 混合（{which}，对齐此前 new4 报告口径）:")
        print("  " + "  ".join(f"{k}={m[k]:.4f}" for k in KS))
