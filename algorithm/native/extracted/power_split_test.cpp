// power_split_test.cpp —— 牌力最大拆牌器(power-DFS)校验
// 编译：vcvarsall x64 && cl /nologo /utf-8 /EHsc /std:c++14 /O2 /Fepower_split_test.exe power_split_test.cpp
#include "../include/landlord.h"
#include "optimal_split.h"
#include "optimal_split_power.h"

#include <cstdio>
#include <cmath>
#include <vector>
#include <random>
#include <algorithm>

using namespace landlord;

static double sumScore(const std::vector<Combo>& cs) {
    DefaultComboScoringStrategy sc; double t = 0; for (auto& c : cs) t += sc.score(c); return t;
}
static std::vector<Card> handFromRanks(std::initializer_list<Rank> rs) {
    std::vector<Card> h; for (auto r : rs) h.push_back({r, Suit::NONE}); return h;
}
// 拆分是否恰好消费整手牌（rank 计数对齐）
static bool splitCoversHand(const std::vector<Card>& hand, const std::vector<Combo>& split) {
    auto hc = HandCardUtils::buildRankCounts(hand);
    std::array<int, 15> sc{};
    std::array<std::pair<int, int>, 15> buf;
    for (auto& m : split) {
        int nb = optdetail::consumeBuf(m, buf);
        for (int i = 0; i < nb; ++i) sc[buf[i].first] += buf[i].second;
    }
    for (int i = 0; i < 15; ++i) if (hc[i] != sc[i]) return false;
    return true;
}

int main() {
    int fails = 0;
    double ppc = scoringConfig().penaltyPerCombo;

    // ---- 已知值：rocket-only -> maxPower = 60 - 8 = 52 ----
    {
        auto h = handFromRanks({Rank::SMALL_JOKER, Rank::BIG_JOKER});
        double mp = optpower::maxPowerValue(h);
        bool pass = std::fabs(mp - 52.0) < 1e-6;
        if (!pass) ++fails;
        printf("[%s] rocket-only maxPower=%.4f (期望 52)\n", pass ? "PASS" : "FAIL", mp);
    }
    // ---- 已知值：§1.2 {6666+7..K} -> maxPower = 55 - 16 = 39 ----
    {
        auto h = handFromRanks({Rank::SIX,Rank::SIX,Rank::SIX,Rank::SIX,
                                Rank::SEVEN,Rank::EIGHT,Rank::NINE,Rank::TEN,
                                Rank::JACK,Rank::QUEEN,Rank::KING});
        double mp = optpower::maxPowerValue(h);
        bool pass = std::fabs(mp - 39.0) < 1e-6;
        if (!pass) ++fails;
        printf("[%s] §1.2 bomb+straight maxPower=%.4f (期望 39)\n", pass ? "PASS" : "FAIL", mp);
    }

    // ---- 支配性 + 还原一致性（2000 随机手）----
    std::mt19937 rng(777);
    auto deck = Deck::fullDeck();
    int domFail = 0, coverFail = 0, consistFail = 0;
    for (int t = 0; t < 2000; ++t) {
        std::shuffle(deck.begin(), deck.end(), rng);
        int size = 17 + (t % 2) * 3;  // 17 与 20 交替
        std::vector<Card> h(deck.begin(), deck.begin() + size);

        auto opt = optimalSplit(h);
        double reduced_opt = sumScore(opt) - ppc * (int)opt.size();
        double mp = optpower::maxPowerValue(h);
        if (!(mp >= reduced_opt - 1e-9)) ++domFail;            // maxPower 恒 >= optimalSplit 约简值

        auto pw = optpower::optimalSplitByPower(h);
        if (!splitCoversHand(h, pw)) ++coverFail;              // 还原拆分须恰好消费手牌
        double reduced_pw = sumScore(pw) - ppc * (int)pw.size();
        if (std::fabs(reduced_pw - mp) > 1e-6) ++consistFail;  // 还原拆分约简值 == maxPower
    }
    bool randPass = (domFail == 0 && coverFail == 0 && consistFail == 0);
    if (!randPass) ++fails;
    printf("[%s] 随机2000手：支配违例=%d 还原未覆盖=%d 一致性违例=%d\n",
           randPass ? "PASS" : "FAIL", domFail, coverFail, consistFail);

    printf("\n汇总: %d 项失败\n", fails);
    return fails ? 1 : 0;
}
