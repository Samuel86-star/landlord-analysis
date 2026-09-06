package com.mamba.landlord.algorithm.scoring.strategy;

import com.mamba.landlord.algorithm.scoring.IComboScoringStrategy;
import com.mamba.landlord.algorithm.scoring.IHandCardsScoringStrategy;
import com.mamba.landlord.core.model.Card;
import com.mamba.landlord.core.model.Combo;
import com.mamba.landlord.core.model.Rank;
import com.mamba.landlord.core.properties.ScoringStrategyProperties;
import com.mamba.landlord.algorithm.utils.HandCardUtils;

import java.util.Arrays;
import java.util.List;

/**
 * 默认牌力评分器：
 * - 负责单个牌型 V_combo 的计算（委托给 {@link IComboScoringStrategy}）
 * - 负责整手牌牌力 V_total 的计算（ΣV_combo - (N-1)*penalty + Control_Bonus）
 *
 * 作为统一的"评分规则层"，供拆牌策略与洗牌策略复用。
 * 所有系数从 {@link ScoringStrategyProperties} 读取。
 */
public final class DefaultHandCardsScoringStrategy implements IHandCardsScoringStrategy {

    private final IComboScoringStrategy comboScoringStrategy;
    private final ScoringStrategyProperties config;

    public DefaultHandCardsScoringStrategy() {
        this(new DefaultComboScoringStrategy(), new ScoringStrategyProperties());
    }

    public DefaultHandCardsScoringStrategy(IComboScoringStrategy comboScoringStrategy) {
        this(comboScoringStrategy, new ScoringStrategyProperties());
    }

    public DefaultHandCardsScoringStrategy(IComboScoringStrategy comboScoringStrategy,
                                           ScoringStrategyProperties config) {
        if (comboScoringStrategy == null) {
            throw new IllegalArgumentException("Combo scoring strategy cannot be null");
        }
        this.comboScoringStrategy = comboScoringStrategy;
        this.config = config != null ? config : new ScoringStrategyProperties();
    }

    /**
     * 计算一手牌的总牌力值 V_total。
     * 公式：V_total = ΣV_combo - (N - 1) * penaltyPerCombo + Control_Bonus
     *
     * @param handCards 手牌列表
     * @param combos    手牌牌型列表
     */
    public double calcTotalHandScore(List<Card> handCards, List<Combo> combos) {
        if (handCards == null || handCards.isEmpty()
            || combos == null || combos.isEmpty()) {
            return 0.0;
        }
        validateComboPartition(handCards, combos);
        int n = combos.size();
        double comboSum = 0.0;
        for (Combo c : combos) {
            comboSum += comboScoringStrategy.score(c);
        }
        double penalty = (n - 1) * config.getPenaltyPerCombo();
        // 控制牌加成（含炸弹/王炸）基于全部手牌计算，与拆牌方式无关（PRD §4.1.3）
        int controlBonus = computeHandControlBonus(handCards);
        return comboSum - penalty + controlBonus;
    }

    private void validateComboPartition(List<Card> handCards, List<Combo> combos) {
        int[] comboCounts = new int[Rank.values().length];
        for (Combo combo : combos) {
            int mainWeight;
            int wingWeight;
            switch (combo.type()) {
                case SINGLE, STRAIGHT, ROCKET -> { mainWeight = 1; wingWeight = 0; }
                case PAIR, CONSECUTIVE_PAIRS -> { mainWeight = 2; wingWeight = 0; }
                case TRIPLE, PLANE -> { mainWeight = 3; wingWeight = 0; }
                case TRIPLE_WITH_SINGLE, PLANE_WITH_SINGLES -> { mainWeight = 3; wingWeight = 1; }
                case TRIPLE_WITH_PAIR, PLANE_WITH_PAIRS -> { mainWeight = 3; wingWeight = 2; }
                case QUAD_WITH_TWO_SINGLES -> { mainWeight = 4; wingWeight = 1; }
                case QUAD_WITH_TWO_PAIRS -> { mainWeight = 4; wingWeight = 2; }
                case BOMB -> { mainWeight = 4; wingWeight = 0; }
                default -> throw new IllegalArgumentException("Unsupported combo type");
            }
            combo.mainRanks().forEach(rank -> comboCounts[rank.ordinal()] += mainWeight);
            combo.wingRanks().forEach(rank -> comboCounts[rank.ordinal()] += wingWeight);
        }
        if (!Arrays.equals(comboCounts, HandCardUtils.buildRankCounts(handCards))) {
            throw new IllegalArgumentException("Combos must partition handCards");
        }
    }

    /**
     * Control_Bonus：基于全部手牌计算，与拆牌方式无关（PRD §4.1.3）。
     * - 持有大王 +bonusBigJoker；持有小王 +bonusSmallJoker
     * - 持有 ≥2 张 2 +bonusTwoPairs
     * - 每个持有炸弹（任意点数张数 ≥ 4）+bonusBombOrRocket
     * - 持有王炸（大小王齐）+bonusBombOrRocket
     */
    private int computeHandControlBonus(List<Card> handCards) {
        int bonus = 0;
        boolean hasBigJoker = false;
        boolean hasSmallJoker = false;
        int twoCount = 0;
        int[] rankCount = new int[15];
        for (Card c : handCards) {
            Rank r = c.rank();
            if (r == Rank.BIG_JOKER) hasBigJoker = true;
            else if (r == Rank.SMALL_JOKER) hasSmallJoker = true;
            else if (r == Rank.TWO) twoCount++;
            rankCount[r.getIndex()]++;
        }
        if (hasBigJoker) bonus += config.getBonusBigJoker();
        if (hasSmallJoker) bonus += config.getBonusSmallJoker();
        if (twoCount >= 2) bonus += config.getBonusTwoPairs();
        for (int cnt : rankCount) {
            if (cnt >= 4) bonus += config.getBonusBombOrRocket();
        }
        if (hasBigJoker && hasSmallJoker) bonus += config.getBonusBombOrRocket();
        return bonus;
    }
}
