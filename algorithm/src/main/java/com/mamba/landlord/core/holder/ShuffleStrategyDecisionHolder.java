package com.mamba.landlord.core.holder;

import com.mamba.landlord.core.AlgorithmConfig;
import com.mamba.landlord.core.properties.ShuffleStrategyDecisionProperties;

/**
 * 发牌过滤/洗牌策略决策配置的静态持有者，供非 Spring 管理的代码按需使用。
 * 由 {@link AlgorithmConfig} 在启动时注入；未注入时使用单例默认配置。
 */
public final class ShuffleStrategyDecisionHolder {

    private static final StrategyConfigHolder<ShuffleStrategyDecisionProperties> HOLDER =
        StrategyConfigHolder.create(ShuffleStrategyDecisionProperties::new);

    private ShuffleStrategyDecisionHolder() {}

    /** 由 Spring 在启动时设置。 */
    public static void set(ShuffleStrategyDecisionProperties props) {
        HOLDER.set(props);
    }

    /** 获取当前配置。未注入时返回单例默认实例。 */
    public static ShuffleStrategyDecisionProperties get() {
        return HOLDER.get();
    }
}

