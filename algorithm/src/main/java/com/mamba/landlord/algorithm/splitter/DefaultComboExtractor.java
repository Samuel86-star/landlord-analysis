package com.mamba.landlord.algorithm.splitter;

import com.mamba.landlord.algorithm.scoring.IHandCardsScoringStrategy;
import com.mamba.landlord.algorithm.splitter.impl.PlaneBombPrioritizedSplitter;
import com.mamba.landlord.algorithm.splitter.impl.StraightPrioritizedSplitter;
import com.mamba.landlord.algorithm.utils.HandCardUtils;
import com.mamba.landlord.core.model.Card;
import com.mamba.landlord.core.model.Combo;

import java.util.List;

/**
 * 默认牌型提取器：双路径拆牌取最优。
 * <p>
 * 当注入了 {@link IHandCardsScoringStrategy} 时，同时运行「飞机/炸弹优先」和「顺子/连对优先」两条路径，
 * 取牌力更高的拆法；否则退化为 {@link DefaultSplitterFactory} 单路径。
 * </p>
 */
public class DefaultComboExtractor implements IComboExtractor {

    private static final AbstractHandSplitter PLANE_BOMB_SPLITTER = new PlaneBombPrioritizedSplitter();
    private static final AbstractHandSplitter STRAIGHT_SPLITTER = new StraightPrioritizedSplitter();

    private final IHandCardsScoringStrategy scoringStrategy;

    public DefaultComboExtractor(IHandCardsScoringStrategy scoringStrategy) {
        this.scoringStrategy = scoringStrategy;
    }

    public DefaultComboExtractor() {
        this(null);
    }

    public List<Combo> extractAllCombos(List<Card> handCards) {
        if (scoringStrategy == null) {
            return DefaultSplitterFactory.extractAllCombos(handCards);
        }
        int[] count = HandCardUtils.buildRankCounts(handCards);
        Boolean[] confidentOut = new Boolean[1];
        boolean useStraight = DefaultSplitterFactory.chooseStrategyWithConfidence(count, confidentOut);

        if (Boolean.TRUE.equals(confidentOut[0])) {
            AbstractHandSplitter chosen = useStraight ? STRAIGHT_SPLITTER : PLANE_BOMB_SPLITTER;
            return chosen.extractAllCombos(handCards, count);
        }

        int[] countB = count.clone();
        List<Combo> combosA = PLANE_BOMB_SPLITTER.extractAllCombos(handCards, count);
        List<Combo> combosB = STRAIGHT_SPLITTER.extractAllCombos(handCards, countB);

        double scoreA = scoringStrategy.calcTotalHandScore(handCards, combosA);
        double scoreB = scoringStrategy.calcTotalHandScore(handCards, combosB);

        return scoreA >= scoreB ? combosA : combosB;
    }
}
