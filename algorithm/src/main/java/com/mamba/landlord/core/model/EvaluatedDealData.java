package com.mamba.landlord.core.model;

import lombok.AllArgsConstructor;
import lombok.Getter;
import lombok.NoArgsConstructor;
import lombok.ToString;
import lombok.experimental.SuperBuilder;

/**
 * 带牌力值与重洗元数据的发牌结果。
 * <p>
 * 包含三家手牌牌力值、是否触发过重洗、重洗次数等，供 API 与日志使用。
 * </p>
 */
@SuperBuilder
@AllArgsConstructor
@NoArgsConstructor
@Getter
@ToString
public class EvaluatedDealData extends DealData {
    /** 0号位手牌牌力值 */
    private double handStrengthPlayer0;
    /** 1号位手牌牌力值 */
    private double handStrengthPlayer1;
    /** 2号位手牌牌力值 */
    private double handStrengthPlayer2;

    /**
     * 0号位手牌的归一化强度（EHS 近似值，范围 [0,1]）。
     * 使用逻辑函数对 {@code handStrengthPlayer0} 做压缩，仅用于日志与分析。
     */
    private double ehsPlayer0;
    /**
     * 1号位手牌的归一化强度（EHS 近似值，范围 [0,1]）。
     * 使用逻辑函数对 {@code handStrengthPlayer1} 做压缩，仅用于日志与分析。
     */
    private double ehsPlayer1;
    /**
     * 2号位手牌的归一化强度（EHS 近似值，范围 [0,1]）。
     * 使用逻辑函数对 {@code handStrengthPlayer2} 做压缩，仅用于日志与分析。
     */
    private double ehsPlayer2;

    /** 是否在发牌过程中触发过重洗重发（只要重发过一次就为 true） */
    private boolean reshuffled;
    /** 实际重洗重发的次数（0 表示未重发） */
    private int reshuffleCnt;
}
