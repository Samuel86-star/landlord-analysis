# -*- coding: utf-8 -*-
"""
Type1 发牌拼牌机制的 Python 复刻（简化版）。

来源：algorithm/native/previous/zgdatbl.cpp（MakeDealByCfg Type1 分支 3993-4079）+
      MakeDealHelper.cpp（MakeDeal_ComposeCard 771-1473、CalHandCardValue 1661、
      get_GroupData 85、getvaluebycardid 21、SpliteCard 698）+ makedeal.json。

【复刻策略】
- 拼牌 MakeDeal_ComposeCard（按 CouPaiStrategy）：完整 1:1 逻辑（通用化重构重复特判）。
- 拆牌：用贪心近似 SpliteCard/GetBestCardType（不递归），给触发条件与拼牌提供输入。
- 牌值体系：Type1 的 v（3-17），与 Type0（1-15）不同。
- 目标：准确反映 CouPaiStrategy 调整对炸弹/牌型的影响。
"""

import math
import random
import sys
sys.stdout.reconfigure(encoding="utf-8", errors="replace") if hasattr(sys.stdout, "reconfigure") else None
from old2_type0_sim import (card_index, build_lay, count_bombs_17, hand_str,
                            HUMAN, ROBOT, CARDS_PER_CHAIR, TOTAL_CARDS, BOTTOM_CARD, TOTAL_CHAIRS)

# CardGroupType（MakeDealHelper.h）
cgERROR = -1
cgSINGLE = 1
cgDOUBLE = 2
cgTHREE = 3
cgSINGLE_LINE = 4
cgDOUBLE_LINE = 5
cgTHREE_LINE = 6
cgBOMB_CARD = 13
cgKING_CARD = 14

# GroupDataExp（makedeal.json MakeDealCommonArgs）：{cgType: [(M, C), ...]}，幂函数 value=Σ C·MaxCard^M
GROUP_DATA_EXP = {
    0: [(0, 0.0)],
    1: [(3, 0.0064), (2, 0.349), (1, -2.688), (0, 16.464)],
    2: [(3, 0.098), (2, -1.957), (1, 14.278), (0, -15.376)],
    3: [(3, 0.03223), (2, -0.4356), (1, 3.6031), (0, 28.5434)],
    4: [(3, -0.1035), (2, 4.1364), (1, -43.7687), (0, 172.5242)],
    5: [(3, 0.0638), (2, -1.3522), (1, 11.3737), (0, 20.1496)],
    6: [(3, 0.0056), (2, -0.0139), (1, 0.9216), (0, 51.9721)],
    7: [(3, 0.03223), (2, -0.4356), (1, 3.6031), (0, 28.5434)],
    8: [(3, 0.03223), (2, -0.4356), (1, 3.6031), (0, 28.5434)],
    9: [(3, 0.0056), (2, -0.0139), (1, 0.9216), (0, 51.9721)],
    10: [(3, 0.0056), (2, -0.0139), (1, 0.9216), (0, 51.9721)],
    11: [(1, 0.5), (0, 112.0)],
    12: [(1, 0.5), (0, 112.0)],
    13: [(1, 0.5), (0, 112.0)],
    14: [(1, 0.5), (0, 112.0)],
}

# Type1 默认配置（default 策略）
DEFAULT_T1 = {
    "BeginMakeNum": 10,
    "BeginSelectBanker": 15,
    "TargetValue": 0.6,     # 正数 → ×MaxCardsValue(106)
    "TargetRound": 4,
    "CouPaiStrategy": [[4, 13, 6, 5, 3, 2]],
}


# ---------------- 牌值体系（Type1: v 3-17）----------------
def type1_value(cardid):
    """getvaluebycardid：cardid%13==0→15(2)，1..12→3..14(3~A)，52→16，53→17。"""
    if cardid < 52:
        tmp = cardid % 13
        return 15 if tmp == 0 else tmp + 2
    if cardid == 52:
        return 16
    return 17


def build_lay_v(hand):
    """Type1 牌值分布 arr[18]（索引 3-17 用）。"""
    arr = [0] * 18
    for c in hand:
        if c >= 0:
            arr[type1_value(c)] += 1
    return arr


# ---------------- get_GroupData（牌型价值）----------------
class CardGroupData:
    __slots__ = ("cgType", "nMaxCard", "nCount", "nValue")

    def __init__(self, cgType, nMaxCard, nCount):
        self.cgType = cgType
        self.nMaxCard = nMaxCard
        self.nCount = nCount
        self.nValue = group_value(cgType, nMaxCard)


def group_value(cgType, maxcard):
    exps = GROUP_DATA_EXP.get(cgType)
    if not exps:
        return 0
    v = 0
    for m, c in exps:
        v += c * (maxcard ** m)
    return int(v)


# ---------------- 拆牌（贪心近似 SpliteCard）----------------
def split_card(hand):
    """贪心拆牌 → list[CardGroupData]。优先级：王炸→炸→飞机→连对→顺→三→对→单。"""
    arr = list(build_lay_v(hand))
    groups = []
    # 王炸
    if arr[16] >= 1 and arr[17] >= 1:
        groups.append(CardGroupData(cgKING_CARD, 17, 2))
        arr[16] -= 1; arr[17] -= 1
    # 炸弹
    for v in range(3, 16):
        if arr[v] == 4:
            groups.append(CardGroupData(cgBOMB_CARD, v, 4))
            arr[v] = 0
    # 飞机（连续三，3-14，2=15 不进）
    for v in range(3, 15):
        if arr[v] >= 3:
            j = v; three = 0
            while j <= 14 and arr[j] >= 3:
                three += 1; j += 1
            if three >= 2:
                groups.append(CardGroupData(cgTHREE_LINE, v + three - 1, three * 3))
                for k in range(v, v + three):
                    arr[k] -= 3
    # 连对（连续>=3 对）
    for v in range(3, 15):
        if arr[v] >= 2:
            j = v; cp = 0
            while j <= 14 and arr[j] >= 2:
                cp += 1; j += 1
            if cp >= 3:
                groups.append(CardGroupData(cgDOUBLE_LINE, v + cp - 1, cp * 2))
                for k in range(v, v + cp):
                    arr[k] -= 2
    # 顺子（连续>=5 单）
    for v in range(3, 15):
        if arr[v] >= 1:
            j = v; sg = 0
            while j <= 14 and arr[j] >= 1:
                sg += 1; j += 1
            if sg >= 5:
                groups.append(CardGroupData(cgSINGLE_LINE, v + sg - 1, sg))
                for k in range(v, v + sg):
                    arr[k] -= 1
    # 三张
    for v in range(3, 16):
        if arr[v] == 3:
            groups.append(CardGroupData(cgTHREE, v, 3))
            arr[v] = 0
    # 对子
    for v in range(3, 16):
        if arr[v] == 2:
            groups.append(CardGroupData(cgDOUBLE, v, 2))
            arr[v] = 0
    # 单牌（含 15=2,16小王,17大王）
    for v in range(3, 18):
        for _ in range(arr[v]):
            groups.append(CardGroupData(cgSINGLE, v, 1))
    return groups


def cal_hand_card_value(groups):
    """Σ nValue − 被三/飞机带走的低值(nMaxCard<15)单/对。返回 (nHandCount, nHandCardAveValue)。"""
    groups = sorted(groups, key=lambda g: g.nValue, reverse=True)
    lesser = 0
    total = 0
    for g in groups:
        total += g.nValue
        if g.cgType in (cgTHREE, cgTHREE_LINE):
            lesser += g.nCount // 3
    n = len(groups)
    i = n - 1
    while i >= 0 and lesser > 0:
        g = groups[i]
        if g.cgType in (cgSINGLE, cgDOUBLE) and g.nMaxCard < 15:
            n -= 1
            total -= g.nValue
            lesser -= 1
        i -= 1
    return n, total


# ---------------- MakeDeal_RemainCardsHaveCard ----------------
def remain_have_card(remain, n_value):
    """在 remain 中找点数 n_value 的牌，找到返回 cardid 并从 remain 移除。"""
    if n_value == 17:
        base = 53
    elif n_value == 16:
        base = 52
    elif n_value == 15:
        base = 0
    else:
        base = n_value - 2
    for idx, c in enumerate(remain):
        if c < 0:
            continue
        if base >= 52 and c == base:
            remain.pop(idx); return c
        if 0 <= base <= 12 and (c % 13) == base:
            remain.pop(idx); return c
    return -1


# ---------------- MakeDeal_ComposeCard（拼牌，按 CouPaiStrategy）----------------
def _max_card_list(groups):
    arr = [0] * 18
    for g in groups:
        arr[g.nMaxCard] = g.nCount
    return arr


def _find_first(groups, cg, maxcard=None):
    for g in groups:
        if g.cgType == cg and (maxcard is None or g.nMaxCard == maxcard):
            return g
    return None


def compose_card(cfg, groups, remain, b_need_make_deal, strategy_choice):
    """按选定 CouPaiStrategy 子集 strategy_choice 拼一张。返回 nRemoveCardID（-1=失败）。"""
    arr = _max_card_list(groups)
    # 找一张缺的牌点
    lack = None
    if remain:
        first_v = type1_value(remain[0]) if remain[0] >= 0 else None
        for c in remain:
            if c < 0:
                continue
            v = type1_value(c)
            if arr[v] == 0:
                lack = v
                break
        if lack is None:
            lack = first_v

    if not b_need_make_deal:
        for g in groups:
            if g.cgType == cgSINGLE:
                cid = remain_have_card(remain, g.nMaxCard)
                if cid != -1:
                    g.cgType = cgDOUBLE; g.nCount = 2; g.nValue = group_value(cgDOUBLE, g.nMaxCard)
                    return cid
            elif g.cgType == cgDOUBLE:
                cid = remain_have_card(remain, g.nMaxCard)
                if cid != -1:
                    g.cgType = cgTHREE; g.nCount = 3; g.nValue = group_value(cgTHREE, g.nMaxCard)
                    return cid
        cid = remain_have_card(remain, lack) if lack is not None else -1
        if cid != -1:
            groups.append(CardGroupData(cgSINGLE, type1_value(cid), 1))
        return cid

    for target in strategy_choice:
        if target == cgBOMB_CARD:
            for g in groups:
                if g.cgType == cgTHREE:
                    cid = remain_have_card(remain, g.nMaxCard)
                    if cid != -1:
                        g.cgType = cgBOMB_CARD; g.nCount = 4
                        g.nValue = group_value(cgBOMB_CARD, g.nMaxCard)
                        return cid
        elif target == cgTHREE_LINE:
            # 三 + 相邻对 → 飞机（含延伸）
            for i in range(len(groups)):
                gi = groups[i]
                if gi.cgType == cgTHREE and gi.nMaxCard != 15:
                    for j in range(len(groups)):
                        if i == j:
                            continue
                        gj = groups[j]
                        if gj.cgType == cgDOUBLE and 3 <= gj.nMaxCard <= 14:
                            if abs(gi.nMaxCard - gj.nMaxCard) == 1:
                                cid = remain_have_card(remain, gj.nMaxCard)
                                if cid != -1:
                                    base = max(gi.nMaxCard, gj.nMaxCard)
                                    # 延伸：往大找连续三
                                    prov = 0
                                    to_erase = [j]
                                    q = base + 1
                                    while q <= 14:
                                        gt = _find_first(groups, cgTHREE, q)
                                        if gt is not None and gt is not gi:
                                            prov += 1; to_erase.append(groups.index(gt))
                                            q += 1
                                        else:
                                            break
                                    gi.cgType = cgTHREE_LINE; gi.nMaxCard = base + prov
                                    gi.nCount = 6 + prov * 3
                                    gi.nValue = group_value(cgTHREE_LINE, gi.nMaxCard)
                                    for k in sorted(to_erase, reverse=True):
                                        groups.pop(k)
                                    return cid
        elif target == cgSINGLE_LINE:
            # 顺子：5 连缺 1（单/对），补缺牌 + 延伸
            cnt = {g.nMaxCard: (g.cgType, g) for g in groups}
            for j in range(3, 11):
                lost = 0; need = -1; ndouble = 0; chai = -1
                idxs = []
                for k in range(j, j + 5):
                    if k not in cnt:
                        lost += 1; need = k
                    elif cnt[k][0] == cgSINGLE:
                        idxs.append(cnt[k][1])
                    elif cnt[k][0] == cgDOUBLE:
                        idxs.append(cnt[k][1]); ndouble += 1; chai = k
                if lost == 1 and ndouble <= 1 and need != -1:
                    cid = remain_have_card(remain, need)
                    if cid != -1:
                        to_erase = [groups.index(g) for g in idxs if g.nMaxCard != chai or ndouble == 0]
                        to_erase = sorted(set([groups.index(g) for g in idxs]), reverse=True)
                        # 删掉用掉的4张牌型（need 那个本就不在）
                        for k in to_erase:
                            groups.pop(k)
                        prov = 4
                        q = j + 5
                        while q <= 14:
                            gt = _find_first(groups, cgSINGLE, q)
                            if gt is not None:
                                prov += 1; groups.pop(groups.index(gt)); q += 1
                            else:
                                break
                        groups.append(CardGroupData(cgSINGLE_LINE, j + prov, prov + 1))
                        if ndouble == 1:
                            groups.append(CardGroupData(cgSINGLE, chai, 1))
                        return cid
        elif target == cgDOUBLE_LINE:
            # 连对：单牌 + 相邻两个对子 → 补单成连对 + 延伸（通用化）
            pair_map = {g.nMaxCard: g for g in groups if g.cgType == cgDOUBLE}
            for gi in list(groups):
                if gi.cgType == cgSINGLE and 3 <= gi.nMaxCard <= 14:
                    mc = gi.nMaxCard
                    combos = [(mc - 2, mc - 1), (mc + 1, mc + 2), (mc - 1, mc + 1)]
                    for (a, b) in combos:
                        if a in pair_map and b in pair_map:
                            cid = remain_have_card(remain, mc)
                            if cid != -1:
                                to_erase = [groups.index(pair_map[a]), groups.index(pair_map[b])]
                                prov = 0
                                start = max(mc, b) + 1
                                q = start
                                while q <= 14:
                                    if q in pair_map:
                                        prov += 1; to_erase.append(groups.index(pair_map[q]))
                                        q += 1
                                    else:
                                        break
                                gi.cgType = cgDOUBLE_LINE; gi.nMaxCard = max(mc, b) + prov
                                gi.nCount = 6 + prov * 2
                                gi.nValue = group_value(cgDOUBLE_LINE, gi.nMaxCard)
                                for k in sorted(set(to_erase), reverse=True):
                                    groups.pop(k)
                                return cid
        elif target == cgTHREE:
            for g in groups:
                if g.cgType == cgDOUBLE:
                    cid = remain_have_card(remain, g.nMaxCard)
                    if cid != -1:
                        g.cgType = cgTHREE; g.nCount = 3
                        g.nValue = group_value(cgTHREE, g.nMaxCard)
                        return cid
        elif target == cgDOUBLE:
            for g in groups:
                if g.cgType == cgSINGLE:
                    cid = remain_have_card(remain, g.nMaxCard)
                    if cid != -1:
                        g.cgType = cgDOUBLE; g.nCount = 2
                        g.nValue = group_value(cgDOUBLE, g.nMaxCard)
                        return cid

    # 兜底：没王优先发王
    if arr[16] == 0 and arr[17] == 0:
        for v in (16, 17):
            cid = remain_have_card(remain, v)
            if cid != -1:
                groups.append(CardGroupData(cgSINGLE, v, 1))
                return cid
    cid = remain_have_card(remain, lack) if lack is not None else -1
    if cid != -1:
        groups.append(CardGroupData(cgSINGLE, type1_value(cid), 1))
    return cid


# ---------------- Type1 主循环 ----------------
def deal_type1(player_types, cfg=DEFAULT_T1, preset_cards=None, seed=None,
               strategies=None, robot_strategy=None, newuser_strategy=None):
    """
    strategies: 每家 [CouPaiStrategy 列表]（覆盖 cfg）。返回 dict：chair_cards/bottom/meta。
    """
    rng = random.Random(seed)
    cards = list(range(TOTAL_CARDS)) if preset_cards is None else list(preset_cards)
    if preset_cards is None:
        rng.shuffle(cards)
    remain = list(cards)
    hands = [[], [], []]
    bottom = [-1, -1, -1]
    # 每家策略
    strat = [None, None, None]
    for k in range(3):
        if player_types[k] == ROBOT and robot_strategy is not None:
            strat[k] = list(robot_strategy)
        elif newuser_strategy is not None and False:  # newuser 判定简化略
            strat[k] = list(newuser_strategy)
        else:
            strat[k] = (strategies[k] if strategies else cfg["CouPaiStrategy"])
        strat[k] = strat[k][rng.randrange(len(strat[k]))] if isinstance(strat[k][0], list) else list(strat[k])

    target_value = int(cfg["TargetValue"] * 106) if cfg["TargetValue"] >= 0 else -int(-cfg["TargetValue"] * 106)
    meta = {}
    for p in range(CARDS_PER_CHAIR + 1):
        for k in range(3):
            n0 = cfg["BeginMakeNum"]
            nb = cfg["BeginSelectBanker"]
            if p < n0:
                hands[k].append(remain.pop(0))
            elif p < nb:
                groups = split_card(hands[k])
                hc, hv = cal_hand_card_value(groups)
                need = (hc > cfg["TargetRound"]) or (hv < target_value)
                cid = compose_card(cfg, groups, remain, need, strat[k])
                hands[k].append(cid if cid != -1 else remain.pop(0))
            elif p == nb:
                bottom[k] = remain.pop(len(remain) - 1)
            else:
                hands[k].append(remain.pop(len(remain) - 1))
    return {"chair_cards": hands, "bottom": bottom, "meta": meta}


# ---------------- 统计 / 测试 ----------------
def _avg(a):
    return sum(a) / len(a)


def sample(player_types, cfg, n, seed=0, **kw):
    rng = random.Random(seed)
    bombs, hands_n = [], []
    for _ in range(n):
        cards = list(range(TOTAL_CARDS)); rng.shuffle(cards)
        r = deal_type1(player_types, cfg=cfg, preset_cards=cards, seed=rng.randint(0, 1 << 30), **kw)
        for c in range(3):
            seat = r["chair_cards"][c]
            bombs.append(count_bombs_17(seat))
            hands_n.append(cal_hand_card_value(split_card(seat))[0])
    return _avg(bombs), _avg(hands_n), bombs


def _test_grid_t1():
    """Type1 寻经典组合：CouPaiStrategy × BeginMakeNum × BeginSelectBanker 三维扫描。
    杠杆：对(2)/三(3)=聚集→顶炸降手数；顺(4)/连对(5)/飞机(6)=散开→压炸升手数；
      begin 越大→拼牌越少→聚集策略炸向自然0.19回落；
      select(BeginSelectBanker) 越大→拼牌覆盖率越高→手数降但炸升（15=部分随机填，17=补满空位，线上 robot/newuser 用17）。
      tv=999/tr=10 恒走策略环（最可控）。"""
    N = 3000
    rng = random.Random(0)
    decks = [rng.sample(range(TOTAL_CARDS), TOTAL_CARDS) for _ in range(N)]
    avg = lambda x: sum(x) / len(x)

    strats = [
        ("no-pair  [4,5,3,6]",     [4, 5, 3, 6]),          # 不收对：少炸偏自然
        ("with-pair [4,6,5,2,3]",  [4, 6, 5, 2, 3]),       # ★推荐（收对）
        ("full     [4,5,6,3,2]",   [4, 5, 6, 3, 2]),       # 全量（带对在三前）
        ("default  [4,13,6,5,3,2]",[4, 13, 6, 5, 3, 2]),   # 线上现状（含炸）
    ]
    begins = [10, 12, 13, 14]
    selects = [15, 17]  # 15=部分随机填(默认/new/level*)；17=补满空位(robot/newuser)
    rows = []
    print(f"[grid Type1 三维 N={N}局×3家 | tv=999/tr=10 | 参考：纯随机 炸弹0.194 手数7.80]")
    print(f"{'策略':<22}{'bmn':>4}{'sel':>4}{'炸弹':>8}{'手数':>8}{'庄炸':>8}{'闲炸':>8}{'庄闲差':>8}{'距0.19':>8}")
    print("-" * 86)
    for name, cp in strats:
        for bmn in begins:
            for sel in selects:
                cfg = {"BeginMakeNum": bmn, "BeginSelectBanker": sel,
                       "TargetValue": 999, "TargetRound": 10, "CouPaiStrategy": [cp]}
                bombs, hands, zb, xb = [], [], [], []
                for cards in decks:
                    r = deal_type1([HUMAN, ROBOT, ROBOT], cfg=cfg, preset_cards=list(cards))
                    for c in range(3):
                        seat = r["chair_cards"][c]
                        bn = count_bombs_17(seat)
                        bombs.append(bn); hands.append(cal_hand_card_value(split_card(seat))[0])
                        (zb if c == 0 else xb).append(bn)
                ab, ah, az, ax = avg(bombs), avg(hands), avg(zb), avg(xb)
                rows.append((name, bmn, sel, ab, ah, az - ax))
                flag = " ←近经典" if abs(ab - 0.19) <= 0.03 else ""
                print(f"{name:<20}{bmn:>4}{sel:>4}{ab:>8.3f}{ah:>8.2f}{az:>8.3f}{ax:>8.3f}{az-ax:>+8.3f}{ab-0.19:>+8.3f}{flag}")
        print()

    near = [r for r in rows if abs(r[3] - 0.19) <= 0.03]
    near.sort(key=lambda r: r[4])
    print(f"[最接近经典(炸弹≈0.19)且最好打的组合（距0.19≤0.03，按手数升序）]")
    for name, bmn, sel, ab, ah, d in near:
        print(f"  {name} b{bmn} sel{sel}: 炸弹={ab:.3f} 手数={ah:.2f} 庄闲差={d:+.3f}")

    pareto, best = [], 99
    for name, bmn, sel, ab, ah, d in sorted(rows, key=lambda r: r[3]):
        if ah < best:
            best = ah
            pareto.append((name, bmn, sel, ab, ah, d))
    print(f"\n[Pareto 前沿（炸弹升序里手数创新低，跨 select 维）]")
    for name, bmn, sel, ab, ah, d in pareto:
        print(f"  {name} b{bmn} sel{sel}: 炸弹={ab:.3f} 手数={ah:.2f} 庄闲差={d:+.3f}")


# ---------------- CLI（产品/运营友好）----------------
# CouPaiStrategy 预设。码：1单 2对 3三 4顺 5连对 6飞机 13炸 14王
# 杠杆：对(2)/三(3)=聚集→顶高炸弹、降手数；顺(4)/连对(5)/飞机(6)=散开→压低炸弹、升手数。
COUPAI = {
    "default":   ([4, 13, 6, 5, 3, 2], "炸第2位（线上 default，炸弹0.64）"),
    "with-pair": ([4, 6, 5, 2, 3],    "★推荐（收对）：b14 sel17 下 0.20炸/6.2手/庄闲均衡"),
    "no-pair":   ([4, 5, 3, 6],       "不收对→少炸偏自然（=线上 new），b13 下 0.18炸/6.8手"),
    "bomb-last": ([4, 6, 5, 3, 2, 13], "炸降最后"),
    "no-bomb":   ([4, 6, 5, 3, 2],    "去炸但仍带对子补（聚集，0.33炸）"),
    "newuser":   ([13, 6, 3, 4, 5, 2], "炸首位（线上 newuser）"),
    "robot":     ([6, 13, 3, 4, 5, 2], "线上 robot"),
}


def _cfg(begin, select, tv, tr, cp):
    return {"BeginMakeNum": begin, "BeginSelectBanker": select,
            "TargetValue": tv, "TargetRound": tr, "CouPaiStrategy": [cp]}


def cli_run(name, n, seed, begin, select, tv, tr):
    cp, desc = COUPAI[name]
    cfg = _cfg(begin, select, tv, tr, cp)
    b, h, _ = sample([HUMAN, ROBOT, ROBOT], cfg, n, seed=seed)
    print(f"[run] CouPaiStrategy={name}（{desc}）  顺序={cp}  N={n} seed={seed}")
    print(f"  炸弹均值 {b:.3f} | 手数 {h:.3f}")
    print(f"  （参照：纯随机 炸弹≈0.19）")


def cli_sweep(n, seed, begin, select, tv, tr):
    print(f"[sweep] N={n} seed={seed}  begin={begin} select={select} tv={tv} tr={tr}")
    print(f"{'CouPaiStrategy':<12}{'说明':<24}{'顺序':<22}{'炸弹':>8}{'手数':>8}")
    print("-" * 74)
    for name, (cp, desc) in COUPAI.items():
        cfg = _cfg(begin, select, tv, tr, cp)
        b, h, _ = sample([HUMAN, ROBOT, ROBOT], cfg, n, seed=seed)
        print(f"{name:<10}{desc:<22}{str(cp):<22}{b:>8.3f}{h:>8.3f}")


def main():
    import argparse
    p = argparse.ArgumentParser(
        prog="old2_type1_sim.py",
        description="Type1 发牌拼牌模拟器（CouPaiStrategy 可调）。Python 3，无第三方依赖。")
    sub = p.add_subparsers(dest="cmd")

    pr = sub.add_parser("run", help="跑单个 CouPaiStrategy")
    pr.add_argument("--coupai", default="default", choices=list(COUPAI), help="策略名（默认 default）")
    pr.add_argument("--n", type=int, default=3000, help="模拟局数（默认 3000）")
    pr.add_argument("--begin", type=int, default=10, help="BeginMakeNum（默认10）")
    pr.add_argument("--select", type=int, default=15, help="BeginSelectBanker（默认15）")
    pr.add_argument("--tv", type=float, default=0.6, help="TargetValue（默认0.6）")
    pr.add_argument("--tr", type=int, default=4, help="TargetRound（默认4）")
    pr.add_argument("--seed", type=int, default=0)

    ps = sub.add_parser("sweep", help="扫描所有 CouPaiStrategy 对比")
    ps.add_argument("--n", type=int, default=2000)
    ps.add_argument("--begin", type=int, default=10)
    ps.add_argument("--select", type=int, default=15)
    ps.add_argument("--tv", type=float, default=0.6)
    ps.add_argument("--tr", type=int, default=4)
    ps.add_argument("--seed", type=int, default=0)

    a = p.parse_args()
    if a.cmd == "run":
        cli_run(a.coupai, a.n, a.seed, a.begin, a.select, a.tv, a.tr)
    elif a.cmd == "sweep":
        cli_sweep(a.n, a.seed, a.begin, a.select, a.tv, a.tr)
    else:
        p.print_help()


if __name__ == "__main__":
    main()
