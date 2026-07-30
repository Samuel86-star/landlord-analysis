package com.mamba.landlord.core.holder;

import com.mamba.landlord.core.AlgorithmConfig;
import com.mamba.landlord.core.properties.SplitStrategyDecisionProperties;

/**
 * 拆牌策略决策配置的静态持有者，供非 Spring 管理的 {@link com.mamba.landlord.algorithm.splitter.DefaultSplitterFactory} 使用。
 * 由 {@link AlgorithmConfig} 在启动时注入；未注入时使用单例默认配置。
 */
public final class SplitStrategyDecisionHolder {

    private static final StrategyConfigHolder<SplitStrategyDecisionProperties> HOLDER =
        StrategyConfigHolder.create(SplitStrategyDecisionProperties::new);

    private SplitStrategyDecisionHolder() {}

    /** 由 Spring 在启动时设置。 */
    public static void set(SplitStrategyDecisionProperties strategyDecisionProperties) {
        HOLDER.set(strategyDecisionProperties);
    }

    /** 获取当前配置。未注入时返回单例默认实例。 */
    public static SplitStrategyDecisionProperties get() {
        return HOLDER.get();
    }
}
