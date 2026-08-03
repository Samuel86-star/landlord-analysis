package com.mamba.landlord.algorithm.scoring.strategy;

import com.mamba.landlord.algorithm.scoring.IComboScoringStrategy;
import com.mamba.landlord.core.model.Combo;
import com.mamba.landlord.core.model.Rank;
import com.mamba.landlord.core.properties.ScoringStrategyProperties;

import java.util.List;

/**
 * 默认的 Excel 牌力表实现版本。
 * <p>
 * 所有公式均来自当前的 Excel V_combo 规则，系数从 {@link ScoringStrategyProperties} 读取。
 * </p>
 */
public class DefaultComboScoringStrategy implements IComboScoringStrategy {

    private final ScoringStrategyProperties config;

    public DefaultComboScoringStrategy() {
        this(new ScoringStrategyProperties());
    }

    public DefaultComboScoringStrategy(ScoringStrategyProperties config) {
        this.config = config != null ? config : new ScoringStrategyProperties();
    }

    /**
     * 计算单个牌型的分值 V_combo。
     *
     * @param c 牌型
     * @return V_combo 分值
     */
    public double score(Combo c) {
        if (c == null) {
            throw new IllegalArgumentException("Combo must not be null");
        }
        return switch (c.type()) {
            case SINGLE -> 0;
            case PAIR -> scoreMainRank(c.mainRanks(), 0, config.getPairCoeff(), config.getPairOffset());
            case TRIPLE -> scoreMainRank(c.mainRanks(), 0, config.getTripleCoeff(), config.getTripleOffset());
            case TRIPLE_WITH_SINGLE -> scoreTripleWithWing(c, config.getTripleWithSingleOffset(), false);
            case TRIPLE_WITH_PAIR -> scoreTripleWithWing(c, config.getTripleWithPairOffset(), true);
            case STRAIGHT -> scoreStraightOrPairs(c.mainRanks(), true, config.getStraightLenCoeff());
            case CONSECUTIVE_PAIRS -> scoreStraightOrPairs(c.mainRanks(), false, config.getConsecutivePairsLenCoeff());
            case PLANE -> scorePlane(c.mainRanks(), config.getPlaneTripleOffset(), config.getPlaneLenCoeff());
            case PLANE_WITH_SINGLES -> scorePlaneWithWings(c, config.getPlaneWithSinglesTripleOffset(), false);
            case PLANE_WITH_PAIRS -> scorePlaneWithWings(c, config.getPlaneWithPairsTripleOffset(), true);
            case QUAD_WITH_TWO_SINGLES -> scoreQuadWithWings(c, config.getQuadWithTwoSinglesOffset(), false);
            case QUAD_WITH_TWO_PAIRS -> scoreQuadWithWings(c, config.getQuadWithTwoPairsOffset(), true);
            case BOMB -> scoreMainRank(c.mainRanks(), 0, config.getBombCoeff(), config.getBombOffset());
            case ROCKET -> config.getRocketScore();
        };
    }

    private double scoreMainRank(List<Rank> mainRanks, int idx, int coeff, int offset) {
        if (mainRanks == null || mainRanks.size() <= idx) {
            return 0;
        }
        return config.getRankBaseValue(mainRanks.get(idx).getIndex()) * coeff + offset;
    }

    private double scoreWingRank(Rank r) {
        return r == null ? 0 : Math.max(0, config.getRankBaseValue(r.getIndex()) * config.getWingRankFactor());
    }

    /**
     * 对子翼（带出的翼是对子）加分 = max(0, 基础分 + 1)。
     * 直接对齐 PRD《发牌均衡性过滤》§4.1.2「翼牌为对子时 翼牌加分 = max(0, 基础分 + 1)」字面口径，
     * 不随评分配置(pairCoeff/pairOffset/wingRankFactor)漂移。
     * 单牌翼仍用 {@link #scoreWingRank}（= max(0, V_base × wingRankFactor)）。
     */
    private double scorePairWingRank(Rank r) {
        if (r == null) {
            return 0;
        }
        return Math.max(0, config.getRankBaseValue(r.getIndex()) + 1);
    }

    private double scoreTripleWithWing(Combo c, int tripleOffset, boolean pairWing) {
        double main = scoreMainRank(c.mainRanks(), 0, config.getTripleCoeff(), tripleOffset);
        double wing = pairWing
            ? scorePairWingRank(safeGet(c.wingRanks(), 0))
            : scoreWingRank(safeGet(c.wingRanks(), 0));
        return main + wing;
    }

    private double scoreStraightOrPairs(List<Rank> mainRanks, boolean clampNegative, int lenCoeff) {
        if (mainRanks == null || mainRanks.isEmpty()) {
            return 0;
        }
        double sum = 0;
        for (Rank r : mainRanks) {
            int v = config.getRankBaseValue(r.getIndex());
            sum += clampNegative ? Math.max(0, v) : v;
        }
        return Math.max(0, sum) + mainRanks.size() * lenCoeff;
    }

    private double scorePlane(List<Rank> mainRanks, int tripleOffset, int lenCoeff) {
        if (mainRanks == null || mainRanks.isEmpty()) {
            return 0;
        }
        double sum = 0;
        for (Rank r : mainRanks) {
            sum += config.getRankBaseValue(r.getIndex()) * config.getTripleCoeff() + tripleOffset;
        }
        return sum + (long) mainRanks.size() * lenCoeff;
    }

    private double scorePlaneWithWings(Combo c, int tripleOffset, boolean pairWing) {
        if (c.mainRanks() == null || c.mainRanks().isEmpty()) {
            return 0;
        }
        double sum = 0;
        for (Rank r : c.mainRanks()) {
            sum += config.getRankBaseValue(r.getIndex()) * config.getTripleCoeff() + tripleOffset;
        }
        sum += c.mainRanks().size() * config.getPlaneWithWingsLenCoeff();
        if (c.wingRanks() != null) {
            for (Rank r : c.wingRanks()) {
                sum += pairWing ? scorePairWingRank(r) : scoreWingRank(r);
            }
        }
        return sum;
    }

    private double scoreQuadWithWings(Combo c, int quadOffset, boolean pairWing) {
        double main = scoreMainRank(c.mainRanks(), 0, config.getQuadBaseCoeff(), quadOffset);
        double w1 = pairWing ? scorePairWingRank(safeGet(c.wingRanks(), 0)) : scoreWingRank(safeGet(c.wingRanks(), 0));
        double w2 = pairWing ? scorePairWingRank(safeGet(c.wingRanks(), 1)) : scoreWingRank(safeGet(c.wingRanks(), 1));
        return main + w1 + w2;
    }

    private static Rank safeGet(List<Rank> list, int idx) {
        if (list == null || idx >= list.size()) {
            return null;
        }
        return list.get(idx);
    }
}
