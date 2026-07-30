package com.mamba.landlord.algorithm.scoring.strategy;

import com.mamba.landlord.core.model.Card;
import com.mamba.landlord.core.model.Combo;
import com.mamba.landlord.core.model.Rank;
import com.mamba.landlord.core.model.Suit;
import org.junit.jupiter.api.DisplayName;
import org.junit.jupiter.api.Test;

import java.util.List;

import static org.junit.jupiter.api.Assertions.assertTrue;

/**
 * {@link DefaultHandCardsScoringStrategy} 的单元测试。
 * 通过构造不同典型手牌，验证整体牌力的相对大小关系。
 */
class DefaultHandCardsScoringStrategyTest {

    private final DefaultHandCardsScoringStrategy strategy = new DefaultHandCardsScoringStrategy();

    @Test
    @DisplayName("包含大王、2、炸弹的强牌应明显高于普通散牌")
    void strongHandShouldScoreMoreThanWeakSingles() {
        // 弱牌：几乎全是小单牌
        List<Card> weakHand = List.of(
            new Card(Rank.THREE, Suit.SPADE),
            new Card(Rank.FOUR, Suit.HEART),
            new Card(Rank.FIVE, Suit.CLUB),
            new Card(Rank.SIX, Suit.DIAMOND)
        );
        List<Combo> weakCombos = List.of(
            Combo.single(Rank.THREE),
            Combo.single(Rank.FOUR),
            Combo.single(Rank.FIVE),
            Combo.single(Rank.SIX)
        );
        double weakScore = strategy.calcTotalHandScore(weakHand, weakCombos);

        // 强牌：包含 2 对、炸弹和王炸等高价值牌
        List<Card> strongHand = List.of(
            new Card(Rank.TWO, Suit.SPADE),
            new Card(Rank.TWO, Suit.HEART),
            new Card(Rank.BIG_JOKER, Suit.NONE),
            new Card(Rank.SMALL_JOKER, Suit.NONE),
            new Card(Rank.ACE, Suit.SPADE),
            new Card(Rank.ACE, Suit.HEART),
            new Card(Rank.ACE, Suit.CLUB),
            new Card(Rank.ACE, Suit.DIAMOND)
        );
        List<Combo> strongCombos = List.of(
            Combo.pair(Rank.TWO),
            Combo.bomb(Rank.ACE),
            Combo.rocket()
        );
        double strongScore = strategy.calcTotalHandScore(strongHand, strongCombos);

        assertTrue(strongScore > weakScore, "包含 2、炸弹和王炸的牌应显著强于全是小单牌的手牌");
    }

    @Test
    @DisplayName("好的结构（顺子、炸弹）应优于同点数但只拆成散牌的结构")
    void structuredHandBeatsSameRanksAsSingles() {
        // 同一批牌：3-7 的顺子以及一个 8 炸弹
        List<Card> cards = List.of(
            new Card(Rank.THREE, Suit.SPADE),
            new Card(Rank.FOUR, Suit.SPADE),
            new Card(Rank.FIVE, Suit.SPADE),
            new Card(Rank.SIX, Suit.SPADE),
            new Card(Rank.SEVEN, Suit.SPADE),
            new Card(Rank.EIGHT, Suit.SPADE),
            new Card(Rank.EIGHT, Suit.HEART),
            new Card(Rank.EIGHT, Suit.CLUB),
            new Card(Rank.EIGHT, Suit.DIAMOND)
        );

        // 结构 1：全拆成单牌
        List<Combo> singlesOnly = List.of(
            Combo.single(Rank.THREE),
            Combo.single(Rank.FOUR),
            Combo.single(Rank.FIVE),
            Combo.single(Rank.SIX),
            Combo.single(Rank.SEVEN),
            Combo.single(Rank.EIGHT),
            Combo.single(Rank.EIGHT),
            Combo.single(Rank.EIGHT),
            Combo.single(Rank.EIGHT)
        );
        double scoreSingles = strategy.calcTotalHandScore(cards, singlesOnly);

        // 结构 2：顺子 + 炸弹
        List<Combo> straightAndBomb = List.of(
            Combo.straight(List.of(Rank.THREE, Rank.FOUR, Rank.FIVE, Rank.SIX, Rank.SEVEN)),
            Combo.bomb(Rank.EIGHT)
        );
        double scoreStructured = strategy.calcTotalHandScore(cards, straightAndBomb);

        assertTrue(scoreStructured > scoreSingles, "顺子+炸弹的结构应优于同一批牌全部拆成散牌");
    }
}

