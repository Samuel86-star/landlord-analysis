package com.mamba.landlord.algorithm.splitter;

import com.mamba.landlord.core.model.Card;
import com.mamba.landlord.core.model.Combo;

import java.util.List;

/**
 * 手牌拆解为牌型组合的提取器接口。
 * 用于将一手牌拆分为若干 Combo，供牌力计算等逻辑使用。
 */
public interface IComboExtractor {

    /**
     * 对手牌进行拆分，返回牌型组合列表。
     *
     * @param handCards 手牌
     * @return 拆牌结果
     */
    List<Combo> extractAllCombos(List<Card> handCards);
}
