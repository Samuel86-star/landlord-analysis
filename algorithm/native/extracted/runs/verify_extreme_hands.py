#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
构造极端手牌，测试 node budget 是否会耗尽
以及耗尽后 nHandCount 的变化
"""
from _verify_budget_bug import (
    splite_card_orig, splite_card_opt, card_name,
    HAND_SEARCH_NODE_BUDGET, cgSINGLE, cgDOUBLE, cgTHREE,
    cgSINGLE_LINE, cgDOUBLE_LINE, cgTHREE_LINE, cgBOMB_CARD
)
import time

def hand_str(hand):
    counts = {}
    for c in hand:
        counts[c] = counts.get(c, 0) + 1
    return " ".join(f"{v}x{counts[v]}" for v in sorted(counts.keys()))

def test_hand(hand, label="", budget=HAND_SEARCH_NODE_BUDGET):
    print(f"\n{'='*70}")
    print(f"测试: {label}")
    print(f"手牌 ({len(hand)} 张): {hand_str(hand)}")
    print(f"-" * 70)

    t0 = time.time()
    orig_types, n_orig = splite_card_orig(hand)
    t1 = time.time()
    opt_types, n_opt, aborted, nodes_used = splite_card_opt(hand, budget)
    t2 = time.time()

    print(f"原版: nHandCount={n_orig}, 耗时={t1-t0:.3f}s")
    print(f"  拆分: {' + '.join(f'{card_name(t[0])}({t[2]})' for t in orig_types)}")
    print(f"优化版: nHandCount={n_opt}, aborted={aborted}, nodes={nodes_used}, 耗时={t2-t1:.3f}s")
    print(f"  拆分: {' + '.join(f'{card_name(t[0])}({t[2]})' for t in opt_types)}")
    print(f"差异: nHandCount {n_opt - n_orig:+d}")

    return n_orig, n_opt, aborted, nodes_used


# 测试用例
tests = []

# 1. 大量连续三张（飞机搜索空间爆炸）
# 3-10 各 3 张 = 8 组三张 = 24 张（超过 17 张，减少点）
hand1 = []
for v in range(3, 11):  # 3..10 = 8 个点数，各 3 张 = 24 张
    hand1.extend([v] * 3)
tests.append((hand1[:17], "8 组三张中取 17 张（很多飞机候选）"))

# 2. 更多连续三张
hand2 = []
for v in range(3, 14):  # 3..13 = 11 个点数，各 2 张 = 22 张
    hand2.extend([v] * 2)
tests.append((hand2[:17], "11 组对子中取 17 张（很多连对候选）"))

# 3. 大量单张（顺子候选多）
hand3 = list(range(3, 16)) * 2  # 3..15 各 2 张 = 26 张
tests.append((hand3[:17], "13 种牌各 2 张取 17 张（顺子+连对候选）"))

# 4. 混合结构：很多三张+对子
hand4 = []
for v in range(3, 9):  # 6 组三张 = 18 张
    hand4.extend([v] * 3)
tests.append((hand4, "6 组连续三张（18张，飞机组合爆炸）"))

# 5. 真实场景：做牌过程中某一步的手牌（15 张，有很多强化过的牌型）
hand5 = []
for v in range(3, 8):  # 5 组三张 = 15 张
    hand5.extend([v] * 3)
tests.append((hand5, "5 组连续三张（15张，Type1 做牌中期可能的结构）"))

# 6. Type1 早期：13 张，较散
hand6 = list(range(3, 16))  # 13 张单牌，3..15 各一张
tests.append((hand6, "13 张单牌（3..A，顺子候选多）"))

# 7. 极端：几乎全是三张 + 几张单
hand7 = []
for v in range(3, 9):  # 6 组三张 = 18 张
    hand7.extend([v] * 3)
hand7 = hand7[:-1]  # 17 张
tests.append((hand7, "5 组三张 + 2 张单（17张）"))

# 8. 更长的飞机候选
hand8 = []
for v in range(3, 14):  # 3..K = 11 个点数各 3 张 = 33 张
    hand8.extend([v] * 3)
tests.append((hand8[:20], "11 组三张中取 20 张（地主 20 张场景）"))

# 运行测试
print("=" * 70)
print(f"node budget = {HAND_SEARCH_NODE_BUDGET}")
print("测试极端手牌是否会触发 budget 耗尽")

abort_count = 0
for hand, label in tests:
    _, _, aborted, _ = test_hand(hand, label)
    if aborted:
        abort_count += 1

print(f"\n\n{'='*70}")
print(f"总结：{len(tests)} 个测试用例，{abort_count} 个触发 budget 耗尽")

if abort_count > 0:
    # 再测一个：逐步减少 budget 看什么时候开始出问题
    print("\n\n渐进式 budget 测试：")
    hand_test = hand4  # 用 6 组三张的
    print(f"测试手牌: {hand_str(hand_test)} ({len(hand_test)} 张)")
    for budget in [1000, 5000, 10000, 50000, 100000, 500000, 1000000]:
        _, n_opt, aborted, nodes = splite_card_opt(hand_test, budget)
        _, n_orig = splite_card_orig(hand_test)
        print(f"  budget={budget:>8}: nHC_orig={n_orig}, nHC_opt={n_opt}, "
              f"diff={n_opt-n_orig:+d}, aborted={aborted}, nodes_used={nodes}")
