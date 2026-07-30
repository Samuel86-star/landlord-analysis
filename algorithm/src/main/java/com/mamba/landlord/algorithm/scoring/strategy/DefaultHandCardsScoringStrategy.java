package com.mamba.landlord.algorithm.scoring.strategy;

import com.mamba.landlord.algorithm.scoring.IComboScoringStrategy;
import com.mamba.landlord.algorithm.scoring.IHandCardsScoringStrategy;
import com.mamba.landlord.core.model.Card;
import com.mamba.landlord.core.model.Combo;
import com.mamba.landlord.core.model.ComboType;
import com.mamba.landlord.core.model.Rank;
import com.mamba.landlord.core.properties.ScoringStrategyProperties;

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
        int n = combos.size();
        double comboSum = 0.0;
        int bombRocketBonus = 0;
        for (Combo c : combos) {
            comboSum += comboScoringStrategy.score(c);
            if (c.type() == ComboType.BOMB || c.type() == ComboType.ROCKET) {
                bombRocketBonus += config.getBonusBombOrRocket();
            }
        }
        double penalty = (n - 1) * config.getPenaltyPerCombo();
        int handBonus = computeHandControlBonus(handCards);
        return comboSum - penalty + handBonus + bombRocketBonus;
    }

    /**
     * Control_Bonus 中来自手牌的部分：大王、小王、双2。
     */
    private int computeHandControlBonus(List<Card> handCards) {
        int bonus = 0;
        boolean hasBigJoker = false;
        boolean hasSmallJoker = false;
        int twoCount = 0;
        for (Card c : handCards) {
            if (c.rank() == Rank.BIG_JOKER) hasBigJoker = true;
            if (c.rank() == Rank.SMALL_JOKER) hasSmallJoker = true;
            if (c.rank() == Rank.TWO) twoCount++;
        }
        if (hasBigJoker) bonus += config.getBonusBigJoker();
        if (hasSmallJoker) bonus += config.getBonusSmallJoker();
        if (twoCount >= 2) bonus += config.getBonusTwoPairs();
        return bonus;
    }
}
