package com.mamba.landlord.algorithm.shuffle;

import com.mamba.landlord.core.model.Card;
import com.mamba.landlord.core.model.DealData;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import java.util.ArrayList;
import java.util.Collections;
import java.util.List;

/**
 * 发牌分布采样器：随机发牌 N 局，统计三家牌力和均衡性指标的分布，
 * 输出各分位点，辅助研发确定 {@code ShuffleStrategyDecisionProperties} 中各阈值的合理取值。
 *
 * <h3>使用场景</h3>
 * <ol>
 *   <li>初次部署或评分公式/拆牌策略调整后，运行采样测试以获取新基线分布。</li>
 *   <li>根据输出的分位点（如 P5/P95）更新 {@code application.properties} 中的对应阈值。</li>
 *   <li>不修改任何生产代码，只用于辅助参数决策。</li>
 * </ol>
 *
 * <h3>示例（在测试类中调用）</h3>
 * <pre>
 * {@code
 * DealDistributionSampler sampler = new DealDistributionSampler(new DefaultReshuffleDealStrategy());
 * DealSampleResult result = sampler.sample(10_000);
 * result.printRecommendedThresholds();
 * }
 * </pre>
 */
public class DealDistributionSampler {

    private static final Logger log = LoggerFactory.getLogger(DealDistributionSampler.class);

    private final AbstractShuffleDealStrategy strategy;

    /**
     * @param strategy 洗牌发牌策略实例，建议使用 {@code enabled=false} 配置的
     *                 {@code DefaultReshuffleDealStrategy}（不过滤，采样原始分布）
     */
    public DealDistributionSampler(AbstractShuffleDealStrategy strategy) {
        if (strategy == null) {
            throw new IllegalArgumentException("strategy must not be null");
        }
        this.strategy = strategy;
    }

    /**
     * 随机发牌 {@code sampleSize} 局，统计各局三家牌力、极差、潜在地主牌力、地主优势
     * 以及手牌结构特征（单牌数、炸弹数）的分布。
     * <p>
     * 注意：采样期间不启用任何重洗过滤，反映原始随机分布。
     * 建议 sampleSize ≥ 5000，推荐 100000。
     * </p>
     *
     * @param sampleSize 采样局数
     * @return 包含所有采样指标原始数据的 {@link DealSampleResult}
     * @throws IllegalArgumentException sampleSize ≤ 0 时抛出
     */
    public DealSampleResult sample(int sampleSize) {
        if (sampleSize <= 0) {
            throw new IllegalArgumentException("sampleSize must be > 0");
        }

        List<Double> minScores = new ArrayList<>(sampleSize);
        List<Double> maxScores = new ArrayList<>(sampleSize);
        List<Double> spreads = new ArrayList<>(sampleSize);
        List<Double> potentialLandlordScores = new ArrayList<>(sampleSize);
        List<Double> landlordAdvantages = new ArrayList<>(sampleSize);
        // 结构特征：每局取三家中单牌数最多的那家
        List<Double> maxSinglesList = new ArrayList<>(sampleSize);
        // 结构特征：每局取三家中炸弹数最多的那家
        List<Double> maxBombsList = new ArrayList<>(sampleSize);

        for (int i = 0; i < sampleSize; i++) {
            // 洗牌并发牌
            List<Card> shuffled = strategy.shuffle();
            DealData dealData = strategy.dealCards(shuffled);

            // 计算三家手牌牌力（不含底牌）
            double[] seatScores = strategy.calcAllSeatHandStrength(dealData);
            double min = Math.min(seatScores[0], Math.min(seatScores[1], seatScores[2]));
            double max = Math.max(seatScores[0], Math.max(seatScores[1], seatScores[2]));

            minScores.add(min);
            maxScores.add(max);
            spreads.add(max - min);

            // 计算底牌协同增益后的潜在地主牌力
            double potentialLandlord = strategy.calcPotentialLandlordScore(dealData);
            potentialLandlordScores.add(potentialLandlord);

            // 计算最坏地主优势（枚举三种叫地主方案，取最大值）
            double advantage = strategy.calcMaxLandlordAdvantage(dealData, seatScores);
            landlordAdvantages.add(advantage);

            // 计算三家手牌结构特征：[seat][0]=单牌数, [seat][1]=炸弹数
            int[][] features = strategy.calcAllStructureFeatures(dealData);
            int maxSingles = 0, maxBombs = 0;
            for (int seat = 0; seat < 3; seat++) {
                if (features[seat][0] > maxSingles) maxSingles = features[seat][0];
                if (features[seat][1] > maxBombs) maxBombs = features[seat][1];
            }
            maxSinglesList.add((double) maxSingles);
            maxBombsList.add((double) maxBombs);
        }

        return new DealSampleResult(minScores, maxScores, spreads,
            potentialLandlordScores, landlordAdvantages, maxSinglesList, maxBombsList);
    }

    /**
     * 从有序列表中取指定分位点的值（线性插值）。
     *
     * @param sorted     已排序的数据列表
     * @param percentile 分位点，范围 [0.0, 1.0]（如 0.05 表示 P5）
     * @return 对应分位点的插值结果
     */
    public static double percentile(List<Double> sorted, double percentile) {
        if (sorted == null || sorted.isEmpty()) {
            return 0.0;
        }
        double idx = percentile * (sorted.size() - 1);
        int lo = (int) Math.floor(idx);
        int hi = (int) Math.ceil(idx);
        if (lo == hi) {
            return sorted.get(lo);
        }
        double frac = idx - lo;
        return sorted.get(lo) * (1 - frac) + sorted.get(hi) * frac;
    }

    /**
     * 发牌分布采样结果，封装各均衡性指标的原始数据，并提供分位点查询与阈值建议打印。
     */
    public static class DealSampleResult {

        private static final Logger log = LoggerFactory.getLogger(DealSampleResult.class);

        private final List<Double> minScores;
        private final List<Double> maxScores;
        private final List<Double> spreads;
        private final List<Double> potentialLandlordScores;
        private final List<Double> landlordAdvantages;
        /** 每局三家中单牌数最多的那家的单牌数 */
        private final List<Double> maxSingles;
        /** 每局三家中炸弹数最多的那家的炸弹数 */
        private final List<Double> maxBombs;

        DealSampleResult(List<Double> minScores, List<Double> maxScores,
                         List<Double> spreads, List<Double> potentialLandlordScores,
                         List<Double> landlordAdvantages,
                         List<Double> maxSingles, List<Double> maxBombs) {
            // 排序一次，后续分位点查询直接使用
            this.minScores = sorted(minScores);
            this.maxScores = sorted(maxScores);
            this.spreads = sorted(spreads);
            this.potentialLandlordScores = sorted(potentialLandlordScores);
            this.landlordAdvantages = sorted(landlordAdvantages);
            this.maxSingles = sorted(maxSingles);
            this.maxBombs = sorted(maxBombs);
        }

        private static List<Double> sorted(List<Double> list) {
            List<Double> copy = new ArrayList<>(list);
            Collections.sort(copy);
            return copy;
        }

        /**
         * 查询手牌牌力最小值（即三家中最差那家）的分位点。
         * 建议将 P5 作为 {@code lowerThreshold}。
         */
        public double minScorePercentile(double p) { return percentile(minScores, p); }

        /**
         * 查询手牌牌力最大值（即三家中最强那家）的分位点。
         * 建议将 P95 作为 {@code upperThreshold}。
         */
        public double maxScorePercentile(double p) { return percentile(maxScores, p); }

        /**
         * 查询三家牌力极差（max - min）的分位点。
         * 建议将 P95 作为 {@code maxSpread}。
         */
        public double spreadPercentile(double p) { return percentile(spreads, p); }

        /**
         * 查询潜在地主牌力（手牌 + 底牌最优叫法）的分位点。
         * 建议将 P95 作为 {@code maxPotentialLandlordScore}。
         */
        public double potentialLandlordPercentile(double p) { return percentile(potentialLandlordScores, p); }

        /**
         * 查询最坏地主优势（潜在地主牌力 - 农民手牌均值）的分位点。
         * 建议将 P95 作为 {@code maxLandlordAdvantage}。
         */
        public double landlordAdvantagePercentile(double p) { return percentile(landlordAdvantages, p); }

        /**
         * 查询每局最多单牌数（三家中单牌最多的那家）的分位点。
         * 建议将 P95 或 P99 作为 {@code maxSinglesPerHand}，避免将极端情况当作常态过滤。
         */
        public double maxSinglesPercentile(double p) { return percentile(maxSingles, p); }

        /**
         * 查询每局最多炸弹数（三家中炸弹最多的那家）的分位点。
         * 建议将 P99 作为 {@code maxBombsPerHand}，炸弹集中是低概率极端事件。
         */
        public double maxBombsPercentile(double p) { return percentile(maxBombs, p); }

        /**
         * 打印各指标的关键分位点，格式可直接对应 {@code application.properties} 的配置项。
         * 研发根据输出内容手动更新配置文件。
         */
        public void printRecommendedThresholds() {
            log.info("===== DealDistributionSampler 推荐阈值 =====");
            log.info("# 手牌牌力下阈值（min 的 P5）      → landlord.shuffle-strategy.lower-threshold={}"  , String.format("%.1f", minScorePercentile(0.05)));
            log.info("# 手牌牌力上阈值（max 的 P95）     → landlord.shuffle-strategy.upper-threshold={}"  , String.format("%.1f", maxScorePercentile(0.95)));
            log.info("# 极差上限（spread 的 P95）        → landlord.shuffle-strategy.max-spread={}"       , String.format("%.1f", spreadPercentile(0.95)));
            log.info("# 潜在地主牌力上限（P95）          → landlord.shuffle-strategy.max-potential-landlord-score={}", String.format("%.1f", potentialLandlordPercentile(0.95)));
            log.info("# 地主优势上限（P95）              → landlord.shuffle-strategy.max-landlord-advantage={}"      , String.format("%.1f", landlordAdvantagePercentile(0.95)));
            log.info("# 单家单牌数上限（maxSingles P95） → landlord.shuffle-strategy.max-singles-per-hand={}"        , String.format("%.0f", maxSinglesPercentile(0.95)));
            log.info("# 单家炸弹数上限（maxBombs P99）   → landlord.shuffle-strategy.max-bombs-per-hand={}"          , String.format("%.0f", maxBombsPercentile(0.99)));
            log.info("============================================");
        }

        /**
         * 打印所有指标的详细分位点分布（P5/P10/P25/P50/P75/P90/P95/P99），供深度分析使用。
         */
        public void printDetailedDistribution() {
            double[] ps = {0.05, 0.10, 0.25, 0.50, 0.75, 0.90, 0.95, 0.99};
            log.info("===== DealDistributionSampler 详细分布 =====");
            logRow("minScore（最差一家）", minScores, ps);
            logRow("maxScore（最强一家）", maxScores, ps);
            logRow("spread（极差）      ", spreads, ps);
            logRow("potentialLandlord  ", potentialLandlordScores, ps);
            logRow("landlordAdvantage  ", landlordAdvantages, ps);
            logRow("maxSingles（单牌数）", maxSingles, ps);
            logRow("maxBombs（炸弹数） ", maxBombs, ps);
            log.info("============================================");
        }

        private void logRow(String label, List<Double> data, double[] ps) {
            StringBuilder sb = new StringBuilder(label).append("：");
            for (double p : ps) {
                sb.append(String.format(" P%.0f=%.1f", p * 100, percentile(data, p)));
            }
            log.info("{}", sb);
        }
    }
}
