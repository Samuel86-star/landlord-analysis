package com.mamba.landlord.algorithm.splitter;

import com.mamba.landlord.algorithm.splitter.impl.PlaneBombPrioritizedSplitter;
import com.mamba.landlord.algorithm.splitter.impl.StraightPrioritizedSplitter;
import com.mamba.landlord.algorithm.utils.HandCardUtils;
import com.mamba.landlord.core.holder.SplitStrategyDecisionHolder;
import com.mamba.landlord.core.properties.SplitStrategyDecisionProperties;
import com.mamba.landlord.core.model.Card;
import com.mamba.landlord.core.model.Combo;
import com.mamba.landlord.core.model.ComboType;
import com.mamba.landlord.core.model.Rank;

import java.util.List;

/**
 * 拆牌算法工厂：根据手牌点数分布决策选用「飞机/炸弹优先」或「顺子/连对优先」，
 * 返回对应的算法实例或直接执行拆牌。决策规则见 docs/split-strategy-decision-rules.md。
 */
public class DefaultSplitterFactory {

    private static final List<Rank> STRAIGHT_RANKS = Rank.STRAIGHT_RANKS;
    /** 飞机/炸弹优先的拆牌算法实例。 */
    private static final AbstractHandSplitter planeBombPrioritizedSplitter = new PlaneBombPrioritizedSplitter();
    /** 顺子/连对优先的拆牌算法实例。 */
    private static final AbstractHandSplitter straightPrioritizedSplitter = new StraightPrioritizedSplitter();

    /**
     * 对手牌进行拆分，返回牌型组合列表（内部先决策再委托给对应算法）。
     * 仅构建一次 count 数组，避免重复计算。
     *
     * @param handCards 手牌数据
     * @return 拆牌结果（组合列表）
     */
    public static List<Combo> extractAllCombos(List<Card> handCards) {
        if (handCards == null) {
            throw new IllegalArgumentException("handCards must not be null");
        }
        int[] count = HandCardUtils.buildRankCounts(handCards);
        return getSplitter(count).extractAllCombos(handCards, count);
    }

    /**
     * 根据点数分布选择拆牌算法。
     *
     * @param rankCounts 各点数张数，下标为 Rank.ordinal()
     * @return 本次选中的算法实例（飞机/炸弹优先 或 顺子/连对优先）
     */
    public static AbstractHandSplitter getSplitter(int[] rankCounts) {
        if (rankCounts == null) {
            throw new IllegalArgumentException("rankCounts must not be null");
        }
        return chooseStrategy(rankCounts) ? straightPrioritizedSplitter : planeBombPrioritizedSplitter;
    }

    /**
     * 根据手牌分布选择本次应使用的拆牌算法（需构建 count，优先使用 {@link #getSplitter(int[])}）。
     *
     * @param handCards 手牌数据
     * @return 本次选中的算法实例（飞机/炸弹优先 或 顺子/连对优先）
     */
    public static AbstractHandSplitter getSplitter(List<Card> handCards) {
        if (handCards == null) {
            throw new IllegalArgumentException("handCards must not be null");
        }
        return getSplitter(HandCardUtils.buildRankCounts(handCards));
    }

    /**
     * 根据点数分布选择是否使用顺子/连对优先。顺子区为 3~A，规则见 docs/split-strategy-decision-rules.md。
     *
     * @param handCardsCnt 各点数的张数，下标为 Rank.ordinal()
     * @return true=顺子/连对优先，false=飞机/炸弹优先
     */
    public static boolean chooseStrategy(int[] handCardsCnt) {
        Boolean[] out = new Boolean[1];
        return chooseStrategyWithConfidence(handCardsCnt, out);
    }

    /**
     * 同 {@link #chooseStrategy}，额外返回是否「高置信度」。
     * 高置信度时（前两条规则命中）可安全使用单路径，否则建议双路径取优。
     *
     * @param handCardsCnt 各点数的张数
     * @param confidentOut 输出：是否高置信度，可为 null
     * @return true=顺子/连对优先，false=飞机/炸弹优先
     */
    public static boolean chooseStrategyWithConfidence(int[] handCardsCnt, Boolean[] confidentOut) {
        if (handCardsCnt == null) {
            throw new IllegalArgumentException("handCardsCnt must not be null");
        }
        SplitStrategyDecisionProperties c = SplitStrategyDecisionHolder.get();
        int maxStraightLen = calcMaxConsecutiveLength(handCardsCnt, ComboType.STRAIGHT);
        int maxConsecutivePairsLen = calcMaxConsecutiveLength(handCardsCnt, ComboType.CONSECUTIVE_PAIRS);
        int maxPlaneLen = calcMaxConsecutiveLength(handCardsCnt, ComboType.PLANE);
        int numTriples = calcSatisfiedRankCnt(handCardsCnt, ComboType.TRIPLE);
        int numBombs = calcSatisfiedRankCnt(handCardsCnt, ComboType.BOMB);

        if ((maxStraightLen >= c.getStrongStraightLen() || maxConsecutivePairsLen >= c.getStrongConsecutivePairsLen())
                && maxPlaneLen <= c.getWeakPlaneLenMax() && numTriples <= c.getFewTriplesStraightFirst()) {
            if (confidentOut != null) confidentOut[0] = true;
            return true;
        }
        if (maxPlaneLen >= c.getPlanePresentLenMin()
                && (maxStraightLen >= c.getStraightLenBreakThreshold() || maxConsecutivePairsLen >= c.getConsecutivePairsLenBreakThreshold())) {
            if (confidentOut != null) confidentOut[0] = true;
            return false;
        }
        if ((maxStraightLen >= c.getStraightLenBreakThreshold() || maxConsecutivePairsLen >= c.getConsecutivePairsLenBreakThreshold())
                && numTriples <= c.getFewTriplesStraightFirst()) {
            if (confidentOut != null) confidentOut[0] = false;
            return true;
        }
        if ((numTriples >= c.getManyTriplesMin() || numBombs >= c.getManyBombsMin())
                && maxStraightLen < c.getStraightLenBreakThreshold() && maxConsecutivePairsLen < c.getConsecutivePairsLenBreakThreshold()) {
            if (confidentOut != null) confidentOut[0] = false;
            return false;
        }
        if (maxStraightLen >= c.getLongStraightLenMin() && numTriples <= c.getFewTriplesLongStraight()) {
            if (confidentOut != null) confidentOut[0] = false;
            return true;
        }
        if (maxPlaneLen >= c.getPlanePresentLenMin() && maxStraightLen <= c.getShortStraightLenMax()) {
            if (confidentOut != null) confidentOut[0] = false;
            return false;
        }
        if (confidentOut != null) confidentOut[0] = false;
        return false;
    }

    /**
     * 顺子区（3~A）内按牌型确定「每个点数至少需要的张数」，返回满足该条件的最长连续区间长度。
     * 顺子=1，连对=2，飞机=3。
     *
     * @param handCardsCnt 各点数的张数，下标为 Rank.ordinal()
     * @param comboType    牌型（STRAIGHT/CONSECUTIVE_PAIRS/PLANE），用于确定每个点数至少需要的张数
     * @return 最长连续长度（0 表示无满足条件的连续段）
     */
    private static int calcMaxConsecutiveLength(int[] handCardsCnt, ComboType comboType) {
        int max = 0;
        for (int i = 0; i < STRAIGHT_RANKS.size(); i++) {
            int len = 0;
            while (i + len < STRAIGHT_RANKS.size()) {
                Rank r = STRAIGHT_RANKS.get(i + len);
                if (hasMinimumCards(handCardsCnt[r.ordinal()], comboType)) {
                    len++;
                } else {
                    break;
                }
            }
            if (len > max) {
                max = len;
            }
        }
        return max;
    }

    /**
     * 顺子区（3~A）内统计满足 comboType 所需最少张数的点数个数。
     * 如 TRIPLE=至少 3 张，BOMB=至少 4 张。
     *
     * @param handCardsCnt 各点数的张数，下标为 Rank.ordinal()
     * @param comboType    牌型（TRIPLE=至少3张，BOMB=至少4张等）
     * @return 满足条件的点数个数
     */
    private static int calcSatisfiedRankCnt(int[] handCardsCnt, ComboType comboType) {
        int n = 0;
        for (Rank r : STRAIGHT_RANKS) {
            if (hasMinimumCards(handCardsCnt[r.ordinal()], comboType)) {
                n++;
            }
        }
        return n;
    }

    private static final int MIN_CARDS_STRAIGHT = 1;
    private static final int MIN_CARDS_PAIRS = 2;
    private static final int MIN_CARDS_TRIPLE_OR_PLANE = 3;
    private static final int MIN_CARDS_BOMB = 4;

    /**
     * 判断某点数的张数是否满足 comboType 所需的最少张数。
     *
     * @param cardCnt   该点数的张数
     * @param comboType 牌型（STRAIGHT=1，CONSECUTIVE_PAIRS=2，PLANE/TRIPLE=3，BOMB=4）
     * @return 是否满足
     */
    private static boolean hasMinimumCards(int cardCnt, ComboType comboType) {
        int need = switch (comboType) {
            case STRAIGHT -> MIN_CARDS_STRAIGHT;
            case CONSECUTIVE_PAIRS -> MIN_CARDS_PAIRS;
            case PLANE, TRIPLE -> MIN_CARDS_TRIPLE_OR_PLANE;
            case BOMB -> MIN_CARDS_BOMB;
            default -> MIN_CARDS_STRAIGHT;
        };
        return cardCnt >= need;
    }

}
