package com.mamba.landlord.core.holder;

import com.mamba.landlord.core.properties.ScoringStrategyProperties;

/**
 * 牌力评分策略配置的静态持有者，供非 Spring 管理的代码按需使用。
 * 由 {@link com.mamba.landlord.core.AlgorithmConfig} 在启动时注入；未注入时使用单例默认配置。
 */
public final class ScoringStrategyHolder {

    private static final StrategyConfigHolder<ScoringStrategyProperties> HOLDER =
        StrategyConfigHolder.create(ScoringStrategyProperties::new);

    private ScoringStrategyHolder() {}

    /** 由 Spring 在启动时设置。 */
    public static void set(ScoringStrategyProperties props) {
        HOLDER.set(props);
    }

    /** 获取当前配置。未注入时返回单例默认实例。 */
    public static ScoringStrategyProperties get() {
        return HOLDER.get();
    }
}
