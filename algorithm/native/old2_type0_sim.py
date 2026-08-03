# -*- coding: utf-8 -*-
"""
old2 / Type 0 发牌做牌逻辑的 Python 1:1 复刻（自包含，可跑测试）。

来源：algorithm/native/previous/zgdatbl.cpp（MakeDealByCfg Type0 分支 3894-3992、
      DoMakeDeal 4187-4288、MatchFirst/OtherChairCards 4291-4356、CopyMatchedCardID 4359）、
      zgdatbl.h（CalcHandCardsCount 766-791 及 8 个 Calc*HandCount 605-763、
      6 个 Match* 813-1015）。
配置：makedeal.json 的 old2 策略（MakeDealType=0）。

【保真说明】忠实复刻，包含已知疑似 bug：DoMakeDeal 第二轮起（i>0）无条件做牌，
            不再判断 FirstChair/OtherChair 阈值——即第一轮因“牌强”未触发的家，
            第二轮起也会被补牌（含凑炸弹）。
"""

import random

# ---------------- 常量（对照 MakeDealHelper.h / zgdatbl.h）----------------
SK_LAYOUT_NUM = 16   # 点数索引 0..15（0 不用；1=2, 2-13=3~A, 14=小王, 15=大王）
SK_LAYOUT_MOD = 13   # 连续牌型上界（顺子/连对/飞机最多到 A=13）
CARDS_PER_CHAIR = 17
TOTAL_CARDS = 54
BOTTOM_CARD = 3
TOTAL_CHAIRS = 3

ROBOT = 1
HUMAN = 0

# old2 策略（makedeal.json）
OLD2 = {
    "MakeDealType": 0,
    "BeginMakeNum": 12,
    "FirstChairHandCount": 5,
    "FirstChairBombCount": 2,
    "OtherChairHandCount": 6,
    "OtherChairBombCount": 2,
    "FirstChairBigCardsCount": 4,
    "OtherChairBigCardsCount": 3,
    "BigCardsTo": 2,        # nReserved[0]：2/王分配上限
    # TargetValue / TargetRound / CouPaiStrategy 在 Type0 不读，略
}

_IDX_NAME = {1: "2", 2: "3", 3: "4", 4: "5", 5: "6", 6: "7", 7: "8", 8: "9",
             9: "10", 10: "J", 11: "Q", 12: "K", 13: "A", 14: "sJ", 15: "bJ"}


# ---------------- 牌 ID <-> 点数索引 ----------------
def card_index(card_id):
    """SK_GetCardIndexEx(cardID, 0)：0-51 -> %13+1（1=2..13=A），52->14 小王，53->15 大王。"""
    if card_id == 52:
        return 14
    if card_id == 53:
        return 15
    return card_id % 13 + 1


def card_name(card_id):
    return _IDX_NAME.get(card_index(card_id), "?")


def build_lay(card_ids):
    lay = [0] * SK_LAYOUT_NUM
    for c in card_ids:
        if c >= 0:
            lay[card_index(c)] += 1
    return lay


# ---------------- CalcHandCardsCount：8 个 Calc*HandCount（zgdatbl.h 605-763）----------------
def _calc_2king(lay, hand, bomb):
    king = lay[14] + lay[15]
    if king == 2:
        bomb[0] += 1
        lay[14] = 0
        lay[15] = 0
    # hand += 0


def _calc_bomb(lay, hand, bomb):
    for i in range(SK_LAYOUT_NUM):
        if lay[i] == 4:
            bomb[0] += 1
            hand[0] += -1
            lay[i] = 0


def _calc_abt_three(lay, hand, bomb):  # 飞机（连续三张）
    for i in range(SK_LAYOUT_NUM):
        if lay[i] >= 3:
            if i <= 1 or i >= 13:
                continue
            j = i
            three = 1
            while True:
                j += 1
                if j <= SK_LAYOUT_MOD and lay[j] >= 3:
                    three += 1
                else:
                    break
            if three <= 1:
                continue
            hand[0] += 1 - three
            t = three
            while t:
                lay[i + t - 1] -= 3
                t -= 1


def _calc_three(lay, hand, bomb):  # 单三张
    for i in range(SK_LAYOUT_NUM):
        if lay[i] == 3:
            # hand += 0
            lay[i] = 0


def _calc_abt_couple(lay, hand, bomb):  # 连对（连续>=3 对）
    for i in range(SK_LAYOUT_NUM):
        if lay[i] >= 2:
            if i <= 1 or i >= 13:
                continue
            j = i
            couple = 1
            while True:
                j += 1
                if j <= SK_LAYOUT_MOD and lay[j] >= 2:
                    couple += 1
                else:
                    break
            if couple <= 2:
                continue
            hand[0] += 1
            t = couple
            while t:
                lay[i + t - 1] -= 2
                t -= 1


def _calc_abt(lay, hand, bomb):  # 顺子（连续>=5 单）
    for i in range(SK_LAYOUT_NUM):
        if lay[i] >= 1:
            if i <= 1 or i >= 13:
                continue
            j = i
            single = 1
            while True:
                j += 1
                if j <= SK_LAYOUT_MOD and lay[j] >= 1:
                    single += 1
                else:
                    break
            if single <= 4:
                continue
            hand[0] += 1
            t = single
            while t:
                lay[i + t - 1] -= 1
                t -= 1


def _calc_couple(lay, hand, bomb):  # 对子
    for i in range(SK_LAYOUT_NUM):
        if lay[i] == 2:
            hand[0] += 1
            lay[i] = 0


def _calc_single(lay, hand, bomb):  # 单张
    for i in range(SK_LAYOUT_NUM):
        if lay[i] == 1:
            hand[0] += 1
            lay[i] = 0


def calc_hand_cards_count(lay_in):
    """返回 (nHandCount, nBombCount, nBigCardCount)。nBigCardCount 用原始 lay。"""
    lay = list(lay_in)
    hand = [0]
    bomb = [0]
    big = lay_in[1] + lay_in[14] + lay_in[15]
    _calc_2king(lay, hand, bomb)
    _calc_bomb(lay, hand, bomb)
    _calc_abt_three(lay, hand, bomb)
    _calc_abt_couple(lay, hand, bomb)
    _calc_abt(lay, hand, bomb)
    _calc_three(lay, hand, bomb)
    _calc_couple(lay, hand, bomb)
    _calc_single(lay, hand, bomb)
    return hand[0], bomb[0], big


# ---------------- 6 个 Match*（zgdatbl.h 813-1015）----------------
def match_2_or_king(lay, reserve, cfg):
    if lay[1] + lay[14] + lay[15] >= cfg["BigCardsTo"]:
        return -1
    for c in reserve:
        if c < 0:
            continue
        if card_index(c) in (1, 14, 15):
            return c
    return -1


def match_bomb(lay, reserve):  # 三张 -> 补第4张成炸
    for i in range(1, 14):  # 代码 i<=0||i>=14 continue -> i in [1,13]
        if lay[i] == 3:
            for c in reserve:
                if c < 0:
                    continue
                if card_index(c) == i:
                    return c
    return -1


def match_three(lay, reserve):  # 对子 -> 补第3张
    for i in range(1, 14):
        if lay[i] == 2:
            for c in reserve:
                if c < 0:
                    continue
                if card_index(c) == i:
                    return c
    return -1


def match_abt(lay, reserve):  # 顺子：5连缺1
    for i in range(SK_LAYOUT_NUM):
        if i <= 1 or i >= 10:  # 起点 i in [2,9]，i+4 最大=13(A)
            continue
        matched_idx = 0
        index_diff = 0
        diff_count = 0
        d1 = 0
        d2 = 0
        for j in range(5):
            v = lay[i + j]
            if v in (1, 2, 3):
                index_diff += 1
                diff_count += v
                if v == 2 and d1 == 0:
                    d1 = i + j
                elif v == 2 and d2 == 0:
                    d2 = i + j
            elif v == 0 and matched_idx == 0:
                matched_idx = i + j
        if not (index_diff == 4 and matched_idx != 0 and diff_count in (4, 5, 6)):
            continue
        if diff_count == 6 and d1 != 0 and d2 != 0 and d2 - d1 <= 2:
            continue
        for c in reserve:
            if c < 0:
                continue
            if card_index(c) == matched_idx:
                return c
    return -1


def match_abt_couple(lay, reserve):  # 连对：3连对缺1
    for i in range(SK_LAYOUT_NUM):
        if i <= 1 or i >= 12:  # i in [2,11]，i+2 最大=13(A)
            continue
        matched_idx = 0
        index_diff = 0
        for k in range(3):
            v = lay[i + k]
            if v == 2:
                index_diff += 1
            elif v == 1 and matched_idx == 0:
                matched_idx = i + k
        if index_diff == 2 and matched_idx != 0:
            for c in reserve:
                if c < 0:
                    continue
                if card_index(c) == matched_idx:
                    return c
    return -1


def match_couple(lay, reserve):  # 单牌 -> 补第2张成对
    for i in range(1, 14):
        if lay[i] == 1:
            for c in reserve:
                if c < 0:
                    continue
                if card_index(c) == i:
                    return c
    return -1


# ---------------- 补牌菜单 / 写入（zgdatbl.cpp 4291-4380）----------------
_MATCH_MENU = [match_2_or_king, match_bomb, match_three, match_abt, match_abt_couple, match_couple]


def _copy_matched(chair_card, i, lay, matched, reserve):
    """CopyMatchedCardID：把 matched 从 reserve 取出（置-1）、填入空位、更新 lay。"""
    if matched == -1:
        return False
    for k in range(len(reserve)):
        if reserve[k] == matched:
            reserve[k] = -1
            chair_card[i] = matched
            lay[card_index(matched)] += 1
            return True
    return False


def _match_chair(chair_card, lay, reserve, cfg, menu):
    """MatchFirstChairCards / MatchOtherChairCards 共用：找第一个空位，按 menu 补1张。"""
    for i in range(CARDS_PER_CHAIR):
        if chair_card[i] != -1:
            continue
        for fn in menu:
            matched = fn(lay, reserve, cfg) if fn is match_2_or_king else fn(lay, reserve)
            if _copy_matched(chair_card, i, lay, matched, reserve):
                return
        return  # 菜单都没补上，本轮该家结束


def _get_one_reserved(reserve):
    for k in range(len(reserve)):
        if reserve[k] != -1:
            v = reserve[k]
            reserve[k] = -1
            return v
    return -1


# ---------------- DoMakeDeal（zgdatbl.cpp 4187-4288）----------------
def do_make_deal(chair_cards, lays, reserve, cfg, hand, bomb, big, banker,
                 player_types, robot_need_make_deal=True, menu=None, fix_bug=False):
    if menu is None:
        menu = _MATCH_MENU
    next1 = (banker + 1) % TOTAL_CHAIRS
    next2 = (banker + 2) % TOTAL_CHAIRS
    triggered = [False] * TOTAL_CHAIRS

    def need_deal(chair):
        return player_types[chair] != ROBOT or robot_need_make_deal

    def first_round_ok(chair):
        if chair == banker:
            return (hand[chair] > cfg["FirstChairHandCount"]
                    and bomb[chair] < cfg["FirstChairBombCount"]
                    and big[chair] < cfg["FirstChairBigCardsCount"])
        return (hand[chair] > cfg["OtherChairHandCount"]
                and bomb[chair] < cfg["OtherChairBombCount"]
                and big[chair] < cfg["OtherChairBigCardsCount"])

    for i in range(CARDS_PER_CHAIR - cfg["BeginMakeNum"]):  # 通常 5 轮
        for chair in (banker, next1, next2):
            if not need_deal(chair):
                continue
            if i == 0:
                if first_round_ok(chair):
                    triggered[chair] = True
                    _match_chair(chair_cards[chair], lays[chair], reserve, cfg, menu)
            else:
                # 含bug：第二轮起无条件；fix_bug：只续做第一轮触发过的家
                if (not fix_bug) or triggered[chair]:
                    _match_chair(chair_cards[chair], lays[chair], reserve, cfg, menu)

    # 剩余空位顺序填满
    for i in range(cfg["BeginMakeNum"], CARDS_PER_CHAIR):
        for c in range(TOTAL_CHAIRS):
            if chair_cards[c][i] == -1:
                v = _get_one_reserved(reserve)
                chair_cards[c][i] = v
                lays[c][card_index(v)] += 1


# ---------------- 定庄（zgdatbl.cpp 843-891）----------------
def calc_banker_chair_before(rng):
    return rng.randint(0, TOTAL_CHAIRS - 1)  # XygGetRandomBetween(3)，线上注释掉的轮庄未启用


def calc_banker(player_types, rng, is_fix=False, robot_special_auction=False):
    robot_count = sum(1 for t in player_types if t == ROBOT)
    if (robot_special_auction or is_fix) and robot_count == 2:
        for i in range(TOTAL_CHAIRS):
            if player_types[i] != ROBOT:
                return i
        return calc_banker_chair_before(rng)
    return calc_banker_chair_before(rng)


# ---------------- 全链发牌 ----------------
def deal(player_types, cfg=OLD2, preset_cards=None, seed=None,
         is_fix_banker=False, robot_special_auction=False, robot_need_make_deal=True,
         menu=None, fix_bug=False):
    """
    返回 dict：chair_cards[3][17]、bottom[3]、banker、meta（体检/调试信息）。
    preset_cards：指定 54 张牌序（测试用）；否则随机洗。
    """
    rng = random.Random(seed)
    cards = list(range(TOTAL_CARDS)) if preset_cards is None else list(preset_cards)
    if preset_cards is None:
        rng.shuffle(cards)

    banker = calc_banker(player_types, rng, is_fix=is_fix_banker,
                         robot_special_auction=robot_special_auction)

    # 前 BeginMakeNum 张固定发
    chair_cards = [[-1] * CARDS_PER_CHAIR for _ in range(TOTAL_CHAIRS)]
    n0 = cfg["BeginMakeNum"]
    for i in range(n0):
        chair_cards[0][i] = cards[i * 3]
        chair_cards[1][i] = cards[i * 3 + 1]
        chair_cards[2][i] = cards[i * 3 + 2]
    reserve = cards[n0 * 3: n0 * 3 + (CARDS_PER_CHAIR - n0) * 3]  # cards[36:51]，15 张
    bottom = cards[n0 * 3 + (CARDS_PER_CHAIR - n0) * 3:]          # cards[51:54]，3 张

    lays = [build_lay(chair_cards[c][:n0]) for c in range(TOTAL_CHAIRS)]
    hand = [0] * TOTAL_CHAIRS
    bomb = [0] * TOTAL_CHAIRS
    big = [0] * TOTAL_CHAIRS
    meta = {}
    for c in range(TOTAL_CHAIRS):
        hand[c], bomb[c], big[c] = calc_hand_cards_count(lays[c])
        meta[c] = {"hand_pre": hand[c], "bomb_pre": bomb[c], "big_pre": big[c],
                   "is_firstchair": (c == banker)}

    do_make_deal(chair_cards, lays, reserve, cfg, hand, bomb, big, banker,
                 player_types, robot_need_make_deal=robot_need_make_deal,
                 menu=menu, fix_bug=fix_bug)

    return {"chair_cards": chair_cards, "bottom": bottom, "banker": banker, "meta": meta}


# ---------------- 统计辅助 ----------------
def count_bombs_17(hand17):
    lay = build_lay(hand17)
    b = sum(1 for i in range(SK_LAYOUT_NUM) if lay[i] == 4)
    if lay[14] == 1 and lay[15] == 1:
        b += 1
    return b


def hand_str(hand17):
    return " ".join(card_name(c) for c in sorted(hand17, key=card_index))


# ---------------- 测试 ----------------
def _test_bomb_fill():
    """场景：庄家前12张含 3 张 8，验证做牌补第 4 张 8 成炸弹。"""
    # 显式构造：座0前12张里 cards[0],cards[3],cards[6] = 三张8(6,19,32，点数8=index7)；
    # 第4张8(45)放进 reserve(cards[36])，确保 MatchBomb 能补到。
    cards = [-1] * TOTAL_CARDS
    cards[0], cards[3], cards[6] = 6, 19, 32     # 座0 三张8
    cards[36] = 45                                # 第4张8放 reserve
    used = {6, 19, 32, 45}
    fill = [x for x in range(TOTAL_CARDS) if x not in used]
    fi = 0
    for i in range(TOTAL_CARDS):
        if cards[i] == -1:
            cards[i] = fill[fi]; fi += 1
    seat0_first12 = [cards[i * 3] for i in range(12)]
    assert sum(1 for c in seat0_first12 if card_index(c) == 7) == 3, seat0_first12

    r = deal([HUMAN, ROBOT, ROBOT], preset_cards=cards, is_fix_banker=True, seed=1)
    # 庄=座0（1真人+2机器人，is_fix -> 真人优先）
    assert r["banker"] == 0, r["banker"]
    seat0 = r["chair_cards"][0]
    n8 = sum(1 for c in seat0 if card_index(c) == 7)
    bombs = count_bombs_17(seat0)
    print(f"[场景] 座0前12含3张8 -> 做9牌后 8的张数={n8}，炸弹数={bombs}，手牌={hand_str(seat0)}")
    assert n8 == 4, f"应补到4张8，实际{n8}"  # MatchBombCardType 补第4张


def _test_strong_hand_bug():
    """复现疑似bug：闲家前12张极强（手数很低，第一轮不该做牌），但第二轮起被补牌。"""
    # 构造闲1(座1)前12张：很多对子/三张 -> 手数低（强牌），不满足 OtherChairHandCount>6
    # 用大量对子：6个对子=12张，手数约6（对子6）—— 不>6，第一轮不触发
    # 对子：每点数2张。取 index 2..7（3..8）各2张
    pair_ids = []
    for idx in range(2, 8):  # index2=3..7=8
        base = (idx - 1) * 1  # cardID with %13+1==idx -> %13==idx-1
        # 两张同点数：cardID = (idx-1) 和 (idx-1+13)
        pair_ids += [(idx - 1), (idx - 1) + 13]
    # 这些放座1前12张：cards[1],cards[4],...,cards[34]（i*3+1, i=0..11）
    cards = list(range(TOTAL_CARDS))
    for e in pair_ids:
        cards.remove(e)
    insert_pos = [i * 3 + 1 for i in range(12)]
    for pos, e in zip(insert_pos, pair_ids):
        cards.insert(pos, e)
    seat1_first12 = [cards[i * 3 + 1] for i in range(12)]
    hand_pre = calc_hand_cards_count(build_lay(seat1_first12))
    print(f"[bug复现] 闲1前12张手数={hand_pre[0]} 炸弹={hand_pre[1]}（手数<=6，第一轮应不触发做牌）")

    r = deal([ROBOT, HUMAN, ROBOT], preset_cards=cards, is_fix_banker=True, seed=2)
    # 1真人在座1 -> 庄=座1（FirstChair）；座0、座2 是 OtherChair
    # 我们要看 OtherChair（座0或座2）里"前12强(手数<=6)"的家是否仍被补牌
    for c in (0, 2):
        m = r["meta"][c]
        if m["hand_pre"] <= OLD2["OtherChairHandCount"]:  # 第一轮本不该做
            seat = r["chair_cards"][c]
            # 前12 vs 后5：后5若被补成对子/三张，说明第二轮被做了
            pre12 = build_lay(seat[:12])
            post5 = seat[12:]
            print(f"  座{c} 前12手数={m['hand_pre']}（<=6不该做），后5张={hand_str(post5)}，整手炸弹={count_bombs_17(seat)}")


def _test_sampling():
    """经典初级房模拟（1真人+2机器人，真人=庄，old2/Type0，1:1含bug）：做牌 vs 纯随机全面对比。"""
    N = 20000
    rng = random.Random(0)
    made_bomb, made_hand, made_big = [], [], []
    raw_bomb, raw_hand, raw_big = [], [], []
    zhuang_bomb, zhuang_hand, zhuang_big = [], [], []
    xian_bomb, xian_hand, xian_big = [], [], []
    table_bomb = []

    for _ in range(N):
        cards = list(range(TOTAL_CARDS))
        rng.shuffle(cards)
        r = deal([HUMAN, ROBOT, ROBOT], preset_cards=list(cards), is_fix_banker=True,
                 seed=rng.randint(0, 1 << 30))
        banker = r["banker"]
        tb = 0
        for c in range(3):
            seat = r["chair_cards"][c]
            lay = build_lay(seat)
            b = count_bombs_17(seat)
            h = calc_hand_cards_count(lay)[0]
            big = lay[1] + lay[14] + lay[15]
            made_bomb.append(b); made_hand.append(h); made_big.append(big)
            tb += b
            if c == banker:
                zhuang_bomb.append(b); zhuang_hand.append(h); zhuang_big.append(big)
            else:
                xian_bomb.append(b); xian_hand.append(h); xian_big.append(big)
            # 纯随机参照：同前12、后5不做牌（交错分发17张）
            raw17 = [cards[i * 3 + c] for i in range(17)]
            rlay = build_lay(raw17)
            rb = sum(1 for i in range(SK_LAYOUT_NUM) if rlay[i] == 4)
            if rlay[14] == 1 and rlay[15] == 1:
                rb += 1
            raw_bomb.append(rb); raw_hand.append(calc_hand_cards_count(rlay)[0])
            raw_big.append(rlay[1] + rlay[14] + rlay[15])
        table_bomb.append(tb)

    def dist(arr):
        d = {}
        for v in arr:
            d[v] = d.get(v, 0) + 1
        return {k: round(d.get(k, 0) / len(arr), 4) for k in sorted(d)}

    def avg(arr):
        return sum(arr) / len(arr)

    print(f"[采样 N={N}局×3家 | 经典初级房：1真人+2机器人、真人=庄(FirstChair)、old2/Type0 含bug]")
    print(f"  单家 炸弹均值 : 做牌 {avg(made_bomb):.3f}  vs  随机 {avg(raw_bomb):.3f}")
    print(f"  单家 手数均值 : 做牌 {avg(made_hand):.3f}  vs  随机 {avg(raw_hand):.3f}   （少=更好打）")
    print(f"  单家 大牌(2/王)均值 : 做牌 {avg(made_big):.3f}  vs  随机 {avg(raw_big):.3f}")
    print(f"  炸弹分布(颗->占比) 做牌: {dist(made_bomb)}")
    print(f"  炸弹分布(颗->占比) 随机: {dist(raw_bomb)}")
    print(f"  ---- 庄(真人,FirstChair) vs 闲(机器人,OtherChair) ----")
    print(f"     炸弹均值 : 庄 {avg(zhuang_bomb):.3f}  vs  闲 {avg(xian_bomb):.3f}")
    print(f"     手数均值 : 庄 {avg(zhuang_hand):.3f}  vs  闲 {avg(xian_hand):.3f}")
    print(f"     大牌均值 : 庄 {avg(zhuang_big):.3f}  vs  闲 {avg(xian_big):.3f}")
    print(f"  ---- 整桌炸弹总数 均值 {avg(table_bomb):.3f}，分布: {dist(table_bomb)} ----")


def _test_sweep():
    """参数/改法扫描：同一批牌，对比多种改法对炸弹/手数/大牌/庄闲的影响。"""
    N = 10000
    rng = random.Random(0)
    decks = [rng.sample(range(TOTAL_CARDS), TOTAL_CARDS) for _ in range(N)]
    player_types = [HUMAN, ROBOT, ROBOT]

    no_bomb_menu = [match_2_or_king, match_three, match_abt, match_abt_couple, match_couple]
    bomb_last_menu = [match_2_or_king, match_three, match_abt, match_abt_couple, match_couple, match_bomb]

    experiments = [
        ("baseline 现状(含bug)",        dict()),
        ("去MatchBomb",                  dict(menu=no_bomb_menu)),
        ("MatchBomb降最后",             dict(menu=bomb_last_menu)),
        ("修bug(只续做触发家)",         dict(fix_bug=True)),
        ("修bug+去MatchBomb",           dict(fix_bug=True, menu=no_bomb_menu)),
        ("BigCardsTo=0(不补大牌)",      dict(cfg={**OLD2, "BigCardsTo": 0})),
        ("BeginMakeNum=8(做牌范围大)",  dict(cfg={**OLD2, "BeginMakeNum": 8})),
    ]

    def avg(arr):
        return sum(arr) / len(arr)

    print(f"[扫描 N={N}局×3家 | 同一批牌，各改法对比]")
    print(f"{'改法':<26}{'炸弹':>8}{'手数':>8}{'大牌':>8}{'庄炸':>8}{'闲炸':>8}")
    print("-" * 74)
    for name, kw in experiments:
        bombs, hands, bigs, zb, xb = [], [], [], [], []
        for cards in decks:
            r = deal(player_types, preset_cards=list(cards), is_fix_banker=True, **kw)
            banker = r["banker"]
            for c in range(3):
                seat = r["chair_cards"][c]
                lay = build_lay(seat)
                b = count_bombs_17(seat)
                bombs.append(b)
                hands.append(calc_hand_cards_count(lay)[0])
                bigs.append(lay[1] + lay[14] + lay[15])
                (zb if c == banker else xb).append(b)
        print(f"{name:<24}{avg(bombs):>8.3f}{avg(hands):>8.3f}{avg(bigs):>8.3f}"
              f"{avg(zb):>8.3f}{avg(xb):>8.3f}")


def analyze_hand(lay_in):
    """贪心拆牌（同 CalcHandCardsCount 优先级），返回各牌型个数。用于看“牌长啥样”。"""
    lay = list(lay_in)
    r = {"rocket": 0, "bomb": 0, "plane": 0, "seq_pair": 0, "straight": 0,
         "triple": 0, "pair": 0, "single": 0}
    if lay[14] >= 1 and lay[15] >= 1:
        r["rocket"] = 1; lay[14] = 0; lay[15] = 0
    for i in range(SK_LAYOUT_NUM):
        if lay[i] == 4:
            r["bomb"] += 1; lay[i] = 0
    for i in range(SK_LAYOUT_NUM):
        if lay[i] >= 3 and not (i <= 1 or i >= 13):
            j = i; three = 1
            while True:
                j += 1
                if j <= SK_LAYOUT_MOD and lay[j] >= 3:
                    three += 1
                else:
                    break
            if three > 1:
                r["plane"] += 1
                t = three
                while t:
                    lay[i + t - 1] -= 3; t -= 1
    for i in range(SK_LAYOUT_NUM):
        if lay[i] >= 2 and not (i <= 1 or i >= 13):
            j = i; cp = 1
            while True:
                j += 1
                if j <= SK_LAYOUT_MOD and lay[j] >= 2:
                    cp += 1
                else:
                    break
            if cp > 2:
                r["seq_pair"] += 1
                t = cp
                while t:
                    lay[i + t - 1] -= 2; t -= 1
    for i in range(SK_LAYOUT_NUM):
        if lay[i] >= 1 and not (i <= 1 or i >= 13):
            j = i; sg = 1
            while True:
                j += 1
                if j <= SK_LAYOUT_MOD and lay[j] >= 1:
                    sg += 1
                else:
                    break
            if sg > 4:
                r["straight"] += 1
                t = sg
                while t:
                    lay[i + t - 1] -= 1; t -= 1
    for i in range(SK_LAYOUT_NUM):
        if lay[i] == 3:
            r["triple"] += 1; lay[i] = 0
    for i in range(SK_LAYOUT_NUM):
        if lay[i] == 2:
            r["pair"] += 1; lay[i] = 0
    for i in range(SK_LAYOUT_NUM):
        if lay[i] == 1:
            r["single"] += 1; lay[i] = 0
    return r


def _test_structure():
    """牌型结构扫描：baseline / 去MatchBomb / 纯随机 的牌型构成对比（看牌长啥样）。"""
    N = 10000
    rng = random.Random(0)
    decks = [rng.sample(range(TOTAL_CARDS), TOTAL_CARDS) for _ in range(N)]
    player_types = [HUMAN, ROBOT, ROBOT]
    no_bomb_menu = [match_2_or_king, match_three, match_abt, match_abt_couple, match_couple]

    cases = [
        ("baseline 现状",   dict()),
        ("去MatchBomb",     dict(menu=no_bomb_menu)),
        ("纯随机(不做牌)",  dict(menu=[])),  # 空菜单 = 不补牌 = 全填空
    ]
    keys = ["rocket", "bomb", "plane", "seq_pair", "straight", "triple", "pair", "single"]

    def avg(arr):
        return sum(arr) / len(arr)

    print(f"[牌型结构 N={N}局×3家 | 单家平均各牌型个数]")
    print(f"{'改法':<18}" + "".join(f"{k:>9}" for k in keys))
    print("-" * (18 + 9 * len(keys)))
    for name, kw in cases:
        acc = {k: [] for k in keys}
        for cards in decks:
            r = deal(player_types, preset_cards=list(cards), is_fix_banker=True, **kw)
            for c in range(3):
                st = analyze_hand(build_lay(r["chair_cards"][c]))
                for k in keys:
                    acc[k].append(st[k])
        print(f"{name:<16}" + "".join(f"{avg(acc[k]):>9.3f}" for k in keys))


def _test_explore():
    """探索：连贯优先+炸弹垫后的菜单顺序，聚焦庄/闲炸弹均衡。"""
    N = 8000
    rng = random.Random(0)
    decks = [rng.sample(range(TOTAL_CARDS), TOTAL_CARDS) for _ in range(N)]
    pt = [HUMAN, ROBOT, ROBOT]
    exps = [
        ("顺,连对,三,对,2王,炸",          dict(menu=[match_abt,match_abt_couple,match_three,match_couple,match_2_or_king,match_bomb])),
        ("2王,顺,连对,三,对,炸",          dict(menu=[match_2_or_king,match_abt,match_abt_couple,match_three,match_couple,match_bomb])),
        ("2王,顺,连对,三,对,炸 +修bug",  dict(fix_bug=True, menu=[match_2_or_king,match_abt,match_abt_couple,match_three,match_couple,match_bomb])),
        ("顺,连对,三,2王,对,炸",          dict(menu=[match_abt,match_abt_couple,match_three,match_2_or_king,match_couple,match_bomb])),
        ("2王,三,顺,连对,对,炸",          dict(menu=[match_2_or_king,match_three,match_abt,match_abt_couple,match_couple,match_bomb])),
        ("2王,顺,连对,三,对 (去炸)",      dict(menu=[match_2_or_king,match_abt,match_abt_couple,match_three,match_couple])),
    ]

    def avg(a):
        return sum(a) / len(a)

    print(f"[explore N={N}局×3家 | 连贯优先+炸弹垫后，看庄/闲炸弹均衡]")
    print(f"{'实验':<34}{'炸弹':>7}{'手数':>7}{'庄炸':>7}{'闲炸':>7}{'|庄闲差|':>9}")
    print("-" * 78)
    for name, kw in exps:
        bombs, hands, zb, xb = [], [], [], []
        for cards in decks:
            r = deal(pt, preset_cards=list(cards), is_fix_banker=True, **kw)
            bk = r["banker"]
            for c in range(3):
                seat = r["chair_cards"][c]
                lay = build_lay(seat)
                bombs.append(count_bombs_17(seat))
                hands.append(calc_hand_cards_count(lay)[0])
                (zb if c == bk else xb).append(count_bombs_17(seat))
        print(f"{name:<32}{avg(bombs):>7.3f}{avg(hands):>7.3f}{avg(zb):>7.3f}{avg(xb):>7.3f}{abs(avg(zb)-avg(xb)):>9.3f}")


def _test_grid():
    """二维扫描：BeginMakeNum（配牌起点）× 菜单顺序（牌型优先级）的联合效果。"""
    N = 4000
    rng = random.Random(0)
    decks = [rng.sample(range(TOTAL_CARDS), TOTAL_CARDS) for _ in range(N)]
    pt = [HUMAN, ROBOT, ROBOT]

    bmn_list = [8, 10, 12, 14]
    menus = [
        ("baseline(2王,炸,三,顺,连对,对)",  [match_2_or_king, match_bomb, match_three, match_abt, match_abt_couple, match_couple]),
        ("控制连贯(2王,顺,连对,三,对,炸)",  [match_2_or_king, match_abt, match_abt_couple, match_three, match_couple, match_bomb]),
        ("连贯优先(顺,连对,三,对,2王,炸)",  [match_abt, match_abt_couple, match_three, match_couple, match_2_or_king, match_bomb]),
        ("去炸(2王,顺,连对,三,对)",          [match_2_or_king, match_abt, match_abt_couple, match_three, match_couple]),
    ]

    def avg(a):
        return sum(a) / len(a)

    print(f"[grid N={N}局×3家 | BeginMakeNum × 菜单顺序]")
    for mname, menu in menus:
        print(f"\n=== 菜单: {mname} ===")
        print(f"{'BeginMakeNum':>12}{'做牌张数':>9}{'炸弹':>8}{'手数':>8}{'庄炸':>8}{'闲炸':>8}{'|庄闲差|':>9}")
        print("-" * 66)
        for bmn in bmn_list:
            cfg = {**OLD2, "BeginMakeNum": bmn}
            bombs, hands, zb, xb = [], [], [], []
            for cards in decks:
                r = deal(pt, preset_cards=list(cards), is_fix_banker=True, cfg=cfg, menu=menu)
                bk = r["banker"]
                for c in range(3):
                    seat = r["chair_cards"][c]
                    lay = build_lay(seat)
                    bombs.append(count_bombs_17(seat))
                    hands.append(calc_hand_cards_count(lay)[0])
                    (zb if c == bk else xb).append(count_bombs_17(seat))
            print(f"{bmn:>12}{17 - bmn:>9}{avg(bombs):>8.3f}{avg(hands):>8.3f}{avg(zb):>8.3f}{avg(xb):>8.3f}{abs(avg(zb) - avg(xb)):>9.3f}")


def _run_tests():
    print("== 测试1：3张同点 -> 做牌补成炸弹 ==")
    _test_bomb_fill()
    print("\n== 测试2：复现「第二轮无条件做牌」bug ==")
    _test_strong_hand_bug()
    print("\n== 测试3：采样对比炸弹频率 ==")
    _test_sampling()
    print("\n== 测试4：参数/改法扫描 ==")
    _test_sweep()
    print("\n完成。")


# ============================================================
# CLI —— 产品/运营友好的命令行接口（Python 3，无第三方依赖）
# ============================================================
FIXES = {
    "baseline":            {"说明": "现状（含bug，全菜单）",              "kw": dict()},
    "no-bomb":             {"说明": "去MatchBomb（不主动凑炸弹）",         "kw": dict(menu=[match_2_or_king, match_three, match_abt, match_abt_couple, match_couple])},
    "bomb-last":           {"说明": "MatchBomb降到最后（最低优先级）",     "kw": dict(menu=[match_2_or_king, match_three, match_abt, match_abt_couple, match_couple, match_bomb])},
    "control-flow":        {"说明": "控制连贯(2王,顺,连对,三,对,炸)★推荐", "kw": dict(menu=[match_2_or_king, match_abt, match_abt_couple, match_three, match_couple, match_bomb])},
    "control-flow-nobomb": {"说明": "控制连贯去炸(2王,顺,连对,三,对)",     "kw": dict(menu=[match_2_or_king, match_abt, match_abt_couple, match_three, match_couple])},
    "fix-bug":             {"说明": "修bug（第二轮只续做触发家）",         "kw": dict(fix_bug=True)},
    "fix-bug-no-bomb":     {"说明": "修bug + 去MatchBomb",                 "kw": dict(fix_bug=True, menu=[match_2_or_king, match_three, match_abt, match_abt_couple, match_couple])},
}


def _decks(n, seed):
    rng = random.Random(seed)
    return [rng.sample(range(TOTAL_CARDS), TOTAL_CARDS) for _ in range(n)]


def _avg(a):
    return sum(a) / len(a)


def cli_run(fix, n, seed, bmn=12):
    decks = _decks(n, seed)
    kw = FIXES[fix]["kw"]
    cfg = {**OLD2, "BeginMakeNum": bmn}
    bombs, hands, bigs, zb, xb = [], [], [], [], []
    for cards in decks:
        r = deal([HUMAN, ROBOT, ROBOT], preset_cards=list(cards), is_fix_banker=True, cfg=cfg, **kw)
        banker = r["banker"]
        for c in range(3):
            seat = r["chair_cards"][c]
            lay = build_lay(seat)
            bombs.append(count_bombs_17(seat))
            hands.append(calc_hand_cards_count(lay)[0])
            bigs.append(lay[1] + lay[14] + lay[15])
            (zb if c == banker else xb).append(count_bombs_17(seat))
    print(f"[run] 改法={fix}（{FIXES[fix]['说明']}）  N={n}局×3家  seed={seed}")
    print(f"  炸弹均值 {_avg(bombs):.3f} | 手数 {_avg(hands):.3f} | 大牌(2/王) {_avg(bigs):.3f} | 庄炸 {_avg(zb):.3f} | 闲炸 {_avg(xb):.3f}")
    print(f"  （参照：纯随机 炸弹≈0.190 / 手数≈7.50）")


def cli_sweep(n, seed, bmn=12):
    decks = _decks(n, seed)
    cfg = {**OLD2, "BeginMakeNum": bmn}
    print(f"[sweep] N={n}局×3家  seed={seed}  bmn={bmn}（同一批牌，改法间可直接对比）")
    print(f"{'改法':<22}{'说明':<30}{'炸弹':>8}{'手数':>8}{'大牌':>8}{'庄炸':>8}{'闲炸':>8}")
    print("-" * 100)
    for name, info in FIXES.items():
        bombs, hands, bigs, zb, xb = [], [], [], [], []
        for cards in decks:
            r = deal([HUMAN, ROBOT, ROBOT], preset_cards=list(cards), is_fix_banker=True, cfg=cfg, **info["kw"])
            banker = r["banker"]
            for c in range(3):
                seat = r["chair_cards"][c]
                lay = build_lay(seat)
                bombs.append(count_bombs_17(seat))
                hands.append(calc_hand_cards_count(lay)[0])
                bigs.append(lay[1] + lay[14] + lay[15])
                (zb if c == banker else xb).append(count_bombs_17(seat))
        print(f"{name:<20}{info['说明']:<28}{_avg(bombs):>8.3f}{_avg(hands):>8.3f}{_avg(bigs):>8.3f}{_avg(zb):>8.3f}{_avg(xb):>8.3f}")


def cli_structure(fix, n, seed, bmn=12):
    decks = _decks(n, seed)
    kw = FIXES[fix]["kw"]
    cfg = {**OLD2, "BeginMakeNum": bmn}
    keys = ["rocket", "bomb", "plane", "seq_pair", "straight", "triple", "pair", "single"]
    acc = {k: [] for k in keys}
    for cards in decks:
        r = deal([HUMAN, ROBOT, ROBOT], preset_cards=list(cards), is_fix_banker=True, cfg=cfg, **kw)
        for c in range(3):
            st = analyze_hand(build_lay(r["chair_cards"][c]))
            for k in keys:
                acc[k].append(st[k])
    print(f"[structure] 改法={fix}（{FIXES[fix]['说明']}）  N={n}局×3家  单家平均牌型个数")
    print("  " + "  ".join(f"{k}={_avg(acc[k]):.3f}" for k in keys))


def main():
    import argparse
    p = argparse.ArgumentParser(
        prog="old2_type0_sim.py",
        description="old2/Type0 发牌做牌模拟器（经典初级房：1真人+2机器人，真人=庄）。Python 3，无第三方依赖。")
    sub = p.add_subparsers(dest="cmd")

    pr = sub.add_parser("run", help="跑单个改法，输出炸弹/手数/大牌/庄闲")
    pr.add_argument("--fix", default="baseline", choices=list(FIXES), help="改法名（默认 baseline）")
    pr.add_argument("--n", type=int, default=20000, help="模拟局数（默认 20000）")
    pr.add_argument("--bmn", type=int, default=12, help="BeginMakeNum：前N张固定发，第N+1张起做牌（默认12）")
    pr.add_argument("--seed", type=int, default=0)

    ps = sub.add_parser("sweep", help="扫描所有改法对比（同一批牌）")
    ps.add_argument("--n", type=int, default=10000)
    ps.add_argument("--bmn", type=int, default=12, help="BeginMakeNum（默认12）")
    ps.add_argument("--seed", type=int, default=0)

    pst = sub.add_parser("structure", help="看某个改法的牌型构成")
    pst.add_argument("--fix", default="baseline", choices=list(FIXES))
    pst.add_argument("--n", type=int, default=10000)
    pst.add_argument("--bmn", type=int, default=12, help="BeginMakeNum（默认12）")
    pst.add_argument("--seed", type=int, default=0)

    sub.add_parser("test", help="跑内置测试 1-4（机制验证 + bug 复现 + 采样）")

    a = p.parse_args()
    if a.cmd == "run":
        cli_run(a.fix, a.n, a.seed, a.bmn)
    elif a.cmd == "sweep":
        cli_sweep(a.n, a.seed, a.bmn)
    elif a.cmd == "structure":
        cli_structure(a.fix, a.n, a.seed, a.bmn)
    elif a.cmd == "test":
        _run_tests()
    else:
        p.print_help()


if __name__ == "__main__":
    main()
