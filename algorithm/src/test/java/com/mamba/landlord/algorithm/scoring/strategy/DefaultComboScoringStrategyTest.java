package com.mamba.landlord.algorithm.scoring.strategy;

import com.mamba.landlord.core.model.Combo;
import com.mamba.landlord.core.model.Rank;
import org.junit.jupiter.api.DisplayName;
import org.junit.jupiter.api.Test;

import java.util.List;

import static org.junit.jupiter.api.Assertions.assertEquals;
import static org.junit.jupiter.api.Assertions.assertTrue;

/**
 * {@link DefaultComboScoringStrategy} 的单元测试。
 * 只关注典型牌型之间的相对大小关系是否符合直觉。
 */
class DefaultComboScoringStrategyTest {

    private final DefaultComboScoringStrategy strategy = new DefaultComboScoringStrategy();

    @Test
    @DisplayName("炸弹与王炸的分值应明显高于普通牌型")
    void bombAndRocketShouldBeHighest() {
        double single3 = strategy.score(Combo.single(Rank.THREE));
        double bomb3 = strategy.score(Combo.bomb(Rank.THREE));
        double rocket = strategy.score(Combo.rocket());

        // 对于任意点数，炸弹远大于对应的单牌得分
        assertTrue(bomb3 > single3, "炸弹应明显高于对应的单牌");
        assertTrue(rocket > bomb3, "王炸应高于普通炸弹");
    }

    @Test
    @DisplayName("同类型牌型中，高点数应比低点数得分更高")
    void higherRankShouldScoreMoreWithinSameType() {
        double pair3 = strategy.score(Combo.pair(Rank.THREE));
        double pairA = strategy.score(Combo.pair(Rank.ACE));
        double triple4 = strategy.score(Combo.triple(Rank.FOUR));
        double tripleQ = strategy.score(Combo.triple(Rank.QUEEN));

        assertTrue(pairA > pair3, "A 对应高于 3 对");
        assertTrue(tripleQ > triple4, "Q 三张应高于 4 三张");
    }

    @Test
    @DisplayName("顺子长度增加时，总分应随之增加")
    void longerStraightShouldScoreMore() {
        double straight5 = strategy.score(Combo.straight(
            List.of(Rank.THREE, Rank.FOUR, Rank.FIVE, Rank.SIX, Rank.SEVEN)
        ));
        double straight7 = strategy.score(Combo.straight(
            List.of(Rank.THREE, Rank.FOUR, Rank.FIVE, Rank.SIX, Rank.SEVEN, Rank.EIGHT, Rank.NINE)
        ));

        assertTrue(straight7 > straight5, "更长的顺子应得到更高分");
    }

    @Test
    @DisplayName("飞机带翅膀的分值应高于不带翅膀的飞机")
    void planeWithWingsShouldScoreMoreThanPlainPlane() {
        double plainPlane = strategy.score(Combo.plane(
            List.of(Rank.FOUR, Rank.FIVE)
        ));
        double planeWithSingles = strategy.score(Combo.planeWithSingles(
            List.of(Rank.FOUR, Rank.FIVE),
            List.of(Rank.THREE, Rank.THREE)
        ));
        double planeWithPairs = strategy.score(Combo.planeWithPairs(
            List.of(Rank.FOUR, Rank.FIVE),
            List.of(Rank.THREE, Rank.THREE)
        ));

        assertTrue(planeWithSingles > plainPlane, "飞机带单应高于不带牌的飞机");
        assertTrue(planeWithPairs > plainPlane, "飞机带对应高于不带牌的飞机");
    }

    @Test
    @DisplayName("对子翼加分 = 基础分 + 1，单牌翼仍为基础分一半")
    void pairWingScoreIsBaseValuePlusOne() {
        // 三带一对 AAA + 22：主分 6*3+10=28；对子翼 = 基础分(10)+1 = 11
        double tripleWithPair = strategy.score(Combo.tripleWithPair(Rank.ACE, Rank.TWO));
        assertEquals(39.0, tripleWithPair, 0.001, "三带一对的对子翼 = 基础分 + 1");

        // 对照：三带一 AAA + 2（单牌翼）= 6*3+8 + max(0,10*0.5) = 26+5 = 31
        double tripleWithSingle = strategy.score(Combo.tripleWithSingle(Rank.ACE, Rank.TWO));
        assertEquals(31.0, tripleWithSingle, 0.001, "三带一的单牌翼 = 基础分 * 0.5");

        // 小牌对子翼被截断为 0：三带一对 AAA + 33 → 翼 max(0, -7+1) = 0，总分 28
        double tripleWithPairLow = strategy.score(Combo.tripleWithPair(Rank.ACE, Rank.THREE));
        assertEquals(28.0, tripleWithPairLow, 0.001, "负基础分对子翼按 0 计");
    }
}

