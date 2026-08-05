// =============================================================================
// harness.cpp — 斗地主发牌逻辑【独立模拟器】(Standalone Harness)
//
// 目标：把线上 zgdatbl.cpp/.h + MakeDealHelper.cpp/.h 里【发牌/配牌/拆牌/洗牌/随机】
// 的真实 C++ 逻辑 1:1 原样剥离，去掉网络/DB/CGameTable 上帝类依赖，编译为独立可执行，
// 用于 100% 物理级精确的发牌概率统计。
//
// 提取原则：发牌算法体（MakeDealByCfg/DoMakeDeal/MakeDeal_ComposeCard/SpliteCard/
//   GetBestCardType/get_MaxHandCardValue/get_GroupData/Match*/Calc*HandCount/
//   SvrXygRandomSort/GetMakeDealCfg/CalcBanker 等）逐字照抄，含原笔误与 rand/srand。
//   仅在顶部用极简 Stub 切断 Table/Player/Config/Windows 依赖。
//
// 编译：MSVC  ->  vcvarsall x64 && cl /nologo /EHsc /std:c++14 /O2 harness.cpp
//       g++   ->  g++ -std=c++14 -O2 harness.cpp -o harness
// 运行：harness --room 742 --reals 3 -n 100000 [--seed 0] > deals.jsonl
// =============================================================================

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdarg>
#include <cmath>
#include <ctime>
#include <string>
#include <vector>
#include <map>
#include <algorithm>

// =============================================================================
// 0. 可移植性 Stub（不发牌逻辑）
// =============================================================================
typedef unsigned char       BOOL;
#define TRUE  1
#define FALSE 0
#define _T(x) x
typedef char TCHAR;
#define _stprintf sprintf
typedef std::string CString;        // GetMakeDealCfg 里仅用 c_str()/operator+/赋值，std::string 足够

static inline int  GetPrivateProfileInt(const TCHAR*, const TCHAR*, int def, const TCHAR*) { return def; } // INI 开关走默认值
static inline unsigned long GetTickCountStub() { static unsigned long t = 0; return t++; }                 // 洗牌种子源（main 里另传 seed，此处仅占位）
#define GetTickCount GetTickCountStub
static inline void UwlLogFile(const char*, ...) {}           // 日志 no-op
static inline void UwlTrace(const char*, ...) {}
#define LOG_INFO(...)
#define LOG_WARN(...)
#define ZeroMemory(d, n) memset(d, 0, n)                       // 替代 Windows.h ZeroMemory

// =============================================================================
// 1. 极简 JSON（仅支持本文件用到的 JsonCpp API 子集：对象/数组/标量/解析 + 访问器）
//    用于加载 makedeal.json 供 get_GroupData / GetMakeDealCfg 读取。
// =============================================================================
class JsonValue {
public:
    enum T { NUL, INT, DBL, STR, OBJ, ARR } type = NUL;
    long long iv = 0;
    double dv = 0;
    std::string sv;
    std::vector<JsonValue> arr;
    std::vector<std::pair<std::string, JsonValue>> obj;

    bool isNull() const { return type == NUL; }
    int asInt() const {
        if (type == INT) return (int)iv;
        if (type == DBL) return (int)dv;
        if (type == STR) return atoi(sv.c_str());
        return 0;
    }
    float asFloat() const {
        if (type == DBL) return (float)dv;
        if (type == INT) return (float)iv;
        if (type == STR) return (float)atof(sv.c_str());
        return 0;
    }
    double asDouble() const {
        if (type == DBL) return dv;
        if (type == INT) return (double)iv;
        if (type == STR) return atof(sv.c_str());
        return 0;
    }
    const char* asCString() const {
        if (type == STR) return sv.c_str();
        static char buf[32];
        if (type == INT) { sprintf(buf, "%lld", iv); return buf; }
        return "";
    }
    std::string asString() const { return type == STR ? sv : std::string(asCString()); }
    size_t size() const { return type == ARR ? arr.size() : (type == OBJ ? obj.size() : 0); }
    bool empty() const { return size() == 0 && type != STR; }
    JsonValue& operator[](const char* k) {                 // 对象访问；缺失返回静态 null
        if (type != OBJ) { static JsonValue nullV; return nullV; }
        for (auto& p : obj) if (p.first == k) return p.second;
        static JsonValue nullV; return nullV;
    }
    const JsonValue& operator[](const char* k) const {
        if (type != OBJ) { static JsonValue nullV; return nullV; }
        for (auto& p : obj) if (p.first == k) return p.second;
        static JsonValue nullV; return nullV;  // 常量版静态
    }
    JsonValue& operator[](size_t i) { return arr[i]; }
    const JsonValue& operator[](size_t i) const { return arr[i]; }
    JsonValue& operator[](int i) { return operator[](size_t(i >= 0 ? i : 0)); }
    std::vector<std::string> getMemberNames() const {
        std::vector<std::string> r; if (type == OBJ) for (auto& p : obj) r.push_back(p.first); return r;
    }
};

// 极简递归下降 JSON 解析器
struct JsonParser {
    const char* p;
    const char* e;
    void skip() { while (p < e && (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')) p++; }
    bool parse(const std::string& s, JsonValue& out) {
        p = s.c_str(); e = p + s.size(); skip();
        return parseValue(out) && (skip(), true);
    }
    bool parseValue(JsonValue& v) {
        skip();
        if (p >= e) return false;
        char c = *p;
        if (c == '{') return parseObj(v);
        if (c == '[') return parseArr(v);
        if (c == '"') return parseStr(v);
        if (c == 't' || c == 'f') return parseBool(v);
        if (c == 'n') return parseNull(v);
        return parseNum(v);
    }
    bool parseObj(JsonValue& v) {
        v.type = JsonValue::OBJ; v.obj.clear(); p++; skip();
        if (p < e && *p == '}') { p++; return true; }
        while (p < e) {
            skip();
            if (p >= e || *p != '"') return false;
            JsonValue key; if (!parseStr(key)) return false;
            skip(); if (p >= e || *p != ':') return false; p++; skip();
            JsonValue val; if (!parseValue(val)) return false;
            v.obj.push_back({ key.sv, val });
            skip();
            if (p < e && *p == ',') { p++; continue; }
            if (p < e && *p == '}') { p++; return true; }
            return false;
        }
        return false;
    }
    bool parseArr(JsonValue& v) {
        v.type = JsonValue::ARR; v.arr.clear(); p++; skip();
        if (p < e && *p == ']') { p++; return true; }
        while (p < e) {
            JsonValue val; if (!parseValue(val)) return false;
            v.arr.push_back(val);
            skip();
            if (p < e && *p == ',') { p++; continue; }
            if (p < e && *p == ']') { p++; return true; }
            return false;
        }
        return false;
    }
    bool parseStr(JsonValue& v) {
        v.type = JsonValue::STR; v.sv.clear(); p++; // 跳过 "
        while (p < e && *p != '"') {
            if (*p == '\\' && p + 1 < e) {
                char c = *(p + 1);
                switch (c) {
                    case 'n': v.sv.push_back('\n'); break;
                    case 't': v.sv.push_back('\t'); break;
                    case 'r': v.sv.push_back('\r'); break;
                    case '"': v.sv.push_back('"'); break;
                    case '\\': v.sv.push_back('\\'); break;
                    case '/': v.sv.push_back('/'); break;
                    default: v.sv.push_back(c); break;
                }
                p += 2;
            } else { v.sv.push_back(*p++); }
        }
        if (p < e) p++; // 跳过 "
        return true;
    }
    bool parseBool(JsonValue& v) {
        if (e - p >= 4 && strncmp(p, "true", 4) == 0) { v.type = JsonValue::INT; v.iv = 1; p += 4; return true; }
        if (e - p >= 5 && strncmp(p, "false", 5) == 0) { v.type = JsonValue::INT; v.iv = 0; p += 5; return true; }
        return false;
    }
    bool parseNull(JsonValue& v) {
        if (e - p >= 4 && strncmp(p, "null", 4) == 0) { v.type = JsonValue::NUL; p += 4; return true; }
        return false;
    }
    bool parseNum(JsonValue& v) {
        const char* s = p;
        bool isDbl = false;
        if (p < e && (*p == '-' || *p == '+')) p++;
        while (p < e && ((*p >= '0' && *p <= '9') || *p == '.' || *p == 'e' || *p == 'E' || *p == '+' || *p == '-')) {
            if (*p == '.' || *p == 'e' || *p == 'E') isDbl = true; p++;
        }
        std::string num(s, p - s);
        if (isDbl) { v.type = JsonValue::DBL; v.dv = atof(num.c_str()); }
        else { v.type = JsonValue::INT; v.iv = strtoll(num.c_str(), nullptr, 10); }
        return true;
    }
};
static bool ParseJsonConfig(std::string& s, JsonValue& out) {
    JsonParser jp; return jp.parse(s, out);
}

// =============================================================================
// 2. 配置容器（替代 CConfigManagerSys::m_jsoncfgobjmgr）
// =============================================================================
#define MAKEDEAL_CONFIG _T("makedeal.json")
struct CConfigManagerSys {
    static std::map<std::string, JsonValue>& m_jsoncfgobjmgr() {
        static std::map<std::string, JsonValue> g; return g;
    }
};
// 代码里写法是 CConfigManagerSys::m_jsoncfgobjmgr[...] —— 用宏把成员访问映射到上面的函数
#define CFG_MGR (CConfigManagerSys::m_jsoncfgobjmgr())

// =============================================================================
// 3. 常量 / 枚举 / 结构体
// =============================================================================
#define TOTAL_CARDS      54
#define CARDS_PER_CHAIR  17
#define BOTTOM_CARD      3
#define TOTAL_CHAIRS     3
#define SK_LAYOUT_NUM    16
#define SK_LAYOUT_MOD    13
#define HandCardMaxLen   20
#define MinCardsValue    (-999)

enum CardGroupType {
    cgERROR = -1, cgZERO = 0, cgSINGLE = 1, cgDOUBLE = 2, cgTHREE = 3,
    cgSINGLE_LINE = 4, cgDOUBLE_LINE = 5, cgTHREE_LINE = 6,
    cgTHREE_TAKE_ONE = 7, cgTHREE_TAKE_TWO = 8, cgTHREE_TAKE_ONE_LINE = 9,
    cgTHREE_TAKE_TWO_LINE = 10, cgFOUR_TAKE_ONE = 11, cgFOUR_TAKE_TWO = 12,
    cgBOMB_CARD = 13, cgKING_CARD = 14
};

struct CardGroupData {
    CardGroupType cgType = cgERROR;
    int nValue = 0;
    int nCount = 0;
    int nMaxCard = 0;
};
struct HandCardValue { int SumValue = 0; int NeedRound = 0; };
struct ComposeCardResult {
    bool bRet = false;
    std::string ComposeCardGroupType = "error";
    int ComposeCardGroupCardCount = -1;
    int nRemoveCardID = -1;
};
typedef struct _tagMakeDealCfg {
    int nMakeDealType = 0;
    int nBeginMakeNum = 0;
    int nBeginSelectBanker = 0;
    int nFirstChairHandCount = -1;
    int nFirstChairBombCount = -1;
    int nFirstChairBigCardsCount = -1;
    int nOtherChairHandCount = -1;
    int nOtherChairBombCount = -1;
    int nOtherChairBigCardsCount = -1;
    int nTargetValue = 0;
    int nTargetRound = 0;
    std::vector<int> arrCouPaiStrategy;
    int nReserved[4] = {0,0,0,0};
} MAKEDEALCFG, *LPMAKEDEALCFG;

#define USER_TYPE_REAL  0
#define USER_TYPE_ROBOT 1

// =============================================================================
// 4. 牌 id <-> 牌值  （MakeDealHelper.cpp:1479 / :1552 / :1563）
// =============================================================================
static int GetValuebyCardid(int cardid) {            // Type1 牌值 3-17
    int ret = 0;
    if (cardid < 52) { int tmp = cardid % SK_LAYOUT_MOD; ret = (tmp == 0 ? 15 : tmp + 2); }
    else if (cardid == 52) ret = 16;                 // 小王
    else if (cardid == 53) ret = 17;                 // 大王
    return ret;
}
static int SK_GetCardIndex(int nCardID) {            // Type0 layout 1-15
    nCardID = nCardID % 54;
    if (nCardID == 52) return 14;
    if (nCardID == 53) return 15;
    return nCardID % SK_LAYOUT_MOD + 1;
}
static int SK_GetCardShape(int nCardID) { (void)nCardID; return 0; }
static inline int SK_GetCardIndexEx(int nCardID, int = 0) { return SK_GetCardIndex(nCardID); }

// =============================================================================
// 5. get_GroupData（MakeDealHelper.cpp:85）—— 读 JSON GroupDataExp
// =============================================================================
static CardGroupData get_GroupData(CardGroupType cgType, int MaxCard, int Count) {
    CardGroupData uct;
    uct.cgType = cgType;
    uct.nCount = Count;
    uct.nMaxCard = MaxCard;
    char szType[32] = {0};
    sprintf(szType, "%d", cgType);
    int nTmpValue = 0, nXianValue = 0, M = 0, D = 0;
    double C = 0;
    const JsonValue& exp = CFG_MGR[MAKEDEAL_CONFIG]["MakeDealCommonArgs"]["GroupDataExp"][szType];
    if (exp.isNull()) {
        uct.nValue = 0;
    } else {
        for (size_t i = 0; i < exp.size(); i++) {
            C = exp[(int)i]["C"].asDouble();
            if (exp[(int)i]["D"].isNull()) {
                M = exp[(int)i]["M"].asInt();
                nXianValue = (int)(C * pow((double)MaxCard, M));     // 幂函数
            } else {
                D = exp[(int)i]["D"].asInt();
                nXianValue = (int)(C * (log((double)MaxCard) / log((double)D))); // 指数函数
            }
            nTmpValue += nXianValue;
        }
        uct.nValue = nTmpValue;
    }
    return uct;
}
static const char* get_GroupCardName(int GroupCardType) {
    switch (GroupCardType) {
        case cgERROR: return "错误"; case cgZERO: return "不出"; case cgSINGLE: return "单牌";
        case cgDOUBLE: return "对牌"; case cgTHREE: return "三条"; case cgSINGLE_LINE: return "单连";
        case cgDOUBLE_LINE: return "对连"; case cgTHREE_LINE: return "三连"; case cgTHREE_TAKE_ONE: return "三带一";
        case cgTHREE_TAKE_TWO: return "三带二"; case cgBOMB_CARD: return "炸弹"; case cgKING_CARD: return "王炸";
        default: return "?";
    }
}

// =============================================================================
// 6. HandCardInfo + SpliteCard 系（MakeDealHelper.cpp:10-727）逐字
// =============================================================================
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

static HandCardValue get_MaxHandCardValue(HandCardInfo& cls); // fwd
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
static HandCardValue get_MaxHandCardValue(HandCardInfo& cls) {
    cls.ClearPutCardList();
    HandCardValue uct;
    if (cls.nHandCardCount == 0) { uct.SumValue = 0; uct.NeedRound = 0; return uct; }
    CardGroupData scd = SurCardsType(cls.value_aHandCardList);
    if (scd.cgType != cgERROR && scd.cgType != cgFOUR_TAKE_ONE && scd.cgType != cgFOUR_TAKE_TWO) {
        uct.SumValue = scd.nValue; uct.NeedRound = 1; return uct;
    }
    // 取一个最优牌型后递归
    extern void GetBestCardType(HandCardInfo&);
    GetBestCardType(cls);
    CardGroupData NowPutCardType = cls.uctPutCardType;
    std::vector<int> NowPutCardList = cls.value_nPutCardList;
    for (size_t it = 0; it < NowPutCardList.size(); it++) cls.value_aHandCardList[NowPutCardList[it]]--;
    cls.nHandCardCount -= NowPutCardType.nCount;
    HandCardValue tmp = get_MaxHandCardValue(cls);
    for (size_t it = 0; it < NowPutCardList.size(); it++) cls.value_aHandCardList[NowPutCardList[it]]++;
    cls.nHandCardCount += NowPutCardType.nCount;
    uct.SumValue = NowPutCardType.nValue + tmp.SumValue;
    uct.NeedRound = tmp.NeedRound + 1;
    return uct;
}
static inline long long _score(const HandCardValue& hv) { return (long long)hv.SumValue - (long long)hv.NeedRound * 7; }
void GetBestCardType(HandCardInfo& cls) {
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
    for (int i = 3; i < 16; i++) {
        if (L[i] != 0 && L[i] != 4) {
            if (L[i] == 1) { L[i]--; cls.nHandCardCount--; HandCardValue tmp = get_MaxHandCardValue(cls); L[i]++; cls.nHandCardCount++;
                if (_score(Best) <= _score(tmp)) { Best = tmp; BestGroup = get_GroupData(cgSINGLE, i, 1); } }
            if (L[i] == 2) { L[i]-=2; cls.nHandCardCount-=2; HandCardValue tmp = get_MaxHandCardValue(cls); L[i]+=2; cls.nHandCardCount+=2;
                if (_score(Best) <= _score(tmp)) { Best = tmp; BestGroup = get_GroupData(cgDOUBLE, i, 2); } }
            if (L[i] == 3) { L[i]-=3; cls.nHandCardCount-=3; HandCardValue tmp = get_MaxHandCardValue(cls); L[i]+=3; cls.nHandCardCount+=3;
                if (_score(Best) <= _score(tmp)) { Best = tmp; BestGroup = get_GroupData(cgTHREE, i, 3); } }
            if (L[i] > 0) { int prov = 0;
                for (int j = i; j < 15; j++) { if (L[j] > 0) prov++; else break;
                    if (prov >= 5) { for (int k = i; k <= j; k++) L[k]--; cls.nHandCardCount -= prov;
                        HandCardValue tmp = get_MaxHandCardValue(cls); for (int k = i; k <= j; k++) L[k]++; cls.nHandCardCount += prov;
                        if (_score(Best) <= _score(tmp)) { Best = tmp; BestGroup = get_GroupData(cgSINGLE_LINE, j, prov); } } } }
            if (L[i] > 1) { int prov = 0;
                for (int j = i; j < 15; j++) { if (L[j] > 1) prov++; else break;
                    if (prov >= 3) { for (int k = i; k <= j; k++) L[k]-=2; cls.nHandCardCount -= prov*2;
                        HandCardValue tmp = get_MaxHandCardValue(cls); for (int k = i; k <= j; k++) L[k]+=2; cls.nHandCardCount += prov*2;
                        if (_score(Best) <= _score(tmp)) { Best = tmp; BestGroup = get_GroupData(cgDOUBLE_LINE, j, prov*2); } } } }
            if (L[i] > 2) { int prov = 0;
                for (int j = i; j < 15; j++) { if (L[j] > 2) prov++; else break;
                    if (prov >= 2) { for (int k = i; k <= j; k++) L[k]-=3; cls.nHandCardCount -= prov*3;
                        HandCardValue tmp = get_MaxHandCardValue(cls); for (int k = i; k <= j; k++) L[k]+=3; cls.nHandCardCount += prov*3;
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
    cls.uctPutCardType = get_GroupData(cgERROR, 0, 0);
}
void SpliteCard(std::vector<int> arrHandCardList, std::vector<CardGroupData>& cardTypeArr) {
    HandCardInfo cls; cls.Init(arrHandCardList);
    if (cls.nHandCardCount <= 0) return;
    cardTypeArr.clear();
    while (1) {
        GetBestCardType(cls);
        cardTypeArr.push_back(cls.uctPutCardType);
        for (size_t it = 0; it < cls.value_nPutCardList.size(); it++) cls.value_aHandCardList[cls.value_nPutCardList[it]]--;
        cls.nHandCardCount -= cls.uctPutCardType.nCount;
        if (cls.nHandCardCount == 0) break;
    }
}
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

// =============================================================================
// 7. Type1 拼牌 MakeDeal_ComposeCard（MakeDealHelper.cpp:730-1475）逐字
// =============================================================================
static int MakeDeal_RemainCardsHaveCard(std::vector<int>& RemainCards, int nCardValue) {
    int nTargetCardID = -1, nCardidBase = 0;
    if (nCardValue == 17) nCardidBase = 53;
    else if (nCardValue == 16) nCardidBase = 52;
    else if (nCardValue == 15) nCardidBase = 0;
    else nCardidBase = nCardValue - 2;
    for (std::vector<int>::iterator iter = RemainCards.begin(); iter != RemainCards.end(); iter++) {
        if (nCardidBase >= 52 && *iter == nCardidBase) { nTargetCardID = nCardidBase; RemainCards.erase(iter); break; }
        else if (nCardidBase <= 12 && nCardidBase >= 0) {
            for (int i = nCardidBase; i <= 51; i += 13) { if (*iter == i) { nTargetCardID = i; RemainCards.erase(iter); break; } }
            if (nTargetCardID != -1) break;
        }
    }
    return nTargetCardID;
}
ComposeCardResult MakeDeal_ComposeCard(LPMAKEDEALCFG pCfg, std::vector<CardGroupData>& CardGroupDatas,
                                       std::vector<int>& RemainCards, bool bNeedMakeDeal) {
    ComposeCardResult stRet; stRet.bRet = false; stRet.ComposeCardGroupType = "error";
    stRet.ComposeCardGroupCardCount = -1; stRet.nRemoveCardID = -1;

    int value_aMaxCardList[18] = {0};
    for (size_t j = 0; j < CardGroupDatas.size(); j++) value_aMaxCardList[CardGroupDatas[j].nMaxCard] = CardGroupDatas[j].nCount;
    int nTmpLackCard = GetValuebyCardid(RemainCards[0]);
    for (size_t k = 0; k < RemainCards.size(); k++) { int nTmpMaxValue = GetValuebyCardid(RemainCards[k]); if (value_aMaxCardList[nTmpMaxValue] == 0) { nTmpLackCard = nTmpMaxValue; break; } }

    if (bNeedMakeDeal == false) {
        for (size_t i = 0; i < CardGroupDatas.size(); i++) {
            if (CardGroupDatas[i].cgType == cgSINGLE) {
                int nTargetCardID = MakeDeal_RemainCardsHaveCard(RemainCards, CardGroupDatas[i].nMaxCard);
                if (nTargetCardID != -1) { CardGroupDatas[i] = get_GroupData(cgDOUBLE, CardGroupDatas[i].nMaxCard, 2); stRet.bRet = true; stRet.ComposeCardGroupType = get_GroupCardName(cgDOUBLE); stRet.ComposeCardGroupCardCount = 2; stRet.nRemoveCardID = nTargetCardID; return stRet; }
            } else if (CardGroupDatas[i].cgType == cgDOUBLE) {
                int nTargetCardID = MakeDeal_RemainCardsHaveCard(RemainCards, CardGroupDatas[i].nMaxCard);
                if (nTargetCardID != -1) { CardGroupDatas[i] = get_GroupData(cgTHREE, CardGroupDatas[i].nMaxCard, 3); stRet.bRet = true; stRet.ComposeCardGroupType = get_GroupCardName(cgTHREE); stRet.ComposeCardGroupCardCount = 3; stRet.nRemoveCardID = nTargetCardID; return stRet; }
            }
        }
        int nTargetCardID = MakeDeal_RemainCardsHaveCard(RemainCards, nTmpLackCard);
        if (nTargetCardID != -1) { CardGroupDatas.push_back(get_GroupData(cgSINGLE, nTmpLackCard, 1)); stRet.bRet = true; stRet.ComposeCardGroupType = get_GroupCardName(cgSINGLE); stRet.ComposeCardGroupCardCount = 1; stRet.nRemoveCardID = nTargetCardID; return stRet; }
    }

    for (size_t i = 0; i < pCfg->arrCouPaiStrategy.size(); i++) {
        int nTargetCardType = pCfg->arrCouPaiStrategy[i];
        if (nTargetCardType == cgBOMB_CARD) {
            for (size_t i2 = 0; i2 < CardGroupDatas.size(); i2++) {
                if (CardGroupDatas[i2].cgType == cgTHREE) {
                    int nTargetCardID = MakeDeal_RemainCardsHaveCard(RemainCards, CardGroupDatas[i2].nMaxCard);
                    if (nTargetCardID != -1) { CardGroupDatas[i2] = get_GroupData(cgBOMB_CARD, CardGroupDatas[i2].nMaxCard, 4); stRet.bRet = true; stRet.ComposeCardGroupType = get_GroupCardName(cgBOMB_CARD); stRet.ComposeCardGroupCardCount = 4; stRet.nRemoveCardID = nTargetCardID; return stRet; }
                }
            }
        } else if (nTargetCardType == cgTHREE_LINE) {
            for (size_t i2 = 0; i2 < CardGroupDatas.size(); i2++) {
                if (CardGroupDatas[i2].cgType == cgTHREE && CardGroupDatas[i2].nMaxCard != 15) {
                    for (size_t j = 0; j < CardGroupDatas.size(); j++) {
                        if (CardGroupDatas[j].cgType == cgDOUBLE && CardGroupDatas[j].nMaxCard <= 14 && CardGroupDatas[j].nMaxCard >= 3) {
                            if (CardGroupDatas[i2].nMaxCard - 1 == CardGroupDatas[j].nMaxCard) {
                                int nTargetCardID = MakeDeal_RemainCardsHaveCard(RemainCards, CardGroupDatas[j].nMaxCard);
                                if (nTargetCardID != -1) {
                                    std::vector<int> deletelist; int prov = 0;
                                    for (size_t q = CardGroupDatas[i2].nMaxCard + 1; q <= 14; q++) {
                                        bool bHasSuitSingle = false;
                                        for (std::vector<CardGroupData>::iterator iter = CardGroupDatas.begin(); iter != CardGroupDatas.end(); iter++) {
                                            if (iter->cgType == cgTHREE && iter->nMaxCard == (int)q) { prov++; deletelist.push_back((int)(iter - CardGroupDatas.begin())); bHasSuitSingle = true; break; }
                                        }
                                        if (!bHasSuitSingle) break;
                                    }
                                    CardGroupDatas[i2] = get_GroupData(cgTHREE_LINE, CardGroupDatas[i2].nMaxCard + prov, 6 + prov * 3);
                                    deletelist.push_back((int)j); sort(deletelist.begin(), deletelist.end());
                                    for (int ii = (int)deletelist.size() - 1; ii >= 0; --ii) CardGroupDatas.erase(CardGroupDatas.begin() + deletelist[ii]);
                                    stRet.bRet = true; stRet.ComposeCardGroupType = get_GroupCardName(cgTHREE_LINE); stRet.ComposeCardGroupCardCount = 6 + 3 * prov; stRet.nRemoveCardID = nTargetCardID; return stRet;
                                }
                            } else if (CardGroupDatas[i2].nMaxCard + 1 == CardGroupDatas[j].nMaxCard) {
                                int nTargetCardID = MakeDeal_RemainCardsHaveCard(RemainCards, CardGroupDatas[j].nMaxCard);
                                if (nTargetCardID != -1) {
                                    std::vector<int> deletelist; int prov = 0;
                                    for (size_t q = CardGroupDatas[j].nMaxCard + 1; q <= 14; q++) {
                                        bool bHasSuitSingle = false;
                                        for (std::vector<CardGroupData>::iterator iter = CardGroupDatas.begin(); iter != CardGroupDatas.end(); iter++) {
                                            if (iter->cgType == cgTHREE && iter->nMaxCard == (int)q) { prov++; deletelist.push_back((int)(iter - CardGroupDatas.begin())); bHasSuitSingle = true; break; }
                                        }
                                        if (!bHasSuitSingle) break;
                                    }
                                    CardGroupDatas[i2] = get_GroupData(cgTHREE_LINE, CardGroupDatas[j].nMaxCard + prov, 6 + 3 * prov);
                                    deletelist.push_back((int)j); sort(deletelist.begin(), deletelist.end());
                                    for (int ii = (int)deletelist.size() - 1; ii >= 0; --ii) CardGroupDatas.erase(CardGroupDatas.begin() + deletelist[ii]);
                                    stRet.bRet = true; stRet.ComposeCardGroupType = get_GroupCardName(cgTHREE_LINE); stRet.ComposeCardGroupCardCount = 6 + 3 * prov; stRet.nRemoveCardID = nTargetCardID; return stRet;
                                }
                            }
                        }
                    }
                }
            }
        } else if (nTargetCardType == cgSINGLE_LINE) {
            std::map<int,int> tmpMaxValueCountMap, tmpMaxValueVectorIndexMap;
            for (size_t i2 = 0; i2 < CardGroupDatas.size(); i2++) {
                if (CardGroupDatas[i2].cgType == cgSINGLE) tmpMaxValueCountMap[CardGroupDatas[i2].nMaxCard] = 1;
                else if (CardGroupDatas[i2].cgType == cgDOUBLE) tmpMaxValueCountMap[CardGroupDatas[i2].nMaxCard] = 2;
                tmpMaxValueVectorIndexMap[CardGroupDatas[i2].nMaxCard] = (int)i2;
            }
            for (size_t j = 3; j <= 10; j++) {
                int nCtDoubleCount = 0, nLostCardCount = 0, nNeedCard = -1, nNeedChaiCard = -1;
                for (size_t k = j; k <= j + 4; k++) {
                    if (tmpMaxValueCountMap[(int)k] == 0) { nLostCardCount++; nNeedCard = (int)k; }
                    if (tmpMaxValueCountMap[(int)k] == 2) { nCtDoubleCount++; nNeedChaiCard = (int)k; }
                }
                if (nLostCardCount == 1 && nCtDoubleCount <= 1 && nNeedCard != -1) {
                    int nTargetCardID = MakeDeal_RemainCardsHaveCard(RemainCards, nNeedCard);
                    if (nTargetCardID != -1) {
                        std::vector<int> tmpSort;
                        for (size_t k = 0; k <= 4; k++) if ((int)(j + k) != nNeedCard) tmpSort.push_back(tmpMaxValueVectorIndexMap[(int)(j + k)]);
                        sort(tmpSort.begin(), tmpSort.end());
                        for (size_t p = 0; p < tmpSort.size(); p++) CardGroupDatas.erase(CardGroupDatas.begin() + tmpSort[3 - p]);
                        int prov = 4;
                        for (size_t q = j + 5; q <= 14; q++) {
                            bool bHasSuitSingle = false;
                            for (std::vector<CardGroupData>::iterator iter = CardGroupDatas.begin(); iter != CardGroupDatas.end(); iter++) {
                                if (iter->cgType == cgSINGLE && iter->nMaxCard == (int)q) { prov++; CardGroupDatas.erase(iter); bHasSuitSingle = true; break; }
                            }
                            if (!bHasSuitSingle) break;
                        }
                        CardGroupDatas.push_back(get_GroupData(cgSINGLE_LINE, (int)j + prov, prov + 1));
                        if (nCtDoubleCount == 1) CardGroupDatas.push_back(get_GroupData(cgSINGLE, nNeedChaiCard, 1));
                        stRet.bRet = true; stRet.ComposeCardGroupType = get_GroupCardName(cgSINGLE_LINE); stRet.ComposeCardGroupCardCount = prov + 1; stRet.nRemoveCardID = nTargetCardID; return stRet;
                    }
                }
            }
        } else if (nTargetCardType == cgDOUBLE_LINE) {
            std::map<int,int> tmpMaxValueMap;
            for (size_t c = 0; c < CardGroupDatas.size(); c++) if (CardGroupDatas[c].cgType == cgDOUBLE) tmpMaxValueMap[CardGroupDatas[c].nMaxCard] = (int)c;
            for (size_t i2 = 0; i2 < CardGroupDatas.size(); i2++) {
                if (CardGroupDatas[i2].cgType == cgSINGLE && CardGroupDatas[i2].nMaxCard != 15) {
                    int mc = CardGroupDatas[i2].nMaxCard;
#define DLINE_EXTEND(qstart) \
    int prov = 0; std::vector<int> deletelist; \
    for (size_t q = (qstart); q <= 14; q++) { bool bHas=false; \
        for (auto iter = CardGroupDatas.begin(); iter != CardGroupDatas.end(); iter++) \
            if (iter->cgType==cgDOUBLE && iter->nMaxCard==(int)q){ prov++; deletelist.push_back((int)(iter-CardGroupDatas.begin())); bHas=true; break; } \
        if(!bHas) break; }
#define DLINE_FINISH(maxc,cnt) \
    deletelist.push_back(0); sort(deletelist.begin(), deletelist.end()); \
    for (int ii=(int)deletelist.size()-1; ii>=0; --ii) CardGroupDatas.erase(CardGroupDatas.begin()+deletelist[ii]); \
    stRet.bRet=true; stRet.ComposeCardGroupType=get_GroupCardName(cgDOUBLE_LINE); stRet.ComposeCardGroupCardCount=(cnt); stRet.nRemoveCardID=nTargetCardID; return stRet;
                    if (mc == 3) {
                        if (tmpMaxValueMap.count(4) && tmpMaxValueMap.count(5)) {
                            int nTargetCardID = MakeDeal_RemainCardsHaveCard(RemainCards, 3);
                            if (nTargetCardID != -1) { DLINE_EXTEND(6); CardGroupDatas[i2]=get_GroupData(cgDOUBLE_LINE,5+prov,6+prov*2);
                                deletelist.push_back(tmpMaxValueMap[4]); deletelist.push_back(tmpMaxValueMap[5]);
                                sort(deletelist.begin(),deletelist.end()); for(int ii=(int)deletelist.size()-1;ii>=0;--ii) CardGroupDatas.erase(CardGroupDatas.begin()+deletelist[ii]);
                                stRet.bRet=true; stRet.ComposeCardGroupType=get_GroupCardName(cgDOUBLE_LINE); stRet.ComposeCardGroupCardCount=6+prov*2; stRet.nRemoveCardID=nTargetCardID; return stRet; }
                        }
                    } else if (mc == 4) {
                        if (tmpMaxValueMap.count(3) && tmpMaxValueMap.count(5)) {
                            int nTargetCardID = MakeDeal_RemainCardsHaveCard(RemainCards, 4);
                            if (nTargetCardID != -1) { DLINE_EXTEND(6); CardGroupDatas[i2]=get_GroupData(cgDOUBLE_LINE,5+prov,6+prov*2);
                                deletelist.push_back(tmpMaxValueMap[3]); deletelist.push_back(tmpMaxValueMap[5]);
                                sort(deletelist.begin(),deletelist.end()); for(int ii=(int)deletelist.size()-1;ii>=0;--ii) CardGroupDatas.erase(CardGroupDatas.begin()+deletelist[ii]);
                                stRet.bRet=true; stRet.ComposeCardGroupType=get_GroupCardName(cgDOUBLE_LINE); stRet.ComposeCardGroupCardCount=6+prov*2; stRet.nRemoveCardID=nTargetCardID; return stRet; }
                        }
                        if (tmpMaxValueMap.count(5) && tmpMaxValueMap.count(6)) {
                            int nTargetCardID = MakeDeal_RemainCardsHaveCard(RemainCards, 4);
                            if (nTargetCardID != -1) { DLINE_EXTEND(7); CardGroupDatas[i2]=get_GroupData(cgDOUBLE_LINE,6+prov,6*prov*2);  /* C++ 原笔误，保留 */
                                deletelist.push_back(tmpMaxValueMap[5]); deletelist.push_back(tmpMaxValueMap[6]);
                                sort(deletelist.begin(),deletelist.end()); for(int ii=(int)deletelist.size()-1;ii>=0;--ii) CardGroupDatas.erase(CardGroupDatas.begin()+deletelist[ii]);
                                stRet.bRet=true; stRet.ComposeCardGroupType=get_GroupCardName(cgDOUBLE_LINE); stRet.ComposeCardGroupCardCount=6*prov*2; stRet.nRemoveCardID=nTargetCardID; return stRet; }
                        }
                    } else if (mc == 13) {
                        if (tmpMaxValueMap.count(12) && tmpMaxValueMap.count(14)) {
                            int nTargetCardID = MakeDeal_RemainCardsHaveCard(RemainCards, 13);
                            if (nTargetCardID != -1) { CardGroupDatas[i2]=get_GroupData(cgDOUBLE_LINE,14,6);
                                CardGroupDatas.erase(CardGroupDatas.begin()+std::max(tmpMaxValueMap[12],tmpMaxValueMap[14]));
                                CardGroupDatas.erase(CardGroupDatas.begin()+std::min(tmpMaxValueMap[12],tmpMaxValueMap[14]));
                                stRet.bRet=true; stRet.ComposeCardGroupType=get_GroupCardName(cgDOUBLE_LINE); stRet.ComposeCardGroupCardCount=6; stRet.nRemoveCardID=nTargetCardID; return stRet; }
                        }
                        if (tmpMaxValueMap.count(11) && tmpMaxValueMap.count(12)) {
                            int nTargetCardID = MakeDeal_RemainCardsHaveCard(RemainCards, 13);
                            if (nTargetCardID != -1) { DLINE_EXTEND(14); CardGroupDatas[i2]=get_GroupData(cgDOUBLE_LINE,13+prov,6+prov*2);
                                deletelist.push_back(tmpMaxValueMap[11]); deletelist.push_back(tmpMaxValueMap[12]);
                                sort(deletelist.begin(),deletelist.end()); for(int ii=(int)deletelist.size()-1;ii>=0;--ii) CardGroupDatas.erase(CardGroupDatas.begin()+deletelist[ii]);
                                stRet.bRet=true; stRet.ComposeCardGroupType=get_GroupCardName(cgDOUBLE_LINE); stRet.ComposeCardGroupCardCount=6+prov*2; stRet.nRemoveCardID=nTargetCardID; return stRet; }
                        }
                    } else if (mc == 14) {
                        if (tmpMaxValueMap.count(12) && tmpMaxValueMap.count(13)) {
                            int nTargetCardID = MakeDeal_RemainCardsHaveCard(RemainCards, 14);
                            if (nTargetCardID != -1) { CardGroupDatas[i2]=get_GroupData(cgDOUBLE_LINE,14,6);
                                CardGroupDatas.erase(CardGroupDatas.begin()+std::max(tmpMaxValueMap[12],tmpMaxValueMap[13]));
                                CardGroupDatas.erase(CardGroupDatas.begin()+std::min(tmpMaxValueMap[12],tmpMaxValueMap[13]));
                                stRet.bRet=true; stRet.ComposeCardGroupType=get_GroupCardName(cgDOUBLE_LINE); stRet.ComposeCardGroupCardCount=6; stRet.nRemoveCardID=nTargetCardID; return stRet; }
                        }
                    } else {
                        if (tmpMaxValueMap.count(mc-1) && tmpMaxValueMap.count(mc-2)) {
                            int nTargetCardID = MakeDeal_RemainCardsHaveCard(RemainCards, mc);
                            if (nTargetCardID != -1) { DLINE_EXTEND(mc+1); CardGroupDatas[i2]=get_GroupData(cgDOUBLE_LINE,mc+prov,6+prov*2);
                                deletelist.push_back(tmpMaxValueMap[mc-1]); deletelist.push_back(tmpMaxValueMap[mc-2]);
                                sort(deletelist.begin(),deletelist.end()); for(int ii=(int)deletelist.size()-1;ii>=0;--ii) CardGroupDatas.erase(CardGroupDatas.begin()+deletelist[ii]);
                                stRet.bRet=true; stRet.ComposeCardGroupType=get_GroupCardName(cgDOUBLE_LINE); stRet.ComposeCardGroupCardCount=6+prov*2; stRet.nRemoveCardID=nTargetCardID; return stRet; }
                        }
                        if (tmpMaxValueMap.count(mc+1) && tmpMaxValueMap.count(mc+2)) {
                            int nTargetCardID = MakeDeal_RemainCardsHaveCard(RemainCards, mc);
                            if (nTargetCardID != -1) { DLINE_EXTEND(mc+2+1); CardGroupDatas[i2]=get_GroupData(cgDOUBLE_LINE,mc+2+prov,6+prov*2);
                                deletelist.push_back(tmpMaxValueMap[mc+1]); deletelist.push_back(tmpMaxValueMap[mc+2]);
                                sort(deletelist.begin(),deletelist.end()); for(int ii=(int)deletelist.size()-1;ii>=0;--ii) CardGroupDatas.erase(CardGroupDatas.begin()+deletelist[ii]);
                                stRet.bRet=true; stRet.ComposeCardGroupType=get_GroupCardName(cgDOUBLE_LINE); stRet.ComposeCardGroupCardCount=6+prov*2; stRet.nRemoveCardID=nTargetCardID; return stRet; }
                        }
                        if (tmpMaxValueMap.count(mc-1) && tmpMaxValueMap.count(mc+1)) {
                            int nTargetCardID = MakeDeal_RemainCardsHaveCard(RemainCards, mc);
                            if (nTargetCardID != -1) { DLINE_EXTEND(mc+1+1); CardGroupDatas[i2]=get_GroupData(cgDOUBLE_LINE,mc+1+prov,6+prov*2);
                                deletelist.push_back(tmpMaxValueMap[mc-1]); deletelist.push_back(tmpMaxValueMap[mc+1]);
                                sort(deletelist.begin(),deletelist.end()); for(int ii=(int)deletelist.size()-1;ii>=0;--ii) CardGroupDatas.erase(CardGroupDatas.begin()+deletelist[ii]);
                                stRet.bRet=true; stRet.ComposeCardGroupType=get_GroupCardName(cgDOUBLE_LINE); stRet.ComposeCardGroupCardCount=6+prov*2; stRet.nRemoveCardID=nTargetCardID; return stRet; }
                        }
                    }
#undef DLINE_EXTEND
#undef DLINE_FINISH
                }
            }
        } else if (nTargetCardType == cgTHREE) {
            for (size_t i2 = 0; i2 < CardGroupDatas.size(); i2++) {
                if (CardGroupDatas[i2].cgType == cgDOUBLE) {
                    int nTargetCardID = MakeDeal_RemainCardsHaveCard(RemainCards, CardGroupDatas[i2].nMaxCard);
                    if (nTargetCardID != -1) { CardGroupDatas[i2] = get_GroupData(cgTHREE, CardGroupDatas[i2].nMaxCard, 3); stRet.bRet = true; stRet.ComposeCardGroupType = get_GroupCardName(cgTHREE); stRet.ComposeCardGroupCardCount = 3; stRet.nRemoveCardID = nTargetCardID; return stRet; }
                }
            }
        } else if (nTargetCardType == cgDOUBLE) {
            for (size_t i2 = 0; i2 < CardGroupDatas.size(); i2++) {
                if (CardGroupDatas[i2].cgType == cgSINGLE) {
                    int nTargetCardID = MakeDeal_RemainCardsHaveCard(RemainCards, CardGroupDatas[i2].nMaxCard);
                    if (nTargetCardID != -1) { CardGroupDatas[i2] = get_GroupData(cgDOUBLE, CardGroupDatas[i2].nMaxCard, 2); stRet.bRet = true; stRet.ComposeCardGroupType = get_GroupCardName(cgDOUBLE); stRet.ComposeCardGroupCardCount = 2; stRet.nRemoveCardID = nTargetCardID; return stRet; }
                }
            }
        }
    }
    if (value_aMaxCardList[16] == 0 && value_aMaxCardList[17] == 0) {
        int nTargetCardID = MakeDeal_RemainCardsHaveCard(RemainCards, 16);
        if (nTargetCardID != -1) { CardGroupDatas.push_back(get_GroupData(cgSINGLE, 16, 1)); stRet.bRet = true; stRet.ComposeCardGroupType = get_GroupCardName(cgSINGLE); stRet.ComposeCardGroupCardCount = 1; stRet.nRemoveCardID = nTargetCardID; return stRet; }
        nTargetCardID = MakeDeal_RemainCardsHaveCard(RemainCards, 17);
        if (nTargetCardID != -1) { CardGroupDatas.push_back(get_GroupData(cgSINGLE, 17, 1)); stRet.bRet = true; stRet.ComposeCardGroupType = get_GroupCardName(cgSINGLE); stRet.ComposeCardGroupCardCount = 1; stRet.nRemoveCardID = nTargetCardID; return stRet; }
    }
    int nTargetCardID = MakeDeal_RemainCardsHaveCard(RemainCards, nTmpLackCard);
    if (nTargetCardID != -1) { CardGroupDatas.push_back(get_GroupData(cgSINGLE, nTmpLackCard, 1)); stRet.bRet = true; stRet.ComposeCardGroupType = get_GroupCardName(cgSINGLE); stRet.ComposeCardGroupCardCount = 1; stRet.nRemoveCardID = nTargetCardID; return stRet; }
    return stRet;
}

// =============================================================================
// 8. Type0 手数/炸弹统计 + 匹配器（zgdatbl.h:605-1016）逐字
// =============================================================================
static inline void Calc2KingHandCount(int nCardLay[], int& nHandCount, int& nBombCount) {
    int nKingCount = nCardLay[14] + nCardLay[15];
    if (2 == nKingCount) { nBombCount++; nCardLay[14] = 0; nCardLay[15] = 0; }
    nHandCount += 0;
}
static inline void CalcBombHandCount(int nCardLay[], int& nHandCount, int& nBombCount) {
    for (int i = 0; i < SK_LAYOUT_NUM; i++) if (4 == nCardLay[i]) { nBombCount++; nHandCount += -1; nCardLay[i] = 0; }
}
static inline void CalcABTThreeHandCount(int nCardLay[], int& nHandCount, int& nBombCount) {
    (void)nBombCount;
    for (int i = 0; i < SK_LAYOUT_NUM; i++) if (nCardLay[i] >= 3) {
        if (i <= 1 || i >= 13) continue;
        int j = i, nThreeCount = 1;
        while ((++j <= SK_LAYOUT_MOD) && nCardLay[j] >= 3) nThreeCount++;
        if (nThreeCount <= 1) continue;
        nHandCount += 1 - nThreeCount;
        while (nThreeCount) { nCardLay[i + nThreeCount - 1] -= 3; nThreeCount--; }
    }
}
static inline void CalcThreeHandCount(int nCardLay[], int& nHandCount, int& nBombCount) {
    (void)nBombCount;
    for (int i = 0; i < SK_LAYOUT_NUM; i++) if (3 == nCardLay[i]) { nHandCount += 0; nCardLay[i] = 0; }
}
static inline void CalcABTCoupleHandCount(int nCardLay[], int& nHandCount, int& nBombCount) {
    (void)nBombCount;
    for (int i = 0; i < SK_LAYOUT_NUM; i++) if (nCardLay[i] >= 2) {
        if (i <= 1 || i >= 13) continue;
        int j = i, nCoupleCount = 1;
        while ((++j <= SK_LAYOUT_MOD) && nCardLay[j] >= 2) nCoupleCount++;
        if (nCoupleCount <= 2) continue;
        nHandCount += 1;
        while (nCoupleCount) { nCardLay[i + nCoupleCount - 1] -= 2; nCoupleCount--; }
    }
}
static inline void CalcABTHandCount(int nCardLay[], int& nHandCount, int& nBombCount) {
    (void)nBombCount;
    for (int i = 0; i < SK_LAYOUT_NUM; i++) if (nCardLay[i] >= 1) {
        if (i <= 1 || i >= 13) continue;
        int j = i, nSingleCount = 1;
        while ((++j <= SK_LAYOUT_MOD) && nCardLay[j] >= 1) nSingleCount++;
        if (nSingleCount <= 4) continue;
        nHandCount += 1;
        while (nSingleCount) { nCardLay[i + nSingleCount - 1] -= 1; nSingleCount--; }
    }
}
static inline void CalcCoupleHandCount(int nCardLay[], int& nHandCount, int& nBombCount) {
    (void)nBombCount;
    for (int i = 0; i < SK_LAYOUT_NUM; i++) if (2 == nCardLay[i]) { nHandCount += 1; nCardLay[i] = 0; }
}
static inline void CalcSingleHandCount(int nCardLay[], int& nHandCount, int& nBombCount) {
    (void)nBombCount;
    for (int i = 0; i < SK_LAYOUT_NUM; i++) if (1 == nCardLay[i]) { nHandCount += 1; nCardLay[i] = 0; }
}
// 已知 CardLay
static inline void CalcHandCardsCount(int nCardsID[], int nCardsCount, int nCardLay[], int& nHandCount, int& nBombCount, int& nBigCardCount) {
    for (int i = 0; i < nCardsCount; i++) if (nCardsID[i] >= 0 && nCardsID[i] < TOTAL_CARDS) { int index = SK_GetCardIndexEx(nCardsID[i], 0); nCardLay[index]++; }
    int tmp[SK_LAYOUT_NUM]; memcpy(tmp, nCardLay, sizeof(int)*SK_LAYOUT_NUM);
    nBigCardCount = nCardLay[1] + nCardLay[14] + nCardLay[15];
    nHandCount = 0; nBombCount = 0;
    Calc2KingHandCount(tmp, nHandCount, nBombCount); CalcBombHandCount(tmp, nHandCount, nBombCount);
    CalcABTThreeHandCount(tmp, nHandCount, nBombCount); CalcABTCoupleHandCount(tmp, nHandCount, nBombCount);
    CalcABTHandCount(tmp, nHandCount, nBombCount); CalcThreeHandCount(tmp, nHandCount, nBombCount);
    CalcCoupleHandCount(tmp, nHandCount, nBombCount); CalcSingleHandCount(tmp, nHandCount, nBombCount);
}
static inline int MatchABTCardType(int nCardLay[], int nReserveCards[], int nReserveCount) {
    for (int i = 0; i < SK_LAYOUT_NUM; i++) {
        if (i <= 1 || i >= 10) continue;
        int nMatchedCardIndex = 0, nIndexDiff = 0, nDiffCardCount = 0, nDoubleTypeCardIndex1 = 0, nDoubleTypeCardIndex2 = 0;
        for (int j = 0; j < 5; j++) {
            if (1 == nCardLay[i+j] || 2 == nCardLay[i+j] || 3 == nCardLay[i+j]) {
                nIndexDiff++; nDiffCardCount += nCardLay[i+j];
                if (2 == nCardLay[i+j] && 0 == nDoubleTypeCardIndex1) nDoubleTypeCardIndex1 = i+j;
                else if (2 == nCardLay[i+j] && 0 == nDoubleTypeCardIndex2) nDoubleTypeCardIndex2 = i+j;
            } else if (0 == nCardLay[i+j] && 0 == nMatchedCardIndex) nMatchedCardIndex = i+j;
        }
        if (FALSE == (4 == nIndexDiff && 0 != nMatchedCardIndex && (nDiffCardCount == 4 || nDiffCardCount == 5 || nDiffCardCount == 6))) continue;
        if (nDiffCardCount == 6 && nDoubleTypeCardIndex1 != 0 && nDoubleTypeCardIndex2 != 0 && nDoubleTypeCardIndex2 - nDoubleTypeCardIndex1 <= 2) continue;
        for (int j = 0; j < nReserveCount; j++) if (nMatchedCardIndex == SK_GetCardIndexEx(nReserveCards[j], 0)) return nReserveCards[j];
    }
    return -1;
}
static inline int MatchABTCoupleCardType(int nCardLay[], int nReserveCards[], int nReserveCount) {
    for (int i = 0; i < SK_LAYOUT_NUM; i++) {
        if (i <= 1 || i >= 12) continue;
        int nMatchedCardIndex = 0, nIndexDiff = 0;
        if (2 == nCardLay[i]) nIndexDiff++; else if (1 == nCardLay[i] && 0 == nMatchedCardIndex) nMatchedCardIndex = i;
        if (2 == nCardLay[i+1]) nIndexDiff++; else if (1 == nCardLay[i+1] && 0 == nMatchedCardIndex) nMatchedCardIndex = i+1;
        if (2 == nCardLay[i+2]) nIndexDiff++; else if (1 == nCardLay[i+2] && 0 == nMatchedCardIndex) nMatchedCardIndex = i+2;
        if (2 == nIndexDiff && 0 != nMatchedCardIndex) for (int j = 0; j < nReserveCount; j++) if (nMatchedCardIndex == SK_GetCardIndexEx(nReserveCards[j], 0)) return nReserveCards[j];
    }
    return -1;
}
static inline int MatchCoupleCardType(int nCardLay[], int nReserveCards[], int nReserveCount) {
    for (int i = 0; i < SK_LAYOUT_NUM; i++) {
        if (i <= 0 || i >= 14) continue;
        if (1 == nCardLay[i]) for (int j = 0; j < nReserveCount; j++) if (i == SK_GetCardIndexEx(nReserveCards[j], 0)) return nReserveCards[j];
    }
    return -1;
}
static inline int MatchThreeCardType(int nCardLay[], int nReserveCards[], int nReserveCount) {
    for (int i = 0; i < SK_LAYOUT_NUM; i++) {
        if (i <= 0 || i >= 14) continue;
        if (2 == nCardLay[i]) for (int j = 0; j < nReserveCount; j++) if (i == SK_GetCardIndexEx(nReserveCards[j], 0)) return nReserveCards[j];
    }
    return -1;
}
static inline int Match2OrKingCardType(int nCardLay[], int nReserveCards[], int nReserveCount, LPMAKEDEALCFG pCfg) {
    if (nCardLay[1] + nCardLay[14] + nCardLay[15] >= pCfg->nReserved[0]) return -1;
    for (int j = 0; j < nReserveCount; j++) if (14 == SK_GetCardIndexEx(nReserveCards[j], 0) || 15 == SK_GetCardIndexEx(nReserveCards[j], 0) || 1 == SK_GetCardIndexEx(nReserveCards[j], 0)) return nReserveCards[j];
    return -1;
}
static inline int MatchBombCardType(int nCardLay[], int nReserveCards[], int nReserveCount) {
    for (int i = 0; i < SK_LAYOUT_NUM; i++) {
        if (i <= 0 || i >= 14) continue;
        if (3 == nCardLay[i]) for (int j = 0; j < nReserveCount; j++) if (i == SK_GetCardIndexEx(nReserveCards[j], 0)) return nReserveCards[j];
    }
    return -1;
}
static inline int GetOneReservedCard(int array[], int length) {
    for (int i = 0; i < length; i++) if (array[i] != -1) { int t = array[i]; array[i] = -1; return t; }
    return -1;
}

// =============================================================================
// 9. 洗牌 SvrXygRandomSort（zgdatbl.h:576-602）逐字（含 rand/srand、s=54000>RAND_MAX）
// =============================================================================
static inline void SvrReversalMoreByValue(int array[], int value[], int length) {
    int i, j, temp;
    for (i = 0; i < length-1; i++)
        for (j = i+1; j < length; j++)
            if (value[i] < value[j]) {
                temp = array[i]; array[i] = array[j]; array[j] = temp;
                temp = value[i]; value[i] = value[j]; value[j] = temp;
            }
}
static inline void SvrXygRandomSort(int array[], int length, int seed) {
    srand(seed);
    int* value = new int[length];
    int s = length * 1000;
    for (int i = 0; i < length; i++) value[i] = rand() % s;
    SvrReversalMoreByValue(array, value, length);
    delete[] value;
}
// 框架辅助（按用法重建）
static inline void XygInitChairCards(int* a, int n) { for (int i = 0; i < n; i++) a[i] = -1; }
static inline int  GetNextChair(int chair) { return (chair + 1) % TOTAL_CHAIRS; }
static inline int  XygGetRandomBetween(int n) { return rand() % n; }

// =============================================================================
// 10. 玩家 / 桌子 Stub + 发牌编排（zgdatbl.cpp:858/4116/4187/3894 逐字方法）
// =============================================================================
struct CPlayer {
    int m_nUserType = USER_TYPE_REAL;
    int m_nBout = 999;
    BOOL IsRoboter() { return m_nUserType == USER_TYPE_ROBOT ? TRUE : FALSE; }
};

struct CGameTable {
    CPlayer* m_ptrPlayers[TOTAL_CHAIRS];
    int m_nBanker = 0;
    int m_nRoomID = 0;
    int m_nTotalChairs = TOTAL_CHAIRS;
    int m_nMakeDealTypes[TOTAL_CHAIRS];

    int CalcBankerChairBefore() { return XygGetRandomBetween(m_nTotalChairs); }
    int CalcBanker(BOOL isFixBankerToSoleRealPlayer) {
        TCHAR szRoomID[16]; memset(szRoomID, 0, sizeof(szRoomID)); _stprintf(szRoomID, _T("%ld"), (long)m_nRoomID);
        BOOL bRobotSpecialAuctionMode = GetPrivateProfileInt(_T("RobotSpecialAuctionMode"), szRoomID, FALSE, "");
        if ((TRUE == bRobotSpecialAuctionMode || isFixBankerToSoleRealPlayer == TRUE) && 2 == GetRobotCount()) {
            m_nBanker = -1;
            for (int i = 0; i < TOTAL_CHAIRS; i++) if (NULL != m_ptrPlayers[i] && m_ptrPlayers[i]->m_nUserType != USER_TYPE_ROBOT) { m_nBanker = i; break; }
            if (-1 == m_nBanker) m_nBanker = CalcBankerChairBefore();
        } else { m_nBanker = CalcBankerChairBefore(); }
        return m_nBanker;
    }
    int GetRobotCount() { int c = 0; for (int i = 0; i < TOTAL_CHAIRS; i++) if (m_ptrPlayers[i]->IsRoboter()) c++; return c; }

    BOOL IsNeedMakeDealByUserBoutInfo(CPlayer* pPlayer) {
        if (CFG_MGR[MAKEDEAL_CONFIG]["PlayerMakeDealRule"]["IsEnable"].asInt() == 0) return FALSE;
        TCHAR szRoomID[16]; memset(szRoomID, 0, sizeof(szRoomID)); _stprintf(szRoomID, _T("%ld"), (long)m_nRoomID);
        if (!CFG_MGR[MAKEDEAL_CONFIG]["PlayerMakeDealRule"]["RoomRule"][szRoomID].isNull()) {
            int nTargetNewUserBout = CFG_MGR[MAKEDEAL_CONFIG]["PlayerMakeDealRule"]["RoomRule"][szRoomID]["NewUserBout"].asInt();
            int nNewUserBout = pPlayer->m_nBout;
            if (nTargetNewUserBout >= nNewUserBout) return TRUE;
            else return FALSE;
        } else return FALSE;
    }

    void GetMakeDealCfg(LPMAKEDEALCFG pCfg, std::string MakeDealStrategy = "") {
        TCHAR szRoomID[16]; memset(szRoomID, 0, sizeof(szRoomID)); _stprintf(szRoomID, _T("%ld"), (long)m_nRoomID);
        CString strMakeDealStrategy = "default";
        if (MakeDealStrategy != "") {
            strMakeDealStrategy = (MakeDealStrategy + szRoomID).c_str();
            if (CFG_MGR[MAKEDEAL_CONFIG]["MakeDealStrategy"][strMakeDealStrategy.c_str()].isNull()) strMakeDealStrategy = MakeDealStrategy.c_str();
        } else {
            if (!CFG_MGR[MAKEDEAL_CONFIG]["MakeDeal"][szRoomID].isNull()) {
                strMakeDealStrategy = CFG_MGR[MAKEDEAL_CONFIG]["MakeDeal"][szRoomID].asCString();
                if (!CFG_MGR[MAKEDEAL_CONFIG]["MakeDealStrategy"][(strMakeDealStrategy + szRoomID).c_str()].isNull()) strMakeDealStrategy = strMakeDealStrategy + szRoomID;
            }
            if (CFG_MGR[MAKEDEAL_CONFIG]["MakeDealStrategy"][strMakeDealStrategy.c_str()].isNull()) strMakeDealStrategy = "default";
        }
        const JsonValue& S = CFG_MGR[MAKEDEAL_CONFIG]["MakeDealStrategy"][strMakeDealStrategy.c_str()];
        pCfg->nMakeDealType = S["MakeDealType"].asInt();
        pCfg->nBeginMakeNum = S["BeginMakeNum"].asInt();
        pCfg->nBeginSelectBanker = S["BeginSelectBanker"].asInt();
        pCfg->nFirstChairHandCount = S["FirstChairHandCount"].asInt();
        pCfg->nFirstChairBombCount = S["FirstChairBombCount"].asInt();
        pCfg->nFirstChairBigCardsCount = S["FirstChairBigCardsCount"].asInt();
        pCfg->nOtherChairHandCount = S["OtherChairHandCount"].asInt();
        pCfg->nOtherChairBombCount = S["OtherChairBombCount"].asInt();
        pCfg->nOtherChairBigCardsCount = S["OtherChairBigCardsCount"].asInt();
        pCfg->nReserved[0] = S["BigCardsTo"].asInt();
        float fTargetValue = S["TargetValue"].asFloat();
        if (fTargetValue >= 0) pCfg->nTargetValue = (int)(fTargetValue * CFG_MGR[MAKEDEAL_CONFIG]["MakeDealCommonArgs"]["MaxCardsValue"].asInt());
        else pCfg->nTargetValue = -(int)(fTargetValue * CFG_MGR[MAKEDEAL_CONFIG]["MakeDealCommonArgs"]["MinCardsValue"].asInt());
        pCfg->nTargetRound = S["TargetRound"].asInt();
        srand((unsigned)time(NULL));
        int nCouPaiStrategyCount = (int)CFG_MGR[MAKEDEAL_CONFIG]["MakeDealStrategy"][strMakeDealStrategy.c_str()]["CouPaiStrategy"].size();
        int nCouPaiStrategySelectIndex = rand() % nCouPaiStrategyCount;
        for (int i = 0; i < (int)CFG_MGR[MAKEDEAL_CONFIG]["MakeDealStrategy"][strMakeDealStrategy.c_str()]["CouPaiStrategy"][nCouPaiStrategySelectIndex].size(); i++)
            pCfg->arrCouPaiStrategy.push_back(CFG_MGR[MAKEDEAL_CONFIG]["MakeDealStrategy"][strMakeDealStrategy.c_str()]["CouPaiStrategy"][nCouPaiStrategySelectIndex][i].asInt());
    }

    BOOL CopyMatchedCardID(int& nPreCardID, int nCardLay[], int nMatchedCardID, int nReserveCards[], int nReserveCount) {
        if (-1 == nMatchedCardID) return FALSE;
        for (int i = 0; i < nReserveCount; i++) if (nReserveCards[i] == nMatchedCardID) { nReserveCards[i] = -1; nPreCardID = nMatchedCardID; nCardLay[SK_GetCardIndexEx(nMatchedCardID, 0)]++; return TRUE; }
        return FALSE;
    }
    void MatchFirstChairCards(int nChairCard[], int nCardLay[], int nReserveCards[], int nReserveCount, LPMAKEDEALCFG pCfg) {
        for (int i = 0; i < CARDS_PER_CHAIR; i++) {
            if (-1 != nChairCard[i]) continue;
            int nMatchedCardID = Match2OrKingCardType(nCardLay, nReserveCards, nReserveCount, pCfg); if (CopyMatchedCardID(nChairCard[i], nCardLay, nMatchedCardID, nReserveCards, nReserveCount)) break;
            nMatchedCardID = MatchBombCardType(nCardLay, nReserveCards, nReserveCount); if (CopyMatchedCardID(nChairCard[i], nCardLay, nMatchedCardID, nReserveCards, nReserveCount)) break;
            nMatchedCardID = MatchThreeCardType(nCardLay, nReserveCards, nReserveCount); if (CopyMatchedCardID(nChairCard[i], nCardLay, nMatchedCardID, nReserveCards, nReserveCount)) break;
            nMatchedCardID = MatchABTCardType(nCardLay, nReserveCards, nReserveCount); if (CopyMatchedCardID(nChairCard[i], nCardLay, nMatchedCardID, nReserveCards, nReserveCount)) break;
            nMatchedCardID = MatchABTCoupleCardType(nCardLay, nReserveCards, nReserveCount); if (CopyMatchedCardID(nChairCard[i], nCardLay, nMatchedCardID, nReserveCards, nReserveCount)) break;
            nMatchedCardID = MatchCoupleCardType(nCardLay, nReserveCards, nReserveCount); if (CopyMatchedCardID(nChairCard[i], nCardLay, nMatchedCardID, nReserveCards, nReserveCount)) break;
            return;
        }
        m_nMakeDealTypes[0] = 2;
    }
    void MatchOtherChairCards(int nChairCard[], int nCardLay[], int nReserveCards[], int nReserveCount, LPMAKEDEALCFG pCfg, int nChairNO = -1) {
        for (int i = 0; i < CARDS_PER_CHAIR; i++) {
            if (-1 != nChairCard[i]) continue;
            int nMatchedCardID = Match2OrKingCardType(nCardLay, nReserveCards, nReserveCount, pCfg); if (CopyMatchedCardID(nChairCard[i], nCardLay, nMatchedCardID, nReserveCards, nReserveCount)) break;
            nMatchedCardID = MatchBombCardType(nCardLay, nReserveCards, nReserveCount); if (CopyMatchedCardID(nChairCard[i], nCardLay, nMatchedCardID, nReserveCards, nReserveCount)) break;
            nMatchedCardID = MatchThreeCardType(nCardLay, nReserveCards, nReserveCount); if (CopyMatchedCardID(nChairCard[i], nCardLay, nMatchedCardID, nReserveCards, nReserveCount)) break;
            nMatchedCardID = MatchABTCardType(nCardLay, nReserveCards, nReserveCount); if (CopyMatchedCardID(nChairCard[i], nCardLay, nMatchedCardID, nReserveCards, nReserveCount)) break;
            nMatchedCardID = MatchABTCoupleCardType(nCardLay, nReserveCards, nReserveCount); if (CopyMatchedCardID(nChairCard[i], nCardLay, nMatchedCardID, nReserveCards, nReserveCount)) break;
            nMatchedCardID = MatchCoupleCardType(nCardLay, nReserveCards, nReserveCount); if (CopyMatchedCardID(nChairCard[i], nCardLay, nMatchedCardID, nReserveCards, nReserveCount)) break;
            return;
        }
        if (nChairNO != -1) m_nMakeDealTypes[nChairNO] = 2;
    }
    BOOL DoMakeDeal(int (*pChairCards)[CARDS_PER_CHAIR], int (*pCardLays)[SK_LAYOUT_NUM], int nReserveCards[], int nReserveCount,
                    LPMAKEDEALCFG pCfg, int nHandCount[], int nBombCount[], int nBigCardsCount[]) {
        TCHAR szRoomID[16]; memset(szRoomID, 0, sizeof(szRoomID)); _stprintf(szRoomID, _T("%ld"), (long)m_nRoomID);
        BOOL bRobotNeedMakeDeal = GetPrivateProfileInt(_T("RobotNeedMakeDeal"), szRoomID, TRUE, "");
        for (int i = 0; i < CARDS_PER_CHAIR - pCfg->nBeginMakeNum; i++) {
            if (m_ptrPlayers[m_nBanker]->m_nUserType != USER_TYPE_ROBOT || (m_ptrPlayers[m_nBanker]->m_nUserType == USER_TYPE_ROBOT && TRUE == bRobotNeedMakeDeal)) {
                if (0 == i && nHandCount[m_nBanker] > pCfg->nFirstChairHandCount && nBombCount[m_nBanker] < pCfg->nFirstChairBombCount && nBigCardsCount[m_nBanker] < pCfg->nFirstChairBigCardsCount) MatchFirstChairCards(pChairCards[m_nBanker], pCardLays[m_nBanker], nReserveCards, nReserveCount, pCfg);
                else if (i > 0) MatchFirstChairCards(pChairCards[m_nBanker], pCardLays[m_nBanker], nReserveCards, nReserveCount, pCfg);
            }
            int nNextChairNo = GetNextChair(m_nBanker);
            if (m_ptrPlayers[nNextChairNo]->m_nUserType != USER_TYPE_ROBOT || (m_ptrPlayers[nNextChairNo]->m_nUserType == USER_TYPE_ROBOT && TRUE == bRobotNeedMakeDeal)) {
                if (0 == i && nHandCount[nNextChairNo] > pCfg->nOtherChairHandCount && nBombCount[nNextChairNo] < pCfg->nOtherChairBombCount && nBigCardsCount[nNextChairNo] < pCfg->nOtherChairBigCardsCount) MatchOtherChairCards(pChairCards[nNextChairNo], pCardLays[nNextChairNo], nReserveCards, nReserveCount, pCfg, 1);
                else if (i > 0) MatchOtherChairCards(pChairCards[nNextChairNo], pCardLays[nNextChairNo], nReserveCards, nReserveCount, pCfg, 1);
            }
            int nNextNextChairNo = GetNextChair(nNextChairNo);
            if (m_ptrPlayers[nNextNextChairNo]->m_nUserType != USER_TYPE_ROBOT || (m_ptrPlayers[nNextNextChairNo]->m_nUserType == USER_TYPE_ROBOT && TRUE == bRobotNeedMakeDeal)) {
                if (0 == i && nHandCount[nNextNextChairNo] > pCfg->nOtherChairHandCount && nBombCount[nNextNextChairNo] < pCfg->nOtherChairBombCount && nBigCardsCount[nNextNextChairNo] < pCfg->nOtherChairBigCardsCount) MatchOtherChairCards(pChairCards[nNextNextChairNo], pCardLays[nNextNextChairNo], nReserveCards, nReserveCount, pCfg, 2);
                else if (i > 0) MatchOtherChairCards(pChairCards[nNextNextChairNo], pCardLays[nNextNextChairNo], nReserveCards, nReserveCount, pCfg, 2);
            }
        }
        for (int i = pCfg->nBeginMakeNum; i < CARDS_PER_CHAIR; i++) {
            for (int c = 0; c < TOTAL_CHAIRS; c++) if (-1 == pChairCards[c][i]) { pChairCards[c][i] = GetOneReservedCard(nReserveCards, (CARDS_PER_CHAIR - pCfg->nBeginMakeNum)*3); pCardLays[c][SK_GetCardIndexEx(pChairCards[c][i], 0)]++; }
        }
        return TRUE;
    }
    void MakeDealByCfg(int cards[], int length) {
        MAKEDEALCFG dealCfg; ZeroMemory(&dealCfg, sizeof(MAKEDEALCFG));
        GetMakeDealCfg(&dealCfg);
        if (0 == dealCfg.nMakeDealType) {
            if (0 < dealCfg.nBeginMakeNum && dealCfg.nBeginMakeNum < CARDS_PER_CHAIR && -1 != dealCfg.nFirstChairHandCount && -1 != dealCfg.nFirstChairBombCount && -1 != dealCfg.nFirstChairBigCardsCount && -1 != dealCfg.nOtherChairHandCount && -1 != dealCfg.nOtherChairBombCount && -1 != dealCfg.nOtherChairBigCardsCount) {
                int nChairCards[TOTAL_CHAIRS][CARDS_PER_CHAIR], *pReserveCards = new int[(CARDS_PER_CHAIR - dealCfg.nBeginMakeNum) * 3];
                if (NULL == pReserveCards) return;
                for (int c = 0; c < TOTAL_CHAIRS; c++) XygInitChairCards(nChairCards[c], CARDS_PER_CHAIR);
                XygInitChairCards(pReserveCards, (CARDS_PER_CHAIR - dealCfg.nBeginMakeNum) * 3);
                for (int i = 0; i < dealCfg.nBeginMakeNum; i++) { nChairCards[0][i] = cards[i*3]; nChairCards[1][i] = cards[i*3+1]; nChairCards[2][i] = cards[i*3+2]; }
                memcpy(pReserveCards, &cards[dealCfg.nBeginMakeNum * 3], sizeof(int) * (CARDS_PER_CHAIR - dealCfg.nBeginMakeNum) * 3);
                int nHandCount[TOTAL_CHAIRS]={0}, nBombCount[TOTAL_CHAIRS]={0}, nBigCardsCount[TOTAL_CHAIRS]={0};
                int nCardLays[TOTAL_CHAIRS][SK_LAYOUT_NUM]; ZeroMemory(nCardLays, sizeof(nCardLays));
                for (int c = 0; c < TOTAL_CHAIRS; c++) CalcHandCardsCount(nChairCards[c], dealCfg.nBeginMakeNum, nCardLays[c], nHandCount[c], nBombCount[c], nBigCardsCount[c]);
                if (FALSE == DoMakeDeal(nChairCards, nCardLays, pReserveCards, (CARDS_PER_CHAIR - dealCfg.nBeginMakeNum) * 3, &dealCfg, nHandCount, nBombCount, nBigCardsCount)) { delete[] pReserveCards; return; }
                for (int i = dealCfg.nBeginMakeNum; i < CARDS_PER_CHAIR; i++) { cards[i*3]=nChairCards[0][i]; cards[i*3+1]=nChairCards[1][i]; cards[i*3+2]=nChairCards[2][i]; }
                delete[] pReserveCards;
            }
        } else if (1 == dealCfg.nMakeDealType) {
            MAKEDEALCFG userdealCfg[3]; for (int i=0;i<3;i++){ userdealCfg[i].nMakeDealType=0; userdealCfg[i].nBeginMakeNum=0; userdealCfg[i].nBeginSelectBanker=0; userdealCfg[i].nFirstChairHandCount=-1; userdealCfg[i].nFirstChairBombCount=-1; userdealCfg[i].nFirstChairBigCardsCount=-1; userdealCfg[i].nOtherChairHandCount=-1; userdealCfg[i].nOtherChairBombCount=-1; userdealCfg[i].nOtherChairBigCardsCount=-1; userdealCfg[i].nTargetValue=0; userdealCfg[i].nTargetRound=0; userdealCfg[i].nReserved[0]=0; userdealCfg[i].nReserved[1]=0; userdealCfg[i].nReserved[2]=0; userdealCfg[i].nReserved[3]=0; }
            for (int i = 0; i < 3; i++) {
                if (m_ptrPlayers[i]->IsRoboter()) { GetMakeDealCfg(&userdealCfg[i], "robot"); }
                else if (IsNeedMakeDealByUserBoutInfo(m_ptrPlayers[i])) { GetMakeDealCfg(&userdealCfg[i], "newuser"); }
                else { userdealCfg[i] = dealCfg; }
                if (0 > userdealCfg[i].nBeginMakeNum || userdealCfg[i].nBeginMakeNum > CARDS_PER_CHAIR || 0 > userdealCfg[i].nBeginSelectBanker || userdealCfg[i].nBeginSelectBanker > CARDS_PER_CHAIR) return;
            }
            std::vector<int> arrHandCardList[3];
            std::vector<int> arrReserveCards(&cards[0], &cards[length]);
            int nBottomCard[BOTTOM_CARD];
            for (int p = 0; p < CARDS_PER_CHAIR + 1; ++p) {
                for (int k = 0; k < 3; ++k) {
                    if (p < userdealCfg[k].nBeginMakeNum) { arrHandCardList[k].push_back(*arrReserveCards.begin()); arrReserveCards.erase(arrReserveCards.begin()); }
                    else if (p < userdealCfg[k].nBeginSelectBanker) {
                        std::vector<CardGroupData> AllCardTypeArr; SpliteCard(arrHandCardList[k], AllCardTypeArr);
                        int nHandCount = 0, nHandCardValue = 0; CalHandCardValue(AllCardTypeArr, nHandCount, nHandCardValue);
                        ComposeCardResult tmpRet = MakeDeal_ComposeCard(&userdealCfg[k], AllCardTypeArr, arrReserveCards, nHandCount > userdealCfg[k].nTargetRound || nHandCardValue < userdealCfg[k].nTargetValue);
                        arrHandCardList[k].push_back(tmpRet.nRemoveCardID);
                        m_nMakeDealTypes[k] = 3;
                    } else if (p == userdealCfg[k].nBeginSelectBanker) { int nLen = (int)arrReserveCards.size(); nBottomCard[k] = arrReserveCards[nLen - 1]; arrReserveCards.erase(arrReserveCards.begin() + nLen - 1); }
                    else { int nLen = (int)arrReserveCards.size(); arrHandCardList[k].push_back(arrReserveCards[nLen - 1]); arrReserveCards.erase(arrReserveCards.begin() + nLen - 1); }
                }
            }
            if (arrHandCardList[0].size() != CARDS_PER_CHAIR || arrHandCardList[1].size() != CARDS_PER_CHAIR || arrHandCardList[2].size() != CARDS_PER_CHAIR) return;
            for (int i = 0; i < CARDS_PER_CHAIR; i++) { cards[i*3]=arrHandCardList[0][i]; cards[i*3+1]=arrHandCardList[1][i]; cards[i*3+2]=arrHandCardList[2][i]; }
            cards[CARDS_PER_CHAIR*3]=nBottomCard[0]; cards[CARDS_PER_CHAIR*3+1]=nBottomCard[1]; cards[CARDS_PER_CHAIR*3+2]=nBottomCard[2];
        }
    }
};

// =============================================================================
// 11. main：加载配置 → 循环发牌 → 输出 JSON Lines
// =============================================================================
static bool loadConfig(const char* path) {
    FILE* fp = fopen(path, "rb");
    if (!fp) return false;
    std::string s; char buf[65536];
    size_t r; while ((r = fread(buf, 1, sizeof(buf), fp)) > 0) s.append(buf, r);
    fclose(fp);
    JsonValue root;
    if (!ParseJsonConfig(s, root)) return false;
    CFG_MGR[MAKEDEAL_CONFIG] = root;
    return true;
}

int main(int argc, char** argv) {
    int room = 742, reals = 3; long n = 100000; unsigned seed = 0;
    const char* cfgPath = "makedeal.json";
    for (int i = 1; i < argc; i++) {
        std::string a = argv[i];
        if (a == "--room" && i+1 < argc) room = atoi(argv[++i]);
        else if (a == "--reals" && i+1 < argc) reals = atoi(argv[++i]);
        else if ((a == "-n" || a == "--n") && i+1 < argc) n = atol(argv[++i]);
        else if (a == "--seed" && i+1 < argc) seed = (unsigned)atol(argv[++i]);
        else if (a == "--cfg" && i+1 < argc) cfgPath = argv[++i];
        else if (a == "--help" || a == "-h") { fprintf(stderr, "usage: harness --room 742 --reals 3 -n 100000 [--seed 0] [--cfg makedeal.json]\n"); return 0; }
    }
    if (reals < 1 || reals > 3) { fprintf(stderr, "--reals must be 1/2/3\n"); return 1; }
    if (!loadConfig(cfgPath)) { fprintf(stderr, "cannot load config %s (use --cfg <path>)\n", cfgPath); return 1; }

    CPlayer players[3];
    for (int i = 0; i < reals; i++)        { players[i].m_nUserType = USER_TYPE_REAL;  players[i].m_nBout = 999; }
    for (int i = reals; i < 3; i++)        { players[i].m_nUserType = USER_TYPE_ROBOT; players[i].m_nBout = 999; }
    CGameTable T;
    T.m_nRoomID = room;
    T.m_nTotalChairs = TOTAL_CHAIRS;
    T.m_ptrPlayers[0] = &players[0]; T.m_ptrPlayers[1] = &players[1]; T.m_ptrPlayers[2] = &players[2];

    for (long deal = 0; deal < n; deal++) {
        // 真人局数随 deal 增长（影响 newuser 判定；默认 999 远大于 NewUserBout，即非新手）
        for (int i = 0; i < reals; i++) players[i].m_nBout = 999 + (int)deal;
        // 新手保护：恰好 1 真人 + 该真人局数 <= NewUserBout → 强制真人当庄（isFixBanker）
        BOOL isFix = FALSE;
        if (reals == 1) {
            TCHAR szRoomID[16]; _stprintf(szRoomID, _T("%ld"), (long)room);
            const JsonValue& rr = CFG_MGR[MAKEDEAL_CONFIG]["PlayerMakeDealRule"]["RoomRule"][szRoomID];
            if (!rr.isNull()) { int nb = rr["NewUserBout"].asInt(); if (nb >= players[0].m_nBout) isFix = TRUE; }
        }
        T.m_nBanker = T.CalcBanker(isFix);
        int card[TOTAL_CARDS]; for (int i = 0; i < TOTAL_CARDS; i++) card[i] = i;
        int shuffleSeed = (int)(seed + deal * 7919u + 17u);
        SvrXygRandomSort(card, TOTAL_CARDS, shuffleSeed);          // 1:1 线上洗牌（rand/srand）
        T.MakeDealByCfg(card, TOTAL_CARDS);                        // 1:1 发牌/配牌/拆牌

        // 输出 JSONL：每家手牌(17)+底牌(3)+炸弹/手数/大牌/做牌类型/是否真人/庄
        printf("{\"deal\":%ld,\"room\":%d,\"reals\":%d,\"banker\":%d,\"seats\":[",
               deal, room, reals, T.m_nBanker);
        for (int c = 0; c < 3; c++) {
            int hand[CARDS_PER_CHAIR];
            for (int i = 0; i < CARDS_PER_CHAIR; i++) hand[i] = card[c + i*3];   // stride-3：chair c
            int lay[SK_LAYOUT_NUM]; memset(lay,0,sizeof(lay));
            int hc=0, bc=0, big=0;
            CalcHandCardsCount(hand, CARDS_PER_CHAIR, lay, hc, bc, big);
            printf("%s{\"seat\":%d,\"is_robot\":%s,\"hand\":[",
                   c?"," : "", c, players[c].m_nUserType==USER_TYPE_ROBOT?"true":"false");
            for (int i = 0; i < CARDS_PER_CHAIR; i++) printf("%s%d", i?",":"", hand[i]);
            printf("],\"bombs\":%d,\"handcount\":%d,\"bigcards\":%d,\"makedeal\":%d}", bc, hc, big, T.m_nMakeDealTypes[c]);
        }
        printf("],\"bottom\":[%d,%d,%d]}\n",
               card[CARDS_PER_CHAIR*3], card[CARDS_PER_CHAIR*3+1], card[CARDS_PER_CHAIR*3+2]);
    }
    return 0;
}
