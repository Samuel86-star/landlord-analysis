package com.mamba.landlord.algorithm.shuffle.strategy;

import com.mamba.landlord.algorithm.scoring.IHandCardsScoringStrategy;
import com.mamba.landlord.algorithm.shuffle.IDealStrategy;
import com.mamba.landlord.algorithm.shuffle.IShuffleDealStrategy;
import com.mamba.landlord.algorithm.shuffle.IShuffleStrategy;
import com.mamba.landlord.algorithm.splitter.IComboExtractor;
import com.mamba.landlord.core.holder.ShuffleStrategyDecisionHolder;
import com.mamba.landlord.core.model.Card;
import com.mamba.landlord.core.model.DealData;
import com.mamba.landlord.core.model.EvaluatedDealData;
import com.mamba.landlord.core.properties.ShuffleStrategyDecisionProperties;

import java.util.List;

/**
 * 带重洗牌逻辑的洗牌+发牌策略。
 * <p>
 * 在 {@link DefaultShuffleDealStrategy} 基础上，增加以下过滤能力：
 * <ol>
 *   <li><b>上下阈值过滤</b>：任一家手牌牌力超出 [lowerThreshold, upperThreshold] 时触发重洗。</li>
 *   <li><b>极差过滤</b>：三家牌力极差（max - min）超过 maxSpread 时触发重洗。</li>
 *   <li><b>底牌协同增益过滤</b>：枚举三家分别叫地主后的潜在牌力，超过 maxPotentialLandlordScore 时触发重洗。
 *       相较于旧方案的「max + bottomBonus * weight」，能识别底牌与特定玩家手牌的协同增益
 *       （如补全炸弹、顺子等）。</li>
 *   <li><b>地主优势过滤</b>：枚举三种地主分配，若「潜在地主牌力 - 农民手牌均值」超过 maxLandlordAdvantage
 *       时触发重洗，从对局均衡性角度防止某家一旦叫地主就形成压倒性优势。</li>
 *   <li><b>手牌结构过滤</b>：检查拆牌后每家的单牌数量和炸弹数量是否超出阈值，
 *       补充 V_total 无法识别的「大单牌堆」「炸弹泛滥」等极端结构。</li>
 * </ol>
 * 每次重洗后通过 {@link #relaxThreshold} 对阈值做渐进式放宽，防止因阈值过严导致死循环。
 * 快速失败优化：若上轮有明确的问题座位，下轮重洗后优先只校验该座位，确认仍超范围则直接进入
 * 下一轮，跳过其他两家及复杂指标的计算。
 * </p>
 * <p>
 * 所有阈值均从 {@link ShuffleStrategyDecisionProperties} 读取，建议通过
 * {@code DealDistributionSampler} 采样基线后取分位点填入，而非直接使用经验值。
 * </p>
 */
public class DefaultReshuffleDealStrategy extends DefaultShuffleDealStrategy
        implements IShuffleDealStrategy, IShuffleStrategy, IDealStrategy {

    public DefaultReshuffleDealStrategy(IHandCardsScoringStrategy handCardsScoringStrategy,
                                        IComboExtractor comboExtractor) {
        super(handCardsScoringStrategy, comboExtractor);
    }

    public DefaultReshuffleDealStrategy() {
        super();
    }

    /**
     * 洗牌+发牌，附带多维均衡性过滤与自动重洗逻辑。
     * <p>
     * 主流程：
     * <ol>
     *   <li>洗牌并发牌。</li>
     *   <li>计算三家手牌牌力、潜在地主牌力、地主优势、手牌结构特征。</li>
     *   <li>调用 {@link #isExtreme} 判断是否需要重洗；若需要则记录问题座位，触发重洗。</li>
     *   <li>重洗时使用「快速失败」：若上轮有明确问题座位，先只算该家；确认仍超范围则跳过全量计算。</li>
     *   <li>每轮重洗后通过 {@link #relaxThreshold} 放宽阈值，最多重洗 maxReshuffleTimes 次。</li>
     * </ol>
     * </p>
     *
     * @return 含三家手牌、底牌、牌力值及重洗元数据的发牌结果
     */
    public EvaluatedDealData shuffleAndDeal() {
        ShuffleStrategyDecisionProperties config = ShuffleStrategyDecisionHolder.get();
        if (!config.isEnabled()) {
            // 未开启过滤时，直接走无重洗的默认策略
            return super.shuffleAndDeal();
        }

        // 从配置中提取各阈值，避免循环内重复调用 getter
        int maxReshuffleTimes = config.getMaxReshuffleTimes();
        double lowerThreshold = config.getLowerThreshold();
        double upperThreshold = config.getUpperThreshold();
        double maxSpread = config.getMaxSpread();
        double maxPotentialLandlordScore = config.getMaxPotentialLandlordScore();
        double maxLandlordAdvantage = config.getMaxLandlordAdvantage();
        int maxSingles = config.getMaxSinglesPerHand();
        int maxBombs = config.getMaxBombsPerHand();
        double relaxStep = config.getThresholdRelaxStep();

        // 第一次洗牌发牌
        List<Card> shuffledCards = shuffle();
        DealData dealData = dealCards(shuffledCards);

        // 计算初始评估数据
        double[] seatScores = calcAllSeatHandStrength(dealData);
        double potentialLandlordScore = calcPotentialLandlordScore(dealData);
        double landlordAdvantage = calcMaxLandlordAdvantage(dealData, seatScores);
        int[][] structureFeatures = calcAllStructureFeatures(dealData);

        // 记录上轮触发重洗的问题座位（用于快速失败优化），-1 表示无法定位到单一座位
        int problematicSeat = -1;
        int reshuffleCnt = 0;

        while (reshuffleCnt < maxReshuffleTimes
                && isExtreme(seatScores, potentialLandlordScore, landlordAdvantage,
                             structureFeatures,
                             relaxThreshold(lowerThreshold, reshuffleCnt, relaxStep),
                             relaxThreshold(upperThreshold, reshuffleCnt, relaxStep),
                             relaxThreshold(maxSpread, reshuffleCnt, relaxStep),
                             maxPotentialLandlordScore,
                             maxLandlordAdvantage,
                             maxSingles, maxBombs)) {

            // 记录本轮问题座位，供下轮快速失败使用
            double currentLower = relaxThreshold(lowerThreshold, reshuffleCnt, relaxStep);
            double currentUpper = relaxThreshold(upperThreshold, reshuffleCnt, relaxStep);
            problematicSeat = findProblematicSeat(seatScores, currentLower, currentUpper);

            reshuffleCnt++;
            shuffledCards = shuffle();
            dealData = dealCards(shuffledCards);

            // 快速失败：若上轮有明确问题座位，先只校验该座位，避免无效全量计算
            if (problematicSeat >= 0) {
                double quickScore = calcHandStrengthScores(dealData.getHandCards(problematicSeat));
                double nextLower = relaxThreshold(lowerThreshold, reshuffleCnt, relaxStep);
                double nextUpper = relaxThreshold(upperThreshold, reshuffleCnt, relaxStep);
                if (quickScore < nextLower || quickScore > nextUpper) {
                    // 该座位仍然超出范围，直接进入下一轮，保留旧的全量数据使循环条件继续为 true
                    continue;
                }
            }

            // 全量重新计算三家牌力与所有均衡性指标
            seatScores = calcAllSeatHandStrength(dealData);
            potentialLandlordScore = calcPotentialLandlordScore(dealData);
            landlordAdvantage = calcMaxLandlordAdvantage(dealData, seatScores);
            structureFeatures = calcAllStructureFeatures(dealData);
        }

        return wrapEvaluatedDealData(dealData, seatScores[0], seatScores[1], seatScores[2],
            reshuffleCnt > 0, reshuffleCnt);
    }

    /**
     * 渐进式阈值放宽：第 i 次重洗时，阈值绝对值按 {@code (1 + i * step)} 比例放大。
     * <p>
     * lower（负数）乘以放大因子后绝对值更大（更负），upper/maxSpread（正数）乘以后更大，
     * 两端同时外扩，使过滤条件逐步放松，避免重洗死循环。
     * </p>
     *
     * @param baseValue    原始阈值（负数表示下限，正数表示上限/极差上限）
     * @param reshuffleCnt 当前重洗次数（第 0 次不放宽）
     * @param relaxStep    每次放宽的步长比例（0 = 不放宽，0.15 = 每次放宽 15%）
     * @return 放宽后的阈值
     */
    private static double relaxThreshold(double baseValue, int reshuffleCnt, double relaxStep) {
        if (reshuffleCnt <= 0 || relaxStep <= 0) {
            return baseValue;
        }
        return baseValue * (1.0 + reshuffleCnt * relaxStep);
    }

    /**
     * 综合判断当前局是否存在极端情况，需要重洗。
     * <p>
     * 按以下顺序依次检查：
     * <ol>
     *   <li>任一家手牌牌力低于 lower 或高于 upper → 极端差牌/好牌。</li>
     *   <li>三家手牌牌力极差超过 maxSpread → 牌力分布不均。</li>
     *   <li>潜在地主牌力超过 maxPotentialLandlord → 某家叫地主后优势过大。</li>
     *   <li>最坏地主优势超过 maxLandlordAdv → 对局均衡性差。</li>
     *   <li>任一家单牌数超过 maxSingles（且配置 > 0）→ 手牌可打出性极差。</li>
     *   <li>任一家炸弹数超过 maxBombs（且配置 > 0）→ 手牌结构过于强势。</li>
     * </ol>
     * </p>
     *
     * @param scores                  三家手牌牌力值
     * @param potentialLandlordScore  潜在地主牌力（三家拿底牌后的最大值）
     * @param landlordAdvantage       最坏地主优势（潜在地主牌力 - 农民均值的最大值）
     * @param structureFeatures       三家手牌结构特征，int[3][2]：[seat][0]=单牌数，[seat][1]=炸弹数
     * @param lower                   当前生效的下阈值（已含 relax）
     * @param upper                   当前生效的上阈值（已含 relax）
     * @param maxSpread               当前生效的极差上限（已含 relax）
     * @param maxPotentialLandlord    潜在地主牌力上限（不随 relax 变化）
     * @param maxLandlordAdv          地主优势上限（不随 relax 变化）
     * @param maxSingles              单牌数上限（0 表示不检测）
     * @param maxBombs                炸弹数上限（0 表示不检测）
     * @return true 表示需要重洗，false 表示可以接受本局
     */
    private static boolean isExtreme(double[] scores,
                                     double potentialLandlordScore,
                                     double landlordAdvantage,
                                     int[][] structureFeatures,
                                     double lower, double upper, double maxSpread,
                                     double maxPotentialLandlord,
                                     double maxLandlordAdv,
                                     int maxSingles, int maxBombs) {
        double min = Math.min(scores[0], Math.min(scores[1], scores[2]));
        double max = Math.max(scores[0], Math.max(scores[1], scores[2]));

        // 检查手牌牌力上下阈值和极差
        if (min < lower || max > upper || (max - min) > maxSpread) {
            return true;
        }

        // 检查潜在地主牌力（底牌协同增益）
        if (potentialLandlordScore > maxPotentialLandlord) {
            return true;
        }

        // 检查最坏地主优势（对局均衡性）
        if (landlordAdvantage > maxLandlordAdv) {
            return true;
        }

        // 检查手牌结构特征（单牌数和炸弹数，配置为 0 时跳过）
        if (maxSingles > 0 || maxBombs > 0) {
            for (int[] feature : structureFeatures) {
                if (maxSingles > 0 && feature[0] > maxSingles) return true;
                if (maxBombs > 0 && feature[1] > maxBombs) return true;
            }
        }

        return false;
    }

    /**
     * 找出本次触发上下阈值极端判断的问题座位。
     * <p>
     * 仅对「任一家手牌牌力超出阈值」这种情况能定位到单一座位；
     * 极差超限、结构特征超限等情况无法定位，返回 -1。
     * 返回值供「快速失败」优化使用：下轮重洗后优先只校验该座位。
     * </p>
     *
     * @param scores 三家手牌牌力值
     * @param lower  当前下阈值（已含 relax）
     * @param upper  当前上阈值（已含 relax）
     * @return 问题座位号（0/1/2），无法定位时返回 -1
     */
    private static int findProblematicSeat(double[] scores, double lower, double upper) {
        for (int i = 0; i < scores.length; i++) {
            if (scores[i] < lower || scores[i] > upper) {
                return i;
            }
        }
        return -1;
    }
}
