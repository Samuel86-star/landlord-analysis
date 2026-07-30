package com.mamba.landlord.algorithm.splitter;

import com.mamba.landlord.algorithm.utils.HandCardUtils;
import com.mamba.landlord.core.model.Card;
import com.mamba.landlord.core.model.Combo;
import com.mamba.landlord.core.model.Rank;

import java.util.ArrayList;
import java.util.List;

/**
 * 牌型识别与基础计数的抽象基类。
 * <p>
 * 职责：围绕 {@link Rank} 计数（使用 rank 下标的数组）提供通用的牌型识别能力，
 * 不包含任何“优先级/策略”逻辑。具体的拆牌策略类（如 PlaneBombFirstSplitAlgorithm、StraightFirstSplitAlgorithm 等），
 * 由 {@link DefaultSplitterFactory} 根据手牌分布选择使用。
 * 只需继承本类并按照自己的优先级组合这些基础方法即可。
 */
public abstract class AbstractHandSplitter implements IHandSplitter {

    private static final List<Rank> STRAIGHT_RANKS = Rank.STRAIGHT_RANKS;
    private static final Rank[] ALL_RANKS = Rank.values();

    /** 复用 findLongestConsecutiveSegment 的返回结果，避免每次 new int[2] */
    private final int[] segmentResult = new int[2];

    /** 顺子最小长度 */
    private static final int MIN_STRAIGHT_LEN = 5;
    /** 连对最小长度 */
    private static final int MIN_CONSECUTIVE_PAIRS_LEN = 3;
    /** 飞机最小长度 */
    private static final int MIN_PLANE_LEN = 2;

    /**
     * 根据手牌构建点数计数数组，委托给 {@link HandCardUtils#buildRankCounts(List)}。
     */
    protected int[] buildRankCounts(List<Card> handCards) {
        return HandCardUtils.buildRankCounts(handCards);
    }

    public List<Combo> extractAllCombos(List<Card> handCards) {
        return extractAllCombos(handCards, buildRankCounts(handCards));
    }

    /**
     * 使用预计算的 count 进行拆分，子类实现具体逻辑。
     */
    public abstract List<Combo> extractAllCombos(List<Card> handCards, int[] rankCounts);

    /**
     * 在顺子区上扫描最长连续区间，其中每个点数至少有 minCardsPerRank 张。
     * 结果写入实例字段 segmentResult 并返回其引用，避免每次分配；未找到时返回 null。
     *
     * @param count           点数计数数组
     * @param minCardsPerRank 每个点数至少需要的张数（顺子=1，连对=2，飞机=3）
     * @param minLen          最小有效长度
     * @return segmentResult（[startIdx, maxLen]），或 null 若不存在满足条件的区间
     */
    protected int[] findLongestConsecutiveSegment(int[] count, int minCardsPerRank, int minLen) {
        int maxLen = 0;
        int startIdx = -1;
        for (int i = 0; i < STRAIGHT_RANKS.size(); i++) {
            int len = 0;
            while (i + len < STRAIGHT_RANKS.size()) {
                Rank r = STRAIGHT_RANKS.get(i + len);
                if (count[r.ordinal()] >= minCardsPerRank) {
                    len++;
                } else {
                    break;
                }
            }
            if (len >= minLen && len > maxLen) {
                maxLen = len;
                startIdx = i;
            }
        }
        if (maxLen < minLen) {
            return null;
        }
        segmentResult[0] = startIdx;
        segmentResult[1] = maxLen;
        return segmentResult;
    }

    /**
     * 从当前计数中提取一条“最长顺子”（长度 ≥5），并写入 combos。
     */
    protected boolean extractStraights(int[] count, List<Combo> combos) {
        int[] seg = findLongestConsecutiveSegment(count, 1, MIN_STRAIGHT_LEN);
        if (seg == null) {
            return false;
        }
        int startIdx = seg[0];
        int maxLen = seg[1];
        List<Rank> ranks = new ArrayList<>();
        for (int j = 0; j < maxLen; j++) {
            Rank r = STRAIGHT_RANKS.get(startIdx + j);
            ranks.add(r);
            count[r.ordinal()]--;
        }
        combos.add(Combo.straight(ranks));
        return true;
    }

    /**
     * 从当前计数中提取一条“最长连对”（长度 ≥3 对），并写入 combos。
     */
    protected boolean extractConsecutivePairs(int[] count, List<Combo> combos) {
        int[] seg = findLongestConsecutiveSegment(count, 2, MIN_CONSECUTIVE_PAIRS_LEN);
        if (seg == null) {
            return false;
        }
        int startIdx = seg[0];
        int maxLen = seg[1];
        List<Rank> ranks = new ArrayList<>();
        for (int j = 0; j < maxLen; j++) {
            Rank r = STRAIGHT_RANKS.get(startIdx + j);
            ranks.add(r);
            count[r.ordinal()] -= 2;
        }
        combos.add(Combo.consecutivePairs(ranks));
        return true;
    }

    /**
     * 若当前计数中有王炸，则提取一个王炸并写入 combos，返回 true；否则不修改并返回 false。
     */
    protected boolean tryExtractRocket(int[] count, List<Combo> combos) {
        int bigIdx = Rank.BIG_JOKER.ordinal();
        int smallIdx = Rank.SMALL_JOKER.ordinal();
        if (count[bigIdx] >= 1 && count[smallIdx] >= 1) {
            combos.add(Combo.rocket());
            count[bigIdx]--;
            count[smallIdx]--;
            return true;
        }
        return false;
    }

    /**
     * 从当前计数中提取所有炸弹（四张同点），写入 combos。不含王炸。
     */
    protected void extractAllBombs(int[] count, List<Combo> combos) {
        for (Rank r : ALL_RANKS) {
            if (r == Rank.SMALL_JOKER || r == Rank.BIG_JOKER) continue;
            int idx = r.ordinal();
            while (count[idx] >= 4) {
                combos.add(Combo.bomb(r));
                count[idx] -= 4;
            }
        }
    }

    /**
     * 从当前计数中提取一条“最长飞机块”（连续三张，长度 ≥2），带牌策略为小牌优先；若无可提取则返回 false。
     */
    protected boolean extractPlanes(int[] count, List<Combo> combos) {
        int[] seg = findLongestConsecutiveSegment(count, 3, MIN_PLANE_LEN);
        if (seg == null) {
            return false;
        }
        int startIdx = seg[0];
        int maxLen = seg[1];
        List<Rank> triRanks = new ArrayList<>();
        for (int j = 0; j < maxLen; j++) {
            Rank r = STRAIGHT_RANKS.get(startIdx + j);
            triRanks.add(r);
            count[r.ordinal()] -= 3;
        }

        List<Rank> singles = collectSingles(count);
        List<Rank> pairs = collectPairs(count);

        if (maxLen <= pairs.size()) {
            List<Rank> wingPairs = new ArrayList<>(pairs.subList(0, maxLen));
            for (Rank r : wingPairs) {
                count[r.ordinal()] -= 2;
            }
            combos.add(Combo.planeWithPairs(triRanks, wingPairs));
        } else if (maxLen <= singles.size()) {
            List<Rank> wingSingles = new ArrayList<>(singles.subList(0, maxLen));
            for (Rank r : wingSingles) {
                count[r.ordinal()] -= 1;
            }
            combos.add(Combo.planeWithSingles(triRanks, wingSingles));
        } else {
            combos.add(Combo.plane(triRanks));
        }
        return true;
    }

    /**
     * 从当前计数中提取一个四带二（优先四带二对，否则四带二单）；若无可提取则返回 false。
     */
    protected boolean extractQuadsWithWings(int[] count, List<Combo> combos) {
        for (Rank r : ALL_RANKS) {
            if (r == Rank.SMALL_JOKER || r == Rank.BIG_JOKER) continue;
            int idx = r.ordinal();
            if (count[idx] < 4) continue;

            List<Rank> pairs = collectPairs(count);
            List<Rank> singles = collectSingles(count);

            if (pairs.size() >= 2) {
                Rank p1 = pairs.get(0);
                Rank p2 = pairs.get(1);
                combos.add(Combo.quadWithTwoPairs(r, p1, p2));
                count[idx] -= 4;
                count[p1.ordinal()] -= 2;
                count[p2.ordinal()] -= 2;
                return true;
            }
            if (singles.size() >= 2) {
                Rank s1 = singles.get(0);
                Rank s2 = singles.get(1);
                combos.add(Combo.quadWithTwoSingles(r, s1, s2));
                count[idx] -= 4;
                count[s1.ordinal()] -= 1;
                count[s2.ordinal()] -= 1;
                return true;
            }
        }
        return false;
    }

    /**
     * 从当前计数中提取所有三张/三带一/三带二，带牌策略为小牌优先。
     */
    protected void extractTriples(int[] count, List<Combo> combos) {
        for (Rank r : ALL_RANKS) {
            int idx = r.ordinal();
            if (count[idx] < 3) continue;

            List<Rank> pairs = collectPairs(count);
            List<Rank> singles = collectSingles(count);

            if (pairs.size() >= 1) {
                Rank p = pairs.get(0);
                combos.add(Combo.tripleWithPair(r, p));
                count[idx] -= 3;
                count[p.ordinal()] -= 2;
            } else if (singles.size() >= 1) {
                Rank s = singles.get(0);
                combos.add(Combo.tripleWithSingle(r, s));
                count[idx] -= 3;
                count[s.ordinal()] -= 1;
            } else {
                combos.add(Combo.triple(r));
                count[idx] -= 3;
            }
        }
    }

    /**
     * 将当前计数中所有 ≥3 张的点数以裸三张形式提取，不带牌。用于顺子优先策略的收尾。
     */
    protected void extractAllBareTriples(int[] count, List<Combo> combos) {
        for (Rank r : ALL_RANKS) {
            int idx = r.ordinal();
            while (count[idx] >= 3) {
                combos.add(Combo.triple(r));
                count[idx] -= 3;
            }
        }
    }

    /**
     * 将当前计数中所有剩余牌按对子形式提取到 combos。
     */
    protected void extractAllPairs(int[] count, List<Combo> combos) {
        for (Rank r : ALL_RANKS) {
            int idx = r.ordinal();
            while (count[idx] >= 2) {
                combos.add(Combo.pair(r));
                count[idx] -= 2;
            }
        }
    }

    /**
     * 将当前计数中所有剩余牌按单牌形式提取到 combos。
     */
    protected void extractAllSingles(int[] count, List<Combo> combos) {
        for (Rank r : ALL_RANKS) {
            int idx = r.ordinal();
            while (count[idx] >= 1) {
                combos.add(Combo.single(r));
                count[idx] -= 1;
            }
        }
    }

    /**
     * 从当前计数中收集所有“可作为单牌带出”的点数列表，按 单牌牌力值 从小到大排序。
     */
    protected List<Rank> collectSingles(int[] count) {
        List<Rank> list = new ArrayList<>();
        for (Rank r : ALL_RANKS) {
            int c = count[r.ordinal()];
            for (int i = 0; i < c; i++) {
                list.add(r);
            }
        }
        list.sort((a, b) -> Integer.compare(a.getBaseValue(), b.getBaseValue()));
        return list;
    }

    /**
     * 从当前计数中收集所有“至少有 2 张、可作为对子带出”的点数列表，按 单牌牌力值 从小到大排序。
     */
    protected List<Rank> collectPairs(int[] count) {
        List<Rank> list = new ArrayList<>();
        for (Rank r : ALL_RANKS) {
            if (count[r.ordinal()] >= 2) {
                list.add(r);
            }
        }
        list.sort((a, b) -> Integer.compare(a.getBaseValue(), b.getBaseValue()));
        return list;
    }
}
