#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
生成 harness_optv2.cpp：
  基于 harness.cpp，把第 6 节（HandCardInfo + SpliteCard 系函数）
  替换成研发优化版（memo + node budget 版 get_MaxHandCardValue / GetBestCardType / SpliteCard）。

原版 harness.exe 完全保留，不做任何修改。

优化版改动（与研发 new/MakeDealHelper 对齐）：
  1. get_GroupData：静态缓存（60s 刷新）—— 性能优化，无行为变化
  2. HandSearchContext：memo 表 + node budget(100000)
  3. get_MaxHandCardValue：递归查 memo + 预算耗尽 abort
  4. GetBestCardType：abort 时跳过主循环 + last-resort 兜底
  5. SpliteCard：no-progress 防御 + abort 日志
"""
import re, shutil, pathlib

HERE = pathlib.Path(__file__).resolve().parent
SRC = HERE / "harness.cpp"
DST = HERE / "harness_optv2.cpp"

src = SRC.read_text(encoding="utf-8")

# =============================================================================
# 替换块 1：get_GroupData 函数（原版 JSON 直读 → 优化版静态缓存版）
# 定位：从 "// 5. get_GroupData" 到 "static const char* get_GroupCardName" 之前
# =============================================================================

old_groupdata_start = "// 5. get_GroupData（MakeDealHelper.cpp:85）—— 读 JSON GroupDataExp"
old_groupdata_end = "static const char* get_GroupCardName(int GroupCardType) {"

new_groupdata = r'''// 5. get_GroupData（研发优化版：静态缓存 + mutex，60s 刷新）
//    行为与原版完全一致，仅性能优化。
// =============================================================================
#include <unordered_map>
#include <mutex>

struct GroupDataExpTerm {
    double dC;
    int nM;
    int nD;
    bool bIsPower;  // true: C * pow(MaxCard, M); false: C * log(MaxCard)/log(D)
};

static std::mutex s_csGroupDataExp;
static std::vector<GroupDataExpTerm> s_arrGroupDataExp[cgKING_CARD + 1];
static unsigned long long s_ullGroupDataExpLoadTick = 0;
static const unsigned long long GROUPDATA_EXP_REFRESH_MS = 60 * 1000;

// Caller must hold s_csGroupDataExp.
static void LoadGroupDataExpCache() {
    const JsonValue& expRoot = CFG_MGR[MAKEDEAL_CONFIG]["MakeDealCommonArgs"]["GroupDataExp"];
    for (int nType = 0; nType <= cgKING_CARD; ++nType) {
        char szType[32] = {0};
        sprintf(szType, "%d", nType);
        const JsonValue& expItem = expRoot[szType];
        std::vector<GroupDataExpTerm> arrTerms;
        if (!expItem.isNull()) {
            for (size_t i = 0; i < expItem.size(); ++i) {
                GroupDataExpTerm term;
                term.dC = expItem[(int)i]["C"].asDouble();
                term.bIsPower = expItem[(int)i]["D"].isNull();
                term.nM = expItem[(int)i]["M"].asInt();
                term.nD = expItem[(int)i]["D"].asInt();
                arrTerms.push_back(term);
            }
        }
        s_arrGroupDataExp[nType].swap(arrTerms);
    }
    s_ullGroupDataExpLoadTick = (unsigned long long)clock();
}

static int CalcGroupDataValue(int cgType, int MaxCard) {
    if (cgType < 0 || cgType > cgKING_CARD) return 0;
    std::lock_guard<std::mutex> guard(s_csGroupDataExp);
    if (s_ullGroupDataExpLoadTick == 0 ||
        (unsigned long long)clock() - s_ullGroupDataExpLoadTick >= GROUPDATA_EXP_REFRESH_MS) {
        LoadGroupDataExpCache();
    }
    int nValue = 0;
    const std::vector<GroupDataExpTerm>& arrTerms = s_arrGroupDataExp[cgType];
    for (size_t i = 0; i < arrTerms.size(); ++i) {
        int nTermValue = arrTerms[i].bIsPower
            ? (int)(arrTerms[i].dC * pow((double)MaxCard, (double)arrTerms[i].nM))
            : (int)(arrTerms[i].dC * (log((double)MaxCard) / log((double)arrTerms[i].nD)));
        nValue += nTermValue;
    }
    return nValue;
}

'''

# 找到起点和终点
idx_start = src.index(old_groupdata_start)
idx_end = src.index(old_groupdata_end)

# 替换 get_GroupData 函数体（保留 CardGroupData 结构等前置定义）
# 我们要替换的是 "// 5. get_GroupData ..." 到 "static const char* get_GroupCardName" 之间的内容
before = src[:idx_start]
after = src[idx_end:]

src = before + new_groupdata + "static CardGroupData get_GroupData(CardGroupType cgType, int MaxCard, int Count) {\n" \
      "    CardGroupData uct;\n" \
      "    uct.cgType = cgType;\n" \
      "    uct.nCount = Count;\n" \
      "    uct.nMaxCard = MaxCard;\n" \
      "    uct.nValue = CalcGroupDataValue(cgType, MaxCard);\n" \
      "    return uct;\n" \
      "}\n" + after

# =============================================================================
# 替换块 2：第 6 节 HandCardInfo + SpliteCard 系
# 定位：从 "// 6. HandCardInfo + SpliteCard 系" 到 "// 7. Type1 拼牌 MakeDeal_ComposeCard" 之前
# =============================================================================

old_sec6_start = "// 6. HandCardInfo + SpliteCard 系（MakeDealHelper.cpp:10-727）逐字"
old_sec6_end = "// 7. Type1 拼牌 MakeDeal_ComposeCard（MakeDealHelper.cpp:730-1475）逐字"

new_sec6 = r'''// 6. HandCardInfo + SpliteCard 系——研发优化版（memo + node budget，对应 MakeDealHelper new/ 版本）
//    原版 harness 用精简重写版；此处替换为线上 MakeDealHelper 优化后的版本。
//    核心差异：get_MaxHandCardValue 加 memo 表 + 100000 节点预算，
//    预算耗尽时 abort（返回最差值），GetBestCardType 跳过搜索直接走兜底。
// =============================================================================

static const int HAND_SEARCH_NODE_BUDGET = 100000;

struct HandSearchContext {
    std::unordered_map<unsigned long long, HandCardValue> memo;
    int nNodeBudget;
    bool bAborted;
    explicit HandSearchContext(int nBudget) : nNodeBudget(nBudget), bAborted(false) {}
};

// Bijective base-5 key over the 18 rank counters (each 0..4).
static unsigned long long HandLayoutKey(const int aHandCardList[18]) {
    unsigned long long ullKey = 0;
    for (int i = 0; i < 18; ++i) {
        ullKey = ullKey * 5 + (unsigned long long)aHandCardList[i];
    }
    return ullKey;
}

class HandCardInfo {
public:
    int value_aHandCardList[18];
    int nHandCardCount = 17;
    CardGroupData uctPutCardType;
    std::vector<int> value_nPutCardList;
    void ClearPutCardList() {
        value_nPutCardList.clear();
        uctPutCardType.cgType = cgERROR; uctPutCardType.nCount = 0; uctPutCardType.nMaxCard = -1; uctPutCardType.nValue = 0;
    }
    int getvaluebycardid(int cardid) { return GetValuebyCardid(cardid); }
    void Init(std::vector<int> CardIdArr) {
        memset(value_aHandCardList, 0, sizeof(value_aHandCardList));
        for (size_t it = 0; it < CardIdArr.size(); it++) value_aHandCardList[getvaluebycardid(CardIdArr[it])]++;
        nHandCardCount = (int)CardIdArr.size();
    }
};

static HandCardValue get_MaxHandCardValue(HandCardInfo& cls, HandSearchContext& ctx); // fwd

static CardGroupData SurCardsType(int arr[]) {
    int nCount = 0;
    for (int i = 3; i < 18; i++) nCount += arr[i];
    CardGroupData ret; ret.nCount = nCount;

    if (nCount == 1) { int prov = 0, SumValue = 0;
        for (int i = 3; i < 18; i++) if (arr[i] == 1) { SumValue = i - 10; prov++; ret.nMaxCard = i; break; }
        if (prov == 1) { ret.cgType = cgSINGLE; ret.nValue = SumValue; return ret; } }
    if (nCount == 2) { int prov = 0, SumValue = 0, i;
        for (i = 3; i < 16; i++) if (arr[i] == 2) { SumValue = i - 10; prov++; ret.nMaxCard = i; break; }
        if (prov == 1) { ret.cgType = cgDOUBLE; ret.nValue = SumValue; return ret; } }
    if (nCount == 3) { int prov = 0, SumValue = 0, i;
        for (i = 3; i < 16; i++) if (arr[i] == 3) { SumValue = i - 10; prov++; ret.nMaxCard = i; break; }
        if (prov == 1) { ret.cgType = cgTHREE; ret.nValue = SumValue; return ret; } }
    if (nCount == 4) { int prov = 0, SumValue = 0;
        for (int i = 3; i < 16; i++) if (arr[i] == 4) { SumValue += i - 3 + 7; prov++; ret.nMaxCard = i; break; }
        if (prov == 1) { ret.cgType = cgBOMB_CARD; ret.nValue = SumValue; return ret; } }
    if (nCount == 2) { if (arr[17] > 0 && arr[16] > 0) { ret.nMaxCard = 17; ret.cgType = cgKING_CARD; ret.nValue = 20; return ret; } }
    if (nCount >= 5) { int prov = 0, SumValue = 0, i;
        for (i = 3; i < 15; i++) { if (arr[i] == 1) prov++; else { if (prov != 0) break; } }
        SumValue = i - 10;
        if (prov == nCount) { ret.nMaxCard = i - 1; ret.cgType = cgSINGLE_LINE; ret.nValue = SumValue; return ret; } }
    if (nCount >= 6) { int prov = 0, SumValue = 0, i;
        for (i = 3; i < 15; i++) { if (arr[i] == 2) prov++; else { if (prov != 0) break; } }
        SumValue = i - 10;
        if (prov * 2 == nCount) { ret.nMaxCard = i - 1; ret.cgType = cgDOUBLE_LINE; ret.nValue = SumValue; return ret; } }
    if (nCount >= 6) { int prov = 0, SumValue = 0, i;
        for (i = 3; i < 15; i++) { if (arr[i] == 3) prov++; else { if (prov != 0) break; } }
        SumValue = (i - 3) / 2;
        if (prov * 3 == nCount) { ret.nMaxCard = i - 1; ret.cgType = cgTHREE_LINE; ret.nValue = SumValue; return ret; } }
    ret.cgType = cgERROR;
    return ret;
}

static HandCardValue get_MaxHandCardValue(HandCardInfo& cls, HandSearchContext& ctx) {
    --ctx.nNodeBudget;
    if (ctx.nNodeBudget < 0) {
        // Budget exhausted: stop the search and unwind with a worst-case value.
        ctx.bAborted = true;
        cls.ClearPutCardList();
        HandCardValue abortValue;
        abortValue.SumValue = MinCardsValue;
        abortValue.NeedRound = 20;
        return abortValue;
    }

    cls.ClearPutCardList();

    // Memoize by hand layout: identical sub-hands are re-evaluated exponentially
    // often during the search, so a cached value prunes whole subtrees.
    unsigned long long ullKey = HandLayoutKey(cls.value_aHandCardList);
    std::unordered_map<unsigned long long, HandCardValue>::const_iterator itMemo = ctx.memo.find(ullKey);
    if (itMemo != ctx.memo.end()) {
        return itMemo->second;
    }

    HandCardValue uct;
    if (cls.nHandCardCount == 0) { uct.SumValue = 0; uct.NeedRound = 0; ctx.memo[ullKey] = uct; return uct; }
    CardGroupData scd = SurCardsType(cls.value_aHandCardList);
    if (scd.cgType != cgERROR && scd.cgType != cgFOUR_TAKE_ONE && scd.cgType != cgFOUR_TAKE_TWO) {
        uct.SumValue = scd.nValue; uct.NeedRound = 1; ctx.memo[ullKey] = uct; return uct;
    }
    // 取一个最优牌型后递归
    extern void GetBestCardType(HandCardInfo&, HandSearchContext&);
    GetBestCardType(cls, ctx);
    CardGroupData NowPutCardType = cls.uctPutCardType;
    std::vector<int> NowPutCardList = cls.value_nPutCardList;
    for (size_t it = 0; it < NowPutCardList.size(); it++) cls.value_aHandCardList[NowPutCardList[it]]--;
    cls.nHandCardCount -= NowPutCardType.nCount;
    HandCardValue tmp = get_MaxHandCardValue(cls, ctx);
    for (size_t it = 0; it < NowPutCardList.size(); it++) cls.value_aHandCardList[NowPutCardList[it]]++;
    cls.nHandCardCount += NowPutCardType.nCount;
    uct.SumValue = NowPutCardType.nValue + tmp.SumValue;
    uct.NeedRound = tmp.NeedRound + 1;
    ctx.memo[ullKey] = uct;
    return uct;
}

static inline long long _score(const HandCardValue& hv) { return (long long)hv.SumValue - (long long)hv.NeedRound * 7; }

void GetBestCardType(HandCardInfo& cls, HandSearchContext& ctx) {
    cls.ClearPutCardList();
    CardGroupData scd = SurCardsType(cls.value_aHandCardList);
    if (scd.cgType != cgERROR && scd.cgType != cgFOUR_TAKE_ONE && scd.cgType != cgFOUR_TAKE_TWO) {
        cls.uctPutCardType = scd;
        for (int i = 0; i < 18; i++) for (int j = 0; j < cls.value_aHandCardList[i]; j++) cls.value_nPutCardList.push_back(i);
        return;
    }
    HandCardValue Best; Best.NeedRound = 20; Best.SumValue = MinCardsValue; Best.NeedRound += 1;
    CardGroupData BestGroup;
    int* L = cls.value_aHandCardList;

    // Skip candidate evaluation once the node budget is exhausted; fall through
    // to the cheap tail picks so the caller still gets a removable group.
    for (int i = ctx.bAborted ? 16 : 3; i < 16; i++) {
        if (L[i] != 0 && L[i] != 4) {
            if (L[i] == 1) { L[i]--; cls.nHandCardCount--; HandCardValue tmp = get_MaxHandCardValue(cls, ctx); L[i]++; cls.nHandCardCount++;
                if (_score(Best) <= _score(tmp)) { Best = tmp; BestGroup = get_GroupData(cgSINGLE, i, 1); } }
            if (L[i] == 2) { L[i]-=2; cls.nHandCardCount-=2; HandCardValue tmp = get_MaxHandCardValue(cls, ctx); L[i]+=2; cls.nHandCardCount+=2;
                if (_score(Best) <= _score(tmp)) { Best = tmp; BestGroup = get_GroupData(cgDOUBLE, i, 2); } }
            if (L[i] == 3) { L[i]-=3; cls.nHandCardCount-=3; HandCardValue tmp = get_MaxHandCardValue(cls, ctx); L[i]+=3; cls.nHandCardCount+=3;
                if (_score(Best) <= _score(tmp)) { Best = tmp; BestGroup = get_GroupData(cgTHREE, i, 3); } }
            if (L[i] > 0) { int prov = 0;
                for (int j = i; j < 15; j++) { if (L[j] > 0) prov++; else break;
                    if (prov >= 5) { for (int k = i; k <= j; k++) L[k]--; cls.nHandCardCount -= prov;
                        HandCardValue tmp = get_MaxHandCardValue(cls, ctx); for (int k = i; k <= j; k++) L[k]++; cls.nHandCardCount += prov;
                        if (_score(Best) <= _score(tmp)) { Best = tmp; BestGroup = get_GroupData(cgSINGLE_LINE, j, prov); } } } }
            if (L[i] > 1) { int prov = 0;
                for (int j = i; j < 15; j++) { if (L[j] > 1) prov++; else break;
                    if (prov >= 3) { for (int k = i; k <= j; k++) L[k]-=2; cls.nHandCardCount -= prov*2;
                        HandCardValue tmp = get_MaxHandCardValue(cls, ctx); for (int k = i; k <= j; k++) L[k]+=2; cls.nHandCardCount += prov*2;
                        if (_score(Best) <= _score(tmp)) { Best = tmp; BestGroup = get_GroupData(cgDOUBLE_LINE, j, prov*2); } } } }
            if (L[i] > 2) { int prov = 0;
                for (int j = i; j < 15; j++) { if (L[j] > 2) prov++; else break;
                    if (prov >= 2) { for (int k = i; k <= j; k++) L[k]-=3; cls.nHandCardCount -= prov*3;
                        HandCardValue tmp = get_MaxHandCardValue(cls, ctx); for (int k = i; k <= j; k++) L[k]+=3; cls.nHandCardCount += prov*3;
                        if (_score(Best) <= _score(tmp)) { Best = tmp; BestGroup = get_GroupData(cgTHREE_LINE, j, prov*3); } } } }
            if (BestGroup.cgType == cgERROR) {}
            else if (BestGroup.cgType == cgSINGLE) { cls.value_nPutCardList.push_back(BestGroup.nMaxCard); cls.uctPutCardType = BestGroup; }
            else if (BestGroup.cgType == cgDOUBLE) { cls.value_nPutCardList.push_back(BestGroup.nMaxCard); cls.value_nPutCardList.push_back(BestGroup.nMaxCard); cls.uctPutCardType = BestGroup; }
            else if (BestGroup.cgType == cgTHREE) { for (int t=0;t<3;t++) cls.value_nPutCardList.push_back(BestGroup.nMaxCard); cls.uctPutCardType = BestGroup; }
            else if (BestGroup.cgType == cgSINGLE_LINE) { for (int j = BestGroup.nMaxCard-BestGroup.nCount+1; j <= BestGroup.nMaxCard; j++) cls.value_nPutCardList.push_back(j); cls.uctPutCardType = BestGroup; }
            else if (BestGroup.cgType == cgDOUBLE_LINE) { for (int j = BestGroup.nMaxCard-(BestGroup.nCount/2)+1; j <= BestGroup.nMaxCard; j++) { cls.value_nPutCardList.push_back(j); cls.value_nPutCardList.push_back(j); } cls.uctPutCardType = BestGroup; }
            else if (BestGroup.cgType == cgTHREE_LINE) { for (int j = BestGroup.nMaxCard-(BestGroup.nCount/3)+1; j <= BestGroup.nMaxCard; j++) { cls.value_nPutCardList.push_back(j); cls.value_nPutCardList.push_back(j); cls.value_nPutCardList.push_back(j); } cls.uctPutCardType = BestGroup; }
            return;
        }
    }
    if (cls.value_aHandCardList[16] == 1 && cls.value_aHandCardList[17] == 0) { cls.value_nPutCardList.push_back(16); cls.uctPutCardType = get_GroupData(cgSINGLE, 16, 1); return; }
    if (cls.value_aHandCardList[16] == 0 && cls.value_aHandCardList[17] == 1) { cls.value_nPutCardList.push_back(17); cls.uctPutCardType = get_GroupData(cgSINGLE, 17, 1); return; }
    for (int i = 3; i < 16; i++) if (cls.value_aHandCardList[i] == 4) { for (int t=0;t<4;t++) cls.value_nPutCardList.push_back(i); cls.uctPutCardType = get_GroupData(cgBOMB_CARD, i, 4); return; }
    if (cls.value_aHandCardList[17] > 0 && cls.value_aHandCardList[16] > 0) { cls.value_nPutCardList.push_back(17); cls.value_nPutCardList.push_back(16); cls.uctPutCardType = get_GroupData(cgKING_CARD, 17, 2); return; }

    // Last resort: emit the lowest available single card so the caller always
    // receives a removable group and SpliteCard can never loop forever.
    for (int i = 3; i < 18; i++) {
        if (cls.value_aHandCardList[i] > 0) {
            cls.value_nPutCardList.push_back(i);
            cls.uctPutCardType = get_GroupData(cgSINGLE, i, 1);
            return;
        }
    }

    cls.uctPutCardType = get_GroupData(cgERROR, 0, 0);
}

void SpliteCard(std::vector<int> arrHandCardList, std::vector<CardGroupData>& cardTypeArr) {
    HandCardInfo cls; cls.Init(arrHandCardList);
    if (cls.nHandCardCount <= 0) return;
    cardTypeArr.clear();
    HandSearchContext ctx(HAND_SEARCH_NODE_BUDGET);
    while (1) {
        GetBestCardType(cls, ctx);
        cardTypeArr.push_back(cls.uctPutCardType);
        for (size_t it = 0; it < cls.value_nPutCardList.size(); it++) cls.value_aHandCardList[cls.value_nPutCardList[it]]--;
        int nCountBefore = cls.nHandCardCount;
        cls.nHandCardCount -= cls.uctPutCardType.nCount;
        if (cls.nHandCardCount <= 0) break;
        if (cls.nHandCardCount >= nCountBefore) {
            // Defensive guard: the chosen group removed nothing, stop instead
            // of spinning on the same hand forever.
            printf("SpliteCard no progress, %d cards left\n", cls.nHandCardCount);
            break;
        }
    }
    if (ctx.bAborted) {
        printf("SpliteCard aborted, node budget %d exhausted, %d cards left\n",
            HAND_SEARCH_NODE_BUDGET, cls.nHandCardCount);
    }
}

// CalHandCardValue（原 harness.cpp 第 6 节自带，未被研发改动，原样保留）
// 牌型数 nHandCount：三张/飞机可带的单/对不计入；nHandCardAveValue = 牌型价值和
void CalHandCardValue(std::vector<CardGroupData>& CardGroupDatas, int& nHandCount, int& nHandCardAveValue) {
    sort(CardGroupDatas.begin(), CardGroupDatas.end(), [](CardGroupData a, CardGroupData b){ return a.nValue > b.nValue ? true : false; });
    int nLesserCount = 0; int nHandCardTotalValue = 0;
    for (int i = 0; i < (int)CardGroupDatas.size(); i++) {
        nHandCardTotalValue += CardGroupDatas[i].nValue;
        if (CardGroupDatas[i].cgType == cgTHREE || CardGroupDatas[i].cgType == cgTHREE_LINE) nLesserCount += CardGroupDatas[i].nCount / 3;
    }
    nHandCount = (int)CardGroupDatas.size();
    for (int i = nHandCount - 1; i >= 0 && nLesserCount > 0; i--, nLesserCount--) {
        if ((CardGroupDatas[i].cgType == cgSINGLE || CardGroupDatas[i].cgType == cgDOUBLE) && CardGroupDatas[i].nMaxCard < 15) {
            nHandCount--; nHandCardTotalValue -= CardGroupDatas[i].nValue;
        }
    }
    nHandCardAveValue = nHandCardTotalValue;
}

'''

idx6_start = src.index(old_sec6_start)
idx6_end = src.index(old_sec6_end)

src = src[:idx6_start] + new_sec6 + src[idx6_end:]

# =============================================================================
# 写文件
# =============================================================================
DST.write_text(src, encoding="utf-8")
print(f"生成: {DST}")
print(f"原版: {SRC} 保留不变")
