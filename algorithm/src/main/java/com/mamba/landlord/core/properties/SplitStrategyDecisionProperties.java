package com.mamba.landlord.core.properties;

import org.springframework.boot.context.properties.ConfigurationProperties;

/**
 * 拆牌策略决策所用阈值，可通过配置文件 landlord.split-strategy.* 调整。
 * 规则说明见 docs/split-strategy-decision-rules.md。
 */
@ConfigurationProperties(prefix = "landlord.split-strategy")
public class SplitStrategyDecisionProperties {

    /** 规则1：顺子潜力 ≥ 该值视为强顺子，可顺子/连对优先（配合弱飞机）。默认 7。 */
    private int strongStraightLen = 7;
    /** 规则1：连对潜力 ≥ 该值视为强连对。默认 5。 */
    private int strongConsecutivePairsLen = 5;
    /** 规则1/2/6：飞机潜力 ≤ 该值视为弱飞机；≥ planePresentLenMin 视为可出飞机。默认 1。 */
    private int weakPlaneLenMax = 1;
    /** 规则1/3：三张点数个数 ≤ 该值时倾向顺子/连对优先。默认 1。 */
    private int fewTriplesStraightFirst = 1;
    /** 规则2/6：飞机潜力 ≥ 该值认为可出飞机。默认 2。 */
    private int planePresentLenMin = 2;
    /** 规则2/3/4：顺子潜力与该值比较（≥ 则可能被飞机拆散；< 则顺子不长）。默认 6。 */
    private int straightLenBreakThreshold = 6;
    /** 规则2/3/4：连对潜力与该值比较（≥ 则可能被拆；< 则连对不长）。默认 4。 */
    private int consecutivePairsLenBreakThreshold = 4;
    /** 规则4：三张点数个数 ≥ 该值视为飞机/炸弹结构明显。默认 3。 */
    private int manyTriplesMin = 3;
    /** 规则4：炸弹个数 ≥ 该值视为飞机/炸弹结构明显。默认 2。 */
    private int manyBombsMin = 2;
    /** 规则5：顺子潜力 ≥ 该值且三张不多时顺子/连对优先。默认 5。 */
    private int longStraightLenMin = 5;
    /** 规则5：三张点数个数 ≤ 该值时可与长顺子一起走顺子优先。默认 2。 */
    private int fewTriplesLongStraight = 2;
    /** 规则6：顺子潜力 ≤ 该值且能出飞机时飞机/炸弹优先。默认 5。 */
    private int shortStraightLenMax = 5;

    public int getStrongStraightLen() { return strongStraightLen; }
    public void setStrongStraightLen(int strongStraightLen) { this.strongStraightLen = strongStraightLen; }
    public int getStrongConsecutivePairsLen() { return strongConsecutivePairsLen; }
    public void setStrongConsecutivePairsLen(int strongConsecutivePairsLen) { this.strongConsecutivePairsLen = strongConsecutivePairsLen; }
    public int getWeakPlaneLenMax() { return weakPlaneLenMax; }
    public void setWeakPlaneLenMax(int weakPlaneLenMax) { this.weakPlaneLenMax = weakPlaneLenMax; }
    public int getFewTriplesStraightFirst() { return fewTriplesStraightFirst; }
    public void setFewTriplesStraightFirst(int fewTriplesStraightFirst) { this.fewTriplesStraightFirst = fewTriplesStraightFirst; }
    public int getPlanePresentLenMin() { return planePresentLenMin; }
    public void setPlanePresentLenMin(int planePresentLenMin) { this.planePresentLenMin = planePresentLenMin; }
    public int getStraightLenBreakThreshold() { return straightLenBreakThreshold; }
    public void setStraightLenBreakThreshold(int straightLenBreakThreshold) { this.straightLenBreakThreshold = straightLenBreakThreshold; }
    public int getConsecutivePairsLenBreakThreshold() { return consecutivePairsLenBreakThreshold; }
    public void setConsecutivePairsLenBreakThreshold(int consecutivePairsLenBreakThreshold) { this.consecutivePairsLenBreakThreshold = consecutivePairsLenBreakThreshold; }
    public int getManyTriplesMin() { return manyTriplesMin; }
    public void setManyTriplesMin(int manyTriplesMin) { this.manyTriplesMin = manyTriplesMin; }
    public int getManyBombsMin() { return manyBombsMin; }
    public void setManyBombsMin(int manyBombsMin) { this.manyBombsMin = manyBombsMin; }
    public int getLongStraightLenMin() { return longStraightLenMin; }
    public void setLongStraightLenMin(int longStraightLenMin) { this.longStraightLenMin = longStraightLenMin; }
    public int getFewTriplesLongStraight() { return fewTriplesLongStraight; }
    public void setFewTriplesLongStraight(int fewTriplesLongStraight) { this.fewTriplesLongStraight = fewTriplesLongStraight; }
    public int getShortStraightLenMax() { return shortStraightLenMax; }
    public void setShortStraightLenMax(int shortStraightLenMax) { this.shortStraightLenMax = shortStraightLenMax; }
}
