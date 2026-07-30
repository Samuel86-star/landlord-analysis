package com.mamba.landlord.core.model;

import java.util.Comparator;
import java.util.Objects;

/**
 * 斗地主单张牌。牌力计算不区分花色，仅看 Rank；花色保留用于展示与扩展。
 */
public record Card(Rank rank, Suit suit) {

    /** 按牌面顺序排序的默认比较器 */
    public static final Comparator<Card> defaultCardRankComp =
            Comparator.comparing(Card::rank, Rank.defaultStraightOrder);

    /**
     * @see Rank#getBaseValue()
     * @return 单牌牌力值
     */
    public int getBaseValue() {
        return rank.getBaseValue();
    }

    public boolean equals(Object o) {
        if (this == o) return true;
        if (o == null || getClass() != o.getClass()) return false;
        Card card = (Card) o;
        return rank == card.rank && suit == card.suit;
    }

    public int hashCode() {
        return Objects.hash(rank, suit);
    }

    public String toString() {
        if (suit == Suit.NONE) {
            return rank.name();
        }
        return suit.name() + "_" + rank.name();
    }
}
