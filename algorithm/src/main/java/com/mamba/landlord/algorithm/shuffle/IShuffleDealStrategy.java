package com.mamba.landlord.algorithm.shuffle;

import com.mamba.landlord.core.model.EvaluatedDealData;

/**
 * 洗牌+发牌策略接口。
 * 支持无过滤的纯洗牌发牌，或带牌力过滤与重洗逻辑的发牌。
 */
public interface IShuffleDealStrategy {

    /**
     * 执行一次洗牌并发牌，返回包含三家手牌、底牌及牌力元数据的结果。
     *
     * @return 发牌结果
     */
    EvaluatedDealData shuffleAndDeal();
}
