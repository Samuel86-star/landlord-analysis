#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
stats.py — 聚合 harness 产出的 JSONL，打印每组配置的发牌统计。

用法：
  py -3 stats.py out_742_3.jsonl out_420_3.jsonl
  py -3 stats.py out_742_3.jsonl            # 单文件
  py -3 stats.py out_*.jsonl                # 通配

JSONL 每行 = 一局：{"deal","room","reals","banker","seats":[{"seat","is_robot","hand","bombs","handcount","bigcards","makedeal"}],"bottom"}
"""
import json
import sys
from collections import Counter

try:
    sys.stdout.reconfigure(encoding="utf-8", errors="replace")
except Exception:
    pass


def agg(path):
    rb, rh, rbig, ob, oh, obig = [], [], [], [], [], []
    md_real, md_robot = Counter(), Counter()
    banker_real = 0
    n = 0
    room = reals = None
    with open(path, encoding="utf-8-sig") as f:
        for line in f:
            line = line.strip()
            if not line:
                continue
            d = json.loads(line)
            n += 1
            room, reals = d["room"], d["reals"]
            if not d["seats"][d["banker"]]["is_robot"]:
                banker_real += 1
            for s in d["seats"]:
                bomb, hc, big, md = s["bombs"], s["handcount"], s["bigcards"], s["makedeal"]
                if s["is_robot"]:
                    ob.append(bomb); oh.append(hc); obig.append(big); md_robot[md] += 1
                else:
                    rb.append(bomb); rh.append(hc); rbig.append(big); md_real[md] += 1
    avg = lambda x: sum(x) / len(x) if x else 0.0
    has = lambda x: sum(1 for v in x if v > 0) / len(x) if x else 0.0

    print("=" * 64)
    print(f"{path}")
    print(f"房间 {room} | 真人数 {reals} | N={n} 局")
    print("-" * 64)
    if rb:
        print(f"  [真人]   n={len(rb)}")
        print(f"           炸弹均值={avg(rb):.4f}  有炸率={has(rb):.4f}")
        print(f"           炸弹分布={dict(sorted(Counter(rb).items()))}")
        print(f"           平均手数={avg(rh):.3f}  平均大牌(2/王)={avg(rbig):.3f}")
        print(f"           做牌类型分布={dict(sorted(md_real.items()))}")
    if ob:
        print(f"  [机器人] n={len(ob)}")
        print(f"           炸弹均值={avg(ob):.4f}  有炸率={has(ob):.4f}")
        print(f"           炸弹分布={dict(sorted(Counter(ob).items()))}")
        print(f"           平均手数={avg(oh):.3f}  平均大牌(2/王)={avg(obig):.3f}")
        print(f"           做牌类型分布={dict(sorted(md_robot.items()))}")
    print(f"  [庄家为真人比率] {banker_real / n:.3f}")


def main():
    args = sys.argv[1:]
    if not args:
        print(__doc__)
        sys.exit(1)
    print("=== harness JSONL 聚合（线上 C++ 发牌真值）===")
    for p in args:
        try:
            agg(p)
        except FileNotFoundError:
            print(f"[跳过] 文件不存在: {p}")
    print("=" * 64)


if __name__ == "__main__":
    main()
