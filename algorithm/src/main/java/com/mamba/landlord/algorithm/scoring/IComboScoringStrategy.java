package com.mamba.landlord.algorithm.scoring;

import com.mamba.landlord.core.model.Combo;

/**
 * 单一牌型评分策略抽象接口：给定一个 {@link Combo}，返回对应的 V_combo 分值。
 */
public interface IComboScoringStrategy {

    /**
     * 计算单个牌型的分值 V_combo。
     */
    double score(Combo combo);
}

