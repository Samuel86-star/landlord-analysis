package com.mamba.landlord.algorithm.splitter.impl;

import com.mamba.landlord.algorithm.splitter.AbstractHandSplitter;
import com.mamba.landlord.algorithm.splitter.IHandSplitter;
import com.mamba.landlord.core.model.Card;
import com.mamba.landlord.core.model.Combo;

import java.util.ArrayList;
import java.util.List;

/**
 * 顺子/连对优先拆牌算法：不做飞机/四带二，先尽量保留长顺子与长连对，再拆三张、对子、单牌，最后提取炸弹。
 * 适用于顺子/连对潜力大、三张/炸弹较少的牌型，避免被飞机/炸弹拆散长顺子。
 */
public class StraightPrioritizedSplitter extends AbstractHandSplitter implements IHandSplitter {

    public List<Combo> extractAllCombos(List<Card> handCards, int[] count) {
        List<Combo> combos = new ArrayList<>(20);

        tryExtractRocket(count, combos);
        while (extractStraights(count, combos)) {}
        while (extractConsecutivePairs(count, combos)) {}
        extractAllBareTriples(count, combos);
        extractAllPairs(count, combos);
        extractAllSingles(count, combos);
        extractAllBombs(count, combos);

        return combos;
    }
}
