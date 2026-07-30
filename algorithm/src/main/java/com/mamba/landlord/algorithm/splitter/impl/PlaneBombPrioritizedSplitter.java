package com.mamba.landlord.algorithm.splitter.impl;

import com.mamba.landlord.algorithm.splitter.AbstractHandSplitter;
import com.mamba.landlord.algorithm.splitter.IHandSplitter;
import com.mamba.landlord.core.model.Card;
import com.mamba.landlord.core.model.Combo;

import java.util.ArrayList;
import java.util.List;

/**
 * 飞机/炸弹优先拆牌算法：按优先级提取牌型，使出牌次数 N 尽量少、组合分尽量高。
 * 优先级：王炸 > 炸弹 > 飞机带翅膀 > 飞机 > 四带二 > 顺子 > 连对 > 三带 > 对子 > 单
 */
public class PlaneBombPrioritizedSplitter extends AbstractHandSplitter implements IHandSplitter {
    public List<Combo> extractAllCombos(List<Card> handCards, int[] count) {
        List<Combo> combos = new ArrayList<>(20);

        tryExtractRocket(count, combos);
        extractAllBombs(count, combos);
        while (extractPlanes(count, combos)) {}
        while (extractQuadsWithWings(count, combos)) {}
        while (extractStraights(count, combos)) {}
        while (extractConsecutivePairs(count, combos)) {}
        extractTriples(count, combos);
        extractAllPairs(count, combos);
        extractAllSingles(count, combos);

        return combos;
    }
}
