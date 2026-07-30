package com.mamba.landlord.algorithm.shuffle;

import com.mamba.landlord.core.model.Card;
import com.mamba.landlord.core.model.DealData;

import java.util.List;

/**
 * 发牌策略
 */
public interface IDealStrategy {
    /**
     * 发牌
     * @param shuffledDeckCards 洗过的手牌
     * @return 发牌数据
     */
    DealData dealCards(List<Card> shuffledDeckCards);
}
