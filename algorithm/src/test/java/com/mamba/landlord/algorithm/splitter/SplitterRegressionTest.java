package com.mamba.landlord.algorithm.splitter;

import com.mamba.landlord.algorithm.scoring.strategy.DefaultHandCardsScoringStrategy;
import com.mamba.landlord.core.model.Card;
import com.mamba.landlord.core.model.Combo;
import com.mamba.landlord.core.model.ComboType;
import com.mamba.landlord.core.model.Rank;
import com.mamba.landlord.core.model.Suit;
import org.junit.jupiter.api.Test;

import java.util.List;

import static org.junit.jupiter.api.Assertions.assertEquals;
import static org.junit.jupiter.api.Assertions.assertTrue;

class SplitterRegressionTest {

    private final DefaultComboExtractor extractor =
        new DefaultComboExtractor(new DefaultHandCardsScoringStrategy());

    @Test
    void isolatedTripleDoesNotUseItselfAsPairWing() {
        List<Card> hand = List.of(
            new Card(Rank.THREE, Suit.SPADE),
            new Card(Rank.THREE, Suit.HEART),
            new Card(Rank.THREE, Suit.CLUB)
        );

        List<Combo> combos = extractor.extractAllCombos(hand);

        assertEquals(List.of(Combo.triple(Rank.THREE)), combos);
        assertEquals(hand.size(), combos.stream().mapToInt(Combo::length).sum());
    }

    @Test
    void strongStraightStillComparesBombPreservingSplit() {
        List<Card> hand = List.of(
            card(Rank.THREE), card(Rank.FOUR), card(Rank.FIVE), card(Rank.SIX), card(Rank.SEVEN),
            new Card(Rank.EIGHT, Suit.SPADE), new Card(Rank.EIGHT, Suit.HEART),
            new Card(Rank.EIGHT, Suit.CLUB), new Card(Rank.EIGHT, Suit.DIAMOND),
            card(Rank.NINE), card(Rank.TEN), card(Rank.JACK), card(Rank.QUEEN), card(Rank.KING),
            card(Rank.ACE), card(Rank.TWO), new Card(Rank.SMALL_JOKER, Suit.NONE)
        );

        List<Combo> combos = extractor.extractAllCombos(hand);

        assertTrue(combos.stream().anyMatch(combo -> combo.type() == ComboType.BOMB
            && combo.mainRanks().equals(List.of(Rank.EIGHT))));
        assertEquals(hand.size(), combos.stream().mapToInt(Combo::length).sum());
    }

    @Test
    void tripleUsesOnlyAnotherRankAsPairWing() {
        assertEquals(List.of(Combo.tripleWithPair(Rank.FIVE, Rank.KING)),
            extractor.extractAllCombos(tripleWithPair(Rank.FIVE, Rank.KING)));
        assertEquals(List.of(Combo.tripleWithPair(Rank.KING, Rank.ACE)),
            extractor.extractAllCombos(tripleWithPair(Rank.KING, Rank.ACE)));
        assertEquals(List.of(Combo.tripleWithPair(Rank.ACE, Rank.THREE)),
            extractor.extractAllCombos(tripleWithPair(Rank.ACE, Rank.THREE)));
    }

    @Test
    void longStraightStillPreservesTwoBomb() {
        List<Card> hand = List.of(
            card(Rank.THREE), card(Rank.FOUR), card(Rank.FIVE), card(Rank.SIX), card(Rank.SEVEN),
            card(Rank.EIGHT), card(Rank.NINE), card(Rank.TEN), card(Rank.JACK), card(Rank.QUEEN),
            card(Rank.KING), card(Rank.ACE),
            new Card(Rank.TWO, Suit.SPADE), new Card(Rank.TWO, Suit.HEART),
            new Card(Rank.TWO, Suit.CLUB), new Card(Rank.TWO, Suit.DIAMOND),
            new Card(Rank.SMALL_JOKER, Suit.NONE)
        );

        List<Combo> combos = extractor.extractAllCombos(hand);

        assertTrue(combos.stream().anyMatch(combo -> combo.type() == ComboType.BOMB
            && combo.mainRanks().equals(List.of(Rank.TWO))));
        assertEquals(hand.size(), combos.stream().mapToInt(Combo::length).sum());
    }

    private static List<Card> tripleWithPair(Rank triple, Rank pair) {
        return List.of(
            new Card(triple, Suit.SPADE), new Card(triple, Suit.HEART), new Card(triple, Suit.CLUB),
            new Card(pair, Suit.SPADE), new Card(pair, Suit.HEART)
        );
    }

    private static Card card(Rank rank) {
        return new Card(rank, Suit.SPADE);
    }
}
