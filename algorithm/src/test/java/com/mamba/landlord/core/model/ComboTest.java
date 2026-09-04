package com.mamba.landlord.core.model;

import com.mamba.landlord.algorithm.scoring.strategy.DefaultComboScoringStrategy;
import org.junit.jupiter.api.Test;

import java.util.List;

import static org.junit.jupiter.api.Assertions.assertEquals;

class ComboTest {

    @Test
    void testComboFactories() {
        DefaultComboScoringStrategy scorer = new DefaultComboScoringStrategy();

        Combo single = Combo.single(Rank.ACE);
        assertEquals(ComboType.SINGLE, single.type());
        assertEquals(1, single.length());
        assertEquals(0.0, scorer.score(single), 0.001);

        Combo pair = Combo.pair(Rank.ACE);
        assertEquals(ComboType.PAIR, pair.type());
        assertEquals(2, pair.length());
        assertEquals(14.0, scorer.score(pair), 0.001);

        Combo triple = Combo.triple(Rank.ACE);
        assertEquals(ComboType.TRIPLE, triple.type());
        assertEquals(3, triple.length());
        assertEquals(23.0, scorer.score(triple), 0.001);

        Combo straight = Combo.straight(List.of(Rank.THREE, Rank.FOUR, Rank.FIVE, Rank.SIX, Rank.SEVEN));
        assertEquals(ComboType.STRAIGHT, straight.type());
        assertEquals(5, straight.length());
        assertEquals(15.0, scorer.score(straight), 0.001);

        Combo consecutivePairs = Combo.consecutivePairs(List.of(Rank.THREE, Rank.FOUR, Rank.FIVE));
        assertEquals(ComboType.CONSECUTIVE_PAIRS, consecutivePairs.type());
        assertEquals(6, consecutivePairs.length());
        assertEquals(12.0, scorer.score(consecutivePairs), 0.001);

        Combo bomb = Combo.bomb(Rank.KING);
        assertEquals(ComboType.BOMB, bomb.type());
        assertEquals(4, bomb.length());
        assertEquals(43.0, scorer.score(bomb), 0.001);

        Combo rocket = Combo.rocket();
        assertEquals(ComboType.ROCKET, rocket.type());
        assertEquals(2, rocket.length());
        assertEquals(60.0, scorer.score(rocket), 0.001);
    }
}
