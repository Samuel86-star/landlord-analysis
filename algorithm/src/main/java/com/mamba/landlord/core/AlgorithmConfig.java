package com.mamba.landlord.core;

import com.mamba.landlord.algorithm.scoring.IHandCardsScoringStrategy;
import com.mamba.landlord.algorithm.scoring.strategy.DefaultComboScoringStrategy;
import com.mamba.landlord.algorithm.scoring.strategy.DefaultHandCardsScoringStrategy;
import com.mamba.landlord.algorithm.shuffle.IShuffleDealStrategy;
import com.mamba.landlord.algorithm.shuffle.strategy.DefaultReshuffleDealStrategy;
import com.mamba.landlord.algorithm.shuffle.strategy.DefaultShuffleDealStrategy;
import com.mamba.landlord.algorithm.splitter.IComboExtractor;
import com.mamba.landlord.algorithm.splitter.DefaultComboExtractor;
import com.mamba.landlord.core.holder.ScoringStrategyHolder;
import com.mamba.landlord.core.holder.ShuffleStrategyDecisionHolder;
import com.mamba.landlord.core.holder.SplitStrategyDecisionHolder;
import com.mamba.landlord.core.properties.ScoringStrategyProperties;
import com.mamba.landlord.core.properties.ShuffleStrategyDecisionProperties;
import com.mamba.landlord.core.properties.SplitStrategyDecisionProperties;
import org.springframework.boot.context.properties.EnableConfigurationProperties;
import org.springframework.context.annotation.Bean;
import org.springframework.context.annotation.Configuration;

import jakarta.annotation.PostConstruct;

@Configuration
@EnableConfigurationProperties({SplitStrategyDecisionProperties.class, ShuffleStrategyDecisionProperties.class, ScoringStrategyProperties.class})
public class AlgorithmConfig {

    private final SplitStrategyDecisionProperties splitStrategyDecisionProperties;
    private final ShuffleStrategyDecisionProperties shuffleStrategyDecisionProperties;
    private final ScoringStrategyProperties scoringStrategyProperties;

    public AlgorithmConfig(SplitStrategyDecisionProperties splitStrategyDecisionProperties,
                           ShuffleStrategyDecisionProperties shuffleStrategyDecisionProperties,
                           ScoringStrategyProperties scoringStrategyProperties) {
        this.splitStrategyDecisionProperties = splitStrategyDecisionProperties;
        this.shuffleStrategyDecisionProperties = shuffleStrategyDecisionProperties;
        this.scoringStrategyProperties = scoringStrategyProperties;
    }

    @PostConstruct
    public void initStrategyHolders() {
        SplitStrategyDecisionHolder.set(splitStrategyDecisionProperties);
        ShuffleStrategyDecisionHolder.set(shuffleStrategyDecisionProperties);
        ScoringStrategyHolder.set(scoringStrategyProperties);
    }

    @Bean
    public IHandCardsScoringStrategy handCardsScoringStrategy() {
        DefaultComboScoringStrategy comboScoringStrategy = new DefaultComboScoringStrategy(scoringStrategyProperties);
        return new DefaultHandCardsScoringStrategy(comboScoringStrategy, scoringStrategyProperties);
    }

    @Bean
    public IComboExtractor comboExtractor(IHandCardsScoringStrategy handCardsScoringStrategy) {
        return new DefaultComboExtractor(handCardsScoringStrategy);
    }

    /**
     * 根据 {@link ShuffleStrategyDecisionProperties#isEnabled()} 选择洗牌策略：
     * 启用时使用带重洗过滤的 {@link DefaultReshuffleDealStrategy}，否则使用 {@link DefaultShuffleDealStrategy}。
     */
    @Bean
    public IShuffleDealStrategy shuffleDealStrategy(IHandCardsScoringStrategy handCardsScoringStrategy,
                                                    IComboExtractor comboExtractor) {
        if (shuffleStrategyDecisionProperties.isEnabled()) {
            return new DefaultReshuffleDealStrategy(handCardsScoringStrategy, comboExtractor);
        }
        return new DefaultShuffleDealStrategy(handCardsScoringStrategy, comboExtractor);
    }
}

