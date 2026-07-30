package com.mamba.landlord.core.properties;

import org.springframework.boot.context.properties.ConfigurationProperties;

import java.util.Arrays;
import java.util.List;
import java.util.stream.Collectors;

/**
 * 牌力评分策略配置：牌面基础分、牌型系数、控制牌加成等。
 * <p>
 * 所有计分相关数值均可通过配置文件调整，便于运营调参与 A/B 实验。
 * </p>
 */
@ConfigurationProperties(prefix = "landlord.scoring")
public class ScoringStrategyProperties {

    /**
     * 牌面基础分，按顺序：3,4,5,6,7,8,9,10,J,Q,K,A,2,小王,大王。
     * 逗号分隔的 15 个整数。
     */
    private String rankBaseValues = "-7,-6,-5,-4,-3,-2,-1,0,1,2,4,6,10,15,18";

    // 牌型计分系数
    private int pairCoeff = 2;
    private int pairOffset = 2;
    private int tripleCoeff = 3;
    private int tripleOffset = 5;
    private int tripleWithSingleOffset = 8;
    private int tripleWithPairOffset = 10;
    private double wingRankFactor = 0.5;
    private int straightLenCoeff = 3;
    private int consecutivePairsLenCoeff = 4;
    private int planeTripleOffset = 5;
    private int planeLenCoeff = 6;
    private int planeWithSinglesTripleOffset = 8;
    private int planeWithPairsTripleOffset = 10;
    private int planeWithWingsLenCoeff = 8;
    private int quadBaseCoeff = 2;
    private int quadWithTwoSinglesOffset = 10;
    private int quadWithTwoPairsOffset = 14;
    private int bombCoeff = 2;
    private int bombOffset = 35;
    private int rocketScore = 60;

    // 整手牌计分
    private double penaltyPerCombo = 8.0;
    private int bonusBigJoker = 5;
    private int bonusSmallJoker = 3;
    private int bonusTwoPairs = 4;
    private int bonusBombOrRocket = 5;

    // EHS 归一化
    private double ehsScale = 40.0;

    private transient int[] rankBaseValuesArray;

    public int getRankBaseValue(int index) {
        if (index < 0 || index >= 15) {
            throw new IndexOutOfBoundsException("rank index must be 0-14, got " + index);
        }
        if (rankBaseValuesArray == null) {
            List<Integer> list = Arrays.stream(rankBaseValues.split(","))
                .map(String::trim)
                .map(Integer::parseInt)
                .collect(Collectors.toList());
            if (list.size() != 15) {
                throw new IllegalStateException("rankBaseValues must have exactly 15 values, got " + list.size());
            }
            rankBaseValuesArray = list.stream().mapToInt(Integer::intValue).toArray();
        }
        return rankBaseValuesArray[index];
    }

    public String getRankBaseValues() {
        return rankBaseValues;
    }

    public void setRankBaseValues(String rankBaseValues) {
        this.rankBaseValues = rankBaseValues;
        this.rankBaseValuesArray = null;
    }

    public int getPairCoeff() {
        return pairCoeff;
    }

    public void setPairCoeff(int pairCoeff) {
        this.pairCoeff = pairCoeff;
    }

    public int getPairOffset() {
        return pairOffset;
    }

    public void setPairOffset(int pairOffset) {
        this.pairOffset = pairOffset;
    }

    public int getTripleCoeff() {
        return tripleCoeff;
    }

    public void setTripleCoeff(int tripleCoeff) {
        this.tripleCoeff = tripleCoeff;
    }

    public int getTripleOffset() {
        return tripleOffset;
    }

    public void setTripleOffset(int tripleOffset) {
        this.tripleOffset = tripleOffset;
    }

    public int getTripleWithSingleOffset() {
        return tripleWithSingleOffset;
    }

    public void setTripleWithSingleOffset(int tripleWithSingleOffset) {
        this.tripleWithSingleOffset = tripleWithSingleOffset;
    }

    public int getTripleWithPairOffset() {
        return tripleWithPairOffset;
    }

    public void setTripleWithPairOffset(int tripleWithPairOffset) {
        this.tripleWithPairOffset = tripleWithPairOffset;
    }

    public double getWingRankFactor() {
        return wingRankFactor;
    }

    public void setWingRankFactor(double wingRankFactor) {
        this.wingRankFactor = wingRankFactor;
    }

    public int getStraightLenCoeff() {
        return straightLenCoeff;
    }

    public void setStraightLenCoeff(int straightLenCoeff) {
        this.straightLenCoeff = straightLenCoeff;
    }

    public int getConsecutivePairsLenCoeff() {
        return consecutivePairsLenCoeff;
    }

    public void setConsecutivePairsLenCoeff(int consecutivePairsLenCoeff) {
        this.consecutivePairsLenCoeff = consecutivePairsLenCoeff;
    }

    public int getPlaneTripleOffset() {
        return planeTripleOffset;
    }

    public void setPlaneTripleOffset(int planeTripleOffset) {
        this.planeTripleOffset = planeTripleOffset;
    }

    public int getPlaneLenCoeff() {
        return planeLenCoeff;
    }

    public void setPlaneLenCoeff(int planeLenCoeff) {
        this.planeLenCoeff = planeLenCoeff;
    }

    public int getPlaneWithSinglesTripleOffset() {
        return planeWithSinglesTripleOffset;
    }

    public void setPlaneWithSinglesTripleOffset(int planeWithSinglesTripleOffset) {
        this.planeWithSinglesTripleOffset = planeWithSinglesTripleOffset;
    }

    public int getPlaneWithPairsTripleOffset() {
        return planeWithPairsTripleOffset;
    }

    public void setPlaneWithPairsTripleOffset(int planeWithPairsTripleOffset) {
        this.planeWithPairsTripleOffset = planeWithPairsTripleOffset;
    }

    public int getPlaneWithWingsLenCoeff() {
        return planeWithWingsLenCoeff;
    }

    public void setPlaneWithWingsLenCoeff(int planeWithWingsLenCoeff) {
        this.planeWithWingsLenCoeff = planeWithWingsLenCoeff;
    }

    public int getQuadBaseCoeff() {
        return quadBaseCoeff;
    }

    public void setQuadBaseCoeff(int quadBaseCoeff) {
        this.quadBaseCoeff = quadBaseCoeff;
    }

    public int getQuadWithTwoSinglesOffset() {
        return quadWithTwoSinglesOffset;
    }

    public void setQuadWithTwoSinglesOffset(int quadWithTwoSinglesOffset) {
        this.quadWithTwoSinglesOffset = quadWithTwoSinglesOffset;
    }

    public int getQuadWithTwoPairsOffset() {
        return quadWithTwoPairsOffset;
    }

    public void setQuadWithTwoPairsOffset(int quadWithTwoPairsOffset) {
        this.quadWithTwoPairsOffset = quadWithTwoPairsOffset;
    }

    public int getBombCoeff() {
        return bombCoeff;
    }

    public void setBombCoeff(int bombCoeff) {
        this.bombCoeff = bombCoeff;
    }

    public int getBombOffset() {
        return bombOffset;
    }

    public void setBombOffset(int bombOffset) {
        this.bombOffset = bombOffset;
    }

    public int getRocketScore() {
        return rocketScore;
    }

    public void setRocketScore(int rocketScore) {
        this.rocketScore = rocketScore;
    }

    public double getPenaltyPerCombo() {
        return penaltyPerCombo;
    }

    public void setPenaltyPerCombo(double penaltyPerCombo) {
        this.penaltyPerCombo = penaltyPerCombo;
    }

    public int getBonusBigJoker() {
        return bonusBigJoker;
    }

    public void setBonusBigJoker(int bonusBigJoker) {
        this.bonusBigJoker = bonusBigJoker;
    }

    public int getBonusSmallJoker() {
        return bonusSmallJoker;
    }

    public void setBonusSmallJoker(int bonusSmallJoker) {
        this.bonusSmallJoker = bonusSmallJoker;
    }

    public int getBonusTwoPairs() {
        return bonusTwoPairs;
    }

    public void setBonusTwoPairs(int bonusTwoPairs) {
        this.bonusTwoPairs = bonusTwoPairs;
    }

    public int getBonusBombOrRocket() {
        return bonusBombOrRocket;
    }

    public void setBonusBombOrRocket(int bonusBombOrRocket) {
        this.bonusBombOrRocket = bonusBombOrRocket;
    }

    public double getEhsScale() {
        return ehsScale;
    }

    public void setEhsScale(double ehsScale) {
        this.ehsScale = ehsScale;
    }
}
