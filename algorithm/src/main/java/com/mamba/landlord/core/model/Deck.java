package com.mamba.landlord.core.model;

import java.util.ArrayList;
import java.util.List;

/**
 * 斗地主牌堆模型。提供一副标准 54 张牌的构造。
 * <p>
 * 洗牌、发牌等算法由 {@code com.mamba.landlord.algorithm.shuffle} 中的工具类负责。
 * </p>
 */
public final class Deck {

    /** 标准斗地主牌堆张数 */
    public static final int DECK_SIZE = 54;
    /** 每家手牌张数 */
    public static final int HAND_SIZE = 17;
    /** 底牌张数 */
    public static final int BOTTOM_SIZE = 3;

    /** 花色 */
    private static final Suit[] SUITS = {Suit.SPADE, Suit.HEART, Suit.CLUB, Suit.DIAMOND};
    private static final Rank[] ALL_RANKS = Rank.values();
    private static final List<Card> fullDeckCards = buildFullDeck();

    private Deck() {
    }

    /**
     * 构建完整一副牌
     * @return 一副完整的一副牌
     */
    private static List<Card> buildFullDeck() {
        List<Card> deck = new ArrayList<>(Deck.DECK_SIZE);
        for (Rank r : ALL_RANKS) {
            if (r == Rank.SMALL_JOKER || r == Rank.BIG_JOKER) {
                deck.add(new Card(r, Suit.NONE));
            } else {
                for (Suit s : SUITS) {
                    deck.add(new Card(r, s));
                }
            }
        }
        return List.copyOf(deck);
    }

    /**
     * 返回一副标准 54 张牌（未洗牌）的副本。
     */
    public static List<Card> copyFullDeckCards() {
        return new ArrayList<>(fullDeckCards);
    }
}

