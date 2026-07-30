package com.mamba.landlord.algorithm.utils;

import com.mamba.landlord.core.model.Card;
import com.mamba.landlord.core.model.Rank;

import java.util.List;

/**
 * 手牌相关工具方法。
 */
public final class HandCardUtils {

    private static final int RANK_COUNT = Rank.values().length;

    private HandCardUtils() {}

    /**
     * 根据手牌构建点数计数数组：index = rank.ordinal()。
     *
     * @param handCards 手牌
     * @return 计数数组，下标为 Rank.ordinal()
     */
    public static int[] buildRankCounts(List<Card> handCards) {
        if (handCards == null) {
            throw new IllegalArgumentException("handCards must not be null");
        }
        int[] count = new int[RANK_COUNT];
        for (Card c : handCards) {
            count[c.rank().ordinal()]++;
        }
        return count;
    }
}
