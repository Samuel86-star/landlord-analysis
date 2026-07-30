package com.mamba.landlord.core.properties;

import com.mamba.landlord.core.holder.ShuffleStrategyDecisionHolder;
import org.springframework.boot.context.properties.ConfigurationProperties;

/**
 * 发牌过滤/洗牌策略决策参数。
 * <p>
 * 阈值建议通过 {@code DealDistributionSampler} 对随机发牌结果采样后，
 * 取 P5/P95 等分位点填入，而非直接拍定经验值。
 * </p>
 * <p>
 * 单元测试中可直接 {@code new ShuffleStrategyDecisionProperties()} 后通过 setter 设置所需值，
 * 再注入使用该配置的洗牌/发牌组件；
 * 或通过 {@link ShuffleStrategyDecisionHolder#set(ShuffleStrategyDecisionProperties)} 设置静态配置。
 * </p>
 */
@ConfigurationProperties(prefix = "landlord.shuffle-strategy")
public class ShuffleStrategyDecisionProperties {

    /**
     * 是否启用发牌过滤。false 时直接返回洗牌结果，不进行任何极端判断或重洗。
     */
    private boolean enabled = true;

    /**
     * 手牌牌力下阈值：任一家低于此分视为极端差牌，触发重洗。
     * 建议取基线分布的 P5 分位点。
     */
    private double lowerThreshold = Double.NEGATIVE_INFINITY;

    /**
     * 手牌牌力上阈值：任一家高于此分视为极端好牌，触发重洗。
     * 建议取基线分布的 P95 分位点。
     */
    private double upperThreshold = Double.POSITIVE_INFINITY;

    /**
     * 三家牌力极差上限：max(scores) - min(scores) 超过此值视为分布不均，触发重洗。
     * 建议取基线极差分布的 P95 分位点。
     */
    private double maxSpread = Double.POSITIVE_INFINITY;

    /**
     * 潜在地主牌力上限：枚举三家分别叫地主（手牌 + 底牌）后，
     * 若任一家潜在地主牌力超过此值，视为地主优势过大，触发重洗。
     * 建议取基线潜在地主分分布的 P95 分位点。
     * {@code Double.POSITIVE_INFINITY} 表示不启用此检测。
     */
    private double maxPotentialLandlordScore = Double.POSITIVE_INFINITY;

    /**
     * 地主优势上限：枚举三家分别叫地主时，
     * 若「潜在地主牌力 - 两个农民手牌均值」超过此值，视为局面失衡，触发重洗。
     * {@code Double.POSITIVE_INFINITY} 表示不启用此检测。
     */
    private double maxLandlordAdvantage = Double.POSITIVE_INFINITY;

    /**
     * 单家手牌中单牌数量上限（拆牌后 SINGLE 类型的组合数）。
     * 超过此值认为手牌可打出性极差（出牌次数过多），触发重洗。
     * 0 表示不启用此检测。
     */
    private int maxSinglesPerHand = 0;

    /**
     * 单家手牌中炸弹数量上限（含王炸）。
     * 超过此值认为某家牌力结构过强，触发重洗。
     * 0 表示不启用此检测。
     */
    private int maxBombsPerHand = 0;

    /**
     * 渐进式阈值放宽步长（0~1）。每次重洗后，阈值区间按 (1 + i * relaxStep) 比例放宽，
     * 其中 i 为当前重洗次数。0 表示不放宽（固定阈值），0.15 表示每次放宽 15%。
     */
    private double thresholdRelaxStep = 0.15;

    /**
     * 最大重洗次数，超过则接受当前结果，避免死循环。
     */
    private int maxReshuffleTimes = 5;

    /**
     * 规则版本标识，用于日志追踪配置变更。
     */
    private String version = "dealing_filter_v2";

    public boolean isEnabled() {
        return enabled;
    }

    public void setEnabled(boolean enabled) {
        this.enabled = enabled;
    }

    public double getLowerThreshold() {
        return lowerThreshold;
    }

    public void setLowerThreshold(double lowerThreshold) {
        this.lowerThreshold = lowerThreshold;
    }

    public double getUpperThreshold() {
        return upperThreshold;
    }

    public void setUpperThreshold(double upperThreshold) {
        this.upperThreshold = upperThreshold;
    }

    public double getMaxSpread() {
        return maxSpread;
    }

    public void setMaxSpread(double maxSpread) {
        this.maxSpread = maxSpread;
    }

    public double getMaxPotentialLandlordScore() {
        return maxPotentialLandlordScore;
    }

    public void setMaxPotentialLandlordScore(double maxPotentialLandlordScore) {
        this.maxPotentialLandlordScore = maxPotentialLandlordScore;
    }

    public double getMaxLandlordAdvantage() {
        return maxLandlordAdvantage;
    }

    public void setMaxLandlordAdvantage(double maxLandlordAdvantage) {
        this.maxLandlordAdvantage = maxLandlordAdvantage;
    }

    public int getMaxSinglesPerHand() {
        return maxSinglesPerHand;
    }

    public void setMaxSinglesPerHand(int maxSinglesPerHand) {
        this.maxSinglesPerHand = maxSinglesPerHand;
    }

    public int getMaxBombsPerHand() {
        return maxBombsPerHand;
    }

    public void setMaxBombsPerHand(int maxBombsPerHand) {
        this.maxBombsPerHand = maxBombsPerHand;
    }

    public double getThresholdRelaxStep() {
        return thresholdRelaxStep;
    }

    public void setThresholdRelaxStep(double thresholdRelaxStep) {
        this.thresholdRelaxStep = thresholdRelaxStep;
    }

    public int getMaxReshuffleTimes() {
        return maxReshuffleTimes;
    }

    public void setMaxReshuffleTimes(int maxReshuffleTimes) {
        this.maxReshuffleTimes = maxReshuffleTimes;
    }

    public String getVersion() {
        return version;
    }

    public void setVersion(String version) {
        this.version = version;
    }
}

