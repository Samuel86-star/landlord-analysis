package com.mamba.landlord.controller;

import com.mamba.landlord.algorithm.shuffle.IShuffleDealStrategy;
import com.mamba.landlord.core.model.EvaluatedDealData;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.springframework.web.bind.annotation.GetMapping;
import org.springframework.web.bind.annotation.RequestMapping;
import org.springframework.web.bind.annotation.RestController;

import java.util.HashMap;
import java.util.Map;

/**
 * 洗牌、发牌以及牌力相关的 HTTP 接口。
 * 使用的策略由 {@link com.mamba.landlord.core.AlgorithmConfig} 根据配置注入，
 * 当 {@code landlord.shuffle-strategy.enabled=true} 时使用带重洗过滤的策略。
 */
@RestController
@RequestMapping("/api")
public class ShuffleAndDealController {

    private static final Logger log = LoggerFactory.getLogger(ShuffleAndDealController.class);

    private final IShuffleDealStrategy shuffleDealStrategy;

    public ShuffleAndDealController(IShuffleDealStrategy shuffleDealStrategy) {
        this.shuffleDealStrategy = shuffleDealStrategy;
    }

    /**
     * 单次洗牌并发牌（带过滤）：GET /api/shuffle
     *
     * @return 三家手牌、底牌及牌力与过滤相关的元数据
     */
    @GetMapping("/shuffle")
    public Map<String, Object> shuffleAndDeal() {
        EvaluatedDealData evaluatedDealData = shuffleDealStrategy.shuffleAndDeal();
        // 将本次发牌的牌力与 EHS 指标打入日志，便于后续离线分析与调参
        log.info("shuffle result: strengths=[{},{},{}], ehs=[{},{},{}], reshuffled={}, reshuffleCnt={}",
            evaluatedDealData.getHandStrengthPlayer0(),
            evaluatedDealData.getHandStrengthPlayer1(),
            evaluatedDealData.getHandStrengthPlayer2(),
            evaluatedDealData.getEhsPlayer0(),
            evaluatedDealData.getEhsPlayer1(),
            evaluatedDealData.getEhsPlayer2(),
            evaluatedDealData.isReshuffled(),
            evaluatedDealData.getReshuffleCnt()
        );
        Map<String, Object> result = new HashMap<>();
        result.put("handDataPlayer0", evaluatedDealData.getHandCards(0).stream().map(c -> c.rank().name()).toList());
        result.put("handDataPlayer1", evaluatedDealData.getHandCards(1).stream().map(c -> c.rank().name()).toList());
        result.put("handDataPlayer2", evaluatedDealData.getHandCards(2).stream().map(c -> c.rank().name()).toList());
        result.put("bottomCards", evaluatedDealData.getBottomCards().stream().map(c -> c.rank().name()).toList());
        result.put("handStrengthPlayer0", evaluatedDealData.getHandStrengthPlayer0());
        result.put("handStrengthPlayer1", evaluatedDealData.getHandStrengthPlayer1());
        result.put("handStrengthPlayer2", evaluatedDealData.getHandStrengthPlayer2());
        result.put("ehsPlayer0", evaluatedDealData.getEhsPlayer0());
        result.put("ehsPlayer1", evaluatedDealData.getEhsPlayer1());
        result.put("ehsPlayer2", evaluatedDealData.getEhsPlayer2());
        result.put("reshuffled", evaluatedDealData.isReshuffled());
        result.put("reshuffleCnt", evaluatedDealData.getReshuffleCnt());
        return result;
    }
}

