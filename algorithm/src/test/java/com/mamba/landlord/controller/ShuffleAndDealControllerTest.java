package com.mamba.landlord.controller;

import com.mamba.landlord.algorithm.shuffle.strategy.DefaultShuffleDealStrategy;
import org.junit.jupiter.api.DisplayName;
import org.junit.jupiter.api.Test;

import static org.junit.jupiter.api.Assertions.assertEquals;
import static org.junit.jupiter.api.Assertions.assertNotNull;

/**
 * {@link ShuffleAndDealController} 的简单集成测试（直接调用控制器方法），
 * 验证返回 Map 结构与关键字段。
 */
class ShuffleAndDealControllerTest {

    private final ShuffleAndDealController controller = new ShuffleAndDealController(new DefaultShuffleDealStrategy());

    @Test
    @DisplayName("shuffleAndDeal 应返回三家手牌、底牌及牌力/重洗元数据")
    void shuffleEndpointShouldReturnExpectedStructure() {
        var result = controller.shuffleAndDeal();
        assertNotNull(result);
        assertEquals(17, ((java.util.List<?>) result.get("handDataPlayer0")).size());
        assertEquals(17, ((java.util.List<?>) result.get("handDataPlayer1")).size());
        assertEquals(17, ((java.util.List<?>) result.get("handDataPlayer2")).size());
        assertEquals(3, ((java.util.List<?>) result.get("bottomCards")).size());
        assertNotNull(result.get("handStrengthPlayer0"));
        assertNotNull(result.get("handStrengthPlayer1"));
        assertNotNull(result.get("handStrengthPlayer2"));
        assertNotNull(result.get("reshuffled"));
        assertNotNull(result.get("reshuffleCnt"));
    }
}

