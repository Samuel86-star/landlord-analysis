package com.mamba.landlord.core.holder;

import java.util.function.Supplier;

/**
 * 策略配置的静态持有者泛型实现。
 * 供非 Spring 管理的代码按需使用；未注入时通过 defaultFactory 提供默认实例。
 *
 * @param <T> 配置类型
 */
public final class StrategyConfigHolder<T> {

    private volatile T instance;
    private final Supplier<T> defaultFactory;

    private StrategyConfigHolder(Supplier<T> defaultFactory) {
        this.defaultFactory = defaultFactory;
    }

    /**
     * 创建持有者。
     *
     * @param defaultFactory 未注入时返回的默认实例工厂
     * @return 持有者实例
     */
    public static <T> StrategyConfigHolder<T> create(Supplier<T> defaultFactory) {
        return new StrategyConfigHolder<>(defaultFactory);
    }

    public void set(T instance) {
        this.instance = instance;
    }

    private volatile T defaultInstance;

    /**
     * 获取当前配置。若未设置，返回并缓存 defaultFactory 提供的单例默认实例。
     */
    public T get() {
        T current = instance;
        if (current != null) {
            return current;
        }
        if (defaultInstance == null) {
            synchronized (this) {
                if (defaultInstance == null) {
                    defaultInstance = defaultFactory.get();
                }
            }
        }
        return defaultInstance;
    }
}
