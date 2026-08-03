#include "StdAfx.h"
#include "MakeDealHelper.h"
#include <ConfigManagerSys.h>

using namespace std;

//手牌数据类

//要打出的牌序列清空//是把要出的牌打入出牌序列前清空现列表的操作，含有花色和无花色，顺便把之前出牌类型的值初始化一下
void HandCardInfo::ClearPutCardList() {
    value_nPutCardList.clear();

    uctPutCardType.cgType = cgERROR;
    uctPutCardType.nCount = 0;
    uctPutCardType.nMaxCard = -1;
    uctPutCardType.nValue = 0;
    return;
}

//根据牌id获取牌权重值
int HandCardInfo::getvaluebycardid(int cardid) {
    int ret = 0;
    if (cardid < 52) {
        int tmp = cardid % SK_LAYOUT_MOD;
        ret = (tmp == 0 ? 15 : tmp + 2);
    }
    else if (cardid == 52) { //小王权重
        ret = 16;
    }
    else if (cardid == 53) { //大王权重
        ret = 17;
    }

    return ret;
}

//初始化 //手牌的初始化，主要用于根据获取的有花色手牌序列转换成无花色手牌序列，手牌序列排序， 计算出手牌个数。
void HandCardInfo::Init(vector<int> CardIdArr) {
    //根据花色手牌获取权值手牌
    memset(value_aHandCardList, 0, sizeof(value_aHandCardList));
    for (vector<int>::iterator iter = CardIdArr.begin(); iter != CardIdArr.end(); iter++)
    {
        value_aHandCardList[getvaluebycardid(*iter)]++;
    }
    //当前手牌个数
    nHandCardCount = CardIdArr.size();
}



//①每个单牌都有一个基础价值②组合牌型的整体价值与这个基础价值有关，但显然计算规则不完全一样。③整手牌可以分成若干个组合牌，但分法不唯一。
//当时，我说了①和②可以直接定义，③需要迭代计算。所以本章的主要内容就是确定基础价值& 组合牌型的价值定义
//对于牌型权值的定义看似简单，实际却需要大量的推敲。这就跟游戏里不同英雄属性、技能反复修改一样。事实上，直至整个工程开发完毕，我还在修改权值定义，因为这是唯一影响逻辑处理的因素。如果你觉得程序返回的出牌策略不太符合你的想法，那么一定是权值定义这里出现的问题。

/*评分逻辑思维：
0.由于新策略引入手牌轮次参数，所以不再考虑拆分价值。
1.牌力基础价值：我们认为10属于中等位置，即<10单牌价值为负，大于10的单牌价值为正
2.控手的价值定义：我们认为一次控手权可以抵消一次手中最小牌型，最小牌型（3）的价值为-7，即我们定义一次控手权的价值为7
3.单牌的价值定义：该牌的基础价值
4.对牌的价值定义：我们认为对牌与单牌价值相等（均可以被三牌带出）故其价值为该牌的基础价值
5.三牌的价值定义：
  三不带：     该牌的基础价值
  三带一：     我们认为通常带出去的牌价值一定无正价值故其价值为该牌的基础价值
  三带二：     我们认为通常带出去的牌价值一定无正价值故其价值为该牌的基础价值
6.四牌的价值定义：
  炸弹：       我们认为炸弹会享有一次控手权利且炸弹被管的概率极小，故其无负价值，非负基础价值+7
  四带二单：   我们认为四带二单管人与被管的概率极小，故其无负价值，其价值为非负基础价值/2（该值最大为6小于一个轮次7)
  四带二对：   我们认为四带二对管人与被管的概率极小，故其无负价值，其价值为非负基础价值/2（该值最大为6小于一个轮次7)
7.王炸的价值定义：已知炸2价值为15-3+7=19分，故王炸价值为20分。
8.顺子的价值定义：
  单顺：       我们认为单顺的价值等于其最大牌的单体价值，且2不参与顺子，故顺子的权值依次提升1
  双顺：       我们认为双顺的价值等于其最大牌的单体价值，且2不参与顺子，故顺子的权值依次提升1
  飞机不带：   由于飞机牌型管人与被管的概率极小，故其无负价值，其价值为非负基础价值/2（该值最大为6小于一个轮次7)
  飞机带双翅： 由于飞机牌型管人与被管的概率极小，故其无负价值，其价值为非负基础价值/2（该值最大为6小于一个轮次7)
  飞机带单翅： 由于飞机牌型管人与被管的概率极小，故其无负价值，其价值为非负基础价值/2（该值最大为6小于一个轮次7)
9.根据数值分布，我们暂定：   <10不叫分，10-14叫一分，15-19叫两分，20以上叫三分
*/

/*封装好的获取各类牌型组合结构函数
CardGroupType cgType：牌型
int MaxCard：决定大小的牌值
int Count：牌数
返回值：CardGroupData
*/
CardGroupData get_GroupData(CardGroupType cgType, int MaxCard, int Count)
{
	CardGroupData uctCardGroupData;

	uctCardGroupData.cgType = cgType;
	uctCardGroupData.nCount = Count;
	uctCardGroupData.nMaxCard = MaxCard;
	char szType[32] = { 0 };
	sprintf(szType, "%d", cgType);
	int nTmpValue = 0, nXianValue = 0;
	int M = 0;
	double C = 0;
	int D = 0;
	if (CConfigManagerSys::m_jsoncfgobjmgr[MAKEDEAL_CONFIG]["MakeDealCommonArgs"]["GroupDataExp"][szType].isNull()) {
		//错误牌型
		uctCardGroupData.nValue = 0;
	}
	else {
		//错误牌型根据配置实时获取
		for (size_t i = 0; i < CConfigManagerSys::m_jsoncfgobjmgr[MAKEDEAL_CONFIG]["MakeDealCommonArgs"]["GroupDataExp"][szType].size(); i++)
		{
			C = CConfigManagerSys::m_jsoncfgobjmgr[MAKEDEAL_CONFIG]["MakeDealCommonArgs"]["GroupDataExp"][szType][i]["C"].asDouble();
			if (CConfigManagerSys::m_jsoncfgobjmgr[MAKEDEAL_CONFIG]["MakeDealCommonArgs"]["GroupDataExp"][szType][i]["D"].isNull())
			{
				M = CConfigManagerSys::m_jsoncfgobjmgr[MAKEDEAL_CONFIG]["MakeDealCommonArgs"]["GroupDataExp"][szType][i]["M"].asInt();
				//说明是幂函数
				nXianValue = (int)(C * pow(MaxCard, M));
			}
			else
			{
				D = CConfigManagerSys::m_jsoncfgobjmgr[MAKEDEAL_CONFIG]["MakeDealCommonArgs"]["GroupDataExp"][szType][i]["D"].asInt();
				//说明是指数函数
				nXianValue = (int)(C * (log(MaxCard) / log(D)));
			}

			nTmpValue += nXianValue;
		}
		uctCardGroupData.nValue = nTmpValue;
	}

    return uctCardGroupData;
}//注意！！！以上价值定义是作者本人主观意愿，并非斗地主游戏最佳策略，请大家遵从自己的内心适当修改~~

//计算手牌最大总价值。
HandCardValue get_MaxHandCardValue(HandCardInfo& clsHandCardData)
{
    //首先清空出牌队列，因为剪枝时是不调用get_PutCardList的
    clsHandCardData.ClearPutCardList();

    HandCardValue uctHandCardValue;
    //出完牌了，其实这种情况只限于手中剩下四带二且被动出牌的情况，因为四带二剪枝做了特殊处理。
    if (clsHandCardData.nHandCardCount == 0)
    {
        uctHandCardValue.SumValue = 0;
        uctHandCardValue.NeedRound = 0;
        return uctHandCardValue;
    }
    //————以下为剪枝：判断是否可以一手出完牌
    CardGroupData uctCardGroupData = SurCardsType(clsHandCardData.value_aHandCardList);
    //————不到万不得已我们都不会出四带二，都尽量保炸弹
    if (uctCardGroupData.cgType != cgERROR && uctCardGroupData.cgType != cgFOUR_TAKE_ONE && uctCardGroupData.cgType != cgFOUR_TAKE_TWO)
    {
        uctHandCardValue.SumValue = uctCardGroupData.nValue;
        uctHandCardValue.NeedRound = 1;
        return uctHandCardValue;
    }

    //非剪枝操作，即非一手能出完的牌

    /*取出一个最优牌型,放入 clsHandCardData.value_nPutCardList及clsHandCardData.uctPutCardType中，
    可使用get_PutCardList返回最优方案*/
    GetBestCardType(clsHandCardData);

    //要保存当前的clsHandCardData.value_nPutCardList及clsHandCardData.uctPutCardType用于回溯
    CardGroupData NowPutCardType = clsHandCardData.uctPutCardType;
    vector<int> NowPutCardList = clsHandCardData.value_nPutCardList;

    if (clsHandCardData.uctPutCardType.cgType == cgERROR)
    {
        printf("GetBestCardType err===========\n");
    }

    //////////////////////////////////////////////////////////////////////////
        //去掉手牌中的最优牌型
    for (vector<int>::iterator iter = NowPutCardList.begin();
        iter != NowPutCardList.end(); iter++)
    {
        clsHandCardData.value_aHandCardList[*iter]--;
    }
    clsHandCardData.nHandCardCount -= NowPutCardType.nCount;
    //---回溯↑
    HandCardValue tmp_SurValue = get_MaxHandCardValue(clsHandCardData);//递归剩余牌最大总价值

    //再将最优牌型加入到手牌中，恢复手牌数据
    for (vector<int>::iterator iter = NowPutCardList.begin();
        iter != NowPutCardList.end(); iter++)
    {
        clsHandCardData.value_aHandCardList[*iter]++;
    }
    clsHandCardData.nHandCardCount += NowPutCardType.nCount;
    //////////////////////////////////////////////////////////////////////////

        //最优牌型牌值与剩下手牌的最大总牌值的和就是整手牌的最大手牌总牌值
    uctHandCardValue.SumValue = NowPutCardType.nValue + tmp_SurValue.SumValue;
    uctHandCardValue.NeedRound = tmp_SurValue.NeedRound + 1;

    return uctHandCardValue;
}

/*
检查剩余的牌是否只是一手牌，只检查整牌
是：  返回手牌类型数据
不是：返回错误类型（cgERROR）
*/
CardGroupData SurCardsType(int arr[])
{

    int nCount = 0;
    for (int i = 3; i < 18; i++)
    {
        nCount += arr[i];
    }

    CardGroupData retCardGroupData;
    retCardGroupData.nCount = nCount;


    //单牌类型
    if (nCount == 1)
    {
        //用于验证的变量
        int prov = 0;
        int SumValue = 0;
        for (int i = 3; i < 18; i++)
        {
            if (arr[i] == 1)
            {
                SumValue = i - 10;
                prov++;
                retCardGroupData.nMaxCard = i;
                break;
            }
        }
        if (prov == 1)
        {
            retCardGroupData.cgType = cgSINGLE;
            retCardGroupData.nValue = SumValue;
            return retCardGroupData;
        }
    }
    //对牌类型
    if (nCount == 2)
    {
        //用于验证的变量
        int prov = 0;
        int SumValue = 0;
        int i = 0;
        for (i = 3; i < 16; i++)
        {
            if (arr[i] == 2)
            {
                SumValue = i - 10;
                prov++;
                retCardGroupData.nMaxCard = i;
                break;
            }
        }
        if (prov == 1)
        {
            retCardGroupData.cgType = cgDOUBLE;
            retCardGroupData.nValue = SumValue;
            return retCardGroupData;
        }
    }
    //三条类型
    if (nCount == 3)
    {
        //用于验证的变量
        int prov = 0;
        int SumValue = 0;
        int i = 0;
        for (i = 3; i < 16; i++)
        {
            if (arr[i] == 3)
            {
                SumValue = i - 10;
                prov++;
                retCardGroupData.nMaxCard = i;
                break;
            }
        }
        if (prov == 1)
        {
            retCardGroupData.cgType = cgTHREE;
            retCardGroupData.nValue = SumValue;
            return retCardGroupData;
        }
    }
    //炸弹类型
    if (nCount == 4)
    {
        //用于验证的变量
        int prov = 0;
        int SumValue = 0;
        for (int i = 3; i < 16; i++)
        {
            if (arr[i] == 4)
            {
                SumValue += i - 3 + 7;
                prov++;
                retCardGroupData.nMaxCard = i;
                break;
            }
        }
        if (prov == 1)
        {
            retCardGroupData.cgType = cgBOMB_CARD;
            retCardGroupData.nValue = SumValue;
            return retCardGroupData;
        }
    }
    //王炸类型
    if (nCount == 2)
    {
        int SumValue = 0;
        if (arr[17] > 0 && arr[16] > 0)
        {
            SumValue = 20;
            retCardGroupData.nMaxCard = 17;
            retCardGroupData.cgType = cgKING_CARD;
            retCardGroupData.nValue = SumValue;
            return retCardGroupData;
        }
    }
    //单连类型
    if (nCount >= 5)
    {
        //用于验证的变量
        int prov = 0;
        int SumValue = 0;
        int i;
        for (i = 3; i < 15; i++)
        {
            if (arr[i] == 1)
            {
                prov++;
            }
            else
            {
                if (prov != 0)
                {
                    break;
                }
            }
        }
        SumValue = i - 10;

        if (prov == nCount)
        {
            retCardGroupData.nMaxCard = i - 1;
            retCardGroupData.cgType = cgSINGLE_LINE;
            retCardGroupData.nValue = SumValue;
            return retCardGroupData;
        }
    }
    //对连类型
    if (nCount >= 6)
    {
        //用于验证的变量
        int prov = 0;
        int SumValue = 0;
        int i;
        for (i = 3; i < 15; i++)
        {
            if (arr[i] == 2)
            {
                prov++;
            }
            else
            {
                if (prov != 0)
                {
                    break;
                }
            }
        }
        SumValue = i - 10;

        if (prov * 2 == nCount)
        {
            retCardGroupData.nMaxCard = i - 1;
            retCardGroupData.cgType = cgDOUBLE_LINE;
            retCardGroupData.nValue = SumValue;
            return retCardGroupData;
        }
    }
    //三连类型
    if (nCount >= 6)
    {
        //用于验证的变量
        int prov = 0;

        int SumValue = 0;
        int i;
        for (i = 3; i < 15; i++)
        {
            if (arr[i] == 3)
            {
                prov++;
            }
            else
            {
                if (prov != 0)
                {
                    break;
                }
            }
        }
        SumValue = (i - 3) / 2;

        if (prov * 3 == nCount)
        {
            retCardGroupData.nMaxCard = i - 1;
            retCardGroupData.cgType = cgTHREE_LINE;
            retCardGroupData.nValue = SumValue;
            return retCardGroupData;
        }
    }


    retCardGroupData.cgType = cgERROR;
    return retCardGroupData;
}


//取出一个最优牌型
//1. 能直接一手牌出去，优先出。
//2. 出一手牌使得接下来自己手牌价值最大化。
void GetBestCardType(HandCardInfo& clsHandCardData)
{
    clsHandCardData.ClearPutCardList();

    CardGroupData SurCardGroupData = SurCardsType(clsHandCardData.value_aHandCardList);
    if (SurCardGroupData.cgType != cgERROR && SurCardGroupData.cgType != cgFOUR_TAKE_ONE && SurCardGroupData.cgType != cgFOUR_TAKE_TWO) {
        /*全部出完*/
        clsHandCardData.uctPutCardType = SurCardGroupData;
        for (int i = 0; i < 18; i++)
            for (int j = 0; j < clsHandCardData.value_aHandCardList[i]; j++)
                clsHandCardData.value_nPutCardList.push_back(i);
        return;
    }
    //暂存最佳的价值
    HandCardValue BestHandCardValue;
    BestHandCardValue.NeedRound = 20;
    BestHandCardValue.SumValue = MinCardsValue;
    //我们认为不出牌的话会让对手一个轮次，即加一轮（权值减少7）便于后续的对比参考。
    BestHandCardValue.NeedRound += 1;

    //暂存最佳的组合
    CardGroupData BestCardGroup;

    //次之处理当前价值最低的牌，现在不必再考虑这张牌可能被三牌带出等情况
    for (int i = 3; i < 16; i++)
    {
        if (clsHandCardData.value_aHandCardList[i] != 0 && clsHandCardData.value_aHandCardList[i] != 4)
        {
            //出单牌
            if (clsHandCardData.value_aHandCardList[i] == 1)
            {
                clsHandCardData.value_aHandCardList[i]--;
                clsHandCardData.nHandCardCount--;
                HandCardValue tmpHandCardValue = get_MaxHandCardValue(clsHandCardData);
                clsHandCardData.value_aHandCardList[i]++;
                clsHandCardData.nHandCardCount++;
                if ((BestHandCardValue.SumValue - (BestHandCardValue.NeedRound * 7)) <= (tmpHandCardValue.SumValue - (tmpHandCardValue.NeedRound * 7)))
                {
                    BestHandCardValue = tmpHandCardValue;
                    BestCardGroup = get_GroupData(cgSINGLE, i, 1);
                }
            }
            //出对牌
            if (clsHandCardData.value_aHandCardList[i] == 2)
            {
                //尝试打出一对牌，估算剩余手牌价值
                clsHandCardData.value_aHandCardList[i] -= 2;
                clsHandCardData.nHandCardCount -= 2;
                HandCardValue tmpHandCardValue = get_MaxHandCardValue(clsHandCardData);
                clsHandCardData.value_aHandCardList[i] += 2;
                clsHandCardData.nHandCardCount += 2;

                //选取总权值-轮次*7值最高的策略  因为我们认为剩余的手牌需要n次控手的机会才能出完，若轮次牌型很大（如炸弹） 则其-7的价值也会为正
                if ((BestHandCardValue.SumValue - (BestHandCardValue.NeedRound * 7)) <= (tmpHandCardValue.SumValue - (tmpHandCardValue.NeedRound * 7)))
                {
                    BestHandCardValue = tmpHandCardValue;
                    BestCardGroup = get_GroupData(cgDOUBLE, i, 2);
                }
            }
            //出三牌
            if (clsHandCardData.value_aHandCardList[i] == 3)
            {
                clsHandCardData.value_aHandCardList[i] -= 3;
                clsHandCardData.nHandCardCount -= 3;
                HandCardValue tmpHandCardValue = get_MaxHandCardValue(clsHandCardData);
                clsHandCardData.value_aHandCardList[i] += 3;
                clsHandCardData.nHandCardCount += 3;

                //选取总权值-轮次*7值最高的策略  因为我们认为剩余的手牌需要n次控手的机会才能出完，若轮次牌型很大（如炸弹） 则其-7的价值也会为正
                if ((BestHandCardValue.SumValue - (BestHandCardValue.NeedRound * 7)) <= (tmpHandCardValue.SumValue - (tmpHandCardValue.NeedRound * 7)))
                {
                    BestHandCardValue = tmpHandCardValue;
                    BestCardGroup = get_GroupData(cgTHREE, i, 3);
                }
            }
            //出单顺
            if (clsHandCardData.value_aHandCardList[i] > 0)
            {
                int prov = 0;
                for (int j = i; j < 15; j++)
                {
                    if (clsHandCardData.value_aHandCardList[j] > 0) {
                        prov++;
                    }
                    else {
                        break;
                    }
                    if (prov >= 5) {
                        for (int k = i; k <= j; k++) {
                            clsHandCardData.value_aHandCardList[k] --;
                        }
                        clsHandCardData.nHandCardCount -= prov;
                        HandCardValue tmpHandCardValue = get_MaxHandCardValue(clsHandCardData);
                        for (int k = i; k <= j; k++) {
                            clsHandCardData.value_aHandCardList[k] ++;
                        }
                        clsHandCardData.nHandCardCount += prov;

                        //选取总权值-轮次*7值最高的策略  因为我们认为剩余的手牌需要n次控手的机会才能出完，若轮次牌型很大（如炸弹） 则其-7的价值也会为正
                        if ((BestHandCardValue.SumValue - (BestHandCardValue.NeedRound * 7)) <= (tmpHandCardValue.SumValue - (tmpHandCardValue.NeedRound * 7))) {
                            BestHandCardValue = tmpHandCardValue;
                            BestCardGroup = get_GroupData(cgSINGLE_LINE, j, prov);
                        }
                    }
                }

            }
            //出双顺
            if (clsHandCardData.value_aHandCardList[i] > 1)
            {
                int prov = 0;
                for (int j = i; j < 15; j++)
                {
                    if (clsHandCardData.value_aHandCardList[j] > 1) {
                        prov++;
                    }
                    else {
                        break;
                    }
                    if (prov >= 3) {
                        for (int k = i; k <= j; k++) {
                            clsHandCardData.value_aHandCardList[k] -= 2;
                        }
                        clsHandCardData.nHandCardCount -= prov * 2;
                        HandCardValue tmpHandCardValue = get_MaxHandCardValue(clsHandCardData);
                        for (int k = i; k <= j; k++) {
                            clsHandCardData.value_aHandCardList[k] += 2;
                        }
                        clsHandCardData.nHandCardCount += prov * 2;

                        //选取总权值-轮次*7值最高的策略  因为我们认为剩余的手牌需要n次控手的机会才能出完，若轮次牌型很大（如炸弹） 则其-7的价值也会为正
                        if ((BestHandCardValue.SumValue - (BestHandCardValue.NeedRound * 7)) <= (tmpHandCardValue.SumValue - (tmpHandCardValue.NeedRound * 7))) {
                            BestHandCardValue = tmpHandCardValue;
                            BestCardGroup = get_GroupData(cgDOUBLE_LINE, j, prov * 2);
                        }
                    }
                }
            }
            //出三顺
            if (clsHandCardData.value_aHandCardList[i] > 2)
            {
                int prov = 0;
                for (int j = i; j < 15; j++) {
                    if (clsHandCardData.value_aHandCardList[j] > 2) {
                        prov++;
                    }
                    else {
                        break;
                    }
                    if (prov >= 2) {
                        for (int k = i; k <= j; k++) {
                            clsHandCardData.value_aHandCardList[k] -= 3;
                        }
                        clsHandCardData.nHandCardCount -= prov * 3;
                        HandCardValue tmpHandCardValue = get_MaxHandCardValue(clsHandCardData);
                        for (int k = i; k <= j; k++) {
                            clsHandCardData.value_aHandCardList[k] += 3;
                        }
                        clsHandCardData.nHandCardCount += prov * 3;

                        //选取总权值-轮次*7值最高的策略  因为我们认为剩余的手牌需要n次控手的机会才能出完，若轮次牌型很大（如炸弹） 则其-7的价值也会为正
                        if ((BestHandCardValue.SumValue - (BestHandCardValue.NeedRound * 7)) <= (tmpHandCardValue.SumValue - (tmpHandCardValue.NeedRound * 7))) {
                            BestHandCardValue = tmpHandCardValue;
                            BestCardGroup = get_GroupData(cgTHREE_LINE, j, prov * 3);
                        }
                    }
                }
            }


            //放在if内是因为若此时i有值那么必须要返回一个结果
            if (BestCardGroup.cgType == cgERROR)
            {

            }
            else if (BestCardGroup.cgType == cgSINGLE)
            {
                clsHandCardData.value_nPutCardList.push_back(BestCardGroup.nMaxCard);
                clsHandCardData.uctPutCardType = BestCardGroup;
            }
            else if (BestCardGroup.cgType == cgDOUBLE)
            {
                clsHandCardData.value_nPutCardList.push_back(BestCardGroup.nMaxCard);
                clsHandCardData.value_nPutCardList.push_back(BestCardGroup.nMaxCard);
                clsHandCardData.uctPutCardType = BestCardGroup;
            }
            else if (BestCardGroup.cgType == cgTHREE)
            {
                clsHandCardData.value_nPutCardList.push_back(BestCardGroup.nMaxCard);
                clsHandCardData.value_nPutCardList.push_back(BestCardGroup.nMaxCard);
                clsHandCardData.value_nPutCardList.push_back(BestCardGroup.nMaxCard);
                clsHandCardData.uctPutCardType = BestCardGroup;
            }
            else if (BestCardGroup.cgType == cgSINGLE_LINE)
            {
                for (int j = BestCardGroup.nMaxCard - BestCardGroup.nCount + 1; j <= BestCardGroup.nMaxCard; j++)
                {
                    clsHandCardData.value_nPutCardList.push_back(j);
                }
                clsHandCardData.uctPutCardType = BestCardGroup;
            }
            else if (BestCardGroup.cgType == cgDOUBLE_LINE)
            {
                for (int j = BestCardGroup.nMaxCard - (BestCardGroup.nCount / 2) + 1; j <= BestCardGroup.nMaxCard; j++)
                {
                    clsHandCardData.value_nPutCardList.push_back(j);
                    clsHandCardData.value_nPutCardList.push_back(j);
                }
                clsHandCardData.uctPutCardType = BestCardGroup;
            }
            else if (BestCardGroup.cgType == cgTHREE_LINE)
            {
                for (int j = BestCardGroup.nMaxCard - (BestCardGroup.nCount / 3) + 1; j <= BestCardGroup.nMaxCard; j++)
                {
                    clsHandCardData.value_nPutCardList.push_back(j);
                    clsHandCardData.value_nPutCardList.push_back(j);
                    clsHandCardData.value_nPutCardList.push_back(j);
                }
                clsHandCardData.uctPutCardType = BestCardGroup;
            }
            return;
        }
    }

    //如果没有3-2的非炸牌，则看看有没有单王
    if (clsHandCardData.value_aHandCardList[16] == 1 && clsHandCardData.value_aHandCardList[17] == 0)
    {
        clsHandCardData.value_nPutCardList.push_back(16);
        clsHandCardData.uctPutCardType = get_GroupData(cgSINGLE, 16, 1);
        return;
    }
    if (clsHandCardData.value_aHandCardList[16] == 0 && clsHandCardData.value_aHandCardList[17] == 1)
    {
        clsHandCardData.value_nPutCardList.push_back(17);
        clsHandCardData.uctPutCardType = get_GroupData(cgSINGLE, 17, 1);
        return;
    }

    //单王也没有，出炸弹
    for (int i = 3; i < 16; i++)
    {
        if (clsHandCardData.value_aHandCardList[i] == 4)
        {
            clsHandCardData.value_nPutCardList.push_back(i);
            clsHandCardData.value_nPutCardList.push_back(i);
            clsHandCardData.value_nPutCardList.push_back(i);
            clsHandCardData.value_nPutCardList.push_back(i);

            clsHandCardData.uctPutCardType = get_GroupData(cgBOMB_CARD, i, 4);

            return;
        }
    }

    /*都没有，出王炸*/
    if (clsHandCardData.value_aHandCardList[17] > 0 && clsHandCardData.value_aHandCardList[16] > 0)
    {
        //         clsHandCardData.value_aHandCardList[17] --;
        //         clsHandCardData.value_aHandCardList[16] --;
        //         clsHandCardData.nHandCardCount -= 2;
        //         HandCardValue tmpHandCardValue = get_MaxHandCardValue(clsHandCardData);
        //         clsHandCardData.value_aHandCardList[16] ++;
        //         clsHandCardData.value_aHandCardList[17] ++;
        //         clsHandCardData.nHandCardCount += 2;
        clsHandCardData.value_nPutCardList.push_back(17);
        clsHandCardData.value_nPutCardList.push_back(16);
        clsHandCardData.uctPutCardType = get_GroupData(cgKING_CARD, 17, 2);
        return;
    }

    //异常错误
    clsHandCardData.uctPutCardType = get_GroupData(cgERROR, 0, 0);
    return;

}
void SpliteCard(std::vector<int> arrHandCardList, vector<CardGroupData>& cardTypeArr)
{
    HandCardInfo clsHandCardData;
    clsHandCardData.Init(arrHandCardList);
    if (clsHandCardData.nHandCardCount <= 0)
    {
        return;
    }
    cardTypeArr.clear();
    while (1)
    {
        //先取一个最优牌型
        GetBestCardType(clsHandCardData);
        //要保存当前的clsHandCardData.value_nPutCardList及clsHandCardData.uctPutCardType用于回溯
        cardTypeArr.push_back(clsHandCardData.uctPutCardType);
        //去掉手牌中的最优牌型
        for (vector<int>::iterator iter = clsHandCardData.value_nPutCardList.begin();
            iter != clsHandCardData.value_nPutCardList.end(); iter++)
        {
            clsHandCardData.value_aHandCardList[*iter]--;
        }
        clsHandCardData.nHandCardCount -= clsHandCardData.uctPutCardType.nCount;
        if (clsHandCardData.nHandCardCount == 0)
        {
            break;
        }
    }

    int i = 0;
}


int MakeDeal_RemainCardsHaveCard(IN OUT std::vector<int>& RemainCards, IN int nCardValue) {
    int nTargetCardID = -1;
    int nCardidBase = 0;
    if (nCardValue == 17) {
        nCardidBase = 53;
    }
    else if (nCardValue == 16) {
        nCardidBase = 52;
    }
    else if (nCardValue == 15) {
        nCardidBase = 0;
    }
    else {
        nCardidBase = nCardValue - 2;
    }
    for (std::vector<int>::iterator iter = RemainCards.begin(); iter != RemainCards.end(); iter++)
    {
        if (nCardidBase >= 52 && *iter == nCardidBase) {
            nTargetCardID = nCardidBase;
            RemainCards.erase(iter);
            break;
        }
        else if (nCardidBase <= 12 && nCardidBase >= 0) {
            for (int i = nCardidBase; i <= 51; i += 13)
            {
                if (*iter == i)
                {
                    nTargetCardID = i;
                    RemainCards.erase(iter);
                    break;
                }
            }
            if (nTargetCardID != -1) {
                break;
            }
        }
    }

    return nTargetCardID;
}

ComposeCardResult MakeDeal_ComposeCard(
	LPMAKEDEALCFG pCfg,
    IN OUT std::vector<CardGroupData>& CardGroupDatas,
    IN OUT std::vector<int>& RemainCards,
    IN bool bNeedMakeDeal /*= true*/)  //是否需要干预做牌，如果false，则凑对子
{
    ComposeCardResult stRet = { 0 };
    stRet.bRet = false;
    stRet.ComposeCardGroupType = "error";
    stRet.ComposeCardGroupCardCount = -1;
    stRet.nRemoveCardID = -1;

    int value_aMaxCardList[18] = { 0 };  //存放以牌值3-17为索引的牌张数
    for (size_t j = 0; j < CardGroupDatas.size(); j++)
    {
        value_aMaxCardList[CardGroupDatas[j].nMaxCard] = CardGroupDatas[j].nCount;
    }

    //保存第一个找到的玩家手里不存在缺少的牌值
    int nTmpLackCard = GetValuebyCardid(RemainCards[0]);        //第一个牌作为初始值，防止找不到缺牌
    for (size_t k = 0; k < RemainCards.size(); k++)
    {
        int nTmpMaxValue = GetValuebyCardid(RemainCards[k]);
        if (value_aMaxCardList[nTmpMaxValue] == 0) {
            nTmpLackCard = nTmpMaxValue;
            break;
        }
    }

    if (bNeedMakeDeal == false)
    {
        //发一张保证手数不增加
        for (size_t i = 0; i < CardGroupDatas.size(); i++)
        {
            if (CardGroupDatas[i].cgType == cgSINGLE) //找单牌凑对子
            {
                //判断剩余牌中是否有该牌
                int nTargetCardID = MakeDeal_RemainCardsHaveCard(RemainCards, CardGroupDatas[i].nMaxCard);
                if (nTargetCardID != -1) {

                    CardGroupDatas[i] = get_GroupData(cgDOUBLE, CardGroupDatas[i].nMaxCard, 2);
                    stRet.bRet = true;
                    stRet.ComposeCardGroupType = get_GroupCardName(cgDOUBLE);
                    stRet.ComposeCardGroupCardCount = 2;
                    stRet.nRemoveCardID = nTargetCardID;
                    return stRet;
                }
            }
            else if (CardGroupDatas[i].cgType == cgDOUBLE) //找对子凑3张
            {
                //判断剩余牌中是否有该牌
                int nTargetCardID = MakeDeal_RemainCardsHaveCard(RemainCards, CardGroupDatas[i].nMaxCard);
                if (nTargetCardID != -1) {

                    CardGroupDatas[i] = get_GroupData(cgTHREE, CardGroupDatas[i].nMaxCard, 3);
                    stRet.bRet = true;
                    stRet.ComposeCardGroupType = get_GroupCardName(cgTHREE);
                    stRet.ComposeCardGroupCardCount = 3;
                    stRet.nRemoveCardID = nTargetCardID;
                    return stRet;
                }
            }
        }

        //如果上面都没找到，发一个玩家没有的单牌
        int nTargetCardID = MakeDeal_RemainCardsHaveCard(RemainCards, nTmpLackCard);
        if (nTargetCardID != -1) {

            CardGroupDatas.push_back(get_GroupData(cgSINGLE, nTmpLackCard, 1));
            stRet.bRet = true;
            stRet.ComposeCardGroupType = get_GroupCardName(cgSINGLE);
            stRet.ComposeCardGroupCardCount = 1;
            stRet.nRemoveCardID = nTargetCardID;
            return stRet;
        }
    }
    //组牌凑牌顺序 1 炸弹(OK)：2 飞机（OK）：3 顺子(OK)：4 连对(OK)：5 三张(OK)：6 对子(OK) 如果上面6种斗组合不了，随机发一张单牌
    //凑成功直接返回
	for (size_t i = 0; i < pCfg->arrCouPaiStrategy.size(); i++)
    {
		int nTargetCardType = pCfg->arrCouPaiStrategy[i];
        if (nTargetCardType == cgBOMB_CARD) {
            for (size_t i = 0; i < CardGroupDatas.size(); i++)
            {
                if (CardGroupDatas[i].cgType == cgTHREE) {
                    //判断剩余牌中是否有该牌
                    int nTargetCardID = MakeDeal_RemainCardsHaveCard(RemainCards, CardGroupDatas[i].nMaxCard);
                    if (nTargetCardID != -1) {

                        CardGroupDatas[i] = get_GroupData(cgBOMB_CARD, CardGroupDatas[i].nMaxCard, 4);
                        stRet.bRet = true;
                        stRet.ComposeCardGroupType = get_GroupCardName(cgBOMB_CARD);
                        stRet.ComposeCardGroupCardCount = 4;
                        stRet.nRemoveCardID = nTargetCardID;
                        return stRet;
                    }
                }
            }
        }
        else if (nTargetCardType == cgTHREE_LINE) {
            for (size_t i = 0; i < CardGroupDatas.size(); i++)
            {
                if (CardGroupDatas[i].cgType == cgTHREE && CardGroupDatas[i].nMaxCard != 15 /*为2的不可能组成三连*/) {
                    //找出这个三个牌型的左右牌，在牌组里是否有对子
                    for (size_t j = 0; j < CardGroupDatas.size(); j++)
                    {
                        if (CardGroupDatas[j].cgType == cgDOUBLE && CardGroupDatas[j].nMaxCard <= 14 && CardGroupDatas[j].nMaxCard >= 3)
                        {
                            if (CardGroupDatas[i].nMaxCard - 1 == CardGroupDatas[j].nMaxCard)
                            {
                                //判断剩余牌中是否有该牌
                                int nTargetCardID = MakeDeal_RemainCardsHaveCard(RemainCards, CardGroupDatas[j].nMaxCard);
                                if (nTargetCardID != -1) {
                                    std::vector<int> deletelist; //删除列表，存放，待移除牌型索引,删除牌型要按照顺序从后往前删除
                                    //查找三连是否能组更长
                                    int prov = 0; //prov存放可以达到的顺子最大maxcard
                                    for (size_t q = CardGroupDatas[i].nMaxCard + 1; q <= 14; q++)
                                    {
                                        bool bHasSuitSingle = false;//标识是否有组成更长三连的三条
                                        for (vector<CardGroupData>::iterator iter = CardGroupDatas.begin(); iter != CardGroupDatas.end(); iter++) {
                                            if (iter->cgType == cgTHREE && iter->nMaxCard == q)
                                            {
                                                //找到合适三条，从vector中删除该三条类型
                                                prov++;
                                                deletelist.push_back(iter - CardGroupDatas.begin());
                                                bHasSuitSingle = true;
                                                break;
                                            }
                                        }
                                        if (!bHasSuitSingle) {
                                            break;//如果直接断了，跳出循环
                                        }
                                    }

                                    CardGroupDatas[i] = get_GroupData(cgTHREE_LINE, CardGroupDatas[i].nMaxCard + prov, 6 + prov * 3);
                                    deletelist.push_back(j);
                                    sort(deletelist.begin(), deletelist.end());
                                    for (int i = deletelist.size() - 1; i >= 0; --i)
                                    {
                                        CardGroupDatas.erase(CardGroupDatas.begin() + deletelist[i]);
                                    }
                                    stRet.bRet = true;
                                    stRet.ComposeCardGroupType = get_GroupCardName(cgTHREE_LINE);
                                    stRet.ComposeCardGroupCardCount = 6 + 3 * prov;
                                    stRet.nRemoveCardID = nTargetCardID;
                                    return stRet;
                                }
                            }
                            else if (CardGroupDatas[i].nMaxCard + 1 == CardGroupDatas[j].nMaxCard)
                            {
                                int nTargetCardID = MakeDeal_RemainCardsHaveCard(RemainCards, CardGroupDatas[j].nMaxCard);
                                if (nTargetCardID != -1) {
                                    std::vector<int> deletelist; //删除列表，存放，待移除牌型索引,删除牌型要按照顺序从后往前删除
                                    //查找三连是否能组更长
                                    int prov = 0; //prov存放可以达到的顺子最大maxcard
                                    for (size_t q = CardGroupDatas[j].nMaxCard + 1; q <= 14; q++)
                                    {
                                        bool bHasSuitSingle = false;//标识是否有组成更长三连的三条
                                        for (vector<CardGroupData>::iterator iter = CardGroupDatas.begin(); iter != CardGroupDatas.end(); iter++) {
                                            if (iter->cgType == cgTHREE && iter->nMaxCard == q)
                                            {
                                                //找到合适三条，从vector中删除该三条类型
                                                prov++;
                                                deletelist.push_back(iter - CardGroupDatas.begin());
                                                bHasSuitSingle = true;
                                                break;
                                            }
                                        }
                                        if (!bHasSuitSingle) {
                                            break;//如果直接断了，跳出循环
                                        }
                                    }

                                    CardGroupDatas[i] = get_GroupData(cgTHREE_LINE, CardGroupDatas[j].nMaxCard + prov, 6 + 3 * prov);
                                    deletelist.push_back(j);
                                    sort(deletelist.begin(), deletelist.end());
                                    for (int i = deletelist.size() - 1; i >= 0; --i)
                                    {
                                        CardGroupDatas.erase(CardGroupDatas.begin() + deletelist[i]);
                                    }
                                    stRet.bRet = true;
                                    stRet.ComposeCardGroupType = get_GroupCardName(cgTHREE_LINE);
                                    stRet.ComposeCardGroupCardCount = 6 + 3 * prov;
                                    stRet.nRemoveCardID = nTargetCardID;
                                    return stRet;
                                }
                            }

                        }
                    }
                }
            }
        }
        else if (nTargetCardType == cgSINGLE_LINE) {
            std::map<int, int> tmpMaxValueCountMap;       //牌值对应牌张数，用来判断对子还是单牌
            std::map<int, int> tmpMaxValueVectorIndexMap; //牌value和vector中对应索引位置
            for (size_t i = 0; i < CardGroupDatas.size(); i++)
            {
                //目前只拆对子
                if (CardGroupDatas[i].cgType == cgSINGLE)
                {
                    tmpMaxValueCountMap[CardGroupDatas[i].nMaxCard] = 1;
                }
                else if (CardGroupDatas[i].cgType == cgDOUBLE)
                {
                    tmpMaxValueCountMap[CardGroupDatas[i].nMaxCard] = 2;
                }
                tmpMaxValueVectorIndexMap[CardGroupDatas[i].nMaxCard] = i;
            }

            //从3到A去寻找可以配成5张单连的牌
            //3 4 5 6 7
            //9 10 11 12 13
            //10 11 12 13 14
            for (size_t j = 3; j <= 10; j++)
            {
                int nCtDoubleCount = 0;//记录对子个数
                int nLostCardCount = 0;//缺少牌个数
                int nNeedCard = -1;    //需要发的牌
                int nNeedChaiCard = -1; //需要拆的对子
                for (size_t k = j; k <= j + 4; k++)
                {
                    if (tmpMaxValueCountMap[k] == 0) {
                        nLostCardCount++;
                        nNeedCard = k;
                    }
                    if (tmpMaxValueCountMap[k] == 2)//判断是不是对子
                    {
                        nCtDoubleCount++;
                        nNeedChaiCard = k;
                    }
                }
                if (nLostCardCount == 1 && nCtDoubleCount <= 1 && nNeedCard != -1) {

                    int nTargetCardID = MakeDeal_RemainCardsHaveCard(RemainCards, nNeedCard);
                    if (nTargetCardID != -1) {
                        //满足条件可以凑顺子
                        std::vector<int> tmpSort;
                        for (size_t k = 0; k <= 4; k++)
                        {
                            if (j + k != nNeedCard) {
                                tmpSort.push_back(tmpMaxValueVectorIndexMap[j + k]);
                            }
                        }
                        sort(tmpSort.begin(), tmpSort.end());
                        for (size_t p = 0; p < tmpSort.size(); p++)
                        {
                            CardGroupDatas.erase(CardGroupDatas.begin() + tmpSort[3 - p]);
                        }

                        //查找单连是否能组更长
                        int prov = 4; //prov存放可以达到的顺子最大maxcard
                        for (size_t q = j + 5; q <= 14; q++)
                        {
                            bool bHasSuitSingle = false;//标识是否有组成更长单连的单牌
                            for (vector<CardGroupData>::iterator iter = CardGroupDatas.begin(); iter != CardGroupDatas.end(); iter++) {
                                if (iter->cgType == cgSINGLE && iter->nMaxCard == q)
                                {
                                    //找到合适单牌，从vector中删除该单牌
                                    prov++;
                                    CardGroupDatas.erase(iter);
                                    bHasSuitSingle = true;
                                    break;
                                }
                            }
                            if (!bHasSuitSingle) {
                                break;//如果直接断了，跳出循环
                            }
                        }

                        CardGroupDatas.push_back(get_GroupData(cgSINGLE_LINE, j + prov, prov + 1));//插入顺子
                        if (nCtDoubleCount == 1) {
                            CardGroupDatas.push_back(get_GroupData(cgSINGLE, nNeedChaiCard, 1));//插入拆下来的单牌
                        }

                        stRet.bRet = true;
                        stRet.ComposeCardGroupType = get_GroupCardName(cgSINGLE_LINE);
                        stRet.ComposeCardGroupCardCount = prov + 1;
                        stRet.nRemoveCardID = nTargetCardID;
                        return stRet;
                    }
                }
            }
        }
        else if (nTargetCardType == cgDOUBLE_LINE) {
            std::map<int, int> tmpMaxValueMap;
            for (size_t c = 0; c < CardGroupDatas.size(); c++)
            {
                if (CardGroupDatas[c].cgType == cgDOUBLE) {
                    tmpMaxValueMap[CardGroupDatas[c].nMaxCard] = c;
                }
            }

            for (size_t i = 0; i < CardGroupDatas.size(); i++)
            {
                //找出所有的单牌，遍历单牌左右对子是否存在，双右对子是否存在，双做对子是否存在，如果满足其中一种，补上该单牌构成连对（考虑边界）
                if (CardGroupDatas[i].cgType == cgSINGLE && CardGroupDatas[i].nMaxCard != 15 /*为2的不可能组成双连*/) { //3 4 5 ... 10 13 14
                    if (CardGroupDatas[i].nMaxCard == 3) {
                        //判断 4 ，5对子是否存在
                        if (tmpMaxValueMap.count(4) > 0 && tmpMaxValueMap.count(5) > 0) {
                            int nTargetCardID = MakeDeal_RemainCardsHaveCard(RemainCards, 3);
                            if (nTargetCardID != -1) {
                                std::vector<int> deletelist; //删除列表，存放，待移除牌型索引,删除牌型要按照顺序从后往前删除

                                //查找双连是否能组更长
                                int prov = 0; //prov存放可以达到的顺子最大maxcard
                                for (size_t q = 6; q <= 14; q++)
                                {
                                    bool bHasSuitSingle = false;//标识是否有组成更长双连的对子
                                    for (vector<CardGroupData>::iterator iter = CardGroupDatas.begin(); iter != CardGroupDatas.end(); iter++) {
                                        if (iter->cgType == cgDOUBLE && iter->nMaxCard == q)
                                        {
                                            //找到合适对子，从vector中删除该对子类型
                                            prov++;
                                            deletelist.push_back(iter - CardGroupDatas.begin());
                                            bHasSuitSingle = true;
                                            break;
                                        }
                                    }
                                    if (!bHasSuitSingle) {
                                        break;//如果直接断了，跳出循环
                                    }
                                }


                                CardGroupDatas[i] = get_GroupData(cgDOUBLE_LINE, 5 + prov, 6 + prov * 2);
                                deletelist.push_back(tmpMaxValueMap[4]);
                                deletelist.push_back(tmpMaxValueMap[5]);
                                sort(deletelist.begin(), deletelist.end());
                                for (int i = deletelist.size() - 1; i >= 0; --i)
                                {
                                    CardGroupDatas.erase(CardGroupDatas.begin() + deletelist[i]);
                                }
                                stRet.bRet = true;
                                stRet.ComposeCardGroupType = get_GroupCardName(cgDOUBLE_LINE);
                                stRet.ComposeCardGroupCardCount = 6 + prov * 2;
                                stRet.nRemoveCardID = nTargetCardID;
                                return stRet;
                            }
                        }
                    }
                    else if (CardGroupDatas[i].nMaxCard == 4)
                    {
                        //判断 3 ， 5对子是否存在
                        if (tmpMaxValueMap.count(3) > 0 && tmpMaxValueMap.count(5) > 0) {
                            int nTargetCardID = MakeDeal_RemainCardsHaveCard(RemainCards, 4);
                            if (nTargetCardID != -1) {
                                std::vector<int> deletelist; //删除列表，存放，待移除牌型索引,删除牌型要按照顺序从后往前删除

                                //查找双连是否能组更长
                                int prov = 0; //prov存放可以达到的顺子最大maxcard
                                for (size_t q = 6; q <= 14; q++)
                                {
                                    bool bHasSuitSingle = false;//标识是否有组成更长双连的对子
                                    for (vector<CardGroupData>::iterator iter = CardGroupDatas.begin(); iter != CardGroupDatas.end(); iter++) {
                                        if (iter->cgType == cgDOUBLE && iter->nMaxCard == q)
                                        {
                                            //找到合适对子，从vector中删除该对子类型
                                            prov++;
                                            deletelist.push_back(iter - CardGroupDatas.begin());
                                            bHasSuitSingle = true;
                                            break;
                                        }
                                    }
                                    if (!bHasSuitSingle) {
                                        break;//如果直接断了，跳出循环
                                    }
                                }

                                CardGroupDatas[i] = get_GroupData(cgDOUBLE_LINE, 5 + prov, 6 + prov * 2);
                                deletelist.push_back(tmpMaxValueMap[3]);
                                deletelist.push_back(tmpMaxValueMap[5]);
                                sort(deletelist.begin(), deletelist.end());
                                for (int i = deletelist.size() - 1; i >= 0; --i)
                                {
                                    CardGroupDatas.erase(CardGroupDatas.begin() + deletelist[i]);
                                }
                                stRet.bRet = true;
                                stRet.ComposeCardGroupType = get_GroupCardName(cgDOUBLE_LINE);
                                stRet.ComposeCardGroupCardCount = 6 + prov * 2;
                                stRet.nRemoveCardID = nTargetCardID;
                                return stRet;
                            }
                        }

                        //判断 5 ， 6对子是否存在
                        if (tmpMaxValueMap.count(5) > 0 && tmpMaxValueMap.count(6) > 0) {
                            int nTargetCardID = MakeDeal_RemainCardsHaveCard(RemainCards, 4);
                            if (nTargetCardID != -1) {
                                std::vector<int> deletelist; //删除列表，存放，待移除牌型索引,删除牌型要按照顺序从后往前删除

                                //查找双连是否能组更长
                                int prov = 0; //prov存放可以达到的顺子最大maxcard
                                for (size_t q = 7; q <= 14; q++)
                                {
                                    bool bHasSuitSingle = false;//标识是否有组成更长双连的对子
                                    for (vector<CardGroupData>::iterator iter = CardGroupDatas.begin(); iter != CardGroupDatas.end(); iter++) {
                                        if (iter->cgType == cgDOUBLE && iter->nMaxCard == q)
                                        {
                                            //找到合适对子，从vector中删除该对子类型
                                            prov++;
                                            deletelist.push_back(iter - CardGroupDatas.begin());
                                            bHasSuitSingle = true;
                                            break;
                                        }
                                    }
                                    if (!bHasSuitSingle) {
                                        break;//如果直接断了，跳出循环
                                    }
                                }

                                CardGroupDatas[i] = get_GroupData(cgDOUBLE_LINE, 6 + prov, 6 * prov * 2);
                                deletelist.push_back(tmpMaxValueMap[5]);
                                deletelist.push_back(tmpMaxValueMap[6]);
                                sort(deletelist.begin(), deletelist.end());
                                for (int i = deletelist.size() - 1; i >= 0; --i)
                                {
                                    CardGroupDatas.erase(CardGroupDatas.begin() + deletelist[i]);
                                }
                                stRet.bRet = true;
                                stRet.ComposeCardGroupType = get_GroupCardName(cgDOUBLE_LINE);
                                stRet.ComposeCardGroupCardCount = 6 * prov * 2;
                                stRet.nRemoveCardID = nTargetCardID;
                                return stRet;
                            }
                        }
                    }
                    else if (CardGroupDatas[i].nMaxCard == 13)
                    {
                        //判断12，14对子是否存在
                        if (tmpMaxValueMap.count(12) > 0 && tmpMaxValueMap.count(14) > 0) {
                            int nTargetCardID = MakeDeal_RemainCardsHaveCard(RemainCards, 13);
                            if (nTargetCardID != -1) {
                                CardGroupDatas[i] = get_GroupData(cgDOUBLE_LINE, 14, 6);
                                CardGroupDatas.erase(CardGroupDatas.begin() + max(tmpMaxValueMap[12], tmpMaxValueMap[14]));
                                CardGroupDatas.erase(CardGroupDatas.begin() + min(tmpMaxValueMap[12], tmpMaxValueMap[14]));
                                stRet.bRet = true;
                                stRet.ComposeCardGroupType = get_GroupCardName(cgDOUBLE_LINE);
                                stRet.ComposeCardGroupCardCount = 6;
                                stRet.nRemoveCardID = nTargetCardID;
                                return stRet;
                            }
                        }
                        //判断11，12对子是否存在
                        if (tmpMaxValueMap.count(11) > 0 && tmpMaxValueMap.count(12) > 0) {
                            int nTargetCardID = MakeDeal_RemainCardsHaveCard(RemainCards, 13);
                            if (nTargetCardID != -1) {
                                std::vector<int> deletelist; //删除列表，存放，待移除牌型索引,删除牌型要按照顺序从后往前删除
                                //查找双连是否能组更长
                                int prov = 0; //prov存放可以达到的顺子最大maxcard
                                for (size_t q = 14; q <= 14; q++)
                                {
                                    bool bHasSuitSingle = false;//标识是否有组成更长双连的对子
                                    for (vector<CardGroupData>::iterator iter = CardGroupDatas.begin(); iter != CardGroupDatas.end(); iter++) {
                                        if (iter->cgType == cgDOUBLE && iter->nMaxCard == q)
                                        {
                                            //找到合适对子，从vector中删除该对子类型
                                            prov++;
                                            deletelist.push_back(iter - CardGroupDatas.begin());
                                            bHasSuitSingle = true;
                                            break;
                                        }
                                    }
                                    if (!bHasSuitSingle) {
                                        break;//如果直接断了，跳出循环
                                    }
                                }
                                CardGroupDatas[i] = get_GroupData(cgDOUBLE_LINE, 13 + prov, 6 + prov * 2);
                                deletelist.push_back(tmpMaxValueMap[11]);
                                deletelist.push_back(tmpMaxValueMap[12]);
                                sort(deletelist.begin(), deletelist.end());
                                for (int i = deletelist.size() - 1; i >= 0; --i)
                                {
                                    CardGroupDatas.erase(CardGroupDatas.begin() + deletelist[i]);
                                }
                                stRet.bRet = true;
                                stRet.ComposeCardGroupType = get_GroupCardName(cgDOUBLE_LINE);
                                stRet.ComposeCardGroupCardCount = 6 + prov * 2;
                                stRet.nRemoveCardID = nTargetCardID;
                                return stRet;
                            }
                        }
                    }
                    else if (CardGroupDatas[i].nMaxCard == 14)
                    {
                        //判断12，13对子是否存在
                        if (tmpMaxValueMap.count(12) > 0 && tmpMaxValueMap.count(13) > 0) {
                            int nTargetCardID = MakeDeal_RemainCardsHaveCard(RemainCards, 14);
                            if (nTargetCardID != -1) {
                                CardGroupDatas[i] = get_GroupData(cgDOUBLE_LINE, 14, 6);
                                CardGroupDatas.erase(CardGroupDatas.begin() + max(tmpMaxValueMap[12], tmpMaxValueMap[13]));
                                CardGroupDatas.erase(CardGroupDatas.begin() + min(tmpMaxValueMap[12], tmpMaxValueMap[13]));
                                stRet.bRet = true;
                                stRet.ComposeCardGroupType = get_GroupCardName(cgDOUBLE_LINE);
                                stRet.ComposeCardGroupCardCount = 6;
                                stRet.nRemoveCardID = nTargetCardID;
                                return stRet;
                            }
                        }
                    }
                    else {
                        //判断CardGroupDatas[i].nMaxCard-1，CardGroupDatas[i].nMaxCard-2 
                        if (tmpMaxValueMap.count(CardGroupDatas[i].nMaxCard - 1) > 0 && tmpMaxValueMap.count(CardGroupDatas[i].nMaxCard - 2) > 0) {
                            int nTargetCardID = MakeDeal_RemainCardsHaveCard(RemainCards, CardGroupDatas[i].nMaxCard);
                            if (nTargetCardID != -1) {
                                std::vector<int> deletelist; //删除列表，存放，待移除牌型索引,删除牌型要按照顺序从后往前删除
                                //查找双连是否能组更长
                                int prov = 0; //prov存放可以达到的顺子最大maxcard
                                for (size_t q = CardGroupDatas[i].nMaxCard + 1; q <= 14; q++)
                                {
                                    bool bHasSuitSingle = false;//标识是否有组成更长双连的对子
                                    for (vector<CardGroupData>::iterator iter = CardGroupDatas.begin(); iter != CardGroupDatas.end(); iter++) {
                                        if (iter->cgType == cgDOUBLE && iter->nMaxCard == q)
                                        {
                                            //找到合适对子，从vector中删除该对子类型
                                            prov++;
                                            deletelist.push_back(iter - CardGroupDatas.begin());
                                            bHasSuitSingle = true;
                                            break;
                                        }
                                    }
                                    if (!bHasSuitSingle) {
                                        break;//如果直接断了，跳出循环
                                    }
                                }

                                CardGroupDatas[i] = get_GroupData(cgDOUBLE_LINE, CardGroupDatas[i].nMaxCard + prov, 6 + prov * 2);
                                deletelist.push_back(tmpMaxValueMap[CardGroupDatas[i].nMaxCard - 1]);
                                deletelist.push_back(tmpMaxValueMap[CardGroupDatas[i].nMaxCard - 2]);
                                sort(deletelist.begin(), deletelist.end());
                                for (int i = deletelist.size() - 1; i >= 0; --i)
                                {
                                    CardGroupDatas.erase(CardGroupDatas.begin() + deletelist[i]);
                                }
                                stRet.bRet = true;
                                stRet.ComposeCardGroupType = get_GroupCardName(cgDOUBLE_LINE);
                                stRet.ComposeCardGroupCardCount = 6 + prov * 2;
                                stRet.nRemoveCardID = nTargetCardID;
                                return stRet;
                            }
                        }

                        //判断CardGroupDatas[i].nMaxCard+1，CardGroupDatas[i].nMaxCard+2
                        if (tmpMaxValueMap.count(CardGroupDatas[i].nMaxCard + 1) > 0 && tmpMaxValueMap.count(CardGroupDatas[i].nMaxCard + 2) > 0) {
                            int nTargetCardID = MakeDeal_RemainCardsHaveCard(RemainCards, CardGroupDatas[i].nMaxCard);
                            if (nTargetCardID != -1) {
                                std::vector<int> deletelist; //删除列表，存放，待移除牌型索引,删除牌型要按照顺序从后往前删除
                                //查找双连是否能组更长
                                int prov = 0; //prov存放可以达到的顺子最大maxcard
                                for (size_t q = CardGroupDatas[i].nMaxCard + 2 + 1; q <= 14; q++)
                                {
                                    bool bHasSuitSingle = false;//标识是否有组成更长双连的对子
                                    for (vector<CardGroupData>::iterator iter = CardGroupDatas.begin(); iter != CardGroupDatas.end(); iter++) {
                                        if (iter->cgType == cgDOUBLE && iter->nMaxCard == q)
                                        {
                                            //找到合适对子，从vector中删除该对子类型
                                            prov++;
                                            deletelist.push_back(iter - CardGroupDatas.begin());
                                            bHasSuitSingle = true;
                                            break;
                                        }
                                    }
                                    if (!bHasSuitSingle) {
                                        break;//如果直接断了，跳出循环
                                    }
                                }

                                CardGroupDatas[i] = get_GroupData(cgDOUBLE_LINE, CardGroupDatas[i].nMaxCard + 2 + prov, 6 + prov * 2);
                                deletelist.push_back(tmpMaxValueMap[CardGroupDatas[i].nMaxCard + 1]);
                                deletelist.push_back(tmpMaxValueMap[CardGroupDatas[i].nMaxCard + 2]);
                                sort(deletelist.begin(), deletelist.end());
                                for (int i = deletelist.size() - 1; i >= 0; --i)
                                {
                                    CardGroupDatas.erase(CardGroupDatas.begin() + deletelist[i]);
                                }
                                stRet.bRet = true;
                                stRet.ComposeCardGroupType = get_GroupCardName(cgDOUBLE_LINE);
                                stRet.ComposeCardGroupCardCount = 6 + prov * 2;
                                stRet.nRemoveCardID = nTargetCardID;
                                return stRet;
                            }
                        }
                        //判断 CardGroupDatas[i].nMaxCard -1 CardGroupDatas[i].nMaxCard + 1 对子是否存在
                        if (tmpMaxValueMap.count(CardGroupDatas[i].nMaxCard - 1) > 0 && tmpMaxValueMap.count(CardGroupDatas[i].nMaxCard + 1) > 0) {
                            int nTargetCardID = MakeDeal_RemainCardsHaveCard(RemainCards, CardGroupDatas[i].nMaxCard);
                            if (nTargetCardID != -1) {
                                std::vector<int> deletelist; //删除列表，存放，待移除牌型索引,删除牌型要按照顺序从后往前删除
                                //查找双连是否能组更长
                                int prov = 0; //prov存放可以达到的顺子最大maxcard
                                for (size_t q = CardGroupDatas[i].nMaxCard + 1 + 1; q <= 14; q++)
                                {
                                    bool bHasSuitSingle = false;//标识是否有组成更长双连的对子
                                    for (vector<CardGroupData>::iterator iter = CardGroupDatas.begin(); iter != CardGroupDatas.end(); iter++) {
                                        if (iter->cgType == cgDOUBLE && iter->nMaxCard == q)
                                        {
                                            //找到合适对子，从vector中删除该对子类型
                                            prov++;
                                            deletelist.push_back(iter - CardGroupDatas.begin());
                                            bHasSuitSingle = true;
                                            break;
                                        }
                                    }
                                    if (!bHasSuitSingle) {
                                        break;//如果直接断了，跳出循环
                                    }
                                }

                                CardGroupDatas[i] = get_GroupData(cgDOUBLE_LINE, CardGroupDatas[i].nMaxCard + 1 + prov, 6 + prov * 2);
                                deletelist.push_back(tmpMaxValueMap[CardGroupDatas[i].nMaxCard - 1]);
                                deletelist.push_back(tmpMaxValueMap[CardGroupDatas[i].nMaxCard + 1]);
                                sort(deletelist.begin(), deletelist.end());
                                for (int i = deletelist.size() - 1; i >= 0; --i)
                                {
                                    CardGroupDatas.erase(CardGroupDatas.begin() + deletelist[i]);
                                }
                                stRet.bRet = true;
                                stRet.ComposeCardGroupType = get_GroupCardName(cgDOUBLE_LINE);
                                stRet.ComposeCardGroupCardCount = 6 + prov * 2;
                                stRet.nRemoveCardID = nTargetCardID;
                                return stRet;
                            }
                        }
                    }
                }
            }
        }
        else if (nTargetCardType == cgTHREE) {
            for (size_t i = 0; i < CardGroupDatas.size(); i++)
            {
                if (CardGroupDatas[i].cgType == cgDOUBLE) {
                    //判断剩余牌中是否有该牌
                    int nTargetCardID = MakeDeal_RemainCardsHaveCard(RemainCards, CardGroupDatas[i].nMaxCard);
                    if (nTargetCardID != -1) {

                        CardGroupDatas[i] = get_GroupData(cgTHREE, CardGroupDatas[i].nMaxCard, 3);
                        stRet.bRet = true;
                        stRet.ComposeCardGroupType = get_GroupCardName(cgTHREE);
                        stRet.ComposeCardGroupCardCount = 3;
                        stRet.nRemoveCardID = nTargetCardID;
                        return stRet;
                    }
                }
            }
        }
        else if (nTargetCardType == cgDOUBLE) {
            for (size_t i = 0; i < CardGroupDatas.size(); i++)
            {
                if (CardGroupDatas[i].cgType == cgSINGLE) {
                    //判断剩余牌中是否有该牌
                    int nTargetCardID = MakeDeal_RemainCardsHaveCard(RemainCards, CardGroupDatas[i].nMaxCard);
                    if (nTargetCardID != -1) {

                        CardGroupDatas[i] = get_GroupData(cgDOUBLE, CardGroupDatas[i].nMaxCard, 2);
                        stRet.bRet = true;
                        stRet.ComposeCardGroupType = get_GroupCardName(cgDOUBLE);
                        stRet.ComposeCardGroupCardCount = 2;
                        stRet.nRemoveCardID = nTargetCardID;
                        return stRet;
                    }
                }
            }
        }
    }



    //如果玩家手牌里没有王，优先尝试到剩余牌里发一张王给该玩家
    if (value_aMaxCardList[16] == 0 && value_aMaxCardList[17] == 0)
    {
        //尝试发小王
        int nTargetCardID = MakeDeal_RemainCardsHaveCard(RemainCards, 16);
        if (nTargetCardID != -1) {
            CardGroupDatas.push_back(get_GroupData(cgSINGLE, 16, 1));
            stRet.bRet = true;
            stRet.ComposeCardGroupType = get_GroupCardName(cgSINGLE);
            stRet.ComposeCardGroupCardCount = 1;
            stRet.nRemoveCardID = nTargetCardID;
            return stRet;
        }
        //尝试发大王
        nTargetCardID = MakeDeal_RemainCardsHaveCard(RemainCards, 17);
        if (nTargetCardID != -1) {
            CardGroupDatas.push_back(get_GroupData(cgSINGLE, 17, 1));
            stRet.bRet = true;
            stRet.ComposeCardGroupType = get_GroupCardName(cgSINGLE);
            stRet.ComposeCardGroupCardCount = 1;
            stRet.nRemoveCardID = nTargetCardID;
            return stRet;
        }
    }

    //如果上面凑牌逻辑都不合适，发一张随机缺乏的单牌
    //如果做牌情况下都没凑成合适牌型，发一个玩家没有的单牌
    int nTargetCardID = MakeDeal_RemainCardsHaveCard(RemainCards, nTmpLackCard);
    if (nTargetCardID != -1) {
        CardGroupDatas.push_back(get_GroupData(cgSINGLE, nTmpLackCard, 1));
        stRet.bRet = true;
        stRet.ComposeCardGroupType = get_GroupCardName(cgSINGLE);
        stRet.ComposeCardGroupCardCount = 1;
        stRet.nRemoveCardID = nTargetCardID;
        return stRet;
    }

    return stRet;
}


//根据牌id获取牌权重值
int GetValuebyCardid(int cardid) {
    int ret = 0;
    if (cardid < 52) {
        int tmp = cardid % SK_LAYOUT_MOD;
        ret = (tmp == 0 ? 15 : tmp + 2);
    }
    else if (cardid == 52) { //小王权重
        ret = 16;
    }
    else if (cardid == 53) { //大王权重
        ret = 17;
    }

    return ret;
}

string GetCardValuebyCardIndex(int cardindex) {
    string ret;
    switch (cardindex)
    {
    case 1:
        ret = "2";
        break;
    case 2:
        ret = "3";
        break;
    case 3:
        ret = "4";
        break;
    case 4:
        ret = "5";
        break;
    case 5:
        ret = "6";
        break;
    case 6:
        ret = "7";
        break;
    case 7:
        ret = "8";
        break;
    case 8:
        ret = "9";
        break;
    case 9:
        ret = "10";
        break;
    case 10:
        ret = "J";
        break;
    case 11:
        ret = "Q";
        break;
    case 12:
        ret = "K";
        break;
    case 13:
        ret = "A";
        break;
    case 14:
        ret = "小王";
        break;
    case 15:
        ret = "大王";
        break;

    default:
        ret = "X";
        break;
    }
    return ret;
}

int SK_GetCardIndex(int nCardID)
{
    nCardID = nCardID % 54;
    if (nCardID == 52)
        return 14;
    else if (nCardID == 53)
        return 15;
    else
        return nCardID % SK_LAYOUT_MOD + 1;
}

int  SK_GetCardShape(int nCardID)
{
    nCardID = nCardID % 54;
    if (nCardID >= 52)
        return PK_CS_JOKER;
    else
        return nCardID / SK_LAYOUT_MOD;
}

std::string get_GroupCardName(int GroupCardType) {
    char szoutput[32];
    switch (GroupCardType)
    {
    case cgERROR:
        sprintf(szoutput, "%s", "错误");
        break;//错误类型
    case cgZERO:
        sprintf(szoutput, "%s", "不出");
        break;//不出类型
    case cgSINGLE:
        sprintf(szoutput, "%s", "单牌");
        break;//单牌类型
    case cgDOUBLE:
        sprintf(szoutput, "%s", "对牌");
        break;//对牌类型
    case cgTHREE:
        sprintf(szoutput, "%s", "三条");
        break;//三条类型
    case cgSINGLE_LINE:
        sprintf(szoutput, "%s", "单连");
        break;//单连类型
    case cgDOUBLE_LINE:
        sprintf(szoutput, "%s", "对连");
        break;//对连类型
    case cgTHREE_LINE:
        sprintf(szoutput, "%s", "三连类型");
        break;//三连类型
    case cgTHREE_TAKE_ONE:
        sprintf(szoutput, "%s", "三带一单");
        break;//三带一单
    case cgTHREE_TAKE_TWO:
        sprintf(szoutput, "%s", "三带一对");
        break;//三带一对
    case cgTHREE_TAKE_ONE_LINE:
        sprintf(szoutput, "%s", "三带一单连");
        break;//三带一单连
    case cgTHREE_TAKE_TWO_LINE:
        sprintf(szoutput, "%s", "三带一对连");
        break;//三带一对连
    case cgFOUR_TAKE_ONE:
        sprintf(szoutput, "%s", "四带两单");
        break;//四带两单
    case cgFOUR_TAKE_TWO:
        sprintf(szoutput, "%s", "四带两对");
        break;//四带两对
    case cgBOMB_CARD:
        sprintf(szoutput, "%s", "炸弹");
        break;//炸弹类型
    case cgKING_CARD:
        sprintf(szoutput, "%s", "王炸");
        break;//王炸类型
    default:
        break;
    }
    return std::string(szoutput);
}

std::string get_CardsName(int cardid) {
    char szoutput[32];
    switch (SK_GetCardShape(cardid))
    {
    case PK_CS_CLUB:
        sprintf(szoutput, "%s%s", "梅花", GetCardValuebyCardIndex(SK_GetCardIndex(cardid)).c_str());
        break;
    case PK_DIAMOND:
        sprintf(szoutput, "%s%s", "方块", GetCardValuebyCardIndex(SK_GetCardIndex(cardid)).c_str());
        break;
    case PK_HEART:
        sprintf(szoutput, "%s%s", "红心", GetCardValuebyCardIndex(SK_GetCardIndex(cardid)).c_str());
        break;
    case PK_JOKER:
        if (cardid == 52) {
            sprintf(szoutput, "%s", "小王");
        }
        else if (cardid == 53) {
            sprintf(szoutput, "%s", "大王");
        }
        break;
    case PK_SPADE:
        sprintf(szoutput, "%s%s", "黑桃", GetCardValuebyCardIndex(SK_GetCardIndex(cardid)).c_str());
        break;
    default:
        break;
    }

    return std::string(szoutput);
}
//计算牌型的手数及总牌值
void CalHandCardValue(std::vector<CardGroupData>& CardGroupDatas, int& nHandCount, int& nHandCardAveValue) {
    sort(CardGroupDatas.begin(), CardGroupDatas.end(), [](CardGroupData a, CardGroupData b) {
        return a.nValue > b.nValue ? true : false;
    });
    int nLesserCount = 0;
    int nHandCardTotalValue = 0;
    for (int i = 0; i < CardGroupDatas.size(); i++)
    {
        nHandCardTotalValue += CardGroupDatas[i].nValue;
        if (CardGroupDatas[i].cgType == cgTHREE
            || CardGroupDatas[i].cgType == cgTHREE_LINE)
        {
            nLesserCount += CardGroupDatas[i].nCount / 3;
        }
    }
    nHandCount = CardGroupDatas.size();
    for (int i = nHandCount - 1; i >= 0 && nLesserCount > 0; i--, nLesserCount--)
    {
        if ((CardGroupDatas[i].cgType == cgSINGLE
            || CardGroupDatas[i].cgType == cgDOUBLE)
            && CardGroupDatas[i].nMaxCard < 15)
        {
            nHandCount--;
            nHandCardTotalValue -= CardGroupDatas[i].nValue;
        }
    }
    nHandCardAveValue = nHandCardTotalValue;
}
