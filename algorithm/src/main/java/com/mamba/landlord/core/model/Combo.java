package com.mamba.landlord.core.model;

import java.util.List;

/**
 * 牌型组合。用于拆牌结果与牌力计算。
 * mainRanks: 主牌（顺子、连对、飞机的连续点数等）
 * wingRanks: 带牌（三带一、飞机带翅膀等）
 */
public record Combo(ComboType type, List<Rank> mainRanks, List<Rank> wingRanks) {

    /** 单牌：只有一张牌，type=SINGLE，mainRanks 中只包含 r。 */
    public static Combo single(Rank r) {
        return new Combo(ComboType.SINGLE, List.of(r), List.of());
    }

    /** 对子：两张相同点数的牌，type=PAIR，mainRanks 中只包含点数 r。 */
    public static Combo pair(Rank r) {
        return new Combo(ComboType.PAIR, List.of(r), List.of());
    }

    /** 三张：三张相同点数的牌，type=TRIPLE，mainRanks 中只包含点数 r。 */
    public static Combo triple(Rank r) {
        return new Combo(ComboType.TRIPLE, List.of(r), List.of());
    }

    /**
     * 三带一：三张相同点数 + 一张单牌。
     * type=TRIPLE_WITH_SINGLE，mainRanks 为三张的点数 tri，wingRanks 为带出的单牌点数 single。
     */
    public static Combo tripleWithSingle(Rank tri, Rank single) {
        return new Combo(ComboType.TRIPLE_WITH_SINGLE, List.of(tri), List.of(single));
    }

    /**
     * 三带一对：三张相同点数 + 一对。
     * type=TRIPLE_WITH_PAIR，mainRanks 为三张的点数 tri，wingRanks 为带出的那一对点数 pair。
     */
    public static Combo tripleWithPair(Rank tri, Rank pair) {
        return new Combo(ComboType.TRIPLE_WITH_PAIR, List.of(tri), List.of(pair));
    }

    /**
     * 顺子：5 张及以上连续点数的单牌。
     * type=STRAIGHT，mainRanks 为顺子中所有点数（按从小到大排列），wingRanks 为空。
     */
    public static Combo straight(List<Rank> ranks) {
        return new Combo(ComboType.STRAIGHT, List.copyOf(ranks), List.of());
    }

    /**
     * 连对：3 组及以上连续点数的对子。
     * type=CONSECUTIVE_PAIRS，mainRanks 为每一对的点数列表（从小到大），wingRanks 为空。
     */
    public static Combo consecutivePairs(List<Rank> ranks) {
        return new Combo(ComboType.CONSECUTIVE_PAIRS, List.copyOf(ranks), List.of());
    }

    /**
     * 飞机不带牌：连续点数的三张牌，且不带额外牌。
     * type=PLANE，mainRanks 为所有三张的点数序列，wingRanks 为空。
     */
    public static Combo plane(List<Rank> triRanks) {
        return new Combo(ComboType.PLANE, List.copyOf(triRanks), List.of());
    }

    /**
     * 飞机带单牌：连续三张 + 同数量的单牌。
     * type=PLANE_WITH_SINGLES，mainRanks 为三张的点数序列，wingRanks 为带出的单牌点数列表。
     */
    public static Combo planeWithSingles(List<Rank> triRanks, List<Rank> singles) {
        return new Combo(ComboType.PLANE_WITH_SINGLES, List.copyOf(triRanks), List.copyOf(singles));
    }

    /**
     * 飞机带对子：连续三张 + 同数量的对子。
     * type=PLANE_WITH_PAIRS，mainRanks 为三张的点数序列，wingRanks 为带出的对子点数列表。
     */
    public static Combo planeWithPairs(List<Rank> triRanks, List<Rank> pairRanks) {
        return new Combo(ComboType.PLANE_WITH_PAIRS, List.copyOf(triRanks), List.copyOf(pairRanks));
    }

    /**
     * 四带二单：四张相同点数 + 两张单牌。
     * type=QUAD_WITH_TWO_SINGLES，mainRanks 只包含四张的点数 quad，wingRanks 为两个单牌点数 s1/s2。
     */
    public static Combo quadWithTwoSingles(Rank quad, Rank s1, Rank s2) {
        return new Combo(ComboType.QUAD_WITH_TWO_SINGLES, List.of(quad), List.of(s1, s2));
    }

    /**
     * 四带二对：四张相同点数 + 两个对子。
     * type=QUAD_WITH_TWO_PAIRS，mainRanks 只包含四张的点数 quad，wingRanks 为两个对子点数 p1/p2。
     */
    public static Combo quadWithTwoPairs(Rank quad, Rank p1, Rank p2) {
        return new Combo(ComboType.QUAD_WITH_TWO_PAIRS, List.of(quad), List.of(p1, p2));
    }

    /**
     * 炸弹：四张相同点数的牌。
     * type=BOMB，mainRanks 只包含炸弹点数 r。
     */
    public static Combo bomb(Rank r) {
        return new Combo(ComboType.BOMB, List.of(r), List.of());
    }

    /**
     * 王炸：大小王。
     * type=ROCKET，mainRanks 固定为 [BIG_JOKER, SMALL_JOKER]，wingRanks 为空。
     */
    public static Combo rocket() {
        return new Combo(ComboType.ROCKET, List.of(Rank.BIG_JOKER, Rank.SMALL_JOKER), List.of());
    }

    /**
     * 牌型长度
     * @return
     */
    public int length() {
        return switch (type) {
            case SINGLE -> 1;
            case PAIR -> 2;
            case TRIPLE -> 3;
            case TRIPLE_WITH_SINGLE -> 4;
            case TRIPLE_WITH_PAIR -> 5;
            case STRAIGHT -> mainRanks.size();
            case CONSECUTIVE_PAIRS -> mainRanks.size() * 2;
            case PLANE -> mainRanks.size() * 3;
            case PLANE_WITH_SINGLES -> mainRanks.size() * 3 + wingRanks.size();
            case PLANE_WITH_PAIRS -> mainRanks.size() * 3 + wingRanks.size() * 2;
            case QUAD_WITH_TWO_SINGLES -> 6;
            case QUAD_WITH_TWO_PAIRS -> 8;
            case BOMB -> 4;
            case ROCKET -> 2;
        };
    }
}
