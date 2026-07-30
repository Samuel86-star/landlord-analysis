package com.mamba.landlord.algorithm.split;

import com.mamba.landlord.algorithm.splitter.DefaultSplitterFactory;
import com.mamba.landlord.core.model.Rank;
import org.junit.jupiter.api.BeforeEach;
import org.junit.jupiter.api.DisplayName;
import org.junit.jupiter.api.Nested;
import org.junit.jupiter.api.Test;

import static org.junit.jupiter.api.Assertions.assertFalse;
import static org.junit.jupiter.api.Assertions.assertTrue;

/**
 * 对 {@link DefaultSplitterFactory#chooseStrategy(int[])} 的单元测试，
 * 覆盖各条决策规则及边界值。顺子区为 Rank 0~11（3~A），count 下标为 Rank.ordinal()。
 */
class SplitterAlgorithmFactoryTest {

    private DefaultSplitterFactory factory;

    /** 顺子区长度 12（3~A），count 数组长度 15（含 2、王）。 */
    private static int[] counts(int c0, int c1, int c2, int c3, int c4, int c5, int c6, int c7, int c8, int c9, int c10, int c11) {
        int[] cnt = new int[Rank.values().length];
        cnt[0] = c0;
        cnt[1] = c1;
        cnt[2] = c2;
        cnt[3] = c3;
        cnt[4] = c4;
        cnt[5] = c5;
        cnt[6] = c6;
        cnt[7] = c7;
        cnt[8] = c8;
        cnt[9] = c9;
        cnt[10] = c10;
        cnt[11] = c11;
        return cnt;
    }

    @BeforeEach
    void setUp() {
        factory = new DefaultSplitterFactory();
    }

    @Nested
    @DisplayName("规则1：强顺子/连对 + 弱飞机 → 顺子/连对优先")
    class Rule1StrongStraightWeakPlane {

        @Test
        @DisplayName("maxStraightLen=7, maxPlaneLen=1, numTriples=1 → true")
        void strongStraightWeakPlane_returnsTrue() {
            int[] cnt = counts(3, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0);
            assertTrue(factory.chooseStrategy(cnt));
        }

        @Test
        @DisplayName("maxConsecutivePairsLen=5, maxPlaneLen=1, numTriples=1 → true")
        void strongPairsWeakPlane_returnsTrue() {
            int[] cnt = counts(2, 2, 2, 2, 2, 0, 0, 0, 0, 0, 0, 0);
            assertTrue(factory.chooseStrategy(cnt));
        }

        @Test
        @DisplayName("边界：maxStraightLen=6 不满足规则1，可能被后续规则命中")
        void boundaryStraight6_notRule1() {
            int[] cnt = counts(1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0);
            assertTrue(factory.chooseStrategy(cnt));
        }

        @Test
        @DisplayName("maxStraightLen=7 但 maxPlaneLen=2 → 不命中规则1，走规则2 → false")
        void strongStraightButPlane2_returnsFalse() {
            int[] cnt = counts(3, 3, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0);
            assertFalse(factory.chooseStrategy(cnt));
        }

        @Test
        @DisplayName("maxStraightLen=7 但 numTriples=2 → 不命中规则1")
        void strongStraightButTwoTriples_notRule1() {
            int[] cnt = counts(3, 3, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0);
            assertFalse(factory.chooseStrategy(cnt));
        }
    }

    @Nested
    @DisplayName("规则2：可组飞机且会破坏顺子/连对 → 飞机/炸弹优先")
    class Rule2PlaneBreaksStraight {

        @Test
        @DisplayName("maxPlaneLen=2, maxStraightLen=6 → false")
        void plane2Straight6_returnsFalse() {
            int[] cnt = counts(3, 3, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0);
            assertFalse(factory.chooseStrategy(cnt));
        }

        @Test
        @DisplayName("maxPlaneLen=2, maxConsecutivePairsLen=4 → false")
        void plane2Pairs4_returnsFalse() {
            int[] cnt = counts(3, 3, 2, 2, 2, 2, 0, 0, 0, 0, 0, 0);
            assertFalse(factory.chooseStrategy(cnt));
        }
    }

    @Nested
    @DisplayName("规则3：顺子/连对潜力明显且三张少 → 顺子/连对优先")
    class Rule3StraightPotentialFewTriples {

        @Test
        @DisplayName("maxStraightLen=6, numTriples=0 → true")
        void straight6NoTriples_returnsTrue() {
            int[] cnt = counts(1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0);
            assertTrue(factory.chooseStrategy(cnt));
        }

        @Test
        @DisplayName("maxConsecutivePairsLen=4, numTriples=1 → true")
        void pairs4OneTriple_returnsTrue() {
            int[] cnt = counts(2, 2, 2, 2, 3, 0, 0, 0, 0, 0, 0, 0);
            assertTrue(factory.chooseStrategy(cnt));
        }
    }

    @Nested
    @DisplayName("规则4：飞机/炸弹结构明显且顺子/连对一般 → 飞机/炸弹优先")
    class Rule4PlaneBombObvious {

        @Test
        @DisplayName("numTriples=3, maxStraightLen<6, maxConsecutivePairsLen<4 → false")
        void threeTriplesShortStraight_returnsFalse() {
            int[] cnt = counts(3, 3, 3, 1, 1, 0, 0, 0, 0, 0, 0, 0);
            assertFalse(factory.chooseStrategy(cnt));
        }

        @Test
        @DisplayName("numBombs=2, maxStraightLen<6, maxConsecutivePairsLen<4 → false")
        void twoBombsShortStraight_returnsFalse() {
            int[] cnt = counts(4, 4, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0);
            assertFalse(factory.chooseStrategy(cnt));
        }

        @Test
        @DisplayName("边界：numTriples=3 但 maxStraightLen=6、maxPlaneLen=1 → 不命中规则2/4，规则3/5不满足，走默认 → false")
        void threeTriplesButLongStraight_returnsFalse() {
            int[] cnt = counts(3, 1, 3, 1, 3, 1, 0, 0, 0, 0, 0, 0);
            assertFalse(factory.chooseStrategy(cnt));
        }
    }

    @Nested
    @DisplayName("规则5：长顺子且三张不多 → 顺子/连对优先")
    class Rule5LongStraightFewTriples {

        @Test
        @DisplayName("maxStraightLen=5, numTriples=2 → true")
        void straight5TwoTriples_returnsTrue() {
            int[] cnt = counts(3, 3, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0);
            assertTrue(factory.chooseStrategy(cnt));
        }

        @Test
        @DisplayName("边界：maxStraightLen=5, numTriples=0 → true")
        void straight5NoTriples_returnsTrue() {
            int[] cnt = counts(1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0);
            assertTrue(factory.chooseStrategy(cnt));
        }

        @Test
        @DisplayName("maxStraightLen=5 但 numTriples=3 → 不命中规则5")
        void straight5ButThreeTriples_notRule5() {
            int[] cnt = counts(3, 3, 3, 1, 1, 0, 0, 0, 0, 0, 0, 0);
            assertFalse(factory.chooseStrategy(cnt));
        }
    }

    @Nested
    @DisplayName("规则6：有飞机潜力且顺子不长 → 飞机/炸弹优先")
    class Rule6PlanePotentialShortStraight {

        @Test
        @DisplayName("maxPlaneLen=2, maxStraightLen=5 且 numTriples=2 → 先命中规则5 → true（规则5先于规则6）")
        void plane2Straight5TwoTriples_rule5First_returnsTrue() {
            int[] cnt = counts(3, 3, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0);
            assertTrue(factory.chooseStrategy(cnt));
        }

        @Test
        @DisplayName("maxPlaneLen=2, maxStraightLen=4 → 不满足规则5，命中规则6 → false")
        void plane2Straight4_returnsFalse() {
            int[] cnt = counts(3, 3, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0);
            assertFalse(factory.chooseStrategy(cnt));
        }

        @Test
        @DisplayName("边界：maxPlaneLen=2, maxStraightLen=6 → 规则2命中 → false")
        void plane2Straight6_rule2_returnsFalse() {
            int[] cnt = counts(3, 3, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0);
            assertFalse(factory.chooseStrategy(cnt));
        }
    }

    @Nested
    @DisplayName("默认：以上均不满足 → 飞机/炸弹优先")
    class DefaultPlaneBombFirst {

        @Test
        @DisplayName("maxStraightLen=4, 无飞机无多三张 → false")
        void shortStraightNoPlane_returnsFalse() {
            int[] cnt = counts(1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0);
            assertFalse(factory.chooseStrategy(cnt));
        }

        @Test
        @DisplayName("顺子区几乎为空 → false")
        void emptyStraightRange_returnsFalse() {
            int[] cnt = counts(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
            assertFalse(factory.chooseStrategy(cnt));
        }
    }

    @Nested
    @DisplayName("边界值：临界阈值")
    class BoundaryThresholds {

        @Test
        @DisplayName("maxStraightLen 恰为 7 且满足规则1 → true")
        void exactStrongStraight7_returnsTrue() {
            int[] cnt = counts(3, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0);
            assertTrue(factory.chooseStrategy(cnt));
        }

        @Test
        @DisplayName("maxStraightLen=6, numTriples=1 → 规则3 → true")
        void straight6OneTriple_returnsTrue() {
            int[] cnt = counts(3, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0);
            assertTrue(factory.chooseStrategy(cnt));
        }

        @Test
        @DisplayName("maxStraightLen=5, maxPlaneLen=2, numTriples=2 → 规则5先于规则6 → true")
        void straight5Plane2_rule5First_returnsTrue() {
            int[] cnt = counts(3, 3, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0);
            assertTrue(factory.chooseStrategy(cnt));
        }
    }
}
