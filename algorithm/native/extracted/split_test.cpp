// split_test.cpp —— 最优拆牌器校验
// 编译：vcvarsall x64 && cl /nologo /utf-8 /EHsc /std:c++14 /O2 /Fesplit_test.exe split_test.cpp
// 校验：(1) 5 组典型案例 = 预期拆解/score；(2) 二跑字节同（确定性）；
//       (3) 1000 随机手 optimal.n ≤ 贪心.n（等 n 时 optimal Σscore ≥ 贪心）。
#include "../include/landlord.h"
#include "optimal_split.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include <algorithm>
#include <random>

using namespace landlord;

static std::string rankStr(Rank r) {
    switch (r) {
        case Rank::THREE: return "3"; case Rank::FOUR: return "4"; case Rank::FIVE: return "5";
        case Rank::SIX: return "6"; case Rank::SEVEN: return "7"; case Rank::EIGHT: return "8";
        case Rank::NINE: return "9"; case Rank::TEN: return "T"; case Rank::JACK: return "J";
        case Rank::QUEEN: return "Q"; case Rank::KING: return "K"; case Rank::ACE: return "A";
        case Rank::TWO: return "2"; case Rank::SMALL_JOKER: return "sj"; case Rank::BIG_JOKER: return "bj";
    }
    return "?";
}
static std::string typeStr(ComboType t) {
    switch (t) {
        case ComboType::SINGLE: return "单"; case ComboType::PAIR: return "对"; case ComboType::TRIPLE: return "三";
        case ComboType::TRIPLE_WITH_SINGLE: return "三带一"; case ComboType::TRIPLE_WITH_PAIR: return "三带二";
        case ComboType::STRAIGHT: return "顺"; case ComboType::CONSECUTIVE_PAIRS: return "连对";
        case ComboType::PLANE: return "飞机"; case ComboType::PLANE_WITH_SINGLES: return "飞机带单";
        case ComboType::PLANE_WITH_PAIRS: return "飞机带对";
        case ComboType::QUAD_WITH_TWO_SINGLES: return "四带二单"; case ComboType::QUAD_WITH_TWO_PAIRS: return "四带二对";
        case ComboType::BOMB: return "炸"; case ComboType::ROCKET: return "王炸";
    }
    return "?";
}
static std::string comboStr(const Combo& c) {
    std::string s = typeStr(c.type) + "[";
    for (auto r : c.mainRanks) s += rankStr(r);
    s += "]";
    if (!c.wingRanks.empty()) { s += "+翼["; for (auto r : c.wingRanks) s += rankStr(r); s += "]"; }
    return s;
}
static double sumScore(const std::vector<Combo>& cs) {
    DefaultComboScoringStrategy sc; double t = 0; for (auto& c : cs) t += sc.score(c); return t;
}
static std::string handStr(const std::vector<Card>& h) {
    auto cc = HandCardUtils::buildRankCounts(h);
    std::string s; for (int i = 14; i >= 0; --i) for (int k = 0; k < cc[i]; ++k) s += rankStr((Rank)i);
    return s;
}

static std::vector<Card> handFromRanks(std::initializer_list<Rank> rs) {
    std::vector<Card> h; for (auto r : rs) h.push_back({r, Suit::NONE}); return h;
}

struct CaseDef { const char* name; std::vector<Card> hand; int expN; double expScore; const char* expHas; };

int main() {
    std::vector<CaseDef> cases = {
        {"1 炸弹vs长顺(§1.2)", handFromRanks({Rank::SIX,Rank::SIX,Rank::SIX,Rank::SIX, Rank::SEVEN,Rank::EIGHT,Rank::NINE,Rank::TEN,Rank::JACK,Rank::QUEEN,Rank::KING}), 2, 55.0, "炸"},
        {"2 四带二(孤立四张)", handFromRanks({Rank::EIGHT,Rank::EIGHT,Rank::EIGHT,Rank::EIGHT, Rank::THREE,Rank::FOUR}), 1, 6.0, "四带二单"},
        {"3 飞机带单",          handFromRanks({Rank::FIVE,Rank::FIVE,Rank::FIVE, Rank::SIX,Rank::SIX,Rank::SIX, Rank::THREE,Rank::FOUR}), 1, 5.0, "飞机带单"},
        {"4 连对",              handFromRanks({Rank::FIVE,Rank::FIVE, Rank::SIX,Rank::SIX, Rank::SEVEN,Rank::SEVEN}), 1, 12.0, "连对"},
        {"5 王炸+顺",          handFromRanks({Rank::SMALL_JOKER,Rank::BIG_JOKER, Rank::THREE,Rank::FOUR,Rank::FIVE,Rank::SIX,Rank::SEVEN}), 2, 75.0, "王炸"},
    };

    int fails = 0;
    printf("===== 5 组验证案例 =====\n");
    for (auto& cs : cases) {
        auto a = optimalSplit(cs.hand);
        auto b = optimalSplit(cs.hand);                       // 二跑（确定性）
        int n = (int)a.size();
        double sc = sumScore(a);
        bool det = (a.size() == b.size());
        if (det) for (size_t i = 0; i < a.size(); ++i) if (optdetail::encodeMove(a[i]) != optdetail::encodeMove(b[i])) det = false;
        std::string decomp;
        for (auto& c : a) { decomp += comboStr(c) + " "; }
        bool hasExp = decomp.find(cs.expHas) != std::string::npos;
        bool pass = (n == cs.expN) && (std::fabs(sc - cs.expScore) < 1e-6) && hasExp && det;
        if (!pass) ++fails;
        printf("[%s] %s\n  手牌=%s\n  拆解=%s (n=%d, Σscore=%.1f)  期望(n=%d,score=%.1f,含\"%s\")  确定性=%s\n",
               pass ? "PASS" : "FAIL", cs.name, handStr(cs.hand).c_str(), decomp.c_str(), n, sc,
               cs.expN, cs.expScore, cs.expHas, det ? "Y" : "N");
    }

    printf("\n===== 1000 随机手：optimal vs 贪心 =====\n");
    std::mt19937 rng(12345);
    int violate = 0, nTie = 0, nOptBetter = 0;
    auto deck = Deck::fullDeck();
    for (int t = 0; t < 1000; ++t) {
        std::shuffle(deck.begin(), deck.end(), rng);
        std::vector<Card> h(deck.begin(), deck.begin() + 17);
        auto opt = optimalSplit(h);
        auto gre = DefaultSplitterFactory::extractAllCombos(h);
        int no = (int)opt.size(), ng = (int)gre.size();
        double so = sumScore(opt), sg = sumScore(gre);
        if (no < ng) ++nOptBetter;
        else if (no == ng) { ++nTie; if (so < sg - 1e-9) ++violate; }
        else ++violate;  // optimal.n > greedy.n ⇒ 违反（贪心不可能更少组合）
    }
    printf("optimal 更少手数: %d/1000, 等手数: %d/1000, 违反(等n却score更低 或 n更大): %d\n",
           nOptBetter, nTie, violate);
    bool randPass = (violate == 0);
    if (!randPass) ++fails;
    printf("[%s] 随机对照（应 0 违反）\n", randPass ? "PASS" : "FAIL");

    printf("\n===== memo 规模: %zu 个状态 =====\n", optdetail::memoMap().size());
    printf("\n汇总: %d 项失败\n", fails);
    return fails ? 1 : 0;
}
