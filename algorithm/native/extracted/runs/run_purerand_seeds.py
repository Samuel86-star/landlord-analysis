#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
纯随机发牌的座位对称性检验：多种子交叉 + 合并大样本。
机制上 stride-3 交错发牌完全对称，预期座位差 = 0；单 seed 的 ±0.3~0.5 波动应为抽样噪声。
"""
import subprocess, sys, os, json, statistics as st
from pathlib import Path

HERE = Path(__file__).resolve().parent
CFG = HERE.parent.parent / "previous" / "makedeal.json"
N = int(sys.argv[1]) if len(sys.argv) > 1 else 20000
SEEDS = [42, 1, 7, 123, 2026]

all_vf = {c: [] for c in range(3)}
all_bm = {c: [] for c in range(3)}

print(f"纯随机座位对称性 | 每种子 {N} 局 × {len(SEEDS)} seeds")
print(f"{'seed':>6}{'0号位':>12}{'1号位':>12}{'2号位':>12}{'0-1差':>10}{'1-2差':>10}{'0-2差':>10}")
print("-" * 72)
for seed in SEEDS:
    log = HERE / f"pr_{seed}.log"
    cmd = [str(HERE.parent / "harness.exe"), "--cfg", str(CFG), "-n", str(N),
           "--seed", str(seed), "--pure-random"]
    with open(log, "wb") as f:
        subprocess.run(cmd, stdout=f, stderr=subprocess.DEVNULL, check=True)
    vf = {c: [] for c in range(3)}
    for line in open(log, encoding="utf-8"):
        line = line.strip()
        if not line.startswith("{"): continue
        r = json.loads(line)
        for c, s in enumerate(r["seats"]):
            vf[c].append(s["val_f"])
            all_vf[c].append(s["val_f"])
            all_bm[c].append(s["bombs"])
    m = [st.mean(vf[c]) for c in range(3)]
    se = st.stdev(vf[0]) / (N ** 0.5)
    print(f"{seed:>6}{m[0]:>12.3f}{m[1]:>12.3f}{m[2]:>12.3f}"
          f"{m[0]-m[1]:>+10.3f}{m[1]-m[2]:>+10.3f}{m[0]-m[2]:>+10.3f}   (SE≈{se:.3f})")
    log.unlink()

m = [st.mean(all_vf[c]) for c in range(3)]
bm = [st.mean(all_bm[c]) for c in range(3)]
n = len(all_vf[0])
se = st.stdev(all_vf[0]) / (n ** 0.5)
se_diff = (2 ** 0.5) * se
print("-" * 72)
print(f"合并 n={n}: val_f = {m[0]:.4f} / {m[1]:.4f} / {m[2]:.4f}")
print(f"          差值 0-1={m[0]-m[1]:+.4f}, 1-2={m[1]-m[2]:+.4f}, 0-2={m[0]-m[2]:+.4f}"
      f"   (差值SE≈{se_diff:.4f}, |z|<2 即无显著差)")
print(f"          炸弹 = {bm[0]:.4f} / {bm[1]:.4f} / {bm[2]:.4f}")
