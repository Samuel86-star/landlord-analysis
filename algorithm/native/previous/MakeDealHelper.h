#pragma once
#include <Windows.h>
#include <vector>
#include <string>
#include <map>
#include <algorithm>

struct ComposeCardResult {
    bool bRet;  //有没有凑成功
    std::string ComposeCardGroupType;  //该次组牌的类型
    int ComposeCardGroupCardCount;   //该次组成牌牌张数
    int nRemoveCardID; //该次组牌发掉的牌
};

//最多手牌
#define HandCardMaxLen 20
//价值最小值
#define MinCardsValue -999
//价值最大值
#define MaxCardsValue 106

#define	SK_LAYOUT_MOD  13

//【单牌】【组合牌】【整手牌】
//手牌组合枚举
enum CardGroupType
{
    cgERROR = -1,						    //错误类型
    cgZERO = 0,						        //不出类型
    cgSINGLE = 1,							//单牌类型
    cgDOUBLE = 2,							//对牌类型
    cgTHREE = 3,							//三条类型
    cgSINGLE_LINE = 4,						//单连类型
    cgDOUBLE_LINE = 5,						//对连类型
    cgTHREE_LINE = 6,						//三连类型
    cgTHREE_TAKE_ONE = 7,					//三带一单
    cgTHREE_TAKE_TWO = 8,					//三带一对
    cgTHREE_TAKE_ONE_LINE = 9,				//三带一单连
    cgTHREE_TAKE_TWO_LINE = 10,				//三带一对连
    cgFOUR_TAKE_ONE = 11,					//四带两单
    cgFOUR_TAKE_TWO = 12,					//四带两对
    cgBOMB_CARD = 13,						//炸弹类型
    cgKING_CARD = 14						//王炸类型
};

enum CardSuit {
    PK_DIAMOND = 0,			                // 方块
    PK_CLUB = 1,			                // 梅花
    PK_HEART = 2,			                // 红心
    PK_SPADE = 3,			                // 黑桃
    PK_KING = 4,			                // 王牌
    PK_JOKER = 5			                // 财神
};

//手牌权值结构
struct HandCardValue
{
    int SumValue;                           //手牌总价值
    int NeedRound;                          // 需要打几手牌
};

//牌型组合数据结构
struct CardGroupData
{
    //枚举类型
    CardGroupType cgType = cgERROR;
    //该牌的价值
    int  nValue = 0;
    //含牌的个数
    int  nCount = 0;
    //牌中决定大小的牌值，用于对比
    int nMaxCard = 0;//说明一下nMaxCard，比如说99，他的nMaxCard就是9，比如说33366，他的nMaxCard就是3。被动出牌规则便是通过这个值进行判断是否可以出牌。
};

struct ALLCardsList
{
    std::vector<int> arrHandCardList[3];
    std::vector<int> arrBottomCardList;
};

//做牌手牌数据类
class HandCardInfo
{
public:
    //手牌序列——状态记录，便于一些计算，值域为该index牌对应的数量0~4
    int value_aHandCardList[18];
    //手牌个数
    int nHandCardCount = 17;
    //玩家要打出去的牌类型
    CardGroupData uctPutCardType;
    //要打出去的牌——无花色
    std::vector <int> value_nPutCardList;
public:

    //要打出的牌序列清空//是把要出的牌打入出牌序列前清空现列表的操作，含有花色和无花色，顺便把之前出牌类型的值初始化一下
    void ClearPutCardList();

    //根据牌id获取牌权重值
    int getvaluebycardid(int cardid);

    //初始化 //手牌的初始化，主要用于根据获取的有花色手牌序列转换成无花色手牌序列，手牌序列排序， 计算出手牌个数。
    void Init(std::vector<int> CardIdArr);
};

CardGroupData get_GroupData(CardGroupType cgType, int MaxCard, int Count);

CardGroupData SurCardsType(int arr[]);
HandCardValue get_MaxHandCardValue(HandCardInfo& clsHandCardData);

void GetBestCardType(HandCardInfo& clsHandCardData);

void SpliteCard(std::vector<int> arrHandCardList, std::vector<CardGroupData>& cardTypeArr);


ComposeCardResult MakeDeal_ComposeCard(IN LPMAKEDEALCFG pCfg, IN OUT std::vector<CardGroupData>& CardGroupDatas, IN std::vector<int>& RemainCards, IN bool bNeedMakeDeal = true);

//下发到三名玩家的手牌序列，此数据只用于测试，作为AI时不会获取
int     GetValuebyCardid(int cardid);
std::string  GetCardValuebyCardIndex(int cardindex);
int     SK_GetCardIndex(int nCardID);
int     SK_GetCardShape(int nCardID);

std::string get_CardsName(int cardid);
std::string get_GroupCardName(int GroupCardType);
void CalHandCardValue(std::vector<CardGroupData>& CardGroupDatas, int& nHandCount, int& nHandCardAveValue);
