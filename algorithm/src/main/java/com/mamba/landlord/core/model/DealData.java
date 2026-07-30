package com.mamba.landlord.core.model;

import lombok.AllArgsConstructor;
import lombok.NoArgsConstructor;
import lombok.experimental.SuperBuilder;

import java.util.List;

/**
 * 原始发牌数据
 */
@SuperBuilder
@NoArgsConstructor
@AllArgsConstructor
public class DealData {
    /** 0号位手牌 */
    private List<Card> handCardsPlayer0;
    /** 1号位手牌 */
    private List<Card> handCardsPlayer1;
    /** 2号位手牌 */
    private List<Card> handCardsPlayer2;
    /** 底牌 */
    private List<Card> bottomCards;

    /**
     * 按座位号取手牌。seat 为 0、1、2 分别对应 0/1/2 号位玩家。
     */
    public List<Card> getHandCards(int seat) {
        return switch (seat) {
            case 0 -> handCardsPlayer0;
            case 1 -> handCardsPlayer1;
            case 2 -> handCardsPlayer2;
            default -> throw new IllegalArgumentException("seat must be 0, 1, or 2");
        };
    }

    /**
     * 获取底牌（3 张）。
     *
     * @return 底牌列表
     */
    public List<Card> getBottomCards() {
        return bottomCards;
    }
}

