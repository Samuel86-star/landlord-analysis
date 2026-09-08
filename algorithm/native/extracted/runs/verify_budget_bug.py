#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
验证假设：node budget 耗尽时，SpliteCard 拆出来的牌型数(nHandCount)会变大，
从而导致 Type1 配牌判断 nHandCount > TargetRound 时更容易触发强做牌。

用 Python 实现两个版本的 get_MaxHandCardValue / GetBestCardType / SpliteCard：
  A. 原版（精确搜索，无 budget）
  B. 优化版（memo + node budget，budget 耗尽时 abort）

对一批随机 17 张手牌做对比，观察：
  - nHandCount 差异（牌型数）
  - 哪些情况会触发 budget 耗尽
  - 对 nHandCount > 10 的触发率影响（对应 TargetRound=10 的判断条件）
"""
import random
import sys
from collections import deque

# =============================================================================
# 常量（与 C++ 对齐）
# =============================================================================
cgERROR = -1
cgSINGLE = 0
cgDOUBLE = 1
cgTHREE = 2
cgSINGLE_LINE = 3
cgDOUBLE_LINE = 4
cgTHREE_LINE = 5
cgFOUR_TAKE_ONE = 6
cgFOUR_TAKE_TWO = 7
cgBOMB_CARD = 8
cgKING_CARD = 9

MinCardsValue = -999
HAND_SEARCH_NODE_BUDGET = 100000

# GroupDataExp 简化版——只需要各牌型的分值用于比较
# 实际上 nHandCount 只和 CardGroupDatas.size() 有关，和分值无关
# 所以这里分值只影响 GetBestCardType 的选择顺序，不影响最终 nHandCount
# 但为了与真实代码一致，我们用 makedeal.json 里的 GroupDataExp 简单近似


def group_value(cg_type, max_card, count):
    """简化版 get_GroupData 的 nValue，只用于比较优劣"""
    if cg_type == cgSINGLE:
        return max_card - 10
    elif cg_type == cgDOUBLE:
        return max_card - 10
    elif cg_type == cgTHREE:
        return max_card - 10
    elif cg_type == cgBOMB_CARD:
        return (max_card - 3) * 2 + 7  # 近似
    elif cg_type == cgKING_CARD:
        return 20
    elif cg_type == cgSINGLE_LINE:
        return max_card - 10
    elif cg_type == cgDOUBLE_LINE:
        return max_card - 10
    elif cg_type == cgTHREE_LINE:
        return (max_card - 3) // 2  # 近似
    elif cg_type == cgFOUR_TAKE_ONE or cg_type == cgFOUR_TAKE_TWO:
        return 0
    return -999


# =============================================================================
# SurCardsType（判断剩余手牌是否已经是单一一手牌型）
# =============================================================================
def sur_cards_type(arr):
    """arr: list of 18 ints (3..17)"""
    n_count = sum(arr[3:18])
    if n_count == 0:
        return (cgERROR, 0, -1)

    # 单张
    singles = [i for i in range(3, 18) if arr[i] == 1]
    if n_count == 1 and len(singles) == 1:
        return (cgSINGLE, 1, singles[0])

    # 对子
    doubles = [i for i in range(3, 16) if arr[i] == 2]
    if n_count == 2 and len(doubles) == 1:
        return (cgDOUBLE, 2, doubles[0])

    # 三张
    threes = [i for i in range(3, 16) if arr[i] == 3]
    if n_count == 3 and len(threes) == 1:
        return (cgTHREE, 3, threes[0])

    # 炸弹
    bombs = [i for i in range(3, 16) if arr[i] == 4]
    if n_count == 4 and len(bombs) == 1:
        return (cgBOMB_CARD, 4, bombs[0])

    # 王炸
    if n_count == 2 and arr[16] > 0 and arr[17] > 0:
        return (cgKING_CARD, 2, 17)

    # 顺子（5+ 张连续单牌）
    for start in range(3, 11):  # A(14) - 5 + 1 = 10, so start<=10
        length = 0
        for i in range(start, 15):
            if arr[i] == 1:
                length += 1
            else:
                break
        if length >= 5 and length == n_count:
            return (cgSINGLE_LINE, length, start + length - 1)

    # 连对（3+ 对连续对子）
    for start in range(3, 12):
        length = 0
        for i in range(start, 15):
            if arr[i] == 2:
                length += 1
            else:
                break
        if length >= 3 and length * 2 == n_count:
            return (cgDOUBLE_LINE, length * 2, start + length - 1)

    # 飞机（2+ 组连续三张）
    for start in range(3, 13):
        length = 0
        for i in range(start, 15):
            if arr[i] == 3:
                length += 1
            else:
                break
        if length >= 2 and length * 3 == n_count:
            return (cgTHREE_LINE, length * 3, start + length - 1)

    return (cgERROR, n_count, -1)


# =============================================================================
# 版本 A：原版 get_MaxHandCardValue（精确搜索，无 budget）
# =============================================================================
class HandCardValue:
    def __init__(self, sum_val=0, need_round=0):
        self.sum_val = sum_val
        self.need_round = need_round


def hv_score(hv):
    return hv.sum_val - hv.need_round * 7


def get_max_hand_value_orig(arr_hand):
    """原版：递归精确搜索"""
    n_count = sum(arr_hand[3:18])
    if n_count == 0:
        return HandCardValue(0, 0)

    scd_type, scd_count, scd_max = sur_cards_type(arr_hand)
    if scd_type != cgERROR and scd_type != cgFOUR_TAKE_ONE and scd_type != cgFOUR_TAKE_TWO:
        return HandCardValue(group_value(scd_type, scd_max, scd_count), 1)

    # GetBestCardType 搜索最优牌型
    best_hv = HandCardValue(MinCardsValue, 20)
    best_group = (cgERROR, 0, -1)  # (type, count, max_card)

    L = arr_hand
    for i in range(3, 16):
        if L[i] == 0 or L[i] == 4:
            continue

        # 单张
        if L[i] >= 1:
            L[i] -= 1
            tmp = get_max_hand_value_orig(L)
            L[i] += 1
            if hv_score(best_hv) <= hv_score(tmp):
                best_hv = tmp
                best_group = (cgSINGLE, 1, i)

        # 对子
        if L[i] >= 2:
            L[i] -= 2
            tmp = get_max_hand_value_orig(L)
            L[i] += 2
            if hv_score(best_hv) <= hv_score(tmp):
                best_hv = tmp
                best_group = (cgDOUBLE, 2, i)

        # 三张
        if L[i] >= 3:
            L[i] -= 3
            tmp = get_max_hand_value_orig(L)
            L[i] += 3
            if hv_score(best_hv) <= hv_score(tmp):
                best_hv = tmp
                best_group = (cgTHREE, 3, i)

        # 顺子
        if L[i] >= 1:
            prov = 0
            for j in range(i, 15):
                if L[j] >= 1:
                    prov += 1
                else:
                    break
                if prov >= 5:
                    for k in range(i, j + 1):
                        L[k] -= 1
                    tmp = get_max_hand_value_orig(L)
                    for k in range(i, j + 1):
                        L[k] += 1
                    if hv_score(best_hv) <= hv_score(tmp):
                        best_hv = tmp
                        best_group = (cgSINGLE_LINE, prov, j)

        # 连对
        if L[i] >= 2:
            prov = 0
            for j in range(i, 15):
                if L[j] >= 2:
                    prov += 1
                else:
                    break
                if prov >= 3:
                    for k in range(i, j + 1):
                        L[k] -= 2
                    tmp = get_max_hand_value_orig(L)
                    for k in range(i, j + 1):
                        L[k] += 2
                    if hv_score(best_hv) <= hv_score(tmp):
                        best_hv = tmp
                        best_group = (cgDOUBLE_LINE, prov * 2, j)

        # 飞机
        if L[i] >= 3:
            prov = 0
            for j in range(i, 15):
                if L[j] >= 3:
                    prov += 1
                else:
                    break
                if prov >= 2:
                    for k in range(i, j + 1):
                        L[k] -= 3
                    tmp = get_max_hand_value_orig(L)
                    for k in range(i, j + 1):
                        L[k] += 3
                    if hv_score(best_hv) <= hv_score(tmp):
                        best_hv = tmp
                        best_group = (cgTHREE_LINE, prov * 3, j)

        # 原版 C++ 代码在找到第一个非空的 i 并选出最优后直接 return
        # （因为 for 循环里 L[i]!=0 && L[i]!=4 的第一个就会 return）
        # 等等——看 C++ 代码，确实是找到第一个 i 满足条件就处理并 return
        # 因为 if (L[i] != 0 && L[i] != 4) { ... 各种尝试 ... return; }
        if best_group[0] != cgERROR:
            break  # 模拟 C++ 的行为：第一个有牌的点就 return

    # 兜底：小王单张 / 大王单张 / 炸弹 / 王炸 / 最小单张
    if best_group[0] == cgERROR:
        if arr_hand[16] == 1 and arr_hand[17] == 0:
            best_group = (cgSINGLE, 1, 16)
        elif arr_hand[16] == 0 and arr_hand[17] == 1:
            best_group = (cgSINGLE, 1, 17)
        else:
            # 炸弹
            for i in range(3, 16):
                if arr_hand[i] == 4:
                    best_group = (cgBOMB_CARD, 4, i)
                    break
            if best_group[0] == cgERROR:
                if arr_hand[16] > 0 and arr_hand[17] > 0:
                    best_group = (cgKING_CARD, 2, 17)
                else:
                    # 最小单张
                    for i in range(3, 18):
                        if arr_hand[i] > 0:
                            best_group = (cgSINGLE, 1, i)
                            break

    # 返回：当前牌型 + 剩余牌的最优值
    remaining = list(arr_hand)
    bg_type, bg_count, bg_max = best_group
    if bg_type in (cgSINGLE, cgBOMB_CARD, cgKING_CARD):
        remaining[bg_max] -= bg_count
    elif bg_type == cgDOUBLE:
        remaining[bg_max] -= 2
    elif bg_type == cgTHREE:
        remaining[bg_max] -= 3
    elif bg_type == cgSINGLE_LINE:
        for k in range(bg_max - bg_count + 1, bg_max + 1):
            remaining[k] -= 1
    elif bg_type == cgDOUBLE_LINE:
        n_pairs = bg_count // 2
        for k in range(bg_max - n_pairs + 1, bg_max + 1):
            remaining[k] -= 2
    elif bg_type == cgTHREE_LINE:
        n_triples = bg_count // 3
        for k in range(bg_max - n_triples + 1, bg_max + 1):
            remaining[k] -= 3

    rest = get_max_hand_value_orig(remaining)
    return HandCardValue(
        group_value(bg_type, bg_max, bg_count) + rest.sum_val,
        1 + rest.need_round
    )


def splite_card_orig(hand_cards):
    """原版 SpliteCard：返回 (card_types_list, n_hand_count)
    card_types_list: list of (cgType, count, maxCard)
    """
    arr = [0] * 18
    for c in hand_cards:
        arr[c] += 1

    card_types = []
    while True:
        n_count = sum(arr[3:18])
        if n_count <= 0:
            break
        # 用 get_max_hand_value_orig 的思路选一张最优牌型
        # 简化：直接调用 get_best_card_type_orig
        best = get_best_card_type_orig(arr)
        if best[0] == cgERROR:
            break
        card_types.append(best)
        bg_type, bg_count, bg_max = best
        if bg_type in (cgSINGLE, cgBOMB_CARD, cgKING_CARD):
            arr[bg_max] -= bg_count
        elif bg_type == cgDOUBLE:
            arr[bg_max] -= 2
        elif bg_type == cgTHREE:
            arr[bg_max] -= 3
        elif bg_type == cgSINGLE_LINE:
            for k in range(bg_max - bg_count + 1, bg_max + 1):
                arr[k] -= 1
        elif bg_type == cgDOUBLE_LINE:
            n_pairs = bg_count // 2
            for k in range(bg_max - n_pairs + 1, bg_max + 1):
                arr[k] -= 2
        elif bg_type == cgTHREE_LINE:
            n_triples = bg_count // 3
            for k in range(bg_max - n_triples + 1, bg_max + 1):
                arr[k] -= 3

    return card_types, len(card_types)


def get_best_card_type_orig(arr_hand):
    """原版 GetBestCardType：返回 (cgType, count, maxCard)"""
    scd_type, scd_count, scd_max = sur_cards_type(arr_hand)
    if scd_type != cgERROR and scd_type != cgFOUR_TAKE_ONE and scd_type != cgFOUR_TAKE_TWO:
        return (scd_type, scd_count, scd_max)

    best_hv = HandCardValue(MinCardsValue, 21)
    best_group = (cgERROR, 0, -1)
    L = arr_hand

    for i in range(3, 16):
        if L[i] == 0 or L[i] == 4:
            continue

        # 单张
        if L[i] >= 1:
            L[i] -= 1
            tmp = get_max_hand_value_orig(L)
            L[i] += 1
            if hv_score(best_hv) <= hv_score(tmp):
                best_hv = tmp
                best_group = (cgSINGLE, 1, i)

        # 对子
        if L[i] >= 2:
            L[i] -= 2
            tmp = get_max_hand_value_orig(L)
            L[i] += 2
            if hv_score(best_hv) <= hv_score(tmp):
                best_hv = tmp
                best_group = (cgDOUBLE, 2, i)

        # 三张
        if L[i] >= 3:
            L[i] -= 3
            tmp = get_max_hand_value_orig(L)
            L[i] += 3
            if hv_score(best_hv) <= hv_score(tmp):
                best_hv = tmp
                best_group = (cgTHREE, 3, i)

        # 顺子
        if L[i] >= 1:
            prov = 0
            for j in range(i, 15):
                if L[j] >= 1:
                    prov += 1
                else:
                    break
                if prov >= 5:
                    for k in range(i, j + 1):
                        L[k] -= 1
                    tmp = get_max_hand_value_orig(L)
                    for k in range(i, j + 1):
                        L[k] += 1
                    if hv_score(best_hv) <= hv_score(tmp):
                        best_hv = tmp
                        best_group = (cgSINGLE_LINE, prov, j)

        # 连对
        if L[i] >= 2:
            prov = 0
            for j in range(i, 15):
                if L[j] >= 2:
                    prov += 1
                else:
                    break
                if prov >= 3:
                    for k in range(i, j + 1):
                        L[k] -= 2
                    tmp = get_max_hand_value_orig(L)
                    for k in range(i, j + 1):
                        L[k] += 2
                    if hv_score(best_hv) <= hv_score(tmp):
                        best_hv = tmp
                        best_group = (cgDOUBLE_LINE, prov * 2, j)

        # 飞机
        if L[i] >= 3:
            prov = 0
            for j in range(i, 15):
                if L[j] >= 3:
                    prov += 1
                else:
                    break
                if prov >= 2:
                    for k in range(i, j + 1):
                        L[k] -= 3
                    tmp = get_max_hand_value_orig(L)
                    for k in range(i, j + 1):
                        L[k] += 3
                    if hv_score(best_hv) <= hv_score(tmp):
                        best_hv = tmp
                        best_group = (cgTHREE_LINE, prov * 3, j)

        break  # C++ 代码：第一个有牌的 i 处理完就 return

    # 兜底
    if best_group[0] == cgERROR:
        if arr_hand[16] == 1 and arr_hand[17] == 0:
            best_group = (cgSINGLE, 1, 16)
        elif arr_hand[16] == 0 and arr_hand[17] == 1:
            best_group = (cgSINGLE, 1, 17)
        else:
            for i in range(3, 16):
                if arr_hand[i] == 4:
                    best_group = (cgBOMB_CARD, 4, i)
                    break
            if best_group[0] == cgERROR:
                if arr_hand[16] > 0 and arr_hand[17] > 0:
                    best_group = (cgKING_CARD, 2, 17)
                else:
                    for i in range(3, 18):
                        if arr_hand[i] > 0:
                            best_group = (cgSINGLE, 1, i)
                            break

    return best_group


# =============================================================================
# 版本 B：优化版（memo + node budget）
# =============================================================================
class OptContext:
    def __init__(self, budget=HAND_SEARCH_NODE_BUDGET):
        self.memo = {}
        self.node_budget = budget
        self.aborted = False


def hand_layout_key(arr):
    """base-5 encoding of 18 counters (each 0..4)"""
    key = 0
    for i in range(18):
        key = key * 5 + arr[i]
    return key


def get_max_hand_value_opt(arr_hand, ctx):
    """优化版：memo + node budget"""
    ctx.node_budget -= 1
    if ctx.node_budget < 0:
        ctx.aborted = True
        return HandCardValue(MinCardsValue, 20)

    key = hand_layout_key(arr_hand)
    if key in ctx.memo:
        return ctx.memo[key]

    n_count = sum(arr_hand[3:18])
    if n_count == 0:
        ctx.memo[key] = HandCardValue(0, 0)
        return HandCardValue(0, 0)

    scd_type, scd_count, scd_max = sur_cards_type(arr_hand)
    if scd_type != cgERROR and scd_type != cgFOUR_TAKE_ONE and scd_type != cgFOUR_TAKE_TWO:
        hv = HandCardValue(group_value(scd_type, scd_max, scd_count), 1)
        ctx.memo[key] = hv
        return hv

    best_group = get_best_card_type_opt(arr_hand, ctx)
    if best_group[0] == cgERROR:
        ctx.memo[key] = HandCardValue(MinCardsValue, 20)
        return HandCardValue(MinCardsValue, 20)

    # 减去已选牌型
    remaining = list(arr_hand)
    bg_type, bg_count, bg_max = best_group
    if bg_type in (cgSINGLE, cgBOMB_CARD, cgKING_CARD):
        remaining[bg_max] -= bg_count
    elif bg_type == cgDOUBLE:
        remaining[bg_max] -= 2
    elif bg_type == cgTHREE:
        remaining[bg_max] -= 3
    elif bg_type == cgSINGLE_LINE:
        for k in range(bg_max - bg_count + 1, bg_max + 1):
            remaining[k] -= 1
    elif bg_type == cgDOUBLE_LINE:
        n_pairs = bg_count // 2
        for k in range(bg_max - n_pairs + 1, bg_max + 1):
            remaining[k] -= 2
    elif bg_type == cgTHREE_LINE:
        n_triples = bg_count // 3
        for k in range(bg_max - n_triples + 1, bg_max + 1):
            remaining[k] -= 3

    rest = get_max_hand_value_opt(remaining, ctx)
    result = HandCardValue(
        group_value(bg_type, bg_max, bg_count) + rest.sum_val,
        1 + rest.need_round
    )
    ctx.memo[key] = result
    return result


def get_best_card_type_opt(arr_hand, ctx):
    """优化版 GetBestCardType：abort 时跳过主循环"""
    scd_type, scd_count, scd_max = sur_cards_type(arr_hand)
    if scd_type != cgERROR and scd_type != cgFOUR_TAKE_ONE and scd_type != cgFOUR_TAKE_TWO:
        return (scd_type, scd_count, scd_max)

    best_hv = HandCardValue(MinCardsValue, 21)
    best_group = (cgERROR, 0, -1)
    L = arr_hand

    # ⚠️ 关键差异：abort 时 i 从 16 开始，主循环完全不执行
    start_i = 16 if ctx.aborted else 3

    for i in range(start_i, 16):
        if L[i] == 0 or L[i] == 4:
            continue

        # 单张
        if L[i] >= 1:
            L[i] -= 1
            tmp = get_max_hand_value_opt(L, ctx)
            L[i] += 1
            if hv_score(best_hv) <= hv_score(tmp):
                best_hv = tmp
                best_group = (cgSINGLE, 1, i)

        # 对子
        if L[i] >= 2:
            L[i] -= 2
            tmp = get_max_hand_value_opt(L, ctx)
            L[i] += 2
            if hv_score(best_hv) <= hv_score(tmp):
                best_hv = tmp
                best_group = (cgDOUBLE, 2, i)

        # 三张
        if L[i] >= 3:
            L[i] -= 3
            tmp = get_max_hand_value_opt(L, ctx)
            L[i] += 3
            if hv_score(best_hv) <= hv_score(tmp):
                best_hv = tmp
                best_group = (cgTHREE, 3, i)

        # 顺子
        if L[i] >= 1:
            prov = 0
            for j in range(i, 15):
                if L[j] >= 1:
                    prov += 1
                else:
                    break
                if prov >= 5:
                    for k in range(i, j + 1):
                        L[k] -= 1
                    tmp = get_max_hand_value_opt(L, ctx)
                    for k in range(i, j + 1):
                        L[k] += 1
                    if hv_score(best_hv) <= hv_score(tmp):
                        best_hv = tmp
                        best_group = (cgSINGLE_LINE, prov, j)

        # 连对
        if L[i] >= 2:
            prov = 0
            for j in range(i, 15):
                if L[j] >= 2:
                    prov += 1
                else:
                    break
                if prov >= 3:
                    for k in range(i, j + 1):
                        L[k] -= 2
                    tmp = get_max_hand_value_opt(L, ctx)
                    for k in range(i, j + 1):
                        L[k] += 2
                    if hv_score(best_hv) <= hv_score(tmp):
                        best_hv = tmp
                        best_group = (cgDOUBLE_LINE, prov * 2, j)

        # 飞机
        if L[i] >= 3:
            prov = 0
            for j in range(i, 15):
                if L[j] >= 3:
                    prov += 1
                else:
                    break
                if prov >= 2:
                    for k in range(i, j + 1):
                        L[k] -= 3
                    tmp = get_max_hand_value_opt(L, ctx)
                    for k in range(i, j + 1):
                        L[k] += 3
                    if hv_score(best_hv) <= hv_score(tmp):
                        best_hv = tmp
                        best_group = (cgTHREE_LINE, prov * 3, j)

        break  # C++ 行为

    # 兜底（与原版相同）
    if best_group[0] == cgERROR:
        if arr_hand[16] == 1 and arr_hand[17] == 0:
            best_group = (cgSINGLE, 1, 16)
        elif arr_hand[16] == 0 and arr_hand[17] == 1:
            best_group = (cgSINGLE, 1, 17)
        else:
            for i in range(3, 16):
                if arr_hand[i] == 4:
                    best_group = (cgBOMB_CARD, 4, i)
                    break
            if best_group[0] == cgERROR:
                if arr_hand[16] > 0 and arr_hand[17] > 0:
                    best_group = (cgKING_CARD, 2, 17)
                else:
                    # last-resort：最小单牌
                    for i in range(3, 18):
                        if arr_hand[i] > 0:
                            best_group = (cgSINGLE, 1, i)
                            break

    return best_group


def splite_card_opt(hand_cards, budget=HAND_SEARCH_NODE_BUDGET):
    """优化版 SpliteCard：返回 (card_types_list, n_hand_count, aborted, nodes_used)"""
    arr = [0] * 18
    for c in hand_cards:
        arr[c] += 1

    ctx = OptContext(budget)
    card_types = []
    prev_count = sum(arr[3:18])

    while True:
        n_count = sum(arr[3:18])
        if n_count <= 0:
            break
        # 防御：无进展则退出
        if n_count >= prev_count and len(card_types) > 0:
            break
        prev_count = n_count

        best = get_best_card_type_opt(arr, ctx)
        if best[0] == cgERROR:
            break
        card_types.append(best)

        bg_type, bg_count, bg_max = best
        if bg_type in (cgSINGLE, cgBOMB_CARD, cgKING_CARD):
            arr[bg_max] -= bg_count
        elif bg_type == cgDOUBLE:
            arr[bg_max] -= 2
        elif bg_type == cgTHREE:
            arr[bg_max] -= 3
        elif bg_type == cgSINGLE_LINE:
            for k in range(bg_max - bg_count + 1, bg_max + 1):
                arr[k] -= 1
        elif bg_type == cgDOUBLE_LINE:
            n_pairs = bg_count // 2
            for k in range(bg_max - n_pairs + 1, bg_max + 1):
                arr[k] -= 2
        elif bg_type == cgTHREE_LINE:
            n_triples = bg_count // 3
            for k in range(bg_max - n_triples + 1, bg_max + 1):
                arr[k] -= 3

    nodes_used = budget - ctx.node_budget if ctx.node_budget > 0 else budget
    return card_types, len(card_types), ctx.aborted, nodes_used


# =============================================================================
# 测试：随机生成手牌对比
# =============================================================================
def random_hand_17():
    """生成一副随机 17 张手牌（从一副 54 张牌中随机抽）"""
    # 牌值：3..15(2), 16(小王), 17(大王)
    deck = []
    for v in range(3, 16):
        deck.extend([v] * 4)
    deck.append(16)
    deck.append(17)
    random.shuffle(deck)
    return sorted(deck[:17])


def card_name(cg_type):
    names = {
        cgSINGLE: "单", cgDOUBLE: "对", cgTHREE: "三",
        cgSINGLE_LINE: "顺", cgDOUBLE_LINE: "连对", cgTHREE_LINE: "飞",
        cgBOMB_CARD: "炸", cgKING_CARD: "王炸", cgERROR: "错"
    }
    return names.get(cg_type, "?")


def run_test(n_hands=200, seed=42):
    random.seed(seed)
    print(f"测试 {n_hands} 副随机 17 张手牌")
    print(f"node budget = {HAND_SEARCH_NODE_BUDGET}")
    print("=" * 80)

    diffs = []
    n_abort = 0
    n_orig_gt10 = 0
    n_opt_gt10 = 0
    nodes_used_list = []

    for i in range(n_hands):
        hand = random_hand_17()
        _, n_orig = splite_card_orig(hand)
        _, n_opt, aborted, nodes_used = splite_card_opt(hand)

        if n_orig > 10:
            n_orig_gt10 += 1
        if n_opt > 10:
            n_opt_gt10 += 1

        nodes_used_list.append(nodes_used)

        if aborted:
            n_abort += 1

        if n_orig != n_opt:
            diffs.append((i, hand, n_orig, n_opt, aborted, nodes_used))

    print(f"\n结果汇总：")
    print(f"  总手牌数:        {n_hands}")
    print(f"  nHandCount 不一致: {len(diffs)} ({len(diffs)*100/n_hands:.1f}%)")
    print(f"  budget 耗尽:     {n_abort} ({n_abort*100/n_hands:.1f}%)")
    print(f"  原版 nHC>10:     {n_orig_gt10} ({n_orig_gt10*100/n_hands:.1f}%)")
    print(f"  优化版 nHC>10:   {n_opt_gt10} ({n_opt_gt10*100/n_hands:.1f}%)")
    if nodes_used_list:
        print(f"  节点使用量 avg:   {sum(nodes_used_list)/len(nodes_used_list):.0f}, "
              f"max: {max(nodes_used_list)}")

    if diffs:
        print(f"\n前 20 个差异样本：")
        print(f"{'#':>3} {'原版':>4} {'优化版':>5} {'差异':>4} {'abort':>5} {'节点数':>8}  手牌构成")
        for idx, hand, n_orig, n_opt, ab, nd in diffs[:20]:
            # 统计手牌构成
            counts = [0] * 18
            for c in hand:
                counts[c] += 1
            composition = []
            for v in range(3, 18):
                if counts[v] > 0:
                    composition.append(f"{v}x{counts[v]}")
            comp_str = " ".join(composition)
            print(f"{idx:>3} {n_orig:>4} {n_opt:>5} {n_opt-n_orig:+4d} {str(ab):>5} {nd:>8}  {comp_str}")

        # 统计差异方向
        pos = sum(1 for _, _, no, ne, _, _ in diffs if ne > no)
        neg = sum(1 for _, _, no, ne, _, _ in diffs if ne < no)
        print(f"\n  差异方向：优化版 > 原版 = {pos}, 优化版 < 原版 = {neg}")
        print(f"  平均差异：{sum(ne-no for _,_,no,ne,_,_ in diffs)/len(diffs):+.2f}")

    return diffs


if __name__ == "__main__":
    # 先用 50 副快速验证，如果 Python 版太慢就减数量
    # 原版是指数级搜索，17 张牌可能很慢，先测小一点
    print("注意：原版是纯递归指数级搜索，17 张牌可能非常慢")
    print("先测 20 副看看速度...")
    import time
    t0 = time.time()
    run_test(20, seed=42)
    t1 = time.time()
    print(f"\n耗时: {t1-t0:.1f}s")
