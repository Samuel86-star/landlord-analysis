#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
测试不同 budget 下的 nHandCount 变化
以及确认：budget 耗尽时，nHandCount 是变大还是变小
"""
from _verify_budget_bug import (
    splite_card_orig, splite_card_opt, hand_layout_key,
    HAND_SEARCH_NODE_BUDGET, random_hand_17
)
import random

def test_budget_sensitivity():
    """测试不同 budget 下的表现"""
    random.seed(42)
    n_hands = 100
    hands = [random_hand_17() for _ in range(n_hands)]

    print("不同 budget 下的表现（100 副随机 17 张手牌）")
    print("=" * 80)
    print(f"{'Budget':>10} {'abort数':>8} {'nHC_avg_orig':>12} {'nHC_avg_opt':>12} {'diff_avg':>10} {'nHC>10_orig':>12} {'nHC>10_opt':>12}")
    print("-" * 80)

    for budget in [10, 50, 100, 200, 500, 1000, 5000, 10000, 100000]:
        n_abort = 0
        diffs = []
        n_gt10_orig = 0
        n_gt10_opt = 0
        n_orig_list = []
        n_opt_list = []

        for hand in hands:
            _, n_orig = splite_card_orig(hand)
            _, n_opt, aborted, _ = splite_card_opt(hand, budget)
            n_orig_list.append(n_orig)
            n_opt_list.append(n_opt)
            if aborted:
                n_abort += 1
            diffs.append(n_opt - n_orig)
            if n_orig > 10:
                n_gt10_orig += 1
            if n_opt > 10:
                n_gt10_opt += 1

        avg_orig = sum(n_orig_list) / len(n_orig_list)
        avg_opt = sum(n_opt_list) / len(n_opt_list)
        avg_diff = sum(diffs) / len(diffs)
        print(f"{budget:>10} {n_abort:>8} {avg_orig:>12.2f} {avg_opt:>12.2f} {avg_diff:+10.2f} {n_gt10_orig:>12} {n_gt10_opt:>12}")

    print()


def test_more_complex_hands():
    """测试更复杂的手牌结构：Type1 做牌过程中可能出现的手牌
    比如：有很多三张，但被单牌打断的结构
    """
    print("更复杂手牌结构测试")
    print("=" * 80)

    test_cases = []

    # 构造：3-5 各3张 + 7-9 各3张 + 单牌若干
    # 这种不连续的三张结构，飞机搜索会有很多分支
    hand = []
    for v in [3, 4, 5, 7, 8, 9]:
        hand.extend([v] * 3)
    hand.extend([11, 12, 13, 15])  # 4 张单
    test_cases.append((hand, "6组不连续三张+4单(22张)"))

    # 3-10 各2张 + 3-5 各再加1张 = 3-5 各3张, 6-10 各2张
    hand = []
    for v in range(3, 11):
        hand.extend([v] * 2)
    for v in range(3, 6):
        hand.append(v)
    test_cases.append((hand, "3组三张+5组对子(19张)"))

    # 大量对子 + 一些三张
    hand = []
    for v in range(3, 14):  # 11 对 = 22 张
        hand.extend([v] * 2)
    hand = hand[:17]
    test_cases.append((hand, "11对中取17张"))

    # 炸弹 + 散牌
    hand = []
    hand.extend([5] * 4)  # 炸弹
    hand.extend([7] * 4)  # 炸弹
    for v in range(3, 10):
        if v != 5 and v != 7:
            hand.append(v)
    test_cases.append((hand, "2炸弹+5单(13张)"))

    # 做牌后期：手牌已经被强化过，有很多三张/对子
    hand = []
    for v in range(3, 9):  # 6 组三张 = 18 张
        hand.extend([v] * 3)
    test_cases.append((hand, "6组连续三张(18张)"))

    # 极其散的牌：每个点数1张，共13张（3..A）
    hand = list(range(3, 16))
    test_cases.append((hand, "13张散牌(顺子候选)"))

    budgets = [50, 100, 500, 1000, 5000, 10000, 100000]

    for hand, label in test_cases:
        _, n_orig = splite_card_orig(hand)
        print(f"\n{label} ({len(hand)}张), 原版nHC={n_orig}")
        print(f"  {'Budget':>8} {'nHC':>6} {'abort':>6} {'nodes':>8}")
        for b in budgets:
            _, n_opt, ab, nodes = splite_card_opt(hand, b)
            print(f"  {b:>8} {n_opt:>6} {str(ab):>6} {nodes:>8}")
            if not ab:
                pass  # 还没触发，继续看更低 budget


def test_abort_nhc_direction():
    """确认：abort 后 nHandCount 是变大还是变小
    用极低 budget 强制 abort，观察变化方向
    """
    print("\n\n确认 abort 后 nHandCount 变化方向")
    print("=" * 80)
    random.seed(123)
    n_hands = 50
    hands = [random_hand_17() for _ in range(n_hands)]

    # 用极低 budget 强制 abort
    budget = 20
    n_increase = 0
    n_decrease = 0
    n_same = 0
    total_diff = 0
    n_abort = 0

    for hand in hands:
        _, n_orig = splite_card_orig(hand)
        _, n_opt, aborted, _ = splite_card_opt(hand, budget)
        if aborted:
            n_abort += 1
            diff = n_opt - n_orig
            total_diff += diff
            if diff > 0:
                n_increase += 1
            elif diff < 0:
                n_decrease += 1
            else:
                n_same += 1

    print(f"Budget={budget}, 测试 {n_hands} 副, {n_abort} 副触发 abort")
    if n_abort > 0:
        print(f"  nHC 变大: {n_increase}")
        print(f"  nHC 变小: {n_decrease}")
        print(f"  nHC 不变: {n_same}")
        print(f"  平均变化: {total_diff/n_abort:+.2f}")


if __name__ == "__main__":
    test_budget_sensitivity()
    test_more_complex_hands()
    test_abort_nhc_direction()
