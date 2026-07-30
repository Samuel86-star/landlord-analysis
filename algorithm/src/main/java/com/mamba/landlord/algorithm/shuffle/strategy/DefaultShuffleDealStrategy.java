package com.mamba.landlord.algorithm.shuffle.strategy;

import com.mamba.landlord.algorithm.scoring.IHandCardsScoringStrategy;
import com.mamba.landlord.algorithm.shuffle.AbstractShuffleDealStrategy;
import com.mamba.landlord.algorithm.shuffle.IDealStrategy;
import com.mamba.landlord.algorithm.shuffle.IShuffleDealStrategy;
import com.mamba.landlord.algorithm.shuffle.IShuffleStrategy;
import com.mamba.landlord.algorithm.splitter.IComboExtractor;
import com.mamba.landlord.core.model.*;

import java.util.Collections;
import java.util.List;
import java.util.random.RandomGenerator;

/**
 * 默认洗牌+发牌算法。
 * <p>
 * 从标准 54 张牌堆中洗牌，并按经典三人斗地主规则发牌：
 * 每人 17 张，底牌 3 张，返回顺序为 [hand0, hand1, hand2, bottomCards]。
 * </p>
 */
public class DefaultShuffleDealStrategy extends AbstractShuffleDealStrategy implements IShuffleDealStrategy, IShuffleStrategy, IDealStrategy {

    public DefaultShuffleDealStrategy(IHandCardsScoringStrategy handCardsScoringStrategy,
                                     IComboExtractor comboExtractor) {
        super(handCardsScoringStrategy, comboExtractor);
    }

    public DefaultShuffleDealStrategy() {
        super();
    }
    /**
     * 洗牌并发牌，计算三家牌力后直接返回，不做任何均衡性过滤或重洗。
     * <p>
     * 流程：洗牌 → 发牌 → 计算三家牌力 → 封装结果。
     * 需要过滤极端局的场景请使用 {@link DefaultReshuffleDealStrategy}。
     * </p>
     *
     * @return 含三家手牌、底牌、牌力值的发牌结果（reshuffled=false，reshuffleCnt=0）
     */
    public EvaluatedDealData shuffleAndDeal() {
        // 随机洗牌，打乱 54 张牌的顺序
        List<Card> shuffledHandCards = shuffle();
        // 按座位切牌：[0,17) 玩家0，[17,34) 玩家1，[34,51) 玩家2，[51,54) 底牌
        DealData dealData = dealCards(shuffledHandCards);
        // 对三家手牌分别拆牌并评分，得到 V_total
        double[] allSeatHandStrength = calcAllSeatHandStrength(dealData);
        // 封装为带牌力值的完整结果（本策略不重洗）
        return wrapEvaluatedDealData(dealData, allSeatHandStrength[0],
            allSeatHandStrength[1], allSeatHandStrength[2], false, 0
        );
    }

    /**
     * 从标准 54 张牌堆中取一副牌并随机洗牌。
     *
     * @return 洗牌后的 54 张牌列表（顺序随机）
     */
    public List<Card> shuffle() {
        // 获取标准 54 张牌的可变副本（Deck 内部缓存不可变列表，此处复制以便原地打乱）
        List<Card> deckCards = Deck.copyFullDeckCards();
        // 使用 JDK 默认随机数生成器就地打乱
        Collections.shuffle(deckCards, RandomGenerator.getDefault());
        return deckCards;
    }
}

