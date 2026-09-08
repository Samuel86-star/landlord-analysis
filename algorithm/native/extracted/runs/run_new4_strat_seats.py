#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
new4 两种 CouPaiStrategy 的座位不对称分解：
  A. 纯随机（--pure-random，不做牌）        → 对照组，预期座位无差
  B. [4,5,3,6]（nopair，无炸弹码）          → 无 13 的策略
  C. [4,5,3,6,13]（+bombcode，末位炸弹码）  → 含 13 的策略
三家同策略注入（--type1 --coupai），隔离"策略本身"的座位效应。
"""
import subprocess, sys, os, json, statistics as st
from pathlib import Path

HERE = Path(__file__).resolve().parent
CFG = HERE.parent.parent / "previous" / "makedeal.json"
N = int(sys.argv[1]) if len(sys.argv) > 1 else 20000
SEED = 42

SETUPS = [
    ("纯随机(不做牌)",   ["--pure-random"]),
    ("[4,5,3,6]",        ["--type1", "--coupai", "4,5,3,6",    "--begin", "13", "--select", "17", "--tv", "999", "--tr", "10"]),
    ("[4,5,3,6,13]",     ["--type1", "--coupai", "4,5,3,6,13", "--begin", "13", "--select", "17", "--tv", "999", "--tr", "10"]),
]

def run(extra, tag):
    log = HERE / f"ss_{tag}.log"
    cmd = [str(HERE.parent / "harness.exe"), "--cfg", str(CFG), "-n", str(N), "--seed", str(SEED)] + extra
    with open(log, "wb") as f:
        subprocess.run(cmd, stdout=f, stderr=subprocess.DEVNULL, check=True)
    return [json.loads(l) for l in open(log, encoding="utf-8") if l.strip().startswith("{")]

results = {}
print(f"{'='*100}")
print(f"new4 策略分解 | 三家同策略注入 | N={N}, seed={SEED}")
print(f"{'='*100}")
for i, (name, extra) in enumerate(SETUPS):
    deals = run(extra, str(i))
    per = {c: {"val_f": [], "bombs": [], "opt_hands": [], "singles": []} for c in range(3)}
    n_max = [0, 0, 0]
    for r in deals:
        vals = [s["val_f"] for s in r["seats"]]
        n_max[max(range(3), key=lambda j: vals[j])] += 1
        for c, s in enumerate(r["seats"]):
            per[c]["val_f"].append(s["val_f"]); per[c]["bombs"].append(s["bombs"])
            per[c]["opt_hands"].append(s["opt_hands"]); per[c]["singles"].append(s["singles"])
    results[name] = per
    n = len(deals)
    m = lambda k: [st.mean(per[c][k]) for c in range(3)]
    vf, bm, hd, sg = m("val_f"), m("bombs"), m("opt_hands"), m("singles")
    se = st.stdev(per[0]["val_f"]) / (n ** 0.5)
    print(f"\n[{name}]  (n={n}局, val_f SE≈{se:.4f})")
    print(f"  {'':14}{'0号位':>12}{'1号位':>12}{'2号位':>12}{'0-1差':>10}{'1-2差':>10}{'0-2差':>10}")
    print(f"  {'牌力val_f':<14}{vf[0]:>12.3f}{vf[1]:>12.3f}{vf[2]:>12.3f}{vf[0]-vf[1]:>+10.3f}{vf[1]-vf[2]:>+10.3f}{vf[0]-vf[2]:>+10.3f}")
    print(f"  {'持有炸弹':<14}{bm[0]:>12.4f}{bm[1]:>12.4f}{bm[2]:>12.4f}{bm[0]-bm[1]:>+10.4f}{bm[1]-bm[2]:>+10.4f}{bm[0]-bm[2]:>+10.4f}")
    print(f"  {'手数(最优拆)':<14}{hd[0]:>12.3f}{hd[1]:>12.3f}{hd[2]:>12.3f}{hd[0]-hd[1]:>+10.3f}{hd[1]-hd[2]:>+10.3f}{hd[0]-hd[2]:>+10.3f}")
    print(f"  {'单牌数':<14}{sg[0]:>12.3f}{sg[1]:>12.3f}{sg[2]:>12.3f}{sg[0]-sg[1]:>+10.3f}{sg[1]-sg[2]:>+10.3f}{sg[0]-sg[2]:>+10.3f}")
    print(f"  最强座占比: " + " / ".join(f"{x/n*100:.1f}%" for x in n_max))

# 汇总对比
print(f"\n{'='*100}")
print("汇总：0-2 号位牌力梯度（val_f）")
for name, per in results.items():
    vf = [st.mean(per[c]["val_f"]) for c in range(3)]
    bm = [st.mean(per[c]["bombs"]) for c in range(3)]
    print(f"  {name:<18} val_f: {vf[0]:6.2f} / {vf[1]:6.2f} / {vf[2]:6.2f}   梯度0-2={vf[0]-vf[2]:+.2f}   炸弹: {bm[0]:.3f}/{bm[1]:.3f}/{bm[2]:.3f}  炸弹梯度={bm[0]-bm[2]:+.3f}")
