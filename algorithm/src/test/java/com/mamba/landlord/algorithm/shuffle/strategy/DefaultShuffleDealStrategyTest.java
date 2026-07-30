package com.mamba.landlord.algorithm.shuffle.strategy;

import com.mamba.landlord.core.model.Card;
import com.mamba.landlord.core.model.DealData;
import com.mamba.landlord.core.model.Deck;
import com.mamba.landlord.core.model.EvaluatedDealData;
import org.junit.jupiter.api.DisplayName;
import org.junit.jupiter.api.Test;

import java.util.HashSet;
import java.util.List;
import java.util.Set;

import static org.junit.jupiter.api.Assertions.*;

/**
 * {@link DefaultShuffleDealStrategy} 的基础行为测试：
 * - 洗牌返回 54 张牌；
 * - 发牌结果 17/17/17/3；
 * - 无重复或丢失的牌。
 */
class DefaultShuffleDealStrategyTest {

    private final DefaultShuffleDealStrategy strategy = new DefaultShuffleDealStrategy();

    @Test
    @DisplayName("shuffle 应返回一副 54 张的完整牌堆")
    void shuffleShouldReturn54Cards() {
        List<Card> shuffled = strategy.shuffle();
        assertNotNull(shuffled);
        assertEquals(54, shuffled.size(), "洗牌后应仍然是 54 张牌");

        // 验证与原始牌堆相比没有缺失/重复（集合比较）
        List<Card> original = Deck.copyFullDeckCards();
        assertEquals(new HashSet<>(original), new HashSet<>(shuffled),
            "洗牌后应只是顺序改变，元素集合与原始牌堆一致");
    }

    @Test
    @DisplayName("dealCards 应按 17/17/17/3 分发手牌和底牌")
    void dealCardsShouldSplitAs17_17_17_3() {
        List<Card> shuffled = Deck.copyFullDeckCards();
        DealData dealData = strategy.dealCards(shuffled);

        assertNotNull(dealData);
        assertEquals(17, dealData.getHandCards(0).size(), "0 号位应有 17 张牌");
        assertEquals(17, dealData.getHandCards(1).size(), "1 号位应有 17 张牌");
        assertEquals(17, dealData.getHandCards(2).size(), "2 号位应有 17 张牌");
        assertEquals(3, dealData.getBottomCards().size(), "底牌应有 3 张");

        // 验证 54 张牌不重不丢
        Set<Card> all = new HashSet<>();
        all.addAll(dealData.getHandCards(0));
        all.addAll(dealData.getHandCards(1));
        all.addAll(dealData.getHandCards(2));
        all.addAll(dealData.getBottomCards());

        assertEquals(54, all.size(), "发牌后 54 张牌应全部且仅出现一次");
    }

    @Test
    @DisplayName("shuffleAndDeal 应返回包含牌力值信息的 EvaluatedDealData，且默认不重洗")
    void shuffleAndDealShouldReturnEvaluatedDataWithoutReshuffleByDefault() {
        EvaluatedDealData evaluated = strategy.shuffleAndDeal();
        assertNotNull(evaluated);

        assertEquals(17, evaluated.getHandCards(0).size());
        assertEquals(17, evaluated.getHandCards(1).size());
        assertEquals(17, evaluated.getHandCards(2).size());
        assertEquals(3, evaluated.getBottomCards().size());

        // 只要能算出非 NaN 的牌力值即可，不强行约束具体数值
        assertFalse(Double.isNaN(evaluated.getHandStrengthPlayer0()));
        assertFalse(Double.isNaN(evaluated.getHandStrengthPlayer1()));
        assertFalse(Double.isNaN(evaluated.getHandStrengthPlayer2()));

        assertFalse(evaluated.isReshuffled(), "默认策略不包含重洗逻辑，应为 false");
        assertEquals(0, evaluated.getReshuffleCnt(), "默认策略不包含重洗逻辑，次数应为 0");
    }
}

