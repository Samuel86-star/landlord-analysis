#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
A/B 对比：原版 harness.exe vs 研发优化版 harness_optv2.exe
跑真实路由（--room 4484 → makedeal.json new4 配置，3 条 CouPaiStrategy 每局随机）
同 seed 下逐行对比输出 + 统计 abort 次数。
"""
import subprocess, sys, os
sys.path.insert(0, os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "tools"))
from sweep import parse_metrics
from pathlib import Path

HERE = Path(__file__).resolve().parent
CFG = HERE.parent.parent / "previous" / "makedeal.json"
N = int(sys.argv[1]) if len(sys.argv) > 1 else 20000
SEED = int(sys.argv[2]) if len(sys.argv) > 2 else 42

def run(exe, out_jsonl, out_log):
    cmd = [str(HERE.parent / exe), "--cfg", str(CFG), "--room", "4484", "--reals", "3",
           "-n", str(N), "--seed", str(SEED)]
    with open(out_log, "wb") as f:
        subprocess.run(cmd, stdout=f, stderr=subprocess.DEVNULL, check=True)
    # 分离 JSON 行与 abort/no-progress 打印行
    n_json, n_abort, n_noprog = 0, 0, 0
    with open(out_log, "rb") as fin, open(out_jsonl, "wb") as fout:
        for line in fin:
            s = line.strip()
            if s.startswith(b"{"):
                fout.write(line); n_json += 1
            elif b"SpliteCard aborted" in s: n_abort += 1
            elif b"no progress" in s: n_noprog += 1
    return n_json, n_abort, n_noprog

print(f"{'='*74}")
print(f"A/B: harness.exe(原版) vs harness_optv2.exe(研发优化版) | new4@room4484")
print(f"N={N}, seed={SEED}")
print(f"{'='*74}")

ja, aa, pa = run("harness.exe", HERE/f"ab_orig_{SEED}.jsonl", HERE/f"ab_orig_{SEED}.log")
jb, ab_, pb = run("harness_optv2.exe", HERE/f"ab_opt_{SEED}.jsonl", HERE/f"ab_opt_{SEED}.log")
print(f"原版:    JSON行={ja}, abort={aa}, noprog={pa}")
print(f"优化版:  JSON行={jb}, abort={ab_}, noprog={pb}")

# 逐行对比
same, diff = 0, 0
first_diffs = []
with open(HERE/f"ab_orig_{SEED}.jsonl", "rb") as fa, open(HERE/f"ab_opt_{SEED}.jsonl", "rb") as fb:
    for i, (la, lb) in enumerate(zip(fa, fb)):
        if la == lb: same += 1
        else:
            diff += 1
            if len(first_diffs) < 5: first_diffs.append(i)
print(f"\n逐行对比: 相同={same}, 不同={diff}")
if first_diffs:
    print(f"首个分歧行号: {first_diffs}")

# 指标对比（即使逐行不同，统计意义更重要）
try:
    ma = parse_metrics(HERE/f"ab_orig_{SEED}.jsonl")
    mb = parse_metrics(HERE/f"ab_opt_{SEED}.jsonl")
    ks = ["hands", "singles", "bigcards", "bomb_held", "bomb_occ", "occ_real",
          "table_3x17", "table_real", "landlord_bomb20", "head_start", "resist",
          "delta_ratio", "hit"]
    print(f"\n{'指标':<18} {'原版':>14} {'优化版':>14} {'差':>12}")
    print("-" * 62)
    for k in ks:
        d = mb[k] - ma[k]
        flag = "  ←←" if abs(d) > 0.002 and k != "hit" else ""
        print(f"{k:<18} {ma[k]:>14.4f} {mb[k]:>14.4f} {d:>+12.4f}{flag}")
    print(f"\n炸分布 3×17 (0/1/2/3+):")
    print(f"  原版:   {[round(x,4) for x in ma['dens']]}")
    print(f"  优化版: {[round(x,4) for x in mb['dens']]}")
except Exception as e:
    print(f"parse_metrics 失败: {e}")
