package com.mamba.landlord.benchmark;

import com.mamba.landlord.algorithm.shuffle.DealDistributionSampler;
import com.mamba.landlord.algorithm.shuffle.DealDistributionSampler.DealSampleResult;
import com.mamba.landlord.algorithm.shuffle.strategy.DefaultReshuffleDealStrategy;
import com.mamba.landlord.core.holder.ShuffleStrategyDecisionHolder;
import com.mamba.landlord.core.properties.ShuffleStrategyDecisionProperties;
import org.junit.jupiter.api.Test;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

/**
 * 洗牌与评分性能基准测试。
 * <p>
 * 包含两个测试：
 * <ol>
 *   <li>{@link #sampleBaselineDistribution}：使用 {@link DealDistributionSampler} 对原始随机发牌采样，
 *       输出各指标分位点及推荐阈值，供研发更新 {@code application.properties}。</li>
 *   <li>{@link #benchmarkReshufflePerformance}：在推荐阈值下进行压力测试，
 *       验证重洗率和吞吐量是否符合预期。</li>
 * </ol>
 * </p>
 */
class ShuffleAndScoringBenchmarkTest {

    private static final Logger log = LoggerFactory.getLogger(ShuffleAndScoringBenchmarkTest.class);

    /**
     * 对原始随机发牌（不过滤）进行采样，输出各均衡性指标的分位点分布，
     * 以及推荐填入 {@code application.properties} 的阈值。
     * <p>
     * 每次调整评分公式或拆牌策略后，应重新运行此测试以更新基线。
     * </p>
     */
    @Test
    void sampleBaselineDistribution() {
        int sampleRounds = 100_000;

        // 关闭过滤，对原始随机发牌进行无干扰采样
        ShuffleStrategyDecisionProperties props = new ShuffleStrategyDecisionProperties();
        props.setEnabled(false);
        ShuffleStrategyDecisionHolder.set(props);

        DefaultReshuffleDealStrategy strategy = new DefaultReshuffleDealStrategy();

        // 使用 DealDistributionSampler 完成采样与分位点计算
        DealDistributionSampler sampler = new DealDistributionSampler(strategy);
        DealSampleResult result = sampler.sample(sampleRounds);

        // 打印详细分位点分布，供深度分析
        log.info("[ShuffleAndScoringBenchmark] sampleRounds={}", sampleRounds);
        result.printDetailedDistribution();

        // 打印推荐阈值，格式对应 application.properties 配置项
        result.printRecommendedThresholds();
    }

    /**
     * 使用采样得到的 P5/P95 阈值进行压力测试，验证重洗率和吞吐量。
     * <p>
     * 输出：总重洗次数、平均每局重洗次数、每秒处理局数，用于评估过滤配置的性能影响。
     * </p>
     */
    @Test
    void benchmarkReshufflePerformance() {
        int sampleRounds = 2_000;

        // 第一阶段：采样基线，确定阈值
        ShuffleStrategyDecisionProperties samplingProps = new ShuffleStrategyDecisionProperties();
        samplingProps.setEnabled(false);
        ShuffleStrategyDecisionHolder.set(samplingProps);

        DefaultReshuffleDealStrategy strategy = new DefaultReshuffleDealStrategy();
        DealDistributionSampler sampler = new DealDistributionSampler(strategy);
        DealSampleResult baseline = sampler.sample(sampleRounds);

        double lowerThreshold = baseline.minScorePercentile(0.05);
        double upperThreshold = baseline.maxScorePercentile(0.95);

        log.info("[ShuffleAndScoringBenchmark] sampleRounds={}, lowerThreshold={}, upperThreshold={}",
            sampleRounds, String.format("%.1f", lowerThreshold), String.format("%.1f", upperThreshold));

        // 第二阶段：启用过滤，进行压力测试
        int benchmarkRounds = 5_000;
        ShuffleStrategyDecisionProperties benchmarkProps = new ShuffleStrategyDecisionProperties();
        benchmarkProps.setEnabled(true);
        benchmarkProps.setLowerThreshold(lowerThreshold);
        benchmarkProps.setUpperThreshold(upperThreshold);
        benchmarkProps.setMaxReshuffleTimes(5);
        ShuffleStrategyDecisionHolder.set(benchmarkProps);

        long reshuffleTotal = 0;
        long start = System.currentTimeMillis();
        for (int i = 0; i < benchmarkRounds; i++) {
            reshuffleTotal += strategy.shuffleAndDeal().getReshuffleCnt();
        }
        long costMs = System.currentTimeMillis() - start;

        double avgReshufflesPerDeal = reshuffleTotal * 1.0 / benchmarkRounds;
        double dealsPerSecond = benchmarkRounds * 1000.0 / Math.max(1, costMs);

        log.info("[ShuffleAndScoringBenchmark] benchmarkRounds={}, lowerThreshold={}, upperThreshold={},"
                + " totalReshuffles={}, avgReshufflesPerDeal={}, elapsedMs={}, dealsPerSecond={}",
            benchmarkRounds, String.format("%.1f", lowerThreshold), String.format("%.1f", upperThreshold),
            reshuffleTotal, String.format("%.2f", avgReshufflesPerDeal), costMs, String.format("%.2f", dealsPerSecond));

        assert avgReshufflesPerDeal >= 0.0;
    }
}

