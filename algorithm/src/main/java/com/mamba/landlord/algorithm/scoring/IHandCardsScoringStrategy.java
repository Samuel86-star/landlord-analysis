package com.mamba.landlord.algorithm.scoring;

import java.util.List;

import com.mamba.landlord.core.model.Card;
import com.mamba.landlord.core.model.Combo;

/**
 * 手牌牌力计算策略抽象接口：给定一个手牌的牌型列表，返回对应的牌力值。
 */
public interface IHandCardsScoringStrategy {

    /**
     * 计算手牌牌力值
     * @param handCards 手牌数据
     * @param combos 手牌牌型列表
     * @return 手牌的牌力值
     */
    double calcTotalHandScore(List<Card> handCards, List<Combo> combos);
}