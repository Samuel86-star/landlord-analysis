package com.mamba.landlord.algorithm.splitter;

import com.mamba.landlord.core.model.Card;
import com.mamba.landlord.core.model.Combo;

import java.util.List;

/**
 * 手牌拆分
 */
public interface IHandSplitter {
    /**
     * 对手牌进行拆分，返回牌型组合列表
     *
     * @param handCards 手牌数据
     * @return 拆牌结果（组合列表）
     */
    List<Combo> extractAllCombos(List<Card> handCards);

    /**
     * 使用预计算的点数计数进行拆分，避免重复构建 count（性能优化）。
     *
     * @param handCards 手牌数据
     * @param rankCounts 各点数张数，下标为 Rank.ordinal()，调用中会被修改
     * @return 拆牌结果（组合列表）
     */
    List<Combo> extractAllCombos(List<Card> handCards, int[] rankCounts);
}
