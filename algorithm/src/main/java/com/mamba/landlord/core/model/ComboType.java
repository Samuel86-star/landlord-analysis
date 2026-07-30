package com.mamba.landlord.core.model;

/**
 * 斗地主牌型。
 */
public enum ComboType {
    /** 单牌 */
    SINGLE,
    /** 对子 */
    PAIR,
    /** 三张 */
    TRIPLE,
    /** 三带一 */
    TRIPLE_WITH_SINGLE,
    /** 三带二 */
    TRIPLE_WITH_PAIR,
    /** 顺子 */
    STRAIGHT,
    /** 连对 */
    CONSECUTIVE_PAIRS,
    /** 飞机 */
    PLANE,
    /** 飞机带单牌 */
    PLANE_WITH_SINGLES,
    /** 飞机带对子 */
    PLANE_WITH_PAIRS,
    /** 四带两单 */
    QUAD_WITH_TWO_SINGLES,
    /** 四带两对 */
    QUAD_WITH_TWO_PAIRS,
    /** 炸弹 */
    BOMB,
    /** 火箭 */
    ROCKET
}
