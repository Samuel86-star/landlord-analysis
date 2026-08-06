// verify_split_vs_power.cpp —— 最少手数拆牌 vs 牌力最大拆牌 等价性验证
// 编译：vcvarsall x64 && cl /nologo /utf-8 /EHsc /std:c++14 /O2 /Fesplit_vs_power.exe verify_split_vs_power.cpp
// 运行：须在 algorithm/native/ 目录（相对路径 config/scoring.properties）
#include "../include/landlord.h"
#include "optimal_split.h"
#include "optimal_split_power.h"

#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <vector>
#include <string>
#include <random>
#include <algorithm>

using namespace landlord;

static double sumScore(const std::vector<Combo>& cs) {
    DefaultComboScoringStrategy sc; double t = 0; for (auto& c : cs) t += sc.score(c); return t;
}
static const char* rk(Rank r) {
    switch (r) { case Rank::TWO: return "2"; case Rank::SMALL_JOKER: return "s"; case Rank::BIG_JOKER: return "b";
        default: static char buf[2];
            switch (r) { case Rank::TEN: return "T"; case Rank::JACK: return "J"; case Rank::QUEEN: return "Q";
                case Rank::KING: return "K"; case Rank::ACE: return "A";
                default: buf[0] = '3' + (int)r; buf[1] = 0; return buf; } }
}
static const char* tp(ComboType t) {
    switch (t) { case ComboType::SINGLE: return "S"; case ComboType::PAIR: return "P"; case ComboType::TRIPLE: return "T";
        case ComboType::TRIPLE_WITH_SINGLE: return "T1"; case ComboType::TRIPLE_WITH_PAIR: return "T2";
        case ComboType::STRAIGHT: return "ST"; case ComboType::CONSECUTIVE_PAIRS: return "CP"; case ComboType::PLANE: return "PL";
        case ComboType::PLANE_WITH_SINGLES: return "PL1"; case ComboType::PLANE_WITH_PAIRS: return "PL2";
        case ComboType::QUAD_WITH_TWO_SINGLES: return "Q1"; case ComboType::QUAD_WITH_TWO_PAIRS: return "Q2";
        case ComboType::BOMB: return "B"; case ComboType::ROCKET: return "R"; }
    return "?";
}
static std::string handStr(const std::vector<Card>& h) {
    auto cc = HandCardUtils::buildRankCounts(h);
    std::string s; for (int i = 14; i >= 0; --i) for (int k = 0; k < cc[i]; ++k) s += rk((Rank)i);
    return s;
}
static std::string splitStr(const std::vector<Combo>& cs) {
    std::string s;
    for (auto& c : cs) {
        s += tp(c.type); s += "[";
        for (auto r : c.mainRanks) s += rk(r);
        s += "]";
        if (!c.wingRanks.empty()) { s += "+"; for (auto r : c.wingRanks) s += rk(r); }
        s += " ";
    }
    return s;
}

// 对单一手牌规模跑 N 手，返回分歧数；反例写 jsonl
static long runSize(int size, long N, unsigned seed, double ppc, const char* outPath) {
    std::mt19937 rng(seed);
    auto deck = Deck::fullDeck();
    FILE* f = std::fopen(outPath, "w");
    long diverge = 0; double maxDiff = 0.0; long nBigger = 0;
    for (long t = 0; t < N; ++t) {
        std::shuffle(deck.begin(), deck.end(), rng);
        std::vector<Card> h(deck.begin(), deck.begin() + size);

        auto opt = optimalSplit(h);
        double S_opt = sumScore(opt);
        double reduced_opt = S_opt - ppc * (double)opt.size();

        double mp = optpower::maxPowerValue(h);
        double diff = mp - reduced_opt;
        if (!(mp >= reduced_opt - 1e-9)) {
            std::printf("[SANITY-FAIL] maxPower < reduced_opt! size=%d t=%ld diff=%.6f\n", size, t, diff);
        }
        if (diff > maxDiff) maxDiff = diff;
        if (diff > 1e-6) {
            ++diverge;
            auto pw = optpower::optimalSplitByPower(h);
            double S_pw = sumScore(pw);
            int n_opt = (int)opt.size(), n_pw = (int)pw.size();
            if (n_pw > n_opt) ++nBigger;  // 机制验证：最大牌力拆用更多手数
            if (f) std::fprintf(f,
                "{\"t\":%ld,\"hand\":\"%s\",\"opt\":\"%s\",\"power\":\"%s\","
                "\"n_opt\":%d,\"n_pw\":%d,\"S_opt\":%.4f,\"S_pw\":%.4f,"
                "\"reduced_opt\":%.4f,\"maxPower\":%.4f,\"diff\":%.6f}\n",
                t, handStr(h).c_str(), splitStr(opt).c_str(), splitStr(pw).c_str(),
                n_opt, n_pw, S_opt, S_pw, reduced_opt, mp, diff);
        }
    }
    if (f) std::fclose(f);
    std::printf("size=%2d  N=%ld  分歧=%ld (%.4f%%)  n_pw>n_opt=%ld  maxDiff=%.6f  -> %s\n",
                size, N, diverge, 100.0 * diverge / N, nBigger, maxDiff, outPath);
    return diverge;
}

int main() {
    loadScoringConfigFromFile("config/scoring.properties");
    double ppc = scoringConfig().penaltyPerCombo;
    std::printf("penaltyPerCombo=%.2f\n", ppc);

    const long N = (std::getenv("VSP_N") ? std::atol(std::getenv("VSP_N")) : 50000);
    long d17 = runSize(17, N, 12345, ppc, "extracted/divergence_17.jsonl");
    long d20 = runSize(20, N, 12345, ppc, "extracted/divergence_20.jsonl");

    std::printf("\n===== 结论 =====\n");
    if (d17 == 0 && d20 == 0) std::printf("0 分歧：最少手数拆牌恒达最大牌力（命题成立）\n");
    else std::printf("存在分歧（17:%ld, 20:%ld）：命题不成立，见 divergence_*.jsonl\n", d17, d20);
    return 0;
}
