package com.mamba.landlord.algorithm.shuffle;

import com.mamba.landlord.core.model.Card;

import java.util.List;

/**
 * 洗牌策略定义
 */
public interface IShuffleStrategy {
    /**
     * 洗牌
     * @return 返回洗牌后的数据
     */
    List<Card> shuffle();
}
