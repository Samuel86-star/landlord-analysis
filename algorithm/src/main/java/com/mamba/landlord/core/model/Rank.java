package com.mamba.landlord.core.model;

import java.util.Comparator;
import java.util.List;

/**
 * 斗地主牌面点数。顺序：3 最小，大王最大。
 * baseValue 定义为“单牌牌力值”。
 */
public enum Rank {
    THREE(0, -7),
    FOUR(1, -6),
    FIVE(2, -5),
    SIX(3, -4),
    SEVEN(4, -3),
    EIGHT(5, -2),
    NINE(6, -1),
    TEN(7, 0),
    JACK(8, 1),
    QUEEN(9, 2),
    KING(10, 4),
    ACE(11, 6),
    TWO(12, 10),
    SMALL_JOKER(13, 15),
    BIG_JOKER(14, 18);

    /** 单牌序号 */
    private final int index;
    /** 单牌牌力值 */
    private final int baseValue;

    Rank(int index, int baseValue) {
        this.index = index;
        this.baseValue = baseValue;
    }

    /**
     * 返回牌面在整体顺序中的序号。3=0, 4=1, ..., 2=12, 小王=13, 大王=14。
     */
    public int getIndex() {
        return index;
    }

    /**
     * 返回单牌牌力值，用于牌型组合分计算。
     */
    public int getBaseValue() {
        return baseValue;
    }

    /**
     * 是否可用于顺子/连对/飞机（不含 2 和王）
     */
    public boolean isStraightRank() {
        return this != TWO && this != SMALL_JOKER && this != BIG_JOKER;
    }

    /**
     * 顺子内的连续顺序：3=0, 4=1, ..., A=11
     */
    public int getStraightIndex() {
        return index;
    }

    /** 顺子/连对/飞机用的默认排序：按 getStraightIndex 升序（3 最小，A 最大）。 */
    public static final Comparator<Rank> defaultStraightOrder =
            Comparator.comparingInt(Rank::getStraightIndex);

    /** 顺子/连对/飞机用的点数序列（3~A，不含 2 和大小王）。供拆牌与策略决策共用。 */
    public static final List<Rank> STRAIGHT_RANKS = List.of(
        THREE, FOUR, FIVE, SIX, SEVEN, EIGHT,
        NINE, TEN, JACK, QUEEN, KING, ACE
    );
}
