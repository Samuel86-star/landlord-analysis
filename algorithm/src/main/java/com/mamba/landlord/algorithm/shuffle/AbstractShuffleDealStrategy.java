package com.mamba.landlord.algorithm.shuffle;

import com.mamba.landlord.algorithm.scoring.IHandCardsScoringStrategy;
import com.mamba.landlord.algorithm.scoring.strategy.DefaultHandCardsScoringStrategy;
import com.mamba.landlord.algorithm.splitter.IComboExtractor;
import com.mamba.landlord.algorithm.splitter.DefaultComboExtractor;
import com.mamba.landlord.core.holder.ScoringStrategyHolder;
import com.mamba.landlord.core.model.*;

import java.util.ArrayList;
import java.util.List;

/**
 * 洗牌+发牌策略抽象基类。
 * <p>
 * 职责：封装所有子类共用的牌力评估与发牌辅助方法，包括：
 * <ul>
 *   <li>按座位切牌（{@link #dealCards}）</li>
 *   <li>三家手牌牌力计算（{@link #calcAllSeatHandStrength}）</li>
 *   <li>底牌协同增益建模（{@link #calcPotentialLandlordScore}）</li>
 *   <li>地主优势估算（{@link #calcMaxLandlordAdvantage}）</li>
 *   <li>手牌结构特征统计（{@link #calcAllStructureFeatures}）</li>
 *   <li>结果数据封装（{@link #wrapEvaluatedDealData}）</li>
 * </ul>
 * 支持通过构造函数注入牌力评分策略与牌型提取器，便于单测与扩展。
 * </p>
 */
public abstract class AbstractShuffleDealStrategy implements IShuffleStrategy, IDealStrategy {

    protected final IHandCardsScoringStrategy handCardsScoringStrategy;
    protected final IComboExtractor comboExtractor;

    protected AbstractShuffleDealStrategy(IHandCardsScoringStrategy handCardsScoringStrategy,
                                         IComboExtractor comboExtractor) {
        if (handCardsScoringStrategy == null || comboExtractor == null) {
            throw new IllegalArgumentException("handCardsScoringStrategy and comboExtractor must not be null");
        }
        this.handCardsScoringStrategy = handCardsScoringStrategy;
        this.comboExtractor = comboExtractor;
    }

    /** 使用默认实现的便捷构造（{@link DefaultHandCardsScoringStrategy} + {@link DefaultComboExtractor}）。 */
    protected AbstractShuffleDealStrategy() {
        this(new DefaultHandCardsScoringStrategy(), new DefaultComboExtractor(new DefaultHandCardsScoringStrategy()));
    }

    /**
     * 洗牌并完成发牌，返回含牌力评估的完整结果。子类实现具体的洗牌和过滤逻辑。
     *
     * @return 含三家手牌、底牌、牌力值及重洗元数据的发牌结果
     */
    protected abstract EvaluatedDealData shuffleAndDeal();

    /**
     * 按斗地主规则将已洗好的 54 张牌切分给三家玩家和底牌区。
     * <p>
     * 切分规则：[0, 17) → 玩家0，[17, 34) → 玩家1，[34, 51) → 玩家2，[51, 54) → 底牌。
     * </p>
     *
     * @param shuffledDeckCards 已洗牌的 54 张牌（顺序随机）
     * @return 三家手牌与底牌封装的 {@link DealData}
     * @throws IllegalArgumentException 若传入牌数不等于 {@link Deck#DECK_SIZE}
     */
    public DealData dealCards(List<Card> shuffledDeckCards) {
        if (shuffledDeckCards == null || shuffledDeckCards.size() != Deck.DECK_SIZE) {
            throw new IllegalArgumentException("Deck must have " + Deck.DECK_SIZE + " cards");
        }
        return new DealData(
            new ArrayList<>(shuffledDeckCards.subList(0, Deck.HAND_SIZE)),
            new ArrayList<>(shuffledDeckCards.subList(Deck.HAND_SIZE, Deck.HAND_SIZE * 2)),
            new ArrayList<>(shuffledDeckCards.subList(Deck.HAND_SIZE * 2, Deck.HAND_SIZE * 3)),
            new ArrayList<>(shuffledDeckCards.subList(Deck.HAND_SIZE * 3, Deck.DECK_SIZE))
        );
    }

    /**
     * 将发牌数据与三家牌力值封装为最终返回的 {@link EvaluatedDealData}。
     *
     * @param dealData             发牌数据（三家手牌 + 底牌）
     * @param handStrengthPlayer0  玩家 0 手牌牌力值
     * @param handStrengthPlayer1  玩家 1 手牌牌力值
     * @param handStrengthPlayer2  玩家 2 手牌牌力值
     * @param reshuffle            本次是否发生过重洗
     * @param reshuffleCnt         实际重洗次数
     * @return 含所有信息的 {@link EvaluatedDealData}
     */
    protected EvaluatedDealData wrapEvaluatedDealData(
        DealData dealData, double handStrengthPlayer0, double handStrengthPlayer1,
        double handStrengthPlayer2, boolean reshuffle, int reshuffleCnt
    ) {
        if (dealData == null) {
            throw new IllegalArgumentException("DealData must not be null");
        }
        double ehs0 = normalizeHandStrength(handStrengthPlayer0);
        double ehs1 = normalizeHandStrength(handStrengthPlayer1);
        double ehs2 = normalizeHandStrength(handStrengthPlayer2);
        return EvaluatedDealData.builder()
            .handCardsPlayer0(dealData.getHandCards(0))
            .handCardsPlayer1(dealData.getHandCards(1))
            .handCardsPlayer2(dealData.getHandCards(2))
            .bottomCards(dealData.getBottomCards())
            .handStrengthPlayer0(handStrengthPlayer0)
            .handStrengthPlayer1(handStrengthPlayer1)
            .handStrengthPlayer2(handStrengthPlayer2)
            .ehsPlayer0(ehs0)
            .ehsPlayer1(ehs1)
            .ehsPlayer2(ehs2)
            .reshuffled(reshuffle)
            .reshuffleCnt(reshuffleCnt)
            .build();
    }

    /**
     * 将 V_total 压缩到 [0,1] 区间的近似 EHS 指标。
     * <p>
     * 当前实现采用简单的逻辑函数：{@code 1 / (1 + exp(-score / scale))}，
     * 其中 scale 控制曲线斜率。该值仅用于日志与分布分析，不参与过滤逻辑。
     * </p>
     *
     * @param score 原始整手牌牌力值 V_total
     * @return 归一化后的近似强度（EHS），范围 [0,1]
     */
    protected static double normalizeHandStrength(double score) {
        double scale = ScoringStrategyHolder.get().getEhsScale();
        if (scale <= 0 || !Double.isFinite(scale)) {
            scale = 40.0;
        }
        double x = score / scale;
        double e = Math.exp(-x);
        return 1.0 / (1.0 + e);
    }

    /**
     * 计算三家玩家的手牌牌力值（不含底牌）。
     * <p>
     * 对每家手牌依次调用 {@link #calcHandStrengthScores} 完成拆牌与评分。
     * </p>
     *
     * @param dealData 发牌数据
     * @return 长度为 3 的数组，下标对应座位号（0/1/2），值为对应的 V_total 分
     */
    protected double[] calcAllSeatHandStrength(DealData dealData) {
        double[] result = new double[3];
        if (dealData != null) {
            for (int seat = 0; seat < 3; seat++) {
                List<Card> handCards = dealData.getHandCards(seat);
                if (handCards != null && !handCards.isEmpty()) {
                    // 对每家手牌独立拆牌并打分
                    result[seat] = calcHandStrengthScores(handCards);
                }
            }
        }
        return result;
    }

    /**
     * 计算单家手牌的牌力值 V_total。
     * <p>
     * 流程：先通过 {@link IComboExtractor} 拆牌，再由 {@link IHandCardsScoringStrategy} 打分。
     * </p>
     *
     * @param handCards 手牌列表（17 张或含底牌的 20 张）
     * @return V_total 分值；手牌为空时返回 0.0
     */
    protected double calcHandStrengthScores(List<Card> handCards) {
        if (handCards == null || handCards.isEmpty()) {
            return 0.0;
        }
        // 拆牌：将手牌分解为牌型组合列表（如顺子、三带一、炸弹等）
        List<Combo> combos = comboExtractor.extractAllCombos(handCards);
        if (combos == null || combos.isEmpty()) {
            return 0.0;
        }
        // 评分：V_total = ΣV_combo - (N-1)*8 + Control_Bonus
        return handCardsScoringStrategy.calcTotalHandScore(handCards, combos);
    }

    /**
     * 枚举三家分别叫地主（手牌 + 底牌）后的牌力，返回其中的最大值。
     * <p>
     * 用于替代原有「max(scores) + bottomBonus * weight」的粗估方式，
     * 能识别底牌与某家手牌协同组成炸弹、补全顺子等情况。
     * 内部对每家调用一次 {@link #calcHandStrengthScores}（20 张牌）。
     * </p>
     *
     * @param dealData 发牌数据（包含三家手牌和底牌）
     * @return 三种地主分配方案中的最高潜在地主牌力
     */
    protected double calcPotentialLandlordScore(DealData dealData) {
        List<Card> bottomCards = dealData.getBottomCards();
        double maxScore = Double.NEGATIVE_INFINITY;
        for (int seat = 0; seat < 3; seat++) {
            // 将该座位的手牌与底牌合并，模拟叫地主后的完整手牌（20 张）
            List<Card> combined = new ArrayList<>(dealData.getHandCards(seat));
            combined.addAll(bottomCards);
            double score = calcHandStrengthScores(combined);
            if (score > maxScore) {
                maxScore = score;
            }
        }
        return maxScore;
    }

    /**
     * 枚举三种地主分配方案，计算每种方案中「潜在地主牌力 - 两个农民手牌均值」的优势分，
     * 返回三种方案中地主优势最大的值，代表本局「最坏均衡性」。
     * <p>
     * 发牌阶段不知道谁会叫地主，通过枚举三种可能来预防最坏情况：
     * 若某玩家一旦叫地主便会形成压倒性优势，则应触发重洗。
     * </p>
     *
     * @param dealData   发牌数据
     * @param seatScores 三家手牌牌力（不含底牌），长度为 3，由 {@link #calcAllSeatHandStrength} 产生
     * @return 三种方案中地主优势最大值（landlordScore - avgFarmerScore）
     */
    protected double calcMaxLandlordAdvantage(DealData dealData, double[] seatScores) {
        List<Card> bottomCards = dealData.getBottomCards();
        double maxAdvantage = Double.NEGATIVE_INFINITY;
        for (int landlordSeat = 0; landlordSeat < 3; landlordSeat++) {
            // 该座位叫地主：手牌 + 底牌合并后重新打分
            List<Card> combined = new ArrayList<>(dealData.getHandCards(landlordSeat));
            combined.addAll(bottomCards);
            double landlordScore = calcHandStrengthScores(combined);

            // 另外两家的手牌分均值（代表农民合力）
            double farmerSum = 0;
            for (int seat = 0; seat < 3; seat++) {
                if (seat != landlordSeat) {
                    farmerSum += seatScores[seat];
                }
            }
            double farmerAvg = farmerSum / 2.0;

            double advantage = landlordScore - farmerAvg;
            if (advantage > maxAdvantage) {
                maxAdvantage = advantage;
            }
        }
        return maxAdvantage;
    }

    /**
     * 统计三家手牌的结构特征（单牌数量和炸弹数量），用于多维均衡性检测。
     * <p>
     * 纯粹依靠 V_total 分值无法识别「大单牌堆」等极端结构；
     * 此方法通过拆牌后统计组合类型数量，作为 V_total 之外的补充检测维度。
     * 对每家手牌调用一次 {@link IComboExtractor#extractAllCombos}。
     * </p>
     *
     * @param dealData 发牌数据
     * @return int[3][2]，第一维为座位（0/1/2），第二维 [0]=单牌数，[1]=炸弹数（含王炸）
     */
    protected int[][] calcAllStructureFeatures(DealData dealData) {
        int[][] features = new int[3][2];
        for (int seat = 0; seat < 3; seat++) {
            List<Card> handCards = dealData.getHandCards(seat);
            // 拆牌，统计 SINGLE 和 BOMB/ROCKET 的数量
            List<Combo> combos = comboExtractor.extractAllCombos(handCards);
            int singles = 0, bombs = 0;
            for (Combo c : combos) {
                if (c.type() == ComboType.SINGLE) singles++;
                if (c.type() == ComboType.BOMB || c.type() == ComboType.ROCKET) bombs++;
            }
            features[seat][0] = singles;
            features[seat][1] = bombs;
        }
        return features;
    }

}
