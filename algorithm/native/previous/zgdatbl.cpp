#ifdef _DEBUG
#define new DEBUG_NEW
#endif

#include "StdAfx.h"
#include <algorithm>
#include <functional>
#include <vector>
#include <ConfigManagerSys.h>
#include "MakeDealHelper.h"
#include "Server.h"
#include <landlord.hpp>
//using namespace std;

const int ROBOTTYPE_PROTECT = 200; // 机器人保护类型，200以上为保护类型

int My_GetRandomBetweenEx(int nMin, int nMax)
{
	if (nMax == nMin){
		return nMax;
	}
	int milliSec = GetCurTimeStampMilli() % 1000;
	srand((unsigned)time(NULL) + milliSec * 7937);
	//srand((unsigned)time(NULL));

	return  nMin + (rand() % (nMax - nMin + 1));
}

CGameTable::CGameTable(int roomid, int tableno, int score_mult, 
			   int totalchairs, DWORD gameflags, DWORD gameflags2, int max_asks,
			   int totalcards, 
			   int totalpacks, int chaircards, int bottomcards,
			   int layoutnum, int layoutmod, int layoutnumex, 
			   int abtpairs[],
			   int throwwait, int maxautothrow, int entrustwait,
			   int max_auction, int min_auction, int def_auction,
			   FP_SK_GetXXX fpSKGetCardIndex,
			   FP_SK_GetXXX fpSKGetCardShape,
			   FP_SK_GetXXX fpSKGetCardValue,
			   FP_SK_GetXXX fpSKGetCardScore,
			   FP_SK_GetXXXEx fpSKGetCardPRI,
			   FP_SK_GetXXXEx fpSKGetIndexPRI)
			   :CSkTable(roomid,tableno,score_mult,totalchairs,gameflags,gameflags2,max_asks,
			   totalcards,
			   totalpacks,chaircards,bottomcards,
			   layoutnum,layoutmod,layoutnumex,abtpairs,
			   throwwait,maxautothrow, entrustwait,
			   max_auction,min_auction,def_auction,
			   fpSKGetCardIndex,fpSKGetCardShape,fpSKGetCardValue,fpSKGetCardScore,fpSKGetCardPRI,fpSKGetIndexPRI)
{
	// 固定信息，跟局数无关
	m_GameTalbeInfo=NULL;
    m_pGameServer = NULL;
	//m_pReportrRecord = NULL;
	ResetMembers();
	ResetPlayerReportedStatus();
}

CGameTable::~CGameTable()
{
 	ResetMembers();
	
	if (m_GameTalbeInfo)
		delete m_GameTalbeInfo;
}

void InitialGameTableInfo(GAME_TABLE_INFO* table)
{
	if (!table) 
		return;
	ZeroMemory(table, sizeof(GAME_TABLE_INFO));
	table->m_Public.nWaitChair=-1;
	table->m_Public.nCurrentRank=1;//定死打1
	XygInitChairCards(table->gamestart.nHandID,CHAIR_CARDS);

	for(int i=0;i<TOTAL_CHAIRS;i++)
	{
		table->m_Player[i].nThrowTime = THROW_WAIT;
	}
}

//扩大了级牌的权值
int  My_GetIndexPRI(int nCardID/*nCardIndex*/,int nRank, DWORD gameflags)
{
	if (nCardID==14)
	{
		return 41;
	}
	else if (nCardID==15)
	{
		return 42;
	}
	else if (nCardID==nRank)
	{
		return 40;
	}
	else
	{
		return nCardID;
	}
}

GAME_TABLE_INFO* CGameTable::GetGameTableInfo()
{
	if (!m_GameTalbeInfo)
		ConstructGameData();

	return m_GameTalbeInfo;
}

GAME_PUBLIC_INFO* CGameTable::GetPublicInfo()
{
	if (!m_GameTalbeInfo)
		ConstructGameData();

	return &m_GameTalbeInfo->m_Public;
}

GAME_PLAYER_INFO* CGameTable::GetPlayerInfo(int chairno)
{
	if (!m_GameTalbeInfo)
		ConstructGameData();

	return &m_GameTalbeInfo->m_Player[chairno];
}

void CGameTable::ResetMembers(BOOL bResetAll)
{
	CSkTable::ResetMembers(bResetAll);

	memset(m_nFirstRazzCardsAlter, 0, sizeof(int)*MAX_RAZZ_COUNT);
	m_nFirstChairNo=-1;
	m_nSecondChairNo=-1;
	m_dwFirstCardType=-1;
	m_dwSecondCardType=-1;


	m_nObjectGains = 0;
	m_nRobCount = 0;
	m_nRazzCardValue = 0;
	m_bIsRazzMode = FALSE;
	m_nCardTypeLimit = 0;
	m_nRoomSilverLimit = 0;
	memset(m_nRazzCardsAlter, 0, sizeof(int)*MAX_RAZZ_COUNT);
	InitialGameTableInfo(m_GameTalbeInfo);
	memset(m_nAutoPlayCount,0,sizeof(m_nAutoPlayCount));
	memset(m_nBombHadDeal,-1,sizeof(m_nBombHadDeal));
	memset(m_nBottomCatch,-1,sizeof(m_nBottomCatch));
	memset(m_Rob, 0, sizeof(m_Rob));
	m_nOperateTime = THROW_WAIT;
	int i = 0;
	if(bResetAll){
		// 动态信息，跟上局相关
		if (m_GameTalbeInfo)
		{	
			for(i=0;i<TOTAL_CHAIRS;i++)
				m_GameTalbeInfo->m_Player[i].nWaitTime=THROW_WAIT;
		}

		m_nAuctionRound = 0;

		memset(m_nResultDiff,0,sizeof(m_nResultDiff));
		memset(m_nTotalResult,0,sizeof(m_nTotalResult));

		m_nScoreFee = 0;
	}

	m_nCurRobotAIType = ROBOTAI_JUNIOR; //默认使用JuniorAI
	m_nRemoteAITimeout = 0;
	m_bIsRemoteAIRobotPeerBottomEnable = FALSE;
	memset(m_nPassTimes, 0, sizeof(m_nPassTimes));
	memset(m_initHandCards, 0, sizeof(m_initHandCards));
	m_nRoundCount = 0;
	memset(m_peeredBottomPlayer, 0, sizeof(m_peeredBottomPlayer));
	m_bIsRemind=FALSE;
	memset(&m_razzCardsAlterValueUnit,0,sizeof(RAZZCARDS_ALTERVALUE_UNIT));
	memset(&m_boutDataCache, 0, sizeof(BOUTDATACACHE));

	m_nSuppRessChairNo = -1;
	m_nCompensate = 0;

	memset(&m_nCardType, 0, sizeof(int) * 10);
	memset(&m_nThrowCardCounts, 0, sizeof(int) * 5 * TOTAL_CHAIRS);
	m_nSectionNum = 0;

	for (i = 0; i < TOTAL_CHAIRS; i++)
	{
		m_nContinueWin[i] = 0;
		m_nLastWaterTime[i] = 0;
		m_nUseCardMaster[i] = 0;
		jsonAddInfo[i].clear();
		m_jsonMatchInfo[i].clear();
		m_jsonDdzTaskInfo[i].clear();
	}

	m_bIsMatchGame = FALSE;
	memset(m_nMatchAbtWinCount, 0, sizeof(m_nMatchAbtWinCount));

	m_sinRecord.str("");
	m_bNoShuffMakeDeal = FALSE;
	m_b2K = FALSE;
	m_nBoutBeginTime = -1;

	m_dwAuctionFinishTime = 0;

	memset(m_PlayerDouble, 0, sizeof(m_PlayerDouble));
	m_bAlreadThrowed = FALSE;

	
	memset(m_nMagnificationTheory, 0, sizeof(m_nMagnificationTheory));
	memset(m_nPlayerIpData, 0, sizeof(m_nPlayerIpData));
	memset(m_nMakeDealTypes, 0, sizeof(m_nMakeDealTypes));
	memset(m_nAILevel, 0, sizeof(m_nAILevel));
	m_bIsProtected = false;
}

void CGameTable::ResetTable()
{
	CTable::ResetTable();

	memset(m_nResultDiff,0,sizeof(m_nResultDiff));
	memset(m_nTotalResult,0,sizeof(m_nTotalResult));
	m_nAuctionRound = 0;
	m_nOperateTime = THROW_WAIT;
	m_nSectionNum = 0;
	m_nScoreFee = 0;

	//游戏结束后如果有玩家离开桌ResetTable函数会被调用
}

void CGameTable::SetScoreFee(int nScoreFee)
{
	m_nScoreFee = nScoreFee;
}

int	CGameTable::CalcWinFeesScore(int nOldScores1[], int nOldScores2[], int nScoreDiffs[], int nWinFees[])
{
	int totalfee = 0;

	for (int i = 0; i < m_nTotalChairs; i++)
	{
		//nOldDeposits2用于扣除服务费后，矫正输赢
		nOldScores2[i] = nOldScores1[i];
	}

	//先扣除服务费
	for (int i = 0; i < m_nTotalChairs; i++)
	{
		if (nOldScores1[i] < m_nScoreFee)
		{
			nWinFees[i] = nOldScores1[i];
			totalfee += nOldScores1[i];
			nOldScores2[i] = 0;
		}
		else
		{
			nWinFees[i] = m_nScoreFee;
			totalfee += m_nScoreFee;
			nOldScores2[i] = nOldScores1[i] - m_nScoreFee;
		}
	}

	return totalfee;
}

int	CGameTable::CompensateScores(int nOldScores[], int nScoreDiffs[])
{
	if (IS_BIT_SET(m_dwGameFlags, GF_LEVERAGE_ALLOWED)){ // 允许以小博大

	}
	else{ // 不允许以小博大

		/*nOldScores[0] = 10000;
		nOldScores[1] = 10000;
		nOldScores[2] = 800;
		nOldScores[3] = 2500;

		nScoreDiffs[0] = 1000;
		nScoreDiffs[1] = 1000;
		nScoreDiffs[2] = 1000;
		nScoreDiffs[3] = -3000;

		m_nBanker = 3; */

		int nScoreMin = 0;
		int nChairNO = -1;
		for (int i = 0; i < m_nTotalChairs; i++)
		{
			if (nOldScores[i]>0)
			{
				if (0 == nScoreMin || nOldScores[i] < nScoreMin)
				{
					nScoreMin = nOldScores[i];
					nChairNO = i;
				}
			}
		}

		int nMinChair = -1;
		int nBankerDiff = nScoreDiffs[m_nBanker];
		if (nBankerDiff > 0) //地主赢
		{
			if (nBankerDiff > nScoreMin * 2) //农民有最小携带，地主赢不到理论值
			{
				nMinChair = nChairNO;
				nScoreDiffs[m_nBanker] = nScoreMin * 2;
				for (int i = 0; i < m_nTotalChairs; i++)
				{
					if (i != m_nBanker)
						nScoreDiffs[i] = -nScoreMin;
				}
			}
		}
		else
		{
			if (abs(nBankerDiff) > nScoreMin * 2) //地主、农民有最小携带，地主输不到理论值
			{
				nMinChair = nChairNO;
				nScoreDiffs[m_nBanker] = -nScoreMin * 2;
				for (int i = 0; i < m_nTotalChairs; i++)
				{
					if (i != m_nBanker)
						nScoreDiffs[i] = nScoreMin;
				}
			}

			//地主携银不够输的
			if (abs(nScoreDiffs[m_nBanker]) > nOldScores[m_nBanker])
			{
				nMinChair = m_nBanker;
				nScoreDiffs[m_nBanker] = -nOldScores[m_nBanker];
				for (int i = 0; i < m_nTotalChairs; i++)
				{
					if (i != m_nBanker)
						nScoreDiffs[i] = nOldScores[m_nBanker] / 2;
				}
			}
		}
	}
	return CalcSurplus(nScoreDiffs);
}

BOOL CGameTable::CheckScoreResults(int nScoreDiffs[], int nWinFees[], int totalfee)
{
	int surplus = CalcSurplus(nScoreDiffs);

	if (surplus + totalfee > 0){ // 产生假分!!!
		UwlLogFile("CheckScoreResults Errors!");
		for (int i = 0; i < m_nTotalChairs; i++){
			UwlLogFile("Chair:%d nScoreDiffs:%d nWinFees:%d", i, nScoreDiffs[i], nWinFees[i]);
			nScoreDiffs[i] = 0;
			nWinFees[i] = 0;
		}
		return FALSE;
	}
	return TRUE;
}

int CGameTable::GetGameTableInfoSize()
{
	return sizeof(GAME_TABLE_INFO);
}

void CGameTable::FillupGameTableInfo(void* pData, int nLen, int chairno, BOOL lookon)
{
	ZeroMemory(pData, nLen);
	FillupStartData(pData,nLen);

	//断线前的耗时
	if (m_dwActionStart>0)
	{
		GAME_START_INFO* pStart = (GAME_START_INFO*)pData;
		pStart->nReserved[0] = (GetTickCount()-m_dwActionStart)/1000;	//断线前的耗时
		if (pStart->nReserved[0]<0)
			pStart->nReserved[0] = 0;
		if (pStart->nReserved[0]>m_nOperateTime-5)
			pStart->nReserved[0] = m_nOperateTime-5;	//至少留5秒
	}

	//跳过GAME_START_INFO,拷贝公共信息
	GAME_TABLE_INFO* pGameInfo=(GAME_TABLE_INFO*) pData;
	//拷贝公共信息
	GAME_TABLE_INFO* pGameData=GetGameTableInfo();

	memcpy(&pGameInfo->m_Public,&(pGameData->m_Public),sizeof(GAME_PUBLIC_INFO));
	//拷贝玩家私密信息
	for(int k=0;k<TOTAL_CHAIRS;k++)
	{
		GetPlayerInfo(k)->nInHandCount=XygCardRemains(m_nCardsLayIn[k],SK_LAYOUT_NUM);
		memcpy(&pGameInfo->m_Player[k],&(pGameData->m_Player[k]),sizeof(GAME_PLAYER_INFO));
	}

	if (!IS_BIT_SET(m_dwStatus,TS_WAITING_AUCTION))
	{
		memcpy(pGameInfo->gamestart.nBottomIDs, m_nBottomIDs, sizeof(pGameInfo->gamestart.nBottomIDs));
	}

	if (!lookon)
	{
		pGameInfo->m_Player[chairno].nAskExitCount = m_nAskExit[chairno];
	}

	BOOL bHaveCard = HaveCards(chairno);
	for(int i=0;i<TOTAL_CARDS;i++)
	{
		if (pGameInfo->m_Public.GameCard[i].nCardStatus==CARD_STATUS_INHAND)
		{
			if (XygGetOptionOneTrue(m_dwRoomConfig, TOTAL_CHAIRS, RC_CLOAKING)) 
			{
				if (lookon)
				{
				}
				else if (pGameInfo->m_Public.GameCard[i].nChairNO!=chairno)
				{
					pGameInfo->m_Public.GameCard[i].nCardID=-1;
				}
			}
			else
			{
				if (lookon || pGameInfo->m_Public.GameCard[i].nChairNO!=chairno)
				{
					pGameInfo->m_Public.GameCard[i].nCardID=-1;
				}
			}
			
			int chairno=pGameInfo->m_Public.GameCard[i].nChairNO;
			pGameInfo->m_Public.GameCard[i].nUniteCount=GetPlayerInfo(chairno)->nInHandCount;
		}
		if (pGameInfo->m_Public.GameCard[i].nCardStatus==CARD_STATUS_WAITDEAL
			&& IS_BIT_SET(m_dwStatus,TS_WAITING_AUCTION))
		{
			pGameInfo->m_Public.GameCard[i].nCardID=-1;
		}
	}


	//玩家状态
	memcpy(pGameInfo->m_Public.dwUserStatus,m_dwUserStatus,sizeof(pGameInfo->m_Public.dwUserStatus));
	pGameInfo->m_Public.nAuctionRound = m_nAuctionRound;
	if(m_bIsRazzMode)
	{
		pGameInfo->m_Public.nRazzAlterValueInfo = 0;
		//每4位存储1张癞子牌改变的牌值，最多4张癞子牌
		pGameInfo->m_Public.nRazzAlterValueInfo |= 0x000F&m_nRazzCardsAlter[0];
		pGameInfo->m_Public.nRazzAlterValueInfo |= 0x00F0&(m_nRazzCardsAlter[1]<<4);
		pGameInfo->m_Public.nRazzAlterValueInfo |= 0x0F00&(m_nRazzCardsAlter[2]<<8);
		pGameInfo->m_Public.nRazzAlterValueInfo |= 0xF000&(m_nRazzCardsAlter[3]<<12);
		
		//兼容线上版本写入的是16-19位
		pGameInfo->m_Public.nRazzAlterValueInfo |= 0x000F0000 &(m_nRazzCardsAlter[3]<<16);

	
		pGameInfo->m_Public.nReserved[0] = 0;   	//存储上上次打出的癞子牌改变的牌值
		//每4位存储1张癞子牌改变的牌值，最多4张癞子牌
		pGameInfo->m_Public.nReserved[0] |= 0x000F&m_nFirstRazzCardsAlter[0];
		pGameInfo->m_Public.nReserved[0] |= 0x00F0&(m_nFirstRazzCardsAlter[1]<<4);
		pGameInfo->m_Public.nReserved[0] |= 0x0F00&(m_nFirstRazzCardsAlter[2]<<8);
		pGameInfo->m_Public.nReserved[0] |= 0xF000&(m_nFirstRazzCardsAlter[3]<<12);

		//兼容线上版本写入的是16-19位
		pGameInfo->m_Public.nReserved[0] |= 0x000F0000 & (m_nFirstRazzCardsAlter[3] << 16);

		pGameInfo->m_Public.nReserved[1] = m_nFirstChairNo;    //上上次打出牌的椅子号
		pGameInfo->m_Public.nReserved[2] = m_dwFirstCardType;  //上上次打出牌的牌型
	} 
	
	memcpy(pGameInfo->m_Public.nResultDiff,m_nResultDiff,sizeof(m_nResultDiff));
	memcpy(pGameInfo->m_Public.nTotalResult,m_nTotalResult,sizeof(m_nTotalResult));



}

int CGameTable::GetEnterGameInfoSize()
{
	return sizeof(GAME_ENTER_INFO);
}

void CGameTable::FillupEnterGameInfo(void* pData, int nLen, int chairno, BOOL lookon)
{
	CTable::FillupEnterGameInfo(pData,nLen,chairno,lookon);

	GAME_ENTER_INFO* pEnterGame=(GAME_ENTER_INFO*)pData;

	memcpy(pEnterGame->nResultDiff,m_nResultDiff,sizeof(m_nResultDiff));
	memcpy(pEnterGame->nTotalResult,m_nTotalResult,sizeof(m_nTotalResult));

	// 进入游戏成功,填充房间加倍信息
	auto doubleComInfo = ReadDoubleCommonInfo();
	pEnterGame->nReserve[0] = doubleComInfo.m_nDoubleType;
	pEnterGame->nReserve[1] = doubleComInfo.m_nDoubleWaitTime;
	pEnterGame->nReserve[2] = doubleComInfo.m_nSuperDoubleCost;
}


int CGameTable::GetGameStartSize()
{
	return sizeof(GAME_START_INFO);
}

void CGameTable::FillupGameStart(void* pData, int nLen, int chairno, BOOL lookon)
{
	ZeroMemory(pData, nLen);
	FillupStartData(pData,nLen);
	GAME_START_INFO* pStartData=(GAME_START_INFO*)pData;

	pStartData->nInHandCount = CARDS_PER_CHAIR;
	pStartData->bIsCrazyMode = m_bIsCrazyMode;

	if (!lookon)
	{
		XygInitChairCards(pStartData->nHandID,CHAIR_CARDS);
		GetInHandCard(chairno,pStartData->nHandID);
// 		GetInHandCard(0, pStartData->nHandID1);
// 		GetInHandCard(1, pStartData->nHandID2);
// 		GetInHandCard(2, pStartData->nHandID3);
	}
}

int CGameTable::GetGameWinSize()
{
    return sizeof(GAME_WIN_RESULT);
}

int CGameTable::FillupGameWin(void* pData, int nLen, int chairno)
{
	GAME_WIN_RESULT* pGameWin=(GAME_WIN_RESULT*)pData;
	ZeroMemory(pGameWin,sizeof(GAME_WIN_RESULT));

	for(int i=0;i<m_nTotalChairs;i++)
	{
		XygInitChairCards(pGameWin->nCardID[i],CHAIR_CARDS);
		pGameWin->nCardCount[i]=GetInHandCard(i,pGameWin->nCardID[i]);
	}

	//底牌
	memcpy(pGameWin->nBottomIDs, m_nBottomIDs, sizeof(pGameWin->nBottomIDs));

	memcpy(pGameWin->szUsername, m_szUsername, sizeof(pGameWin->szUsername));
	pGameWin->nReserved[3] = m_bIsCrazyMode;

	if (m_bNeedDeposit){ //银子房走原来逻辑
		int nResult = CTable::FillupGameWin(pData, nLen, chairno);

		pGameWin->nReserved[2] = m_nCompensate;

		return nResult;
	}
	else{ //积分房积分fee, 针对pc积分转银子处理
		pGameWin->nReserved[2] = m_nCompensate;

		LPGAME_WIN pGameWinData = (LPGAME_WIN)pData;

		pGameWinData->dwWinFlags = m_dwWinFlags;
		pGameWinData->nTotalChairs = m_nTotalChairs;
		pGameWinData->nBoutCount = m_nBoutCount;
		pGameWinData->nBanker = m_nBanker;

		memcpy(pGameWinData->nPartnerGroup, m_nPartnerGroup, sizeof(m_nPartnerGroup));

		pGameWinData->nBaseScore = m_nBaseScore;
		pGameWinData->nBaseDeposit = m_nBaseDeposit;

		//填充pGameWin->nOldScores和pGameWin->nOldDeposits，参与玩家之前的积分和银两
		FillupOldScoreDeposit(pData, nLen);

		(void)CalcWinPoints(pData, nLen, chairno, pGameWinData->nWinPoints);
		pGameWinData->bBankWin = IsBankWin(pData, nLen, chairno);
		(void)CalcResultDiffs(pData, nLen, pGameWinData->nScoreDiffs, pGameWinData->nDepositDiffs);
		(void)CalcResultDiffsEx(pData, nLen, pGameWinData->nScoreDiffs, pGameWinData->nDepositDiffs);

		if (m_nBaseScore){
			assert(0 == CalcSurplus(pGameWinData->nScoreDiffs));

			int totalfee = 0;
			int	nOldScores[MAX_CHAIRS_PER_TABLE];						// 旧积分
			memset(nOldScores, 0, sizeof(nOldScores));

			//计算茶水费以及去掉茶水费后的原有积分
			totalfee = CalcWinFeesScore(pGameWinData->nOldScores, nOldScores, pGameWinData->nScoreDiffs, pGameWinData->nWinFees);

			//矫正输赢
			(void)CompensateScores(nOldScores, pGameWinData->nScoreDiffs);

			//输赢+服务费
			for (int i = 0; i < m_nTotalChairs; i++)
			{
				pGameWinData->nScoreDiffs[i] -= pGameWinData->nWinFees[i];
			}

			//验证输赢
			(void)CheckScoreResults(pGameWinData->nScoreDiffs, pGameWinData->nWinFees, totalfee);
		}

		(void)CalcLevelIDs(pGameWinData->nOldScores, pGameWinData->nScoreDiffs, pGameWinData->nLevelIDs);

		pGameWinData->dwNextFlags |= IsNextBoutNoLeave(pData, nLen);
		pGameWinData->dwNextFlags |= IsNextBoutBankerReset(pData, nLen);

		//Add on 20130503
		//是否是还未结算的非Idle玩家，给个状态	
		//低位0～7位表示各个玩家状态，1为IdlePlayer，已经结算的空闲玩家，或者后来进入的空闲玩家
		FillPlayerStatus(pData, nLen);
		//Add end

		return 1;
	}
}

void CGameTable::FillupStartData(void* pData, int nLen)
{
	GAME_START_INFO* pStartData=(GAME_START_INFO*)pData;
	memcpy(pStartData->szSerialNO, m_szSerialNO, sizeof(m_szSerialNO));
	pStartData->dwStatus=m_dwStatus;
	pStartData->nBoutCount=m_nBoutCount;
	pStartData->nBaseScore=m_nBaseScore;
	pStartData->nBaseDeposit=m_nBaseDeposit;
	
	pStartData->bForbidDesert=m_bForbidDesert;  //禁止强退,提醒凯子发布的时候打开
	pStartData->bNeedDeposit=m_bNeedDeposit;
	
	pStartData->nBanker=m_nBanker;
	pStartData->nCurrentChair=GetCurrentChair();
	pStartData->nAutoGiveUp=SK_AUTO_END_GAME;
	pStartData->nOffline=SK_AUTO_CHECK_OFFLINE;
	pStartData->nThrowWait=m_nOperateTime;
	pStartData->bIsCrazyMode=m_bIsCrazyMode;
	pStartData->nObjectGains=m_nObjectGains;
	if(m_bIsRazzMode)
	{
		pStartData->nRazzModeInfo |= 0x000F&m_nRazzCardValue;	//低4位存储癞子牌牌值
		pStartData->nRazzModeInfo |= 0x0001<<4;	                //高4位存储是否癞子场的标识
	}
}	

int	CGameTable::CalcBaseDeposit(int nDeposits[], int tableno)
{
	assert(m_nTotalChairs <= MAX_CHAIRS_PER_TABLE);
	
	CString strIniFile = GetINIFileName();

	//获取底注系数
	int nCoefficient = GetPrivateProfileInt(
		_T("ChangeableBaseDeposit"),	// section name
		_T("Coefficient"),				// key name
		DEFAULT_BASE_DEPOSIT_COEFFICIENT,	// default int
		strIniFile						// initialization file name
			);

	if (0 >= nCoefficient)
		nCoefficient = DEFAULT_BASE_DEPOSIT_COEFFICIENT;

	//获取底注倍数
	int nMultiple = GetPrivateProfileInt(
		_T("ChangeableBaseDeposit"),	// section name
		_T("Multiple"),					// key name
		DEFAULT_BASE_DEPOSIT_MULTIPLE,	// default int
		strIniFile						// initialization file name
			);

	if (0 >= nMultiple)
		nMultiple = DEFAULT_BASE_DEPOSIT_MULTIPLE;

	//获取获取最大可变基础银
	int nMaxBaseDeposit = GetPrivateProfileInt(
		_T("ChangeableBaseDeposit"),	// section name
		_T("MaxBaseDeposit"),			// key name
		DEFAULT_MAX_BASE_DEPOSIT,		// default int
		strIniFile						// initialization file name
		);

	//获取房间最小可变基础银
	TCHAR szRoomID[16];
	memset(szRoomID, 0, sizeof(szRoomID));
	_stprintf(szRoomID, _T("%ld"), m_nRoomID);
	int nMinDeposit = GetPrivateProfileInt(
		_T("MinChangeableBaseDeposit"),	// section name
		szRoomID,						// key name
		DEFAULT_MIN_CHANGE_BASE_DEPOSIT,// default int
		strIniFile						// initialization file name
			);

	int nBase = 0;
	int mindeposit = nDeposits[0];
	for(int i = 1; i < m_nTotalChairs; i++){
		if(nDeposits[i] < mindeposit){
			mindeposit = nDeposits[i];
		}
	}
	int nTemp = mindeposit;
	if(nTemp <= 100){
		nTemp = 100;
	}
	nTemp /= nCoefficient;
	if (1 > nTemp)
		nTemp = 1;

	nTemp = UwlLog2(nTemp);
	
	nBase = UwlPow2(nTemp) * nMultiple;

	if (nBase < nMinDeposit)
		nBase = nMinDeposit;

	if (nBase > nMaxBaseDeposit)
		nBase = nMaxBaseDeposit;
	
	return nBase;
}

int CGameTable::CalcWinFeesEx(int nOldDeposits1[], int nOldDeposits2[], int nDepositDiffs[], int nWinFees[])
{
	int totalfee = 0; 


	for(int i = 0; i < m_nTotalChairs; i++)
	{ 
		//nOldDeposits2用于扣除服务费后，矫正输赢
		nOldDeposits2[i] = nOldDeposits1[i];
	}

	//服务费模式，先扣除服务费
	if (FEE_MODE_SERVICE_FIXED == m_nFeeMode)
	{
		//先扣除服务费
		for(int i = 0; i < m_nTotalChairs; i++)
		{ 
			if (nOldDeposits1[i]<m_nFeeValue)
			{
				nWinFees[i] = nOldDeposits1[i];
				totalfee += nOldDeposits1[i];
				nOldDeposits2[i] = 0;
			}
			else
			{
				nWinFees[i] = m_nFeeValue;
				totalfee += m_nFeeValue;
				nOldDeposits2[i] = nOldDeposits1[i]-m_nFeeValue;
				
			}
		}
	}
	else if (FEE_MODE_SERVICE_MINDEPOSIT == m_nFeeMode)
	{
		int depositMin = 0;
		for(int i = 0; i < m_nTotalChairs; i++)
		{
			if (nOldDeposits1[i]>0)
			{
				if ((0 == depositMin)
					|| (nOldDeposits1[i] < depositMin))
					depositMin = nOldDeposits1[i];
			}
		}

		if (IsVariableChairRoom())
		{
			if (depositMin>=m_nFeeMinimum)
			{
				int fee = ceil((double)depositMin*m_nFeeTenThousandth/10000);
				for(int i = 0; i < m_nTotalChairs; i++)
				{
					if (m_PlayersBackup[i].nUserID>0
						&& m_PlayersBackup[i].nDeposit>0)
					{
						if (nOldDeposits1[i]<fee)
						{
							nWinFees[i] = nOldDeposits1[i];
							totalfee += nOldDeposits1[i];
							nOldDeposits2[i] = 0;
						}
						else
						{
							nWinFees[i] = fee;
							totalfee += fee;
							nOldDeposits2[i] = nOldDeposits1[i]-fee;
						}
					}
				}
			}
		}
		else 
		{
			int fee = ceil((double)m_nBaseDeposit*m_nFeeTenThousandth/10000);

			//获取房间最小可变茶水费
			TCHAR szRoomID[16];
			memset(szRoomID, 0, sizeof(szRoomID));
			_stprintf(szRoomID, _T("%ld"), m_nRoomID);
			int nMinFeeValue = GetPrivateProfileInt(
				_T("MinChangeableFeeValue"),	// section name
				szRoomID,						// key name
				DEFAULT_MIN_CHANGE_FEE_VALUE,	// default int
				GetINIFileName()				// initialization file name
			);

			if (fee < nMinFeeValue)
				fee = nMinFeeValue;

			for(int i = 0; i < m_nTotalChairs; i++)
			{
				if (nOldDeposits1[i]<fee)
				{
					nWinFees[i] = nOldDeposits1[i];
					totalfee += nOldDeposits1[i];
					nOldDeposits2[i] = 0;
				}
				else
				{
					nWinFees[i] = fee;
					totalfee += fee;
					nOldDeposits2[i] = nOldDeposits1[i]-fee;
				}
			}
		}
	}
	else if (FEE_MODE_SERVICE_SELFDEPOSIT == m_nFeeMode)
	{
		for(int i = 0; i < m_nTotalChairs; i++)
		{
			if (nOldDeposits1[i]>=m_nFeeMinimum)
			{
				int fee = ceil((double)nOldDeposits1[i]*m_nFeeTenThousandth/10000);
				if (nOldDeposits1[i]<fee)
				{
					nWinFees[i] = nOldDeposits1[i];
					totalfee += nOldDeposits1[i];
					nOldDeposits2[i] = 0;
				}
				else
				{
					nWinFees[i] = fee;
					totalfee += fee;
					nOldDeposits2[i] = nOldDeposits1[i]-fee;
				}
			}
		}
	}

	return totalfee;
}

int CGameTable::CalcBankerChairBefore()
{
	int result = 0;
// 	if(1 == m_nBoutCount)
// 	{
// 		result = XygGetRandomBetween(m_nTotalChairs);	
// 	}else
// 	{
// 		result = m_nBanker;
// 	}
	result = XygGetRandomBetween(m_nTotalChairs); 
	//result = 2;
	return result;
}

int	CGameTable::CalcBanker(BOOL isFixBankerToSoleRealPlayer)
{
	CString strIniFile = GetINIFileName();
	TCHAR szRoomID[16];
	memset(szRoomID, 0, sizeof(szRoomID));
	_stprintf(szRoomID, _T("%ld"), m_nRoomID);

	BOOL bRobotSpecialAuctionMode = GetPrivateProfileInt(
		_T("RobotSpecialAuctionMode"),//是否是机器人特殊叫地主模式
		szRoomID,
		FALSE,
		strIniFile);

	if ((TRUE == bRobotSpecialAuctionMode || isFixBankerToSoleRealPlayer == TRUE) && 2 == GetRobotCount())
	{//机器人特殊叫地主模式
		for (int i=0; i<TOTAL_CHAIRS; i++)
		{
			if (NULL != m_ptrPlayers[i] && m_ptrPlayers[i]->m_nUserType != USER_TYPE_ROBOT)
			{
				m_nBanker = i;
				break;
			}
		}
		
		if (-1 == m_nBanker)
			m_nBanker = CalcBankerChairBefore();
	}
	else
	{
		m_nBanker = CalcBankerChairBefore();
	}

	return m_nBanker;
}

int CGameTable::CalcRazzCardValue()
{
	int result = 0;
	//result = XygGetRandomBetweenEx(0, TOTAL_CARDS-3);//除去大小王
	result = My_GetRandomBetweenEx(0, TOTAL_CARDS - 3);
	return GetCardValueById(result);
}


int CGameTable::CalcBankerChairAfter(void* pData, int nLen)
{
    return m_nBanker;
	//return GetNextChair(m_nBanker);
}

BOOL CGameTable::CalcWinPoints(void* pData, int nLen, int chairno, int nWinPoints[])
{
	GAME_WIN_RESULT* pGameWin = (GAME_WIN_RESULT*)pData;

	if (m_dwWinFlags==GW_NORMAL)
	{
		BOOL bBankerWin = FALSE;
		if (chairno==m_nBanker)
			bBankerWin = TRUE;

		//炸弹翻番
		pGameWin->nBombFan = GetPublicInfo()->nBombFan;

		int nFarmer[TOTAL_CHAIRS-1];
		int nFarmerCount=0;
		for (int i=0;i<TOTAL_CHAIRS;i++)
		{
			if (i!=m_nBanker)
				nFarmer[nFarmerCount++]=i;
		}
		//农民一张未出翻番
		if (bBankerWin
			&& GetPlayerInfo(nFarmer[0])->nThrowCount==0
			&& GetPlayerInfo(nFarmer[1])->nThrowCount==0)
		{
			pGameWin->nSpring++;
		}
		//地主只出过一次翻番
		if (!bBankerWin
			&& GetPlayerInfo(m_nBanker)->nThrowCount==1)
		{
			pGameWin->nSpring++;
		}
		//检查一遍加倍情况
		for (int i = 0; i < TOTAL_CHAIRS; i++)
		{
			if (m_PlayerDouble[i] != 1 && m_PlayerDouble[i] != 2 && m_PlayerDouble[i] != 4)
			{
				m_PlayerDouble[i] = 1;
			}
			pGameWin->nPlayerDouble[i] = m_PlayerDouble[i];

		}
		int gains = m_nObjectGains * UwlPow2(pGameWin->nSpring+pGameWin->nBombFan);
		if (bBankerWin)
		{
			nWinPoints[m_nBanker] += gains * m_PlayerDouble[m_nBanker] * (m_PlayerDouble[nFarmer[0]] + m_PlayerDouble[nFarmer[1]]);
			nWinPoints[nFarmer[0]] -= gains * m_PlayerDouble[m_nBanker] *  m_PlayerDouble[nFarmer[0]];
			nWinPoints[nFarmer[1]] -= gains * m_PlayerDouble[m_nBanker] * m_PlayerDouble[nFarmer[1]];
		}
		else
		{
			nWinPoints[m_nBanker] -= gains * m_PlayerDouble[m_nBanker] * (m_PlayerDouble[nFarmer[0]] + m_PlayerDouble[nFarmer[1]]);
			nWinPoints[nFarmer[0]] += gains * m_PlayerDouble[m_nBanker] * m_PlayerDouble[nFarmer[0]];
			nWinPoints[nFarmer[1]] += gains * m_PlayerDouble[m_nBanker] * m_PlayerDouble[nFarmer[1]];
		}

		// 记录下实际倍率
		for (int i = 0; i < TOTAL_CHAIRS; i++)
        {
			m_nMagnificationTheory[i] = nWinPoints[i];
        }
	}

	/////////////////////////////////////////////////////
	int total=0;
	for(int i = 0; i < m_nTotalChairs; i++)
	total+=nWinPoints[i];
    if (total!=0)
	{
		  UwlLogFile("错误的记分错误，记分总和不为0!");
		  for(int i = 0; i < m_nTotalChairs; i++)
		  nWinPoints[i]=0;
	}
	////////////////////////////////////////////////////*/
	
	if (m_bIsMatchGame)
	{
		int limit = GetPrivateProfileInt(_T("MatchGame"), "multilimit", 64, GetINIFileName());
		int nMultiple[TOTAL_CHAIRS];
		ZeroMemory(nMultiple, sizeof(nMultiple));

		for (int i = 0; i < m_nTotalChairs; i++)
		{
			// m_jsonMatchInfo[i]["winorder"] = Json::Int(GetPublicInfo()->nWinChairs[i]);
			m_jsonMatchInfo[i]["winorder"] = Json::Int(0);
			m_jsonMatchInfo[i]["doublemulti"] = Json::Int(0);
			m_jsonMatchInfo[i]["abtwinmulti"] = Json::Int(0);
			m_jsonMatchInfo[i]["winmulti"] = Json::Int(nWinPoints[i]);
			m_jsonMatchInfo[i]["totalmulti"] = Json::Int(nWinPoints[i]);

			/*if (nWinPoints[i] > 0)
			{
				m_jsonMatchInfo[i]["doublemulti"] = Json::Int(0);
				m_jsonMatchInfo[i]["abtwinmulti"] = Json::Int(0);
				m_jsonMatchInfo[i]["winmulti"] = Json::Int(nWinPoints[i]);

				//双扣加成 TODO 改成斗地主条件
				if (IS_BIT_SET(m_dwWinFlags, SK_GW_COUPLE))
				{
					if (i == nFirst || i == nSecond)
					{
						nMultiple[i]++;
						m_jsonMatchInfo[i]["doublemulti"] = Json::Int(2);
					}
				}
				

				//连胜加成
				if (m_nMatchAbtWinCount[i] > 0)
				{
					m_nMatchAbtWinCount[i] += 1; //加上本局的次数
					if (m_nMatchAbtWinCount[i] > 1)
					{
						nMultiple[i] += m_nMatchAbtWinCount[i] - 1;
						m_jsonMatchInfo[i]["abtwinmulti"] = Json::Int(std::pow(2, m_nMatchAbtWinCount[i] - 1));
					}
				}
				

				//加成上限
				nMultiple[i] = std::pow(2, nMultiple[i]);
				nMultiple[i] = (nMultiple[i] > limit) ? limit : nMultiple[i];
				

				//总倍数
				nWinPoints[i] *= nMultiple[i];
				m_jsonMatchInfo[i]["totalmulti"] = Json::Int(nWinPoints[i]);
				

				LOG_DEBUG("MatchGame %s: RoomID:%d, TableNO:%d, ChairNO:%d, UserID:%d, MultiWin:%d, MultiSK:%d, MultiABT:%d, MultiTotal:%d", __FUNCTION__,
					m_nRoomID, m_nTableNO, i, m_ptrPlayers[i] ? m_ptrPlayers[i]->m_nUserID : 0, m_jsonMatchInfo[i]["winmulti"].asInt(), m_jsonMatchInfo[i]["doublemulti"].asInt(), m_jsonMatchInfo[i]["abtwinmulti"].asInt(), m_jsonMatchInfo[i]["totalmulti"].asInt());
			}
			else
			{
				m_jsonMatchInfo[i]["doublemulti"] = Json::Int(0);
				m_jsonMatchInfo[i]["abtwinmulti"] = Json::Int(0);
				m_jsonMatchInfo[i]["winmulti"] = Json::Int(nWinPoints[i]);
				m_jsonMatchInfo[i]["totalmulti"] = Json::Int(nWinPoints[i]);
			}*/
		}
	}

	return TRUE;
}




int CGameTable::CalcResultDiffs(void* pData, int nLen, int nScoreDiffs[], int nDepositDiffs[])
{
	LPGAME_WIN pGameWin = (LPGAME_WIN)pData;
	
	if (m_bNeedDeposit && m_nBaseDeposit) {
		
		// 玩银子，计算银子结果
		for (int i = 0; i < m_nTotalChairs; i++)
		{
			nDepositDiffs[i] = m_nBaseDeposit * pGameWin->nWinPoints[i];
		}
	}
	else {
		// 玩积分
		if (m_bIsMatchGame)
		{
			for (int i = 0; i < m_nTotalChairs; i++)
			{
				m_jsonMatchInfo[i]["basescore"] = Json::Int(m_nBaseScore);
				m_jsonMatchInfo[i]["score"] = Json::Int(m_nBaseScore * pGameWin->nWinPoints[i]);
				LOG_DEBUG("MatchGame %s: RoomID:%d, TableNO:%d, ChairNO:%d, UserID:%d, BaseScore:%d, MatchScore:%d", __FUNCTION__,
					m_nRoomID, m_nTableNO, i, m_ptrPlayers[i] ? m_ptrPlayers[i]->m_nUserID : 0, m_nBaseScore, m_jsonMatchInfo[i]["score"].asInt());
			}
		}
		else{
			// 计算积分结果
			for (int i = 0; i < m_nTotalChairs; i++)
			{
				nScoreDiffs[i] = m_nBaseScore * pGameWin->nWinPoints[i];
			}
		}
	}
	return 0;
}

void CGameTable::ConstructGameData()
{
	if (!m_GameTalbeInfo)
	{
		m_GameTalbeInfo=new GAME_TABLE_INFO;
		InitialGameTableInfo(m_GameTalbeInfo);
	}
}


int  CGameTable::GetRandomBombNum(int seed)
{
	int nBombNum = 0;
	
	srand( (unsigned)time( NULL ) + seed * 1000 );
	
	int nRand = rand()%1001;
	if (nRand >= 0 && nRand <350)
		nBombNum = 0;
	else if(nRand >= 350 && nRand < 995)
		nBombNum = 1;
	else if (nRand >= 995&& nRand < 998)
	{
		nBombNum = 2;
	}
	else if(nRand >= 998 && nRand < 1000)
	{
		nBombNum = XygGetRandomBetweenEx(3, 4);
	}
	else if(nRand == 1000)
	{
		nBombNum = XygGetRandomBetweenEx(5, 7);
	}
	
	return nBombNum;
}

BOOL CGameTable::MakeDealCards(int nMakeChance)
{
	if (nMakeChance==MAKEBOMB_CHANCE)
		return FALSE;

	CPlayer* ptrP = m_ptrPlayers[0];
	if (!ptrP)
		return FALSE;

	int seed = ptrP->m_lTokenID * 10 + ptrP->m_hSocket;
	srand( (unsigned)time( NULL ) + seed * 1000 );
	
	BOOL bMake = FALSE;
	int nRand = rand()%101;
	if (nRand > 0 && nRand <=nMakeChance)
		bMake = TRUE;
	
	if(!bMake)
		return FALSE;
	
	int BombNum = GetRandomBombNum(seed);
	if (BombNum == 0)
		return FALSE;
	
	int ChairValue[TOTAL_CHAIRS];
	XygInitChairCards(ChairValue, TOTAL_CHAIRS);
	
	int i;
	for (i = 0; i < TOTAL_CHAIRS; i++)
	{
		ChairValue[i] = i;
	}
	
	std::random_shuffle(ChairValue, ChairValue+TOTAL_CHAIRS);
	
	int	BombValue[LAYOUT_NUM];
	XygInitChairCards(BombValue, LAYOUT_NUM);
	for (i = 1; i < 14; i++)	//从2到A
	{
		BombValue[i] = i;
	}
	std::random_shuffle(BombValue+1, BombValue+14);
	
	memset(m_nBombHadDeal,-1,sizeof(m_nBombHadDeal));
	int nCount = 1;
	int Bomb0 = XygGetRandomBetween(BombNum);
	Bomb0 = (Bomb0 > 3) ? 3 : Bomb0;
	MakeCardsLayIn(ChairValue[0], Bomb0, nCount, BombValue);
	int Bomb1 = XygGetRandomBetween(BombNum-Bomb0);
	Bomb1 = (Bomb1 > 3) ? 3 : Bomb1;
	MakeCardsLayIn(ChairValue[1], Bomb1, nCount, BombValue);
	int Bomb2 = BombNum - Bomb0 - Bomb1;
	Bomb2 = (Bomb2 > 3) ? 3 : Bomb2;
	MakeCardsLayIn(ChairValue[2], Bomb2, nCount, BombValue);
	
	return TRUE;
}

void CGameTable::MakeCardsLayIn(int nChair, int nBombNum, int& nCurIndex, int nBombValue[])
{
	for (int i = 0; i < nBombNum; i++)
	{
		int nValue = nBombValue[nCurIndex];
		m_nBombHadDeal[nCurIndex-1].chairno = nChair;
		m_nBombHadDeal[nCurIndex-1].nValueIndex = nValue;
		m_nCardsLayIn[nChair][nValue] = 4;
		nCurIndex++;
		UwlLogFile(_T("chair:%d, %d"),nChair,nValue+1);
	}
}

void CGameTable::DealCardNormal()
{
	int nCardID = -1;
	for(int j=0;j<CARDS_PER_CHAIR;j++)
	{
		for(int i=0;i<TOTAL_CHAIRS;i++)
		{
			nCardID = CatchOneCard(i);
		}
	}
	
	//底牌
	if (GetPublicInfo()->nCurrentCatch==m_nTotalCards-BOTTOM_CARD)
	{
		int nCount = 0;
		GAME_PUBLIC_INFO* pPublicInfo=GetPublicInfo();
		int nCurrentCatch = GetPublicInfo()->nCurrentCatch;
		for (int i=0;i<BOTTOM_CARD;i++)
		{
			m_nBottomIDs[nCount++] = pPublicInfo->GameCard[nCurrentCatch].nCardID;
			nCurrentCatch++;
			if (nCurrentCatch>=m_nTotalCards)
				break;
		}
	}
	else
	{
		//Error 底牌不够了
	}
}

int CGameTable::IsMakedCard(int nCardID)
{
	int nIndex = SK_GetCardIndexEx(nCardID,0);
	if (nIndex<0)
		return -1;

	for (int i=0;i<LAYOUT_NUM;i++)
	{
		if (m_nBombHadDeal[i].chairno<0
			|| m_nBombHadDeal[i].nValueIndex<0)
			continue;
		
		if (m_nBombHadDeal[i].nValueIndex == nIndex)
			return m_nBombHadDeal[i].chairno;
	}

	return -1;
}

void CGameTable::DealCardMakeMode()
{
	int nCardID = -1;
	int nCardChair = -1;
	GAME_PUBLIC_INFO* pPublicInfo=GetPublicInfo();
	for(int j=0;j<CARDS_PER_CHAIR;j++)
	{
		int chairno = 0;
		for(int i=0;i<TOTAL_CHAIRS;i++)
		{
//			nCardID = CatchOneCard(i);
			if (CARDS_PER_CHAIR == XygCardRemains(m_nCardsLayIn[chairno],SK_LAYOUT_NUM))
			{
				chairno = (chairno + m_nTotalChairs - 1) % m_nTotalChairs;
				continue;
			}
			//如果是炸弹，发到那个人手上
			while (pPublicInfo->GameCard[pPublicInfo->nCurrentCatch].nCardID<52 
				&& (nCardChair=IsMakedCard(pPublicInfo->GameCard[pPublicInfo->nCurrentCatch].nCardID))>=0)
			{
				pPublicInfo->GameCard[pPublicInfo->nCurrentCatch].nCardStatus=CARD_STATUS_INHAND;
				pPublicInfo->GameCard[pPublicInfo->nCurrentCatch].nChairNO=nCardChair;
				pPublicInfo->GameCard[pPublicInfo->nCurrentCatch].nPositionIndex=0;
				pPublicInfo->nCurrentCatch++;
			}
			
			CatchOneCard(chairno);
			chairno = (chairno + m_nTotalChairs - 1) % m_nTotalChairs;
		}
	}
	
	for(int i = 0; i < BOTTOM_CARD; i++)
	{
			//如果是炸弹，发到那个人手上
			while (pPublicInfo->GameCard[pPublicInfo->nCurrentCatch].nCardID<52 
				&& (nCardChair=IsMakedCard(pPublicInfo->GameCard[pPublicInfo->nCurrentCatch].nCardID))>=0)
			{
				pPublicInfo->GameCard[pPublicInfo->nCurrentCatch].nCardStatus=CARD_STATUS_INHAND;
				pPublicInfo->GameCard[pPublicInfo->nCurrentCatch].nChairNO=nCardChair;
				pPublicInfo->GameCard[pPublicInfo->nCurrentCatch].nPositionIndex=0;
				pPublicInfo->nCurrentCatch++;
			}

		m_nBottomIDs[i] = pPublicInfo->GameCard[pPublicInfo->nCurrentCatch].nCardID;
		m_nBottomCatch[i] = pPublicInfo->nCurrentCatch;
		pPublicInfo->nCurrentCatch++;
	}


	while (pPublicInfo->nCurrentCatch<m_nTotalCards
		&& pPublicInfo->GameCard[pPublicInfo->nCurrentCatch].nCardID<52 
		&& (nCardChair=IsMakedCard(pPublicInfo->GameCard[pPublicInfo->nCurrentCatch].nCardID))>=0)
	{
			pPublicInfo->GameCard[pPublicInfo->nCurrentCatch].nCardStatus=CARD_STATUS_INHAND;
			pPublicInfo->GameCard[pPublicInfo->nCurrentCatch].nChairNO=nCardChair;
			pPublicInfo->GameCard[pPublicInfo->nCurrentCatch].nPositionIndex=0;
			pPublicInfo->nCurrentCatch++;
	}
}

void CGameTable::StartDeal()
{
	m_nBoutBeginTime = GetCurTimeStampMilli(); //开局时间

	CString strIniFile = GetINIFileName();
	TCHAR szRoomID[16];
	memset(szRoomID, 0, sizeof(szRoomID));
	_stprintf(szRoomID, _T("%ld"), m_nRoomID);

	m_nOperateTime = GetPrivateProfileInt(
		_T("throwwait"),		// section name
		szRoomID,				// key name
		THROW_WAIT,		// default int
		strIniFile				// initialization file name
			);

	m_bIsCrazyMode = GetPrivateProfileInt(
		_T("CrazyMode"),//是否是疯狂玩法
		szRoomID,
		0,
		strIniFile);

	m_bIsRazzMode= GetPrivateProfileInt(
		_T("RazzMode"),//是否是赖子玩法
		szRoomID,
		0,
		strIniFile);

	m_nCardTypeLimit = GetPrivateProfileInt(_T("CardTypeLimit"), _T("open"), 0, GetINIFileName());

	InitEvaluateSysForClassic();

	// 封顶只有固定服务费模式才生效
	if (FEE_MODE_SERVICE_FIXED == m_nFeeMode) {
		int limitNum = GetPrivateProfileInt(_T("RoomSilverLimit"), szRoomID, 0, GetINIFileName());
		m_nRoomSilverLimit = limitNum > 0 ? limitNum : 0;
	}
	
	BOOL bMakeDeal = FALSE;
	//判断做牌配置没有拿到，不做牌
	if (CConfigManagerSys::m_jsoncfgobjmgr.find(MAKEDEAL_CONFIG) == CConfigManagerSys::m_jsoncfgobjmgr.end())
	{
		bMakeDeal = FALSE;
	}
	else
	{
		bMakeDeal = !CConfigManagerSys::m_jsoncfgobjmgr[MAKEDEAL_CONFIG]["MakeDeal"][szRoomID].empty();
	}

	for(int i=0;i<TOTAL_CHAIRS;i++)
	{
		GetPlayerInfo(i)->nThrowTime = m_nOperateTime;
	}

	GetPublicInfo()->nCurrentCatch=0;
	BOOL isReadCardsFromFileForNovice = FALSE;

	ReadNewUserAiLevel();


#if defined(_DEBUG) || defined(_RS125)
	DWORD dwRead = CConfigManagerSys::m_jsoncfgobjmgr[MAKEDEAL_CONFIG]["StartDeal"]["ReadCardsFromFile"].asInt();
	if (dwRead)
	{
		m_nBanker=0;
		m_nRazzCardValue = 8;
		ReadCardsFromFile();
	}
	else
#endif
	{
		BOOL isMakeDealNoShuff = FALSE;
		if (!m_bIsProtected && TRUE == IsNeedMakeDealForNovice())
		{
			CalcBanker(TRUE);
			isReadCardsFromFileForNovice = ReadNoviceCardsFromFile();
			m_bIsProtected = isReadCardsFromFileForNovice;
		}

		if (!isReadCardsFromFileForNovice)
		{
			CalcBanker(FALSE);// 决定庄家

			if (TRUE == m_bIsRazzMode)
			{
				m_nRazzCardValue = CalcRazzCardValue();
			}

			int card[TOTAL_CARDS];
			int i;
			for (i = 0; i < TOTAL_CARDS; i++)
				card[i] = i;

			CPlayer* ptrP = m_ptrPlayers[0];
			SvrXygRandomSort(card, TOTAL_CARDS, GetTickCount() + ptrP->m_lTokenID * 10 + ptrP->m_hSocket);

#ifdef _MAKEDEALINFO
			{
				for (int i = 0; i < TOTAL_CARDS - BOTTOM_CARD; i++)
				{
					m_nCardsLayIn[i % 3][SK_GetCardIndexEx(card[i])] ++;
				}
			}

			UwlLogFile(_T("原始牌："));
			CompareAndLogCardLays(NULL, m_nCardsLayIn, TOTAL_CHAIRS);
#endif

			if (TRUE == bMakeDeal)//根据配置做牌
				MakeDealByCfg(card, TOTAL_CARDS);

			GAME_PUBLIC_INFO* pPublicInfo = GetPublicInfo();
			for (i = 0; i < TOTAL_CARDS; i++)
			{
				pPublicInfo->GameCard[i].nCardID = card[i];
				pPublicInfo->GameCard[i].nCardIndex = SK_GetCardIndexEx(card[i], 0);
				pPublicInfo->GameCard[i].nShape = SK_GetCardShapeEx(card[i], 0);
				pPublicInfo->GameCard[i].nValue = SK_GetCardValueEx(card[i], 0);
				pPublicInfo->GameCard[i].nCardStatus = CARD_STATUS_WAITDEAL;
				pPublicInfo->GameCard[i].nChairNO = -1;
				pPublicInfo->GameCard[i].nPositionIndex = i;
			}

#if defined(_DEBUG) || defined(_RS125)
			ZeroMemory(m_nCardsLayIn, sizeof(int) * SK_LAYOUT_NUM * TOTAL_CHAIRS);
#endif

			DealCardNormal();
		}
	}

	ASSERT(CheckCards());

	for (int i = 0; i < TOTAL_CHAIRS; ++i)
	{
		GetInHandCard(i, m_initHandCards[i]);
	}
	if (this->IsRobotTable())
	{
		int aiLevel = m_nAILevel[3];
		if (IsNewUserM()) {
			aiLevel = -1; // 前三局固定本地机器人
		}
		//else if (m_boutDataCache.nProtectRobotType > ROBOTTYPE_PROTECT + 1) {
		//	aiLevel = -1; // 触发保护也使用固定本地机器人
		//}
		this->Robot_InitRobotAITypeOnBoutStart(aiLevel); //初始化使用哪种AI

		//JuniorRobotAI
		{
			int nCardIDs[CHAIR_CARDS];
			for (int i = 0; i<m_nTotalChairs; i++)
			{
				m_GameAI[i].ResetMember();
				XygInitChairCards(nCardIDs, CARDS_PER_CHAIR);
				GetChairCards(i, nCardIDs, CARDS_PER_CHAIR);
				m_GameAI[i].InitMyCards(i, nCardIDs, CARDS_PER_CHAIR);
			}
		}

		//RemoteRobotAI
		if (Robot_IsUseRemoteAI() == TRUE)
		{
			Robot_SetRemoteAIRobotPeerBottomEnable(); //设置是否机器人看底牌

			{
				int arr[TOTAL_CHAIRS];
				for (int i = 0; i < TOTAL_CHAIRS; i++) {
					arr[i] = (m_ptrPlayers[i] ? m_ptrPlayers[i]->m_nUserType : 0);
				}

				{
					Json::Value data(Json::objectValue);
					Json::Value userTypes(Json::arrayValue);

					for (int i = 0; i < TOTAL_CHAIRS; i++) {
						userTypes.append((m_ptrPlayers[i] ? m_ptrPlayers[i]->m_nUserType : 0));
					}
					data["userTypes"] = userTypes;

					Json::StreamWriterBuilder builder;
					const std::string json = Json::writeString(builder, data);


					std::vector<CAIEngineItem> vecAIEngineItems;
					vecAIEngineItems.push_back(CAIEngineItem{ 0, CAI_Dll::e_AI_ResetMember, json });

					m_pGameServer->PostAIEngineAction(this, vecAIEngineItems); // 该接口会自己通知本桌所有ai玩家
				}


			}
			/*for (int i = 0; i < m_nTotalChairs; i++)
			{
				int nCardIDs[CHAIR_CARDS];
				XygInitChairCards(nCardIDs, CARDS_PER_CHAIR);
				GetChairCards(i, nCardIDs, CARDS_PER_CHAIR);
				m_pGameServer->PostAIEngineAction(this, {
					{ i, CAI_Dll::e_AI_InitChairCards, nCardIDs, sizeof(int) * CARDS_PER_CHAIR, TRUE }
				});
			}*/

			for (int i = 0; i < m_nTotalChairs; i++)
			{
				SOLO_PLAYER sp;
				memset(&sp, 0, sizeof(sp));
				m_pGameServer->LookupSoloPlayer(m_ptrPlayers[i]->m_nUserID, sp);


				{
					Json::Value data(Json::objectValue);
					data["sex"] = &sp.nNickSex;

					Json::StreamWriterBuilder builder;
					const std::string json = Json::writeString(builder, data);


					std::vector<CAIEngineItem> vecAIEngineItems;
					vecAIEngineItems.push_back(CAIEngineItem{ i, CAI_Dll::e_AI_Sex, json });

					m_pGameServer->PostAIEngineAction(this, vecAIEngineItems); // 该接口会自己通知本桌所有ai玩家
				}

			}

			auto szRoomID = std::to_string(m_nRoomID);

			//支持机器人也能看底牌
			//Q：这里的if和else里有啥区别？A：只是底牌的区别
			if (m_bIsRemoteAIRobotPeerBottomEnable == TRUE)
			{
				for (int i = 0; i < m_nTotalChairs; i++)
				{
					int nCardIDs[CHAIR_CARDS + 1];
					XygInitChairCards(nCardIDs, CHAIR_CARDS + 1);
					GetChairCards(i, nCardIDs, CARDS_PER_CHAIR);
					memcpy(nCardIDs + CARDS_PER_CHAIR, m_nBottomIDs, sizeof(int[BOTTOM_CARD])); //追加数据-3张底牌
					nCardIDs[CHAIR_CARDS] = m_nBanker; //追加数据-初始发牌（叫地主）桌子号
					
					{
						Json::Value data(Json::objectValue);
						Json::Value cardIDs(Json::arrayValue);

						for (int j = 0; j < CHAIR_CARDS; ++j) {
							Json::Value cardObj;
							cardObj["id"] = nCardIDs[j];
							cardObj["value"] = CardIDTobitIdx(nCardIDs[j]);
							cardIDs.append(cardObj);
						}

						data["cardIDs"] = cardIDs;

						if (m_bIsRazzMode) {
							data["laiziIDs"] = m_nRazzCardValue;
						}

						Json::Value versionFlag;
						if (isReadCardsFromFileForNovice && IsNewUserM2()) {
							versionFlag["callFlag"] = m_nAILevel[0];
							versionFlag["robFlag"] = m_nAILevel[1];
							versionFlag["doubleFlag"] = m_nAILevel[2];
							versionFlag["throwTileFlag"] = m_nAILevel[3];
						}
						else {
							auto vec = GetIntIniCfgs("DefaultAiLevel", szRoomID, 4);

							versionFlag["callFlag"] = vec[0];
							versionFlag["robFlag"] = vec[1];
							versionFlag["doubleFlag"] = vec[2];
							versionFlag["throwTileFlag"] = vec[3];
						}
						data["versionFlag"] = versionFlag;

						Json::StreamWriterBuilder builder;
						const std::string json = Json::writeString(builder, data);

						LOG_INFO("%s json:%s", __FUNCTION__, json);


						std::vector<CAIEngineItem> vecAIEngineItems;
						vecAIEngineItems.push_back(CAIEngineItem{ i, CAI_Dll::e_AI_InitChairCards, json });

						m_pGameServer->PostAIEngineAction(this, vecAIEngineItems); // 该接口会自己通知本桌所有ai玩家
					}
					
				}
			}
			else
			{
				for (int i = 0; i < m_nTotalChairs; i++)
				{
					int nCardIDs[CHAIR_CARDS];
					XygInitChairCards(nCardIDs, CARDS_PER_CHAIR);
					int cardsCount = GetChairCards(i, nCardIDs, CARDS_PER_CHAIR);

					{
						Json::Value data(Json::objectValue);
						Json::Value cardIDs(Json::arrayValue);

						for (int j = 0; j < cardsCount; ++j) {
							Json::Value cardObj;
							cardObj["id"] = nCardIDs[j];
							cardObj["value"] = CardIDTobitIdx(nCardIDs[j]);
							cardIDs.append(cardObj);
						}

						data["cardIDs"] = cardIDs;

						if (m_bIsRazzMode) {
							data["laiziIDs"] = m_nRazzCardValue;
						}

						Json::Value versionFlag;
						if (isReadCardsFromFileForNovice && IsNewUserM2()) {
							versionFlag["callFlag"] = m_nAILevel[0];
							versionFlag["robFlag"] = m_nAILevel[1];
							versionFlag["doubleFlag"] = m_nAILevel[2];
							versionFlag["throwTileFlag"] = m_nAILevel[3];
							
						}
						else {
							auto vec = GetIntIniCfgs("DefaultAiLevel", szRoomID, 4);

							versionFlag["callFlag"] = vec[0];
							versionFlag["robFlag"] = vec[1];
							versionFlag["doubleFlag"] = vec[2];
							versionFlag["throwTileFlag"] = vec[3];
						}
						data["versionFlag"] = versionFlag;

						Json::StreamWriterBuilder builder;
						const std::string json = Json::writeString(builder, data);

						LOG_INFO("%s json:%s", __FUNCTION__, json);


						std::vector<CAIEngineItem> vecAIEngineItems;
						vecAIEngineItems.push_back(CAIEngineItem{ i, CAI_Dll::e_AI_InitChairCards, json });

						m_pGameServer->PostAIEngineAction(this, vecAIEngineItems); // 该接口会自己通知本桌所有ai玩家
					}
				}
			}
		}
	}

	if(IsNeedRecord())
	{
		m_sinRecord
			<< "Version" << " " << "1.0" << std::endl
			<< "Timestamp" << " " << m_nBoutStartSeconds << std::endl
			<< "RoomID" << " " << m_nRoomID << std::endl
			<< "TableNO" << " " << m_nTableNO << std::endl;

		for (int i = 0; i < m_nTotalChairs; i++)
		{
			m_sinRecord
				<< "ChairNO"
				<< " " << i
				<< " " << m_ptrPlayers[i]->m_nUserID
				<< " " << m_ptrPlayers[i]->m_nUserType
				<< " " << AI_GetAIEngineID(i)
				<< std::endl;

			SOLO_PLAYER sp = {};
			if (m_pGameServer->LookupSoloPlayer(m_ptrPlayers[i]->m_nUserID, sp))
			{
				m_sinRecord
					<< "Name"
					<< " " << i
					<< " " << sp.szUsername
					<< std::endl;

				m_sinRecord
					<< "Sex"
					<< " " << i
					<< " " << sp.nNickSex
					<< std::endl;

				m_sinRecord
					<< "Win_Loss_StandOff"
					<< " " << i
					<< " " << sp.nWin
					<< " " << sp.nLoss
					<< " " << sp.nStandOff
					<< std::endl;
			}

		}
		m_sinRecord<< "Laizi"<< " " << m_nRazzCardValue<< std::endl;

		int nCardIDs[CHAIR_CARDS];
		for (int i = 0; i < m_nTotalChairs; i++)
		{
			XygInitChairCards(nCardIDs, CARDS_PER_CHAIR);
			GetChairCards(i, nCardIDs, CARDS_PER_CHAIR);

			m_sinRecord
				<< "Deal"
				<< " " << i
				<< " " << CoverCardIDsEx(nCardIDs, CARDS_PER_CHAIR)
				<< std::endl;
		}
	}
	
	SetCurrentChair(m_nBanker,m_nOperateTime);

	m_boutVideo.SetBoutBegin(m_nRoomID, m_nTableNO); //BoutVideo日志记录


	// 启动时，为玩家重置操作时间记录。
	if (isSupportChessFestival() && IsFriendRoom()) {
		ClearCostTime();
		// 为所有玩家设置开始操作时间。
		int firstOPChairNo = GetCurrentChair();
		int nUserID = m_ptrPlayers[firstOPChairNo]->m_nUserID;
		if (compManager->getPlayerType(nUserID) != -1) {
			SetPlayerLastOPTime(firstOPChairNo, GetTickCount() + 25 * 17);
		}
	}
}
BOOL CGameTable::ReadNoviceCardsFromFile()
{
	int nMyChairNo = -1;
	for (int m = 0; m < TOTAL_CHAIRS;m++)
	{
		if (!m_ptrPlayers[m]->IsRoboter())
		{
			nMyChairNo = m;
			break;
		}
	}

	if (CConfigManagerSys::m_jsoncfgobjmgr[MAKEDEAL_CONFIG]["PlayerMakeDealRule"]["NewUserDealCardGroups2"].isNull())
	{
		return FALSE;
	}

	auto szRoomId = std::to_string(m_nRoomID);

	int nNoviceStartDeal = CConfigManagerSys::m_jsoncfgobjmgr[MAKEDEAL_CONFIG]["PlayerMakeDealRule"]["NewUserDealCardGroups2"].size();
	if (nMyChairNo == -1 || nNoviceStartDeal == 0)
	{
		return FALSE;
	}

	int cardId = GetUserCardPoolId();
	if (cardId == 0) {
		return FALSE;
	}

	m_boutDataCache.nProtectCard[1] = cardId;

	srand((unsigned)time(NULL));
	auto iNoviceStartDeal = std::to_string(cardId);
	TCHAR szKey2[30];
	TCHAR szCards[256];
	int nIndex = 0;
	GAME_PUBLIC_INFO *pPublicInfo = GetPublicInfo();
	for (int i = 0; i < TOTAL_CHAIRS;i++)
	{
		sprintf(szKey2, _T("Chair%d"), i);
		sprintf_s(szCards, "%s", CConfigManagerSys::m_jsoncfgobjmgr[MAKEDEAL_CONFIG]["PlayerMakeDealRule"]["NewUserDealCardGroups2"][iNoviceStartDeal][szKey2].asCString());
		if (strlen(szCards) == 0)
		{
			return FALSE;
		}
		TCHAR *p1, *p2;
		TCHAR *fields[128];
		int nCount=0;
		p1 = szCards;
		nCount = RetrieveFields(p1, fields, 60, &p2);
		if (nCount != CARDS_PER_CHAIR)
		{
			return FALSE;
		}
		for (int j = 0; j < nCount;j++)
		{
			pPublicInfo->GameCard[nIndex].nPositionIndex = nIndex;
			int nCardID = atoi(fields[j]);
			pPublicInfo->GameCard[nIndex].nCardID = nCardID;
			pPublicInfo->GameCard[nIndex].nCardIndex = SK_GetCardIndexEx(nCardID, 0);
			pPublicInfo->GameCard[nIndex].nShape = SK_GetCardShapeEx(nCardID, 0);
			pPublicInfo->GameCard[nIndex].nValue = SK_GetCardValueEx(nCardID, 0);
			nIndex++;
			CatchOneCard((nMyChairNo+i)%TOTAL_CHAIRS);
		}
	}
	{
		TCHAR szBottoms[256];
		sprintf(szKey2, _T("Bottom"));
		sprintf_s(szBottoms, "%s", CConfigManagerSys::m_jsoncfgobjmgr[MAKEDEAL_CONFIG]["PlayerMakeDealRule"]["NewUserDealCardGroups2"][iNoviceStartDeal][szKey2].asCString());
		TCHAR *p1, *p2;
		TCHAR *fields[128];
		int nCount = 0;
		p1 = szBottoms;
		nCount = RetrieveFields(p1, fields, 60, &p2);
		if (nCount != BOTTOM_CARD)
		{
			return FALSE;
		}
		for (int j = 0; j < nCount;j++)
		{
			int nCardID = atoi(fields[j]);
			pPublicInfo->GameCard[nIndex].nCardID = nCardID;
			pPublicInfo->GameCard[nIndex].nCardIndex = SK_GetCardIndexEx(nCardID, 0);
			pPublicInfo->GameCard[nIndex].nShape = SK_GetCardShapeEx(nCardID, 0);
			pPublicInfo->GameCard[nIndex].nValue = SK_GetCardValueEx(nCardID, 0);
			pPublicInfo->GameCard[nIndex].nCardStatus = CARD_STATUS_WAITDEAL;
			pPublicInfo->GameCard[nIndex].nChairNO = -1;
			pPublicInfo->GameCard[nIndex].nPositionIndex = nIndex;
			nIndex++;
			m_nBottomIDs[j] = nCardID;
		}

		if (m_bIsRazzMode) {
			auto razzValue = CConfigManagerSys::m_jsoncfgobjmgr[MAKEDEAL_CONFIG]["PlayerMakeDealRule"]["NewUserDealCardGroups2"][iNoviceStartDeal]["RazzValue"];
			if (razzValue.isNumeric()) {
				m_nRazzCardValue = razzValue.asInt();
			}
			else {
				m_nRazzCardValue = CalcRazzCardValue();
			}
		}
	}

	if (FALSE == CheckCards())
	{
		return FALSE;
	}

	for (int i = 0; i < TOTAL_CHAIRS; i++)
	{
		m_nMakeDealTypes[i] = 1;
	}

	return TRUE;
}
BOOL CGameTable::IsNeedMakeDealForNovice()
{
	if (CConfigManagerSys::m_jsoncfgobjmgr[MAKEDEAL_CONFIG]["PlayerMakeDealRule"]["IsEnable"].asInt() == 0)
	{
		return FALSE;
	}

	TCHAR szRoomID[16];
	memset(szRoomID, 0, sizeof(szRoomID));
	_stprintf(szRoomID, _T("%ld"), m_nRoomID);
	//如果房间没有指定用户做牌规则，则不给新用户做牌
	if (!CConfigManagerSys::m_jsoncfgobjmgr[MAKEDEAL_CONFIG]["PlayerMakeDealRule"]["RoomRule"][szRoomID].isNull()) {
		int nPlayerForNoRobot = 0;
		int iPlayerForForNoRobot = -1;
		for (int i = 0; i < TOTAL_CHAIRS; i++)
		{
			if (m_ptrPlayers == NULL || m_ptrPlayers[i] == NULL)
			{
				return FALSE;
			}
			if (!m_ptrPlayers[i]->IsRoboter())
			{
				nPlayerForNoRobot++;
				iPlayerForForNoRobot = i;
			}
		}
		if (nPlayerForNoRobot != 1 || iPlayerForForNoRobot == -1)
		{
			return FALSE;
		}

		int nNoviceBountLimit = CConfigManagerSys::m_jsoncfgobjmgr[MAKEDEAL_CONFIG]["PlayerMakeDealRule"]["RoomRule"][szRoomID]["NewUserBout"].asInt();
		//用户在配桌完成后就会增加局数因此此处局数已经为用户实际局数加1
		if (m_ptrPlayers[iPlayerForForNoRobot] == NULL || m_ptrPlayers[iPlayerForForNoRobot]->m_nBout > nNoviceBountLimit)
		{
			return  FALSE;
		}

		//判断是否开启手动做牌，如果没有开启，使用自动做牌
		if (CConfigManagerSys::m_jsoncfgobjmgr[MAKEDEAL_CONFIG]["PlayerMakeDealRule"]["RoomRule"][szRoomID]["ManualMakeDealBout"].isNull())
		{
			return FALSE;
		}

		int nManualMakeDealBout = CConfigManagerSys::m_jsoncfgobjmgr[MAKEDEAL_CONFIG]["PlayerMakeDealRule"]["RoomRule"][szRoomID]["ManualMakeDealBout"].asInt();
		if (nManualMakeDealBout <= 0)
		{
			return FALSE;
		}
		else if (m_ptrPlayers[iPlayerForForNoRobot]->m_nBout > nManualMakeDealBout) {
			return IsNeedNewUserAiCfg();
		}
	}
	else {
		return FALSE;
	}

	return TRUE;
}

BOOL CGameTable::IsNeedMakeDealForNoShuff(TCHAR *pRoomID)
{
	CalcBanker(FALSE);

	int nOrder = 0;

	if (!CConfigManagerSys::m_jsoncfgobjmgr[MAKEDEAL_CONFIG]["PlayerMakeDealRule"].isNull() 
		&& !CConfigManagerSys::m_jsoncfgobjmgr[MAKEDEAL_CONFIG]["PlayerMakeDealRule"]["IsEnable"].isNull()
		&& CConfigManagerSys::m_jsoncfgobjmgr[MAKEDEAL_CONFIG]["PlayerMakeDealRule"]["IsEnable"].asInt() != 0 
		&& !CConfigManagerSys::m_jsoncfgobjmgr[MAKEDEAL_CONFIG]["PlayerMakeDealRule"]["RoomRule"].isNull()
		&& !CConfigManagerSys::m_jsoncfgobjmgr[MAKEDEAL_CONFIG]["PlayerMakeDealRule"]["RoomRule"][pRoomID].isNull() 
		&& !CConfigManagerSys::m_jsoncfgobjmgr[MAKEDEAL_CONFIG]["PlayerMakeDealRule"]["RoomRule"][pRoomID]["NewUserBout"].isNull())
	{
		int nBount = CConfigManagerSys::m_jsoncfgobjmgr[MAKEDEAL_CONFIG]["PlayerMakeDealRule"]["RoomRule"][pRoomID]["NewUserBout"].asInt();
		for (int m = 0; m < TOTAL_CHAIRS; m++)
		{
			if (m_ptrPlayers[m]->m_nBout <= nBount)
			{
				return FALSE;
			}
		}
	}


	BOOL bMakeDeal = !CConfigManagerSys::m_jsoncfgobjmgr[MAKEDEAL_CONFIG]["NoShuffMakeDeal"][pRoomID].isNull();
	if (!bMakeDeal)
	{
		return FALSE;
	}
	else
	{
		//DWORD dwTicket0 = GetTickCount();
		int nBomb = 0;
		int nThree = 0;
		int nStraight = 0;

		nOrder = CConfigManagerSys::m_jsoncfgobjmgr[MAKEDEAL_CONFIG]["NoShuffMakeDeal"][pRoomID].asInt();

		if (nOrder == 0)
		{
			m_bNoShuffMakeDeal = FALSE;
			return FALSE;
		}

		int nProbability = 0;

		if (!CConfigManagerSys::m_jsoncfgobjmgr[MAKEDEAL_CONFIG]["NoShuffProbability"].isNull() && !CConfigManagerSys::m_jsoncfgobjmgr[MAKEDEAL_CONFIG]["NoShuffProbability"][pRoomID].isNull())
		{
			nProbability = CConfigManagerSys::m_jsoncfgobjmgr[MAKEDEAL_CONFIG]["NoShuffProbability"][pRoomID].asInt();
		}

		srand(time(NULL));
		int nRandPro = rand() % 100;
		if (nRandPro >= nProbability)
		{
			m_bNoShuffMakeDeal = FALSE;
			return FALSE;
		}

		int n2KProbability = 0;

		if (!CConfigManagerSys::m_jsoncfgobjmgr[MAKEDEAL_CONFIG]["NoShuff2KProbability"].isNull() && !CConfigManagerSys::m_jsoncfgobjmgr[MAKEDEAL_CONFIG]["NoShuff2KProbability"][pRoomID].isNull())
		{
			n2KProbability = CConfigManagerSys::m_jsoncfgobjmgr[MAKEDEAL_CONFIG]["NoShuff2KProbability"][pRoomID].asInt();
		}

		if (nRandPro >= n2KProbability)
		{
			m_b2K = FALSE;
		}
		else
		{
			m_b2K = TRUE;
		}

		m_bNoShuffMakeDeal = TRUE;

		if (!CConfigManagerSys::m_jsoncfgobjmgr[MAKEDEAL_CONFIG]["NoShuffStrategy"].isNull())
		{
			if (!CConfigManagerSys::m_jsoncfgobjmgr[MAKEDEAL_CONFIG]["NoShuffStrategy"]["BombLow"].isNull() && !CConfigManagerSys::m_jsoncfgobjmgr[MAKEDEAL_CONFIG]["NoShuffStrategy"]["BombHight"].isNull())
			{
				int nBombRangeLow = CConfigManagerSys::m_jsoncfgobjmgr[MAKEDEAL_CONFIG]["NoShuffStrategy"]["BombLow"].asInt();
				int nBombRangeHight = CConfigManagerSys::m_jsoncfgobjmgr[MAKEDEAL_CONFIG]["NoShuffStrategy"]["BombHight"].asInt();
				int nBombDiff = nBombRangeHight - nBombRangeLow;
				if (nBombDiff > 0)
				{
					srand(time(NULL) + nBombRangeLow * 1000);
					nBomb = rand() % nBombDiff + nBombRangeLow;
				}
			}

			if (!CConfigManagerSys::m_jsoncfgobjmgr[MAKEDEAL_CONFIG]["NoShuffStrategy"]["ThreeLow"].isNull() && !CConfigManagerSys::m_jsoncfgobjmgr[MAKEDEAL_CONFIG]["NoShuffStrategy"]["ThreeHight"].isNull())
			{
				int nThreeRangeLow = CConfigManagerSys::m_jsoncfgobjmgr[MAKEDEAL_CONFIG]["NoShuffStrategy"]["ThreeLow"].asInt();
				int nThreeRangeHight = CConfigManagerSys::m_jsoncfgobjmgr[MAKEDEAL_CONFIG]["NoShuffStrategy"]["ThreeHight"].asInt();
				int nThreeDiff = nThreeRangeHight - nThreeRangeLow;
				if (nThreeDiff > 0)
				{
					srand(time(NULL) + nThreeRangeHight * 1000);
					nThree = rand() % nThreeDiff + nThreeRangeLow;
				}
			}

			if (!CConfigManagerSys::m_jsoncfgobjmgr[MAKEDEAL_CONFIG]["NoShuffStrategy"]["StraightLow"].isNull() && !CConfigManagerSys::m_jsoncfgobjmgr[MAKEDEAL_CONFIG]["NoShuffStrategy"]["StraightHight"].isNull())
			{
				int nStraightRangeLow = CConfigManagerSys::m_jsoncfgobjmgr[MAKEDEAL_CONFIG]["NoShuffStrategy"]["StraightLow"].asInt();
				int nStraightRangeHight = CConfigManagerSys::m_jsoncfgobjmgr[MAKEDEAL_CONFIG]["NoShuffStrategy"]["StraightHight"].asInt();
				int nStraightDiff = nStraightRangeHight - nStraightRangeLow;
				if (nStraightDiff > 0)
				{
					srand(time(NULL) + nStraightDiff * 1000);
					nStraight = rand() % nStraightDiff + nStraightRangeLow;
				}
			}
		}

		int card[TOTAL_CARDS];
		int i;
		for (i = 0; i < TOTAL_CARDS; i++)
		{
			card[i] = i;
		}
			
		CPlayer* ptrP = m_ptrPlayers[0];
		SvrXygRandomSort(card, TOTAL_CARDS, GetTickCount() + ptrP->m_lTokenID * 10 + ptrP->m_hSocket);

		std::vector<int> vecBomb;
		std::vector<int> vecThree;
		std::vector<int> vecStraight;
		std::vector<int> vecCouple;
		std::vector<int> vecSingle;

		int nDFS = 0;

		if (MakeDealForNoShuff(nOrder, nBomb, nThree, nStraight, card, vecBomb, vecThree, vecStraight, vecCouple, vecSingle, nDFS))
		{

		}
		else
		{
			//重发
			//LOG_INFO("NEED RECATCH");
			for (i = 0; i < TOTAL_CARDS; i++)
			{
				card[i] = i;
			}
			SvrXygRandomSort(card, TOTAL_CARDS, GetTickCount() + m_nTableNO + m_nRoomID);

			nDFS = 0;
			vecBomb.clear();
			vecThree.clear();
			vecStraight.clear();
			vecCouple.clear();
			vecSingle.clear();
			MakeDealForNoShuff(nOrder, nBomb, nThree, nStraight, card, vecBomb, vecThree, vecStraight, vecCouple, vecSingle, nDFS);
		}

		GAME_PUBLIC_INFO* pPublicInfo = GetPublicInfo();
		for (i = 0; i < TOTAL_CARDS; i++)
		{
			pPublicInfo->GameCard[i].nCardID = card[i];
			pPublicInfo->GameCard[i].nCardIndex = SK_GetCardIndexEx(card[i], 0);
			pPublicInfo->GameCard[i].nShape = SK_GetCardShapeEx(card[i], 0);
			pPublicInfo->GameCard[i].nValue = SK_GetCardValueEx(card[i], 0);
			pPublicInfo->GameCard[i].nCardStatus = CARD_STATUS_WAITDEAL;
			pPublicInfo->GameCard[i].nChairNO = -1;
			pPublicInfo->GameCard[i].nPositionIndex = i;
		}

		DealCardNormal();

		//DWORD dwTicket1 = GetTickCount();
		//LOG_INFO("TotalRound Cost Time : %d, nDFS : %d", dwTicket1 - dwTicket0, nDFS);

		return TRUE;
	}

	return FALSE;
}

BOOL CGameTable::MakeDealForNoShuff(int nOrder, int nBomb, int nThree, int nStraight, int cards[], std::vector<int> &vecBomb, std::vector<int> &vecThree, std::vector<int> &vecStraight, std::vector<int> &vecCouple, std::vector<int> &vecSingle, int &nDFS)
{
	std::list<int> listCardIndex;

	int nChooseArr[SK_LAYOUT_NUM - 2] = { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14 };
	int nCardLays[SK_LAYOUT_NUM] = { 0, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 1, 1 };
	int nChairCards[TOTAL_CHAIRS][CARDS_PER_CHAIR + 3];
	int nCounts[3] = { 0, 0, 0 };
	int i = 0;

	int seed = 0;

	CPlayer* ptrP = m_ptrPlayers[0];

	SvrXygRandomSort(nChooseArr, SK_LAYOUT_NUM - 2, GetTickCount() + ptrP->m_lTokenID * 10 + ptrP->m_hSocket);

	nDFS = 0;

	int nBoomNum = 0;
	if (nOrder == 1)
	{
		FindBomb(nBomb, nChooseArr, nCardLays, vecBomb);
		FindThree(nThree, nChooseArr, nCardLays, vecThree, vecCouple, vecSingle);
		FindStraight(nStraight, nChooseArr, nCardLays, vecStraight);
	}
	else if (nOrder == 2)
	{
		FindBomb(nBomb, nChooseArr, nCardLays, vecBomb);
		FindStraight(nStraight, nChooseArr, nCardLays, vecStraight);
		FindThree(nThree, nChooseArr, nCardLays, vecThree, vecCouple, vecSingle);
	}
	else if (nOrder == 3)
	{
		FindThree(nThree, nChooseArr, nCardLays, vecThree, vecCouple, vecSingle);
		FindBomb(nBomb, nChooseArr, nCardLays, vecBomb);
		FindStraight(nStraight, nChooseArr, nCardLays, vecStraight);
	}
	else if (nOrder == 4)
	{
		FindThree(nThree, nChooseArr, nCardLays, vecThree, vecCouple, vecSingle);
		FindStraight(nStraight, nChooseArr, nCardLays, vecStraight);
		FindBomb(nBomb, nChooseArr, nCardLays, vecBomb);
	}
	else if (nOrder == 5)
	{
		FindStraight(nStraight, nChooseArr, nCardLays, vecStraight);
		FindBomb(nBomb, nChooseArr, nCardLays, vecBomb);
		FindThree(nThree, nChooseArr, nCardLays, vecThree, vecCouple, vecSingle);
	}
	else if (nOrder == 6)
	{
		FindStraight(nStraight, nChooseArr, nCardLays, vecStraight);
		FindThree(nThree, nChooseArr, nCardLays, vecThree, vecCouple, vecSingle);
		FindBomb(nBomb, nChooseArr, nCardLays, vecBomb);
	}
	else
	{
		FindBomb(nBomb, nChooseArr, nCardLays, vecBomb);
		FindThree(nThree, nChooseArr, nCardLays, vecThree, vecCouple, vecSingle);
		FindStraight(nStraight, nChooseArr, nCardLays, vecStraight);
	}

	int m = 0;
	nBoomNum += vecBomb.size();
	DealChooseCards(cards, nChairCards, nCounts, vecBomb, vecThree, vecStraight, vecCouple, vecSingle);
	nBoomNum -= vecBomb.size();
	AddLeftLayout(nCardLays, vecBomb, vecThree, vecStraight, vecCouple, vecSingle);

	if (!m_b2K)
	{
		srand(GetTickCount() + ptrP->m_lTokenID * 10 + ptrP->m_hSocket);
		int nRandChair0 = rand() % TOTAL_CHAIRS;
		if (nCardLays[15] == 1)
		{
			nCardLays[15] = 0;
			int nCardID = FindOneCardByIndex(cards, 15);
			nChairCards[nRandChair0][nCounts[nRandChair0]] = nCardID;
			nCounts[nRandChair0]++;
		}
		
		srand(GetTickCount() + m_nTableNO + m_nRoomID);
		int nRandChair1 = rand() % TOTAL_CHAIRS;
		if (nCardLays[14] == 1)
		{
			nCardLays[14] = 0;
			int nCardID = FindOneCardByIndex(cards, 14);
			nChairCards[nRandChair1][nCounts[nRandChair1]] = nCardID;
			nCounts[nRandChair1]++;
		}

		//LOG_INFO("2K Chair0: %d, Chair1: %d", nRandChair0, nRandChair1);
	}
	//第三轮
	ADJUST_CARDS stFind;
	ClearAdjustCards(stFind);

	ADJUST_CARDS stBestFind;
	ClearAdjustCards(stBestFind);

	int nCountLimit = 0;
	int nDeep = 0;

	int nTempnCardLays[SK_LAYOUT_NUM];
	for (m = 0; m < SK_LAYOUT_NUM; ++m)
	{
		nTempnCardLays[m] = nCardLays[m];
	}

	// 给第一个抢庄的做剩下的牌
	nCountLimit = CARDS_PER_CHAIR + 3 - nCounts[m_nBanker];
	DWORD dwFlag = CARD_UNITE_TYPE_2KING | CARD_UNITE_TYPE_BOMB | CARD_UNITE_TYPE_THREE | CARD_UNITE_TYPE_ABT_SINGLE | CARD_UNITE_TYPE_ABT_THREE | CARD_UNITE_TYPE_ABT_COUPLE;
	FindDFS(nTempnCardLays, dwFlag, nCountLimit, stFind, stBestFind, nDeep, nDFS);
	assert(nCountLimit == stBestFind.nCardCount);
	nBoomNum += stBestFind.vecBomb.size();
	FitChooseCards(stBestFind, nChairCards, m_nBanker, cards, nCardLays, nCounts);

	//给第一个抢庄的上家
	for (m = 0; m < SK_LAYOUT_NUM; ++m)
	{
		nTempnCardLays[m] = nCardLays[m];
	}

	int nChairno = (m_nBanker + 1) % TOTAL_CHAIRS;
	nCountLimit = CARDS_PER_CHAIR - nCounts[nChairno];
	ClearAdjustCards(stFind);
	ClearAdjustCards(stBestFind);
	dwFlag = CARD_UNITE_TYPE_2KING | CARD_UNITE_TYPE_BOMB | CARD_UNITE_TYPE_THREE | CARD_UNITE_TYPE_ABT_SINGLE | CARD_UNITE_TYPE_ABT_THREE | CARD_UNITE_TYPE_ABT_COUPLE;
	FindDFS(nTempnCardLays, dwFlag, nCountLimit, stFind, stBestFind, nDeep, nDFS);
	assert(nCountLimit == stBestFind.nCardCount);
	nBoomNum += stBestFind.vecBomb.size();
	FitChooseCards(stBestFind, nChairCards, nChairno, cards, nCardLays, nCounts);

	// 给抢庄的下家
	for (m = 0; m < SK_LAYOUT_NUM; ++m)
	{
		nTempnCardLays[m] = nCardLays[m];
	}
	nChairno = (m_nBanker + 2) % TOTAL_CHAIRS;
	nCountLimit = CARDS_PER_CHAIR - nCounts[nChairno];
	ClearAdjustCards(stFind);
	ClearAdjustCards(stBestFind);
	dwFlag = CARD_UNITE_TYPE_2KING | CARD_UNITE_TYPE_BOMB | CARD_UNITE_TYPE_THREE | CARD_UNITE_TYPE_ABT_SINGLE | CARD_UNITE_TYPE_ABT_THREE | CARD_UNITE_TYPE_ABT_COUPLE;
	FindDFS(nTempnCardLays, dwFlag, nCountLimit, stFind, stBestFind, nDeep, nDFS);
	assert(nCountLimit == stBestFind.nCardCount);
	nBoomNum += stBestFind.vecBomb.size();
	FitChooseCards(stBestFind, nChairCards, nChairno, cards, nCardLays, nCounts);
	DWORD dwTicket12 = GetTickCount();



	OpelChooseCard(nChairCards, cards);
	//LOG_INFO("BoomNum: %d", nBoomNum);
	
	int nHandBoomCount = 0;
	for (int n = 0; n < TOTAL_CHAIRS; n++)
	{
		int nHandCount[SK_LAYOUT_NUM] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
		for (m = 0; m < CARDS_PER_CHAIR; m++)
		{
			int nCardIndex = SK_GetCardIndex(nChairCards[n][m]);
			nHandCount[nCardIndex] ++;
			if (nHandCount[nCardIndex] == 4)
			{
				//LOG_INFO("HandBoomIndex: %d", nCardIndex);
				nHandBoomCount++;
			}
		}
	}
	
	//LOG_INFO("HandBoomNum: %d", nHandBoomCount);
	//选出炸弹大于7个的时候重新发一次
	if (nHandBoomCount >= 7)
	{
		return FALSE;
	}
	else
	{
		return TRUE;
	}

	return TRUE;
}

void CGameTable::ClearAdjustCards(ADJUST_CARDS &stFind)
{
	stFind.nCardCount = 0;
	stFind.nHandCount = 0;
	stFind.nHandCountValue = 0;
	stFind.nCardValue = 0;
	stFind.vecBomb.clear();
	stFind.vecThree.clear();
	stFind.vecStraight.clear();
	stFind.vecAbt_Three.clear();
	stFind.vecAbt_Couple.clear();
	stFind.vecCouple.clear();
	stFind.vecSingle.clear();
}

void CGameTable::DelChooseLayer(int nCardLays[], ADJUST_CARDS &stBestFind)
{
	//TCHAR szOutput[1024];
	//sprintf(szOutput, "选中炸弹情况:");
	std::vector<int>::iterator it = stBestFind.vecBomb.begin();
	for (; it != stBestFind.vecBomb.end();)
	{
		int nCardIndex = *it;
		//sprintf(szOutput, "%s %d", szOutput, nCardIndex);
		if (nCardIndex == 14)
		{
			nCardLays[14] -= 1;
			nCardLays[15] -= 1;
		}
		else
		{
			nCardLays[nCardIndex] -= 4;
		}

		it = stBestFind.vecBomb.erase(it);
	}

	//sprintf(szOutput, "选中三张情况:");
	it = stBestFind.vecThree.begin();
	for (; it != stBestFind.vecThree.end();)
	{
		int nCardIndex = *it;
		//sprintf(szOutput, "%s %d", szOutput, nCardIndex);
		nCardLays[nCardIndex] -= 3;
		it = stBestFind.vecThree.erase(it);
	}

	//sprintf(szOutput, "选中顺子情况:");
	it = stBestFind.vecStraight.begin();
	for (; it != stBestFind.vecStraight.end();)
	{
		int nCardIndex = *it;
		//sprintf(szOutput, "%s %d", szOutput, nCardIndex);
		nCardLays[nCardIndex] -= 1;
		nCardLays[nCardIndex + 1] -= 1;
		nCardLays[nCardIndex + 2] -= 1;
		nCardLays[nCardIndex + 3] -= 1;
		nCardLays[nCardIndex + 4] -= 1;
		it = stBestFind.vecStraight.erase(it);
	}

	//sprintf(szOutput, "选中飞机的情况:");
	it = stBestFind.vecAbt_Three.begin();
	for (; it != stBestFind.vecAbt_Three.end();)
	{
		int nCardIndex = *it;
		//sprintf(szOutput, "%s %d", szOutput, nCardIndex);
		nCardLays[nCardIndex] -= 3;
		nCardLays[nCardIndex + 1] -= 3;
		it = stBestFind.vecAbt_Three.erase(it);
	}

	//sprintf(szOutput, "选中连对情况:");
	it = stBestFind.vecAbt_Couple.begin();
	for (; it != stBestFind.vecAbt_Couple.end();)
	{
		int nCardIndex = *it;
		//sprintf(szOutput, "%s %d", szOutput, nCardIndex);
		nCardLays[nCardIndex] -= 2;
		nCardLays[nCardIndex + 1] -= 2;
		nCardLays[nCardIndex + 2] -= 2;
		it = stBestFind.vecAbt_Couple.erase(it);
	}

	//sprintf(szOutput, "选中对子情况:");
	it = stBestFind.vecCouple.begin();
	for (; it != stBestFind.vecCouple.end();)
	{
		int nCardIndex = *it;
		//sprintf(szOutput, "%s %d", szOutput, nCardIndex);
		nCardLays[nCardIndex] -= 2;
		it = stBestFind.vecCouple.erase(it);
	}

	//sprintf(szOutput, "选中单张情况:");
	it = stBestFind.vecSingle.begin();
	for (; it != stBestFind.vecSingle.end();)
	{
		int nCardIndex = *it;
		//sprintf(szOutput, "%s %d", szOutput, nCardIndex);
		nCardLays[nCardIndex] -= 1;
		it = stBestFind.vecSingle.erase(it);
	}

	//LOG_INFO("%s", szOutput);
}

void CGameTable::OpelChooseCard(int nChairCards[][CARDS_PER_CHAIR + 3], int cards[])
{
	int i = 0;
	for (i = 0; i < TOTAL_CHAIRS; ++i)
	{
		int nCount = CARDS_PER_CHAIR;
		if (i == m_nBanker)
		{
			nCount = CARDS_PER_CHAIR + 3;
		}

		CPlayer* ptrP = m_ptrPlayers[i];
		SvrXygRandomSort(nChairCards[i], nCount, GetTickCount() + ptrP->m_lTokenID * 10 + ptrP->m_hSocket);
	}

	for (int i = 0; i < CARDS_PER_CHAIR; i++)
	{
		cards[i * 3] = nChairCards[0][i];
		cards[i * 3 + 1] = nChairCards[1][i];
		cards[i * 3 + 2] = nChairCards[2][i];
	}

	cards[CARDS_PER_CHAIR * 3] = nChairCards[m_nBanker][CARDS_PER_CHAIR];
	cards[CARDS_PER_CHAIR * 3 + 1] = nChairCards[m_nBanker][CARDS_PER_CHAIR + 1];
	cards[CARDS_PER_CHAIR * 3 + 2] = nChairCards[m_nBanker][CARDS_PER_CHAIR + 2];
}

void CGameTable::FitChooseCards(ADJUST_CARDS stBestFind, int nChairCards[][CARDS_PER_CHAIR + 3], int nChairNo, int cards[], int nCardLays[], int nCounts[])
{
	int j = 0;
	//TCHAR szOutput[1024];
	//sprintf(szOutput, "选中炸弹情况:");
	std::vector<int>::iterator it = stBestFind.vecBomb.begin();
	for (;it < stBestFind.vecBomb.end();)
	{
		int nCardIndex = *it;
		//sprintf(szOutput, "%s %d", szOutput, nCardIndex);
		if (nCardIndex == 14)
		{
			nCardLays[14] -= 1;
			nCardLays[15] -= 1;

			int nCardID0 = FindOneCardByIndex(cards, 14);
			int nCardID1 = FindOneCardByIndex(cards, 15);
			nChairCards[nChairNo][nCounts[nChairNo]] = nCardID0;
			nChairCards[nChairNo][nCounts[nChairNo]+1] = nCardID1;

			nCounts[nChairNo] += 2;
			it = stBestFind.vecBomb.erase(it);
		}
		else
		{
			nCardLays[nCardIndex] -= 4;

			int nCardID0 = FindOneCardByIndex(cards, nCardIndex);
			int nCardID1 = FindOneCardByIndex(cards, nCardIndex);
			int nCardID2 = FindOneCardByIndex(cards, nCardIndex);
			int nCardID3 = FindOneCardByIndex(cards, nCardIndex);

			nChairCards[nChairNo][nCounts[nChairNo]] = nCardID0;
			nChairCards[nChairNo][nCounts[nChairNo] + 1] = nCardID1;
			nChairCards[nChairNo][nCounts[nChairNo] + 2] = nCardID2;
			nChairCards[nChairNo][nCounts[nChairNo] + 3] = nCardID3;

			nCounts[nChairNo] += 4;
			it = stBestFind.vecBomb.erase(it);
		}
	}
	//LOG_INFO("%s", szOutput);

	//sprintf(szOutput, "选中三张情况:");
	it = stBestFind.vecThree.begin();
	for (it = stBestFind.vecThree.begin(); it != stBestFind.vecThree.end();)
	{
		int nCardIndex = *it;
		//sprintf(szOutput, "%s %d", szOutput, nCardIndex);
		nCardLays[nCardIndex] -= 3;
		int nCardID0 = FindOneCardByIndex(cards, nCardIndex);
		int nCardID1 = FindOneCardByIndex(cards, nCardIndex);
		int nCardID2 = FindOneCardByIndex(cards, nCardIndex);

		nChairCards[nChairNo][nCounts[nChairNo]] = nCardID0;
		nChairCards[nChairNo][nCounts[nChairNo] + 1] = nCardID1;
		nChairCards[nChairNo][nCounts[nChairNo] + 2] = nCardID2;

		nCounts[nChairNo] += 3;
		it = stBestFind.vecThree.erase(it);
	}
	//LOG_INFO("%s", szOutput);

	//sprintf(szOutput, "选中顺子情况:");
	it = stBestFind.vecStraight.begin();
	for (; it != stBestFind.vecStraight.end();)
	{
		int nCardIndex = *it;
		//sprintf(szOutput, "%s %d", szOutput, nCardIndex);

		nCardLays[nCardIndex] -= 1;
		nCardLays[nCardIndex + 1] -= 1;
		nCardLays[nCardIndex + 2] -= 1;
		nCardLays[nCardIndex + 3] -= 1;
		nCardLays[nCardIndex + 4] -= 1;

		int nCardID0 = FindOneCardByIndex(cards, nCardIndex);
		int nCardID1 = FindOneCardByIndex(cards, nCardIndex + 1);
		int nCardID2 = FindOneCardByIndex(cards, nCardIndex + 2);
		int nCardID3 = FindOneCardByIndex(cards, nCardIndex + 3);
		int nCardID4 = FindOneCardByIndex(cards, nCardIndex + 4);

		nChairCards[nChairNo][nCounts[nChairNo]] = nCardID0;
		nChairCards[nChairNo][nCounts[nChairNo] + 1] = nCardID1;
		nChairCards[nChairNo][nCounts[nChairNo] + 2] = nCardID2;
		nChairCards[nChairNo][nCounts[nChairNo] + 3] = nCardID3;
		nChairCards[nChairNo][nCounts[nChairNo] + 4] = nCardID4;

		nCounts[nChairNo] += 5;
		it = stBestFind.vecStraight.erase(it);
	}
	//LOG_INFO("%s", szOutput);

	//sprintf(szOutput, "选中飞机的情况:");
	it = stBestFind.vecAbt_Three.begin();
	for (; it != stBestFind.vecAbt_Three.end();)
	{
		int nCardIndex = *it;

		//sprintf(szOutput, "%s %d", szOutput, nCardIndex);
		nCardLays[nCardIndex] -= 3;
		nCardLays[nCardIndex + 1] -= 3;

		int nCardID0 = FindOneCardByIndex(cards, nCardIndex);
		int nCardID1 = FindOneCardByIndex(cards, nCardIndex);
		int nCardID2 = FindOneCardByIndex(cards, nCardIndex);
		int nCardID3 = FindOneCardByIndex(cards, nCardIndex + 1);
		int nCardID4 = FindOneCardByIndex(cards, nCardIndex + 1);
		int nCardID5 = FindOneCardByIndex(cards, nCardIndex + 1);

		nChairCards[nChairNo][nCounts[nChairNo]] = nCardID0;
		nChairCards[nChairNo][nCounts[nChairNo] + 1] = nCardID1;
		nChairCards[nChairNo][nCounts[nChairNo] + 2] = nCardID2;
		nChairCards[nChairNo][nCounts[nChairNo] + 3] = nCardID3;
		nChairCards[nChairNo][nCounts[nChairNo] + 4] = nCardID4;
		nChairCards[nChairNo][nCounts[nChairNo] + 5] = nCardID5;

		nCounts[nChairNo] += 6;
		it = stBestFind.vecAbt_Three.erase(it);
	}
	//LOG_INFO("%s", szOutput);

	//sprintf(szOutput, "选中连对情况:");
	it = stBestFind.vecAbt_Couple.begin();
	for (; it != stBestFind.vecAbt_Couple.end();)
	{
		int nCardIndex = *it;

		//sprintf(szOutput, "%s %d", szOutput, nCardIndex);
		nCardLays[nCardIndex] -= 2;
		nCardLays[nCardIndex + 1] -= 2;
		nCardLays[nCardIndex + 2] -= 2;

		int nCardID0 = FindOneCardByIndex(cards, nCardIndex);
		int nCardID1 = FindOneCardByIndex(cards, nCardIndex);
		int nCardID2 = FindOneCardByIndex(cards, nCardIndex + 1);
		int nCardID3 = FindOneCardByIndex(cards, nCardIndex + 1);
		int nCardID4 = FindOneCardByIndex(cards, nCardIndex + 2);
		int nCardID5 = FindOneCardByIndex(cards, nCardIndex + 2);

		nChairCards[nChairNo][nCounts[nChairNo]] = nCardID0;
		nChairCards[nChairNo][nCounts[nChairNo] + 1] = nCardID1;
		nChairCards[nChairNo][nCounts[nChairNo] + 2] = nCardID2;
		nChairCards[nChairNo][nCounts[nChairNo] + 3] = nCardID3;
		nChairCards[nChairNo][nCounts[nChairNo] + 4] = nCardID4;
		nChairCards[nChairNo][nCounts[nChairNo] + 5] = nCardID5;

		nCounts[nChairNo] += 6;
		it = stBestFind.vecAbt_Couple.erase(it);
	}
	//LOG_INFO("%s", szOutput);

	//sprintf(szOutput, "选中对子情况:");
	it = stBestFind.vecCouple.begin();
	for (; it != stBestFind.vecCouple.end();)
	{
		int nCardIndex = *it;

		//sprintf(szOutput, "%s %d", szOutput, nCardIndex);
		nCardLays[nCardIndex] -= 2;

		int nCardID0 = FindOneCardByIndex(cards, nCardIndex);
		int nCardID1 = FindOneCardByIndex(cards, nCardIndex);

		nChairCards[nChairNo][nCounts[nChairNo]] = nCardID0;
		nChairCards[nChairNo][nCounts[nChairNo] + 1] = nCardID1;

		nCounts[nChairNo] += 2;
		it = stBestFind.vecCouple.erase(it);
	}
	//LOG_INFO("%s", szOutput);

	//sprintf(szOutput, "选中单张情况:");
	it = stBestFind.vecSingle.begin();
	for (; it != stBestFind.vecSingle.end();)
	{
		int nCardIndex = *it;

		//sprintf(szOutput, "%s %d", szOutput, nCardIndex);
		nCardLays[nCardIndex] -= 1;

		int nCardID0 = FindOneCardByIndex(cards, nCardIndex);

		nChairCards[nChairNo][nCounts[nChairNo]] = nCardID0;

		nCounts[nChairNo] += 1;
		it = stBestFind.vecSingle.erase(it);
	}
	//LOG_INFO("%s", szOutput);
}

void CGameTable::DealChooseCards(int cards[], int nCardCount[][CARDS_PER_CHAIR + 3], int nCounts[], std::vector<int> &vecBomb, std::vector<int> &vecThree, std::vector<int> &vecStraight, std::vector<int> &vecCouple, std::vector<int> &vecSingle)
{
	TCHAR szOutput[128];
	
	int nOtherTotal = vecBomb.size() + vecThree.size();

	if (nOtherTotal == 0)
	{
		return;
	}

	srand(time(NULL));
	int nRandGive = rand() % nOtherTotal;
	
	int i = 0;

	int nCurBanker = m_nBanker;
	for (i = 0; i < nOtherTotal; i++)
	{
		if (nRandGive >= i)
		{
			std::vector<int>::iterator iter = vecStraight.begin();
			//按策划案，要求每人第一轮最多发一个顺子，但是配置文件策划又说能配置大于3
			int j = 0;

			for (; iter != vecStraight.end() && j < 3;)
			{
				int nCount = nCounts[nCurBanker];
				int nLeftCount = CARDS_PER_CHAIR - nCount;
				if (nCurBanker == m_nBanker)
				{
					nLeftCount += 3;
				}

				int nCardIndex = *iter;
				if (nLeftCount >= 5)
				{
					int cardid0 = FindOneCardByIndex(cards, nCardIndex);
					int cardid1 = FindOneCardByIndex(cards, nCardIndex + 1);
					int cardid2 = FindOneCardByIndex(cards, nCardIndex + 2);
					int cardid3 = FindOneCardByIndex(cards, nCardIndex + 3);
					int cardid4 = FindOneCardByIndex(cards, nCardIndex + 4);
					nCardCount[nCurBanker][nCount] = cardid0;
					nCardCount[nCurBanker][nCount + 1] = cardid1;
					nCardCount[nCurBanker][nCount + 2] = cardid2;
					nCardCount[nCurBanker][nCount + 3] = cardid3;
					nCardCount[nCurBanker][nCount + 4] = cardid4;
					nCounts[nCurBanker] += 5;
					iter = vecStraight.erase(iter);
					//sprintf(szOutput, "%d 获得顺子, index:%d", nCurBanker, nCardIndex);
				}
				else
				{
					//分配给的玩家不符合条件直接丢弃
					//sprintf(szOutput, "%d 丢弃顺子, index:%d", nCurBanker, nCardIndex);
					++iter;
				}

				j++;
				nCurBanker = (nCurBanker + 1) % TOTAL_CHAIRS;
				//LOG_INFO("%s", szOutput);
			}
		}

		std::vector<int>::iterator iterBomb = vecBomb.begin();
		std::vector<int>::iterator iterThree = vecThree.begin();
		srand(time(NULL) + i * 1000);
		int randChoose = rand() % 2;
		if ((randChoose == 0 && vecBomb.size() > 0) || (randChoose == 1 && vecThree.size() <= 0))
		{
			int nCount = nCounts[nCurBanker];
			int nLeftCount = CARDS_PER_CHAIR - nCount;
			
			if (nCurBanker == m_nBanker)
			{
				nLeftCount += 3;
			}

			int nCardIndex = *iterBomb;
			if (nCardIndex == 14 && nLeftCount >= 2)
			{
				int cardid0 = FindOneCardByIndex(cards, 14);
				int cardid1 = FindOneCardByIndex(cards, 15);
				nCardCount[nCurBanker][nCount] = cardid0;
				nCardCount[nCurBanker][nCount + 1] = cardid1;
				nCounts[nCurBanker] += 2;
				iterBomb = vecBomb.erase(iterBomb);
				//sprintf(szOutput, "%d 获得炸弹, index:%d", nCurBanker, nCardIndex);
			}
			else if (nLeftCount >= 4)
			{
				int cardid0 = FindOneCardByIndex(cards, nCardIndex);
				int cardid1 = FindOneCardByIndex(cards, nCardIndex);
				int cardid2 = FindOneCardByIndex(cards, nCardIndex);
				int cardid3 = FindOneCardByIndex(cards, nCardIndex);
				nCardCount[nCurBanker][nCount] = cardid0;
				nCardCount[nCurBanker][nCount + 1] = cardid1;
				nCardCount[nCurBanker][nCount + 2] = cardid2;
				nCardCount[nCurBanker][nCount + 3] = cardid3;
				nCounts[nCurBanker] += 4;
				iterBomb = vecBomb.erase(iterBomb);
				//sprintf(szOutput, "%d 获得炸弹, index:%d", nCurBanker, nCardIndex);
			}
			else
			{
				//分配给的玩家不符合直接丢弃
				//sprintf(szOutput, "%d 丢弃炸弹, index:%d", nCurBanker, nCardIndex);
				++iterBomb;
			}

			//LOG_INFO("%s", szOutput);
		}
		else
		{
			//LOG_INFO("vecThree Num %d, random %d", vecThree.size(), randChoose);
			assert(vecThree.size() > 0);
			int nCount = nCounts[nCurBanker];
			int nLeftCount = CARDS_PER_CHAIR - nCount;
			if (nCurBanker == m_nBanker)
			{
				nLeftCount += 3;
			}
			int nCardIndex = *iterThree;
			if (nLeftCount >= 3)
			{
				int cardid0 = FindOneCardByIndex(cards, nCardIndex);
				int cardid1 = FindOneCardByIndex(cards, nCardIndex);
				int cardid2 = FindOneCardByIndex(cards, nCardIndex);
				nCardCount[nCurBanker][nCount] = cardid0;
				nCardCount[nCurBanker][nCount + 1] = cardid1;
				nCardCount[nCurBanker][nCount + 2] = cardid2;
				nCounts[nCurBanker] += 3;
				iterThree = vecThree.erase(iterThree);
				//sprintf(szOutput, "%d 获得三张, index:%d", nCurBanker, nCardIndex);

				//原来没考虑倒，临时搞个僵硬的处理，其实应该修改vec<int>改成一个合适的vec<结构体>
				std::vector<int>::iterator itCouple = vecCouple.begin();
				std::vector<int>::iterator itSingle = vecSingle.begin();
				if (randChoose == 0 && vecCouple.size() > 0 && nLeftCount - 3 >= 2)
				{
					int nCardIndex = *itCouple;
					int cardid0 = FindOneCardByIndex(cards, nCardIndex);
					int cardid1 = FindOneCardByIndex(cards, nCardIndex);
					nCount = nCounts[nCurBanker];
					nCardCount[nCurBanker][nCount] = cardid0;
					nCardCount[nCurBanker][nCount + 1] = cardid1;
					nCounts[nCurBanker] += 2;
					itCouple = vecCouple.erase(itCouple);

					//LOG_INFO("%d 获得对子, index: %d", nCurBanker, nCardIndex);
				}
				else if (randChoose == 1 && vecSingle.size() > 0 && nLeftCount - 3 >= 1)
				{
					int nCardIndex = *itSingle;
					int cardid0 = FindOneCardByIndex(cards, nCardIndex);
					nCount = nCounts[nCurBanker];
					nCardCount[nCurBanker][nCount] = cardid0;
					nCounts[nCurBanker] += 1;
					itSingle = vecSingle.erase(itSingle);

					//LOG_INFO("%d 获得单张, index: %d", nCurBanker, nCardIndex);
				}
				else if (vecCouple.size() > 0 && nLeftCount - 3 >= 2)
				{
					int nCardIndex = *itCouple;
					int cardid0 = FindOneCardByIndex(cards, nCardIndex);
					int cardid1 = FindOneCardByIndex(cards, nCardIndex);
					nCount = nCounts[nCurBanker];
					nCardCount[nCurBanker][nCount] = cardid0;
					nCardCount[nCurBanker][nCount + 1] = cardid1;
					nCounts[nCurBanker] += 2;
					itCouple = vecCouple.erase(itCouple);

					//LOG_INFO("%d 获得对子, index: %d", nCurBanker, nCardIndex);
				}
				else if (vecSingle.size() > 0 && nLeftCount - 3 >= 1)
				{
					int nCardIndex = *itSingle;
					int cardid0 = FindOneCardByIndex(cards, nCardIndex);
					nCount = nCounts[nCurBanker];
					nCardCount[nCurBanker][nCount] = cardid0;
					nCounts[nCurBanker] += 1;
					itSingle = vecSingle.erase(itSingle);

					//LOG_INFO("%d 获得单张, index: %d", nCurBanker, nCardIndex);
				}
			}
			else
			{
				//分配给的玩家不符合条件直接丢弃
				//sprintf(szOutput, "%d 丢弃三张, index:%d", nCurBanker, nCardIndex);
				++iterThree;
			}

			//LOG_INFO("%s", szOutput);
		}

		nCurBanker = (nCurBanker + 1) % TOTAL_CHAIRS;
	}
}

int CGameTable::FindOneCardByIndex(int cards[], int nCardIndex)
{
	int nFindId = INVALID_OBJECT_ID;
	for (int i = 0; i < TOTAL_CARDS; i++)
	{
		int cardid = cards[i];
		int skindex = SK_GetCardIndex(cards[i]);
		if (cards[i] != INVALID_OBJECT_ID && SK_GetCardIndex(cards[i]) == nCardIndex)
		{
			nFindId = cards[i];
			cards[i] = INVALID_OBJECT_ID;
			break;
		}
	}

	return nFindId;
}

void CGameTable::FindBomb(int nBomb, int nChooseArr[], int nCardLays[], std::vector<int> &vecBomb)
{
	int nFindNum = 0;

	if (nBomb < 0)
	{
		return;
	}

	for (int j = 0; j < SK_LAYOUT_NUM - 2; j++)
	{
		int nCardIndex = nChooseArr[j];
		if (nCardIndex == 14 && nCardLays[14] == 1 && nCardLays[15] == 1 && m_b2K)
		{
			nFindNum += 1;
			nCardLays[14] = 0;
			nCardLays[15] = 0;
			vecBomb.push_back(nCardIndex);
		}
		else if (nCardLays[nCardIndex] == 4)
		{
			nFindNum += 1;
			nCardLays[nCardIndex] -= 4;
			vecBomb.push_back(nCardIndex);
		}

		if (nFindNum >= nBomb)
		{
			break;
		}
	}	
}

void CGameTable::FindThree(int nThree, int nChooseArr[], int nCardLays[], std::vector<int> &vecThree, std::vector<int> &vecCouple, std::vector<int> &vecSingle)
{
	int nFindNum = 0;

	if (nThree < 0)
	{
		return;
	}

	for (int j = 0; j < SK_LAYOUT_NUM - 2; j++)
	{
		int nCardIndex = nChooseArr[j];
		if (nCardIndex == 14)
		{
			continue;
		}
		else if (nCardLays[nCardIndex] >= 3)
		{
			nFindNum += 1;
			nCardLays[nCardIndex] -= 3;
			vecThree.push_back(nCardIndex);
		}

		srand(GetTickCount() + j * 10);
		int randWith = rand() % 3;
		int m = 0;
		int n = 0;
		if (randWith == 1)
		{
			for (m = 1; m < SK_LAYOUT_NUM - 2; m++)
			{
				if (nCardLays[m] >= 1)
				{
					nCardLays[m] -= 1;
					vecSingle.push_back(m);
					break;
				}
			}
		}
		else if (randWith == 2)
		{
			for (m = 0; m < SK_LAYOUT_NUM - 2; m++)
			{
				if (nCardLays[m] >= 2)
				{
					nCardLays[m] -= 2;
					vecCouple.push_back(m);
					break;
				}
			}
		}

		if (nFindNum >= nThree)
		{
			break;
		}
	}
}

void CGameTable::FindStraight(int nStraight, int nChooseArr[], int nCardLays[], std::vector<int> &vecStraight)
{
	int nFindNum = 0;

	if (nStraight < 0)
	{
		return;
	}

	for (int j = 0; j < SK_LAYOUT_NUM - 2; j++)
	{
		int nCardIndex = nChooseArr[j];
		if (nCardIndex >= 10)
		{
			continue;
		}

		if (nCardLays[nCardIndex] >= 1 && nCardLays[nCardIndex + 1] >= 1 && nCardLays[nCardIndex + 2] >= 1 && nCardLays[nCardIndex + 3] >= 1 && nCardLays[nCardIndex + 4] >= 1)
		{
			nFindNum += 1;
			vecStraight.push_back(nCardIndex);
			nCardLays[nCardIndex] -= 1;
			nCardLays[nCardIndex + 1] -= 1;
			nCardLays[nCardIndex + 2] -= 1;
			nCardLays[nCardIndex + 3] -= 1;
			nCardLays[nCardIndex + 4] -= 1;
		}

		if (nFindNum >= nStraight)
		{
			break;
		}
	}
}

void CGameTable::AddLeftLayout(int nCardLays[], std::vector<int> vecBomb, std::vector<int> vecThree, std::vector<int> vecStraight, std::vector<int> vecCouple, std::vector<int> vecSingle)
{
	std::vector<int>::iterator itbomb = vecBomb.begin();
	//调试输出
	LOG_INFO("第一轮返还炸弹数目:%d", vecBomb.size());
	//
	for (; itbomb != vecBomb.end();)
	{
		int nCardIndex = *itbomb;
		//调试输出
		LOG_INFO("%d", nCardIndex);
		//
		if (nCardIndex == 14)
		{
			nCardLays[14] += 1;
			nCardLays[15] += 1;
		}
		else
		{
			nCardLays[nCardIndex] += 4;
		}
		
		itbomb = vecBomb.erase(itbomb);
	}

	//调试输出
	LOG_INFO("第一轮返还三张数目:%d", vecThree.size());
	//
	std::vector<int>::iterator itthree = vecThree.begin();
	for (; itthree != vecThree.end();)
	{
		int nCardIndex = *itthree;
		//调试输出
		LOG_INFO("%d", nCardIndex);
		//
		nCardLays[nCardIndex] += 3;
		itthree = vecThree.erase(itthree);
	}

	std::vector<int>::iterator itstraight = vecStraight.begin();
	//调试输出
	LOG_INFO("第一轮返还顺子数目:%d", vecStraight.size());
	//
	for (; itstraight != vecStraight.end();)
	{
		int nCardIndex = *itstraight;
		//调试输出
		LOG_INFO("%d", nCardIndex);
		//
		nCardLays[nCardIndex] += 1;
		nCardLays[nCardIndex + 1] += 1;
		nCardLays[nCardIndex + 2] += 1;
		nCardLays[nCardIndex + 3] += 1;
		nCardLays[nCardIndex + 4] += 1;
		itstraight = vecStraight.erase(itstraight);
	}

	//调试输出
	LOG_INFO("第一轮返还2带数目:%d", vecCouple.size());
	//
	std::vector<int>::iterator itcouple = vecCouple.begin();
	for (; itcouple != vecCouple.end();)
	{
		int nCardIndex = *itcouple;
		//调试输出
		LOG_INFO("%d", nCardIndex);
		//
		nCardLays[nCardIndex] += 2;
		itcouple = vecCouple.erase(itcouple);
	}

	//调试输出
	LOG_INFO("第一轮返还1带数目:%d", vecSingle.size());
	//
	std::vector<int>::iterator itsingle = vecSingle.begin();
	for (; itsingle != vecSingle.end();)
	{
		int nCardIndex = *itsingle;
		//调试输出
		LOG_INFO("%d", nCardIndex);
		//
		nCardLays[nCardIndex] += 1;
		itsingle = vecSingle.erase(itsingle);
	}
}

void CGameTable::FindDFS(int nCardLays[], DWORD &dwFlag, int nCountLimit, ADJUST_CARDS &stFind, ADJUST_CARDS &stBestFind, int &nDeep, int &nDFS)
{
	BOOL bCountEnough = FALSE;

	nDFS += 1;

	//调试输出
	/*TCHAR szOutput[1024] = "";
	sprintf(szOutput, "DFS Deep:%d HandCount:%d nCardCount:%d nCountLimit:%d Bomb:%d Tree:%d Abt:%d AbtTree:%d AbtCouple:%d Couple:%d Single:%d Layout剩余情况:", nDeep, stFind.nHandCount, stFind.nCardCount, nCountLimit,
		stFind.vecBomb.size(), stFind.vecThree.size(), stFind.vecStraight.size(), stFind.vecAbt_Three.size(), stFind.vecAbt_Couple.size(), stFind.vecCouple.size(), stFind.vecSingle.size());

	for (int m = 1; m < SK_LAYOUT_NUM; ++m)
	{
		sprintf(szOutput, "%s %d", szOutput, nCardLays[m]);
	}
	LOG_INFO("%s", szOutput);*/

	if (stFind.nCardCount >= nCountLimit || dwFlag == 0)
	{
		//LOG_INFO("Finish 1");
		if (stFind.nCardCount > nCountLimit)
		{
			LOG_ERROR("FINDERROR stFind.nCardCount: %d, nCountLimit: %d", stFind.nCardCount, nCountLimit);
		}

		stFind.nHandCountValue = stFind.nHandCount;
		
		int nThreeCount = stFind.vecThree.size();
		int nCoulpeCount = stFind.vecCouple.size();
		int nSingleCount = stFind.vecSingle.size();

		if (nThreeCount >= (nSingleCount + nCoulpeCount))
		{
			stFind.nHandCountValue -= (nSingleCount + nCoulpeCount);
		}
		else
		{
			stFind.nHandCountValue -= nThreeCount;
		}

		CalcFindCountAndValue(stFind);
		

		/*if (stBestFind.nHandCount == 0 || stBestFind.nCardCount > stFind.nCardCount)
		{
			stBestFind = stFind;
		}*/

		//LOG_INFO("stFind.nCardCount:%d, stFind.nHandCountValue :%d, stFind.nCardValue: %d", stFind.nCardCount, stFind.nHandCountValue, stFind.nCardValue);
		if (stBestFind.nHandCount == 0 || stBestFind.nHandCountValue > stFind.nHandCountValue)
		{
			stBestFind = stFind;
		}
		else if (stBestFind.nHandCountValue == stFind.nHandCountValue && stBestFind.nCardValue < stFind.nCardValue)
		{
			stBestFind = stFind;
		}
		else if (stBestFind.nHandCountValue == stFind.nHandCountValue && stBestFind.nCardValue < stFind.nCardValue)
		{
			//如果手数相同，随机一下要不要替换
			srand(time(NULL) + stBestFind.nHandCountValue);
			int randChoose = rand() % 2;
			if (randChoose == 0)
			{
				stBestFind = stFind;
			}
		}
		
		bCountEnough = TRUE;
	}
	
	if (IS_BIT_SET(dwFlag, CARD_UNITE_TYPE_2KING) && !bCountEnough)
	{
		if (m_b2K)
		{
			if (nCardLays[14] == 1 && nCardLays[15] == 1)
			{
				//LOG_INFO("Pre 2K");
				stFind.nCardCount += 2;

				if (stFind.nCardCount <= nCountLimit)
				{
					//LOG_INFO("2K");
					nCardLays[14] = 0;
					nCardLays[15] = 0;
					stFind.nHandCount += 1;
					stFind.vecBomb.push_back(14);
					nDeep += 1;
					FindDFS(nCardLays, dwFlag, nCountLimit, stFind, stBestFind, nDeep, nDFS);
					if (stFind.nCardCount >= nCountLimit)
					{
						bCountEnough = TRUE;
					}
					nCardLays[14] = 1;
					nCardLays[15] = 1;
					stFind.nHandCount -= 1;
					stFind.vecBomb.pop_back();
					nDeep -= 1;
				}

				stFind.nCardCount -= 2;
			}
		}
		
		dwFlag &= ~CARD_UNITE_TYPE_2KING;
	}

	if (IS_BIT_SET(dwFlag, CARD_UNITE_TYPE_BOMB) && !bCountEnough)
	{
		BOOL bFind = FALSE;
		int nCardIndex = 0;
		for (int i = 1; i < 14; i++)
		{
			if (nCardLays[i] == 4)
			{
				bFind = TRUE;
				nCardIndex = i;
				break;
			}
		}

		if (bFind)
		{
			//LOG_INFO("Pre Bomb");
			stFind.nCardCount += 4;
			if (stFind.nCardCount <= nCountLimit)
			{
				//LOG_INFO("Bomb");
				nCardLays[nCardIndex] -= 4;
				//测试正确性
				assert(nCardLays[nCardIndex] == 0);
				stFind.nHandCount += 1;
				stFind.vecBomb.push_back(nCardIndex);
				nDeep += 1;
				FindDFS(nCardLays, dwFlag, nCountLimit, stFind, stBestFind, nDeep, nDFS);
				if (stFind.nCardCount >= nCountLimit)
				{
					bCountEnough = TRUE;
				}
				nCardLays[nCardIndex] += 4;
				stFind.nHandCount -= 1;
				stFind.vecBomb.pop_back();
				nDeep -= 1;
			}
			
			stFind.nCardCount -= 4;
		}
	}

	if (IS_BIT_SET(dwFlag, CARD_UNITE_TYPE_THREE) && !bCountEnough)
	{
		BOOL bFind = FALSE;
		int nCardIndex = 0;
		for (int i = 1; i < 14; i++)
		{
			if (nCardLays[i] >= 3)
			{
				bFind = TRUE;
				nCardIndex = i;
				break;
			}
		}

		if (bFind)
		{
			//LOG_INFO("Pre Three");
			stFind.nCardCount += 3;

			if (stFind.nCardCount <= nCountLimit)
			{
				//LOG_INFO("Three");
				nCardLays[nCardIndex] -= 3;
				//测试正确性
				assert(nCardLays[nCardIndex] >= 0);
				stFind.nHandCount += 1;
				stFind.vecThree.push_back(nCardIndex);
				nDeep += 1;
				FindDFS(nCardLays, dwFlag, nCountLimit, stFind, stBestFind, nDeep, nDFS);
				if (stFind.nCardCount >= nCountLimit)
				{
					bCountEnough = TRUE;
				}
				nCardLays[nCardIndex] += 3;
				stFind.nHandCount -= 1;
				stFind.vecThree.pop_back();
				nDeep -= 1;
			}

			stFind.nCardCount -= 3;
			
		}
	}

	if (IS_BIT_SET(dwFlag, CARD_UNITE_TYPE_ABT_SINGLE) && !bCountEnough)
	{
		BOOL bFind = FALSE;
		int nCardIndex = 0;

		for (int i = 1; i < 10; i++)
		{
			if (nCardLays[i] >= 1 && nCardLays[i + 1] >= 1 && nCardLays[i + 2] >= 1 && nCardLays[i + 3] >= 1 && nCardLays[i + 4] >= 1)
			{
				bFind = TRUE;
				nCardIndex = i;
				break;
			}
		}

		if (bFind)
		{
			//LOG_INFO("Pre ABT");
			stFind.nCardCount += 5;

			if (stFind.nCardCount <= nCountLimit)
			{
				//LOG_INFO("ABT");
				nCardLays[nCardIndex] -= 1;
				nCardLays[nCardIndex + 1] -= 1;
				nCardLays[nCardIndex + 2] -= 1;
				nCardLays[nCardIndex + 3] -= 1;
				nCardLays[nCardIndex + 4] -= 1;
				stFind.nHandCount += 1;
				stFind.vecStraight.push_back(nCardIndex);
				nDeep += 1;
				FindDFS(nCardLays, dwFlag, nCountLimit, stFind, stBestFind, nDeep, nDFS);
				if (stFind.nCardCount >= nCountLimit)
				{
					bCountEnough = TRUE;
				}
				nCardLays[nCardIndex] += 1;
				nCardLays[nCardIndex + 1] += 1;
				nCardLays[nCardIndex + 2] += 1;
				nCardLays[nCardIndex + 3] += 1;
				nCardLays[nCardIndex + 4] += 1;
				stFind.nHandCount -= 1;
				stFind.vecStraight.pop_back();
				nDeep -= 1;
			}
			
			stFind.nCardCount -= 5;
		}
	}

	if (IS_BIT_SET(dwFlag, CARD_UNITE_TYPE_ABT_THREE) && !bCountEnough)
	{
		BOOL bFind = FALSE;
		int nCardIndex = 0;

		for (int i = 1; i < 13; i++)
		{
			if (nCardLays[i] >= 3 && nCardLays[i + 1] >= 3)
			{
				bFind = TRUE;
				nCardIndex = i;
				break;
			}
		}

		if (bFind)
		{
			//LOG_INFO("Pre ABT_TREE");
			stFind.nCardCount += 6;

			if (stFind.nCardCount <= nCountLimit)
			{
				//LOG_INFO("ABT_TREE");
				nCardLays[nCardIndex] -= 3;
				nCardLays[nCardIndex + 1] -= 3;
				stFind.nHandCount += 1;
				stFind.vecAbt_Three.push_back(nCardIndex);
				nDeep += 1;
				FindDFS(nCardLays, dwFlag, nCountLimit, stFind, stBestFind, nDeep, nDFS);
				if (stFind.nCardCount >= nCountLimit)
				{
					bCountEnough = TRUE;
				}
				nCardLays[nCardIndex] += 3;
				nCardLays[nCardIndex + 1] += 3;
				stFind.nHandCount -= 1;
				stFind.vecAbt_Three.pop_back();
				nDeep -= 1;
			}

			stFind.nCardCount -= 6;
			
		}
	}

	if (IS_BIT_SET(dwFlag, CARD_UNITE_TYPE_ABT_COUPLE) && !bCountEnough)
	{
		BOOL bFind = FALSE;
		int nCardIndex = 0;

		for (int i = 1; i < 12; i++)
		{
			if (nCardLays[i] >= 2 && nCardLays[i + 1] >= 2 && nCardLays[i + 2] >= 2)
			{
				bFind = TRUE;
				nCardIndex = i;
				break;
			}
		}

		if (bFind)
		{
			//LOG_INFO("Pre ABT_COUPLE");
			stFind.nCardCount += 6;

			if (stFind.nCardCount <= nCountLimit)
			{
				//LOG_INFO("ABT_COUPLE");
				nCardLays[nCardIndex] -= 2;
				nCardLays[nCardIndex + 1] -= 2;
				nCardLays[nCardIndex + 2] -= 2;
				stFind.nHandCount += 1;
				stFind.vecAbt_Couple.push_back(nCardIndex);
				nDeep += 1;
				FindDFS(nCardLays, dwFlag, nCountLimit, stFind, stBestFind, nDeep, nDFS);
				if (stFind.nCardCount >= nCountLimit)
				{
					bCountEnough = TRUE;
				}
				nCardLays[nCardIndex] += 2;
				nCardLays[nCardIndex + 1] += 2;
				nCardLays[nCardIndex + 2] += 2;
				stFind.nHandCount -= 1;
				stFind.vecAbt_Couple.pop_back();
				nDeep -= 1;
			}

			stFind.nCardCount -= 6;
		}
	}

	if (IS_BIT_SET(dwFlag, CARD_UNITE_TYPE_COUPLE) && !bCountEnough)
	{
		BOOL bFind = FALSE;
		int nCardIndex = 0;
		int nFindCount = 0;

		int i = 0;
		for (i = 13; i > 0; i--)
		{
			if (nCountLimit - stFind.nCardCount < 2)
			{
				break;
			}

			if (nCardLays[i] >= 2)
			{
				//LOG_INFO("Pre COUPLE");
				nFindCount += 1;
				stFind.nCardCount += 2;

				if (stFind.nCardCount <= nCountLimit)
				{
					bFind = TRUE;
					//LOG_INFO("COUPLE");
					nCardLays[i] -= 2;
					stFind.nHandCount += 1;
					stFind.vecCouple.push_back(i);
					nDeep += 1;
					
					if (stFind.nCardCount >= nCountLimit)
					{
						bCountEnough = TRUE;
						break;
					}
					
				}
			}
		}

		if (i == 0 && !bCountEnough)
		{
			for (i = 13; i > 0; i--)
			{
				if (nCountLimit - stFind.nCardCount < 2)
				{
					break;
				}

				if (nCardLays[i] == 2)
				{
					//LOG_INFO("Pre COUPLE");
					nFindCount += 1;
					stFind.nCardCount += 2;

					if (stFind.nCardCount <= nCountLimit)
					{
						bFind = TRUE;
						//LOG_INFO("COUPLE");
						nCardLays[i] -= 2;
						stFind.nHandCount += 1;
						stFind.vecCouple.push_back(i);
						nDeep += 1;

						if (stFind.nCardCount >= nCountLimit)
						{
							bCountEnough = TRUE;
							break;
						}

					}
				}
			}
		}

		if (bFind)
		{
			dwFlag &= ~CARD_UNITE_TYPE_COUPLE;
			FindDFS(nCardLays, dwFlag, nCountLimit, stFind, stBestFind, nDeep, nDFS);

			std::vector<int>::iterator it = stFind.vecCouple.begin();
			for (; it != stFind.vecCouple.end();)
			{
				int nCardIndex = *it;
				nCardLays[nCardIndex] += 2;
				stFind.nHandCount -= 1;
				it = stFind.vecCouple.erase(it);
				nDeep -= 1;

			}

		}

		stFind.nCardCount -= 2 * nFindCount;
	}

	if (IS_BIT_SET(dwFlag, CARD_UNITE_TYPE_SINGLE) && !bCountEnough)
	{
		BOOL bFind = FALSE;
		int nCardIndex = 0;
		int nFindCount = 0;

		int i = 0;
		for (i = 15; i > 0; i--)
		{
			if (nCardLays[i] >= 1)
			{
				//LOG_INFO("Pre SINGLE");
				nFindCount += 1;
				stFind.nCardCount += 1;

				if (stFind.nCardCount <= nCountLimit)
				{
					bFind = TRUE;
					//LOG_INFO("SINGLE");
					nCardLays[i] -= 1;
					stFind.nHandCount += 1;
					stFind.vecSingle.push_back(i);
					nDeep += 1;
					if (stFind.nCardCount >= nCountLimit)
					{
						bCountEnough = TRUE;
						break;
					}
					
				}
			}
		}

		if (i == 0 && !bCountEnough)
		{
			for (int i = 15; i > 0; i--)
			{
				if (nCardLays[i] >= 1)
				{
					//LOG_INFO("Pre SINGLE");
					nFindCount += 1;
					stFind.nCardCount += 1;

					if (stFind.nCardCount <= nCountLimit)
					{
						bFind = TRUE;
						//LOG_INFO("SINGLE");
						nCardLays[i] -= 1;
						stFind.nHandCount += 1;
						stFind.vecSingle.push_back(i);
						nDeep += 1;
						if (stFind.nCardCount >= nCountLimit)
						{
							bCountEnough = TRUE;
							break;
						}

					}

				}
			}
		}

		if (i == 0 && !bCountEnough)
		{
			for (int i = 15; i > 0; i--)
			{
				if (nCardLays[i] >= 1)
				{
					//LOG_INFO("Pre SINGLE");
					nFindCount += 1;
					stFind.nCardCount += 1;

					if (stFind.nCardCount <= nCountLimit)
					{
						bFind = TRUE;
						//LOG_INFO("SINGLE");
						nCardLays[i] -= 1;
						stFind.nHandCount += 1;
						stFind.vecSingle.push_back(i);
						nDeep += 1;
						if (stFind.nCardCount >= nCountLimit)
						{
							bCountEnough = TRUE;
							break;
						}

					}

				}
			}
		}

		if (i == 0 && !bCountEnough)
		{
			for (int i = 15; i > 0; i--)
			{
				if (nCardLays[i] >= 1)
				{
					//LOG_INFO("Pre SINGLE");
					nFindCount += 1;
					stFind.nCardCount += 1;

					if (stFind.nCardCount <= nCountLimit)
					{
						bFind = TRUE;
						//LOG_INFO("SINGLE");
						nCardLays[i] -= 1;
						stFind.nHandCount += 1;
						stFind.vecSingle.push_back(i);
						nDeep += 1;
						if (stFind.nCardCount >= nCountLimit)
						{
							bCountEnough = TRUE;
							break;
						}

					}

				}
			}
		}

		if (bFind)
		{
			dwFlag &= ~CARD_UNITE_TYPE_SINGLE;

			FindDFS(nCardLays, dwFlag, nCountLimit, stFind, stBestFind, nDeep, nDFS);

			//LOG_INFO("AFTER SINGLE");

			std::vector<int>::iterator it = stFind.vecSingle.begin();
			for (; it != stFind.vecSingle.end();)
			{
				int nCardIndex = *it;
				nCardLays[nCardIndex] += 1;
				stFind.nHandCount -= 1;
				it = stFind.vecSingle.erase(it);
				nDeep -= 1;
			}
		}

		stFind.nCardCount -= nFindCount;
	}
	
	if (!bCountEnough && dwFlag != 0)
	{
		/*sprintf(szOutput, "DFS Deep:%d HandCount:%d nCardCount:%d nCountLimit:%d Bomb:%d Tree:%d Abt:%d AbtTree:%d AbtCouple:%d Couple:%d Single:%d Layout剩余情况:", nDeep, stFind.nHandCount, stFind.nCardCount, nCountLimit,
			stFind.vecBomb.size(), stFind.vecThree.size(), stFind.vecStraight.size(), stFind.vecAbt_Three.size(), stFind.vecAbt_Couple.size(), stFind.vecCouple.size(), stFind.vecSingle.size());

		for (int m = 1; m < SK_LAYOUT_NUM; ++m)
		{
			sprintf(szOutput, "%s %d", szOutput, nCardLays[m]);
		}
		LOG_INFO("%s", szOutput);*/

		//如果剩余可以补足的牌大于3张且有大于3张的牌，直接丢弃该次遍历
		if (nCountLimit - stFind.nCardCount >= 3)
		{
			for (int j = 1; j < 14; j++)
			{
				if (nCardLays[j] >= 3)
				{
					//LOG_INFO("NOTENOUGH PASS");
					return;
				}
			}
		}
		
		DWORD dwLeftFlag = CARD_UNITE_TYPE_COUPLE | CARD_UNITE_TYPE_SINGLE;
		//LOG_INFO("NOTENOUGH");
		FindDFS(nCardLays, dwLeftFlag, nCountLimit, stFind, stBestFind, nDeep, nDFS);
		/*sprintf(szOutput, "DFS Deep:%d HandCount:%d nCardCount:%d nCountLimit:%d Bomb:%d Tree:%d Abt:%d AbtTree:%d AbtCouple:%d Couple:%d Single:%d Layout剩余情况:", nDeep, stFind.nHandCount, stFind.nCardCount, nCountLimit,
			stFind.vecBomb.size(), stFind.vecThree.size(), stFind.vecStraight.size(), stFind.vecAbt_Three.size(), stFind.vecAbt_Couple.size(), stFind.vecCouple.size(), stFind.vecSingle.size());

		for (int m = 1; m < SK_LAYOUT_NUM; ++m)
		{
			sprintf(szOutput, "%s %d", szOutput, nCardLays[m]);
		}
		LOG_INFO("%s", szOutput);*/

		//LOG_INFO("Finish 2");
	}
	else
	{
		//LOG_INFO("Finish 3");
	}
}

void CGameTable::CalcFindCountAndValue(ADJUST_CARDS &stFind)
{
	//DWORD dwTicket0 = GetTickCount();
	int nCardValue[SK_LAYOUT_NUM - 1] = {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1};
	int nTypeValue[8] = { 1, 1, 1, 1, 1, 1, 1, 1 };
	int nCountValue[20] = { 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1 };
	int i = 0;

	if (!CConfigManagerSys::m_jsoncfgobjmgr[MAKEDEAL_CONFIG]["NoShuffCardValue"].isNull())
	{
		TCHAR szKey[32];
		for (i = 0; i < SK_LAYOUT_NUM - 1; i++)
		{
			if (i >= 0 && i <= 8)        //2倒10对应的值
			{
				sprintf(szKey, "%d", i + 2);
			}
			else if (i == 9)
			{
				sprintf(szKey, "J");
			}
			else if (i == 10)
			{
				sprintf(szKey, "Q");
			}
			else if (i == 11)
			{
				sprintf(szKey, "K");
			}
			else if (i == 12)
			{
				sprintf(szKey, "A");
			}
			else if (i == 13)
			{
				sprintf(szKey, "Joker Small");
			}
			else if (i == 14)
			{
				sprintf(szKey, "Joker Big");
			}

			if (!CConfigManagerSys::m_jsoncfgobjmgr[MAKEDEAL_CONFIG]["NoShuffCardValue"][szKey].isNull())
			{
				nCardValue[i] = CConfigManagerSys::m_jsoncfgobjmgr[MAKEDEAL_CONFIG]["NoShuffCardValue"][szKey].asInt();
			}
		}

		for (i = 0; i < 8; i++)
		{
			if (i == 0)
			{
				sprintf(szKey, "2K");
			}
			else if (i == 1)
			{
				sprintf(szKey, "Bomb");
			}
			else if (i == 2)
			{
				sprintf(szKey, "Abt_Three");
			}
			else if (i == 3)
			{
				sprintf(szKey, "Three");
			}
			else if (i == 4)
			{
				sprintf(szKey, "Abt_Couple");
			}
			else if (i == 5)
			{
				sprintf(szKey, "Straight");
			}
			else if (i == 6)
			{
				sprintf(szKey, "Couple");
			}
			else if (i == 7)
			{
				sprintf(szKey, "Single");
			}

			if (!CConfigManagerSys::m_jsoncfgobjmgr[MAKEDEAL_CONFIG]["NoShuffCardType"][szKey].isNull())
			{
				nTypeValue[i] = CConfigManagerSys::m_jsoncfgobjmgr[MAKEDEAL_CONFIG]["NoShuffCardType"][szKey].asInt();
			}
		}

		for (i = 0; i < 20; i++)
		{
			sprintf(szKey, "%d", i + 1);

			if (!CConfigManagerSys::m_jsoncfgobjmgr[MAKEDEAL_CONFIG]["NoShuffCountValue"][szKey].isNull())
			{
				nCountValue[i] = CConfigManagerSys::m_jsoncfgobjmgr[MAKEDEAL_CONFIG]["NoShuffCountValue"][szKey].asInt();
			}
		}
	}

	stFind.nHandCountValue = stFind.nHandCount;

	stFind.nCardValue = 0;

	//TCHAR szOutput[1024];

	//sprintf(szOutput, "Bomb:");
	std::vector<int>::iterator it = stFind.vecBomb.begin();
	for (; it != stFind.vecBomb.end(); it++)
	{
		int nCardIndex = *it;
		//sprintf(szOutput, "%s %d", szOutput, nCardIndex);
		if (nCardIndex == 14)
		{
			stFind.nCardValue += nCardValue[nCardIndex - 1] + nTypeValue[0] + nCountValue[2 - 1];
			//LOG_INFO("cardValue: %d, typeValue: %d, countValue: %d, total: %d", nCardValue[nCardIndex - 1], nTypeValue[0], nCountValue[2 - 1], stFind.nCardValue);
		}
		else
		{
			stFind.nCardValue += nCardValue[nCardIndex - 1] + nTypeValue[1] + nCountValue[4 - 1];
			//LOG_INFO("cardValue: %d, typeValue: %d, countValue: %d, total: %d", nCardValue[nCardIndex - 1], nTypeValue[1], nCountValue[4 - 1], stFind.nCardValue);
		}
		
	}
	//LOG_INFO("%s", szOutput);

	//sprintf(szOutput, "AbtTree:");
	it = stFind.vecAbt_Three.begin();
	for (; it != stFind.vecAbt_Three.end(); it++)
	{
		int nCardIndex = *it;
		//sprintf(szOutput, "%s %d", szOutput, nCardIndex);
		stFind.nCardValue += nCardValue[nCardIndex - 1] + nTypeValue[2] + nCountValue[6 - 1];
		//LOG_INFO("cardValue: %d, typeValue: %d, countValue: %d, total: %d", nCardValue[nCardIndex - 1], nTypeValue[2], nCountValue[6 - 1], stFind.nCardValue);
	}
	//LOG_INFO("%s", szOutput);

	//sprintf(szOutput, "Tree:");
	it = stFind.vecThree.begin();
	for (; it != stFind.vecThree.end(); it++)
	{
		int nCardIndex = *it;
		//sprintf(szOutput, "%s %d", szOutput, nCardIndex);
		stFind.nCardValue += nCardValue[nCardIndex - 1] + nTypeValue[3] + nCountValue[3 - 1];
		//LOG_INFO("cardValue: %d, typeValue: %d, countValue: %d, total: %d", nCardValue[nCardIndex - 1], nTypeValue[3], nCountValue[3 - 1], stFind.nCardValue);
	}
	//LOG_INFO("%s", szOutput);

	//sprintf(szOutput, "AbtCouple:");
	it = stFind.vecAbt_Couple.begin();
	for (; it != stFind.vecAbt_Couple.end(); it++)
	{
		int nCardIndex = *it;
		//sprintf(szOutput, "%s %d", szOutput, nCardIndex);
		stFind.nCardValue += nCardValue[nCardIndex - 1] + nTypeValue[4] + nCountValue[6 - 1];
		//LOG_INFO("cardValue: %d, typeValue: %d, countValue: %d, total: %d", nCardValue[nCardIndex - 1], nTypeValue[4], nCountValue[6 - 1], stFind.nCardValue);
	}
	//LOG_INFO("%s", szOutput);

	//sprintf(szOutput, "Straight:");
	it = stFind.vecStraight.begin();
	for (; it != stFind.vecStraight.end(); it++)
	{
		int nCardIndex = *it;
		//sprintf(szOutput, "%s %d", szOutput, nCardIndex);
		stFind.nCardValue += nCardValue[nCardIndex - 1] + nTypeValue[5] + nCountValue[5 - 1];
		//LOG_INFO("cardValue: %d, typeValue: %d, countValue: %d, total: %d", nCardValue[nCardIndex - 1], nTypeValue[5], nCountValue[5 - 1], stFind.nCardValue);
	}
	//LOG_INFO("%s", szOutput);

	//sprintf(szOutput, "Couple:");
	it = stFind.vecCouple.begin();
	for (; it != stFind.vecCouple.end(); it++)
	{
		int nCardIndex = *it;
		//sprintf(szOutput, "%s %d", szOutput, nCardIndex);
		stFind.nCardValue += nCardValue[nCardIndex - 1] + nTypeValue[6] + nCountValue[2 - 1];
		//LOG_INFO("cardValue: %d, typeValue: %d, countValue: %d, total: %d", nCardValue[nCardIndex - 1], nTypeValue[6], nCountValue[2 - 1], stFind.nCardValue);
	}
	//LOG_INFO("%s", szOutput);

	//sprintf(szOutput, "Single:");
	it = stFind.vecSingle.begin();
	for (; it != stFind.vecSingle.end(); it++)
	{
		int nCardIndex = *it;
		//sprintf(szOutput, "%s %d", szOutput, nCardIndex);
		stFind.nCardValue += nCardValue[nCardIndex - 1] + nTypeValue[7] + nCountValue[1 - 1];
		//LOG_INFO("cardValue: %d, typeValue: %d, countValue: %d, total: %d", nCardValue[nCardIndex - 1], nTypeValue[7], nCountValue[1 - 1], stFind.nCardValue);
	}
	//LOG_INFO("%s", szOutput);

	//DWORD dwTicket1 = GetTickCount();
	//LOG_INFO("SecondRound BestFind Cost Time %d", dwTicket1 - dwTicket0);
}

void CGameTable::SimulateStartDeal(CGameServer* pGameServer)
{
	CString strIniFile = GetINIFileName();
	TCHAR szRoomID[16];
	memset(szRoomID, 0, sizeof(szRoomID));
	_stprintf(szRoomID, _T("%ld"), m_nRoomID);

	BOOL bMakeDeal = FALSE;

	if (CConfigManagerSys::m_jsoncfgobjmgr.find(MAKEDEAL_CONFIG) == CConfigManagerSys::m_jsoncfgobjmgr.end())
	{
		bMakeDeal = FALSE;
	}
	else
	{
		bMakeDeal = !CConfigManagerSys::m_jsoncfgobjmgr[MAKEDEAL_CONFIG]["MakeDeal"][szRoomID].empty();
	}

	{
		m_nBanker = CalcBankerChairBefore(); // 决定庄家

		int card[TOTAL_CARDS];
		for(int i=0;i<TOTAL_CARDS;i++)
			card[i]=i;

		CPlayer* ptrP = m_ptrPlayers[0];
		SvrXygRandomSort(card,TOTAL_CARDS,GetTickCount());

		{
			for (int i = 0; i < TOTAL_CARDS-BOTTOM_CARD; i++)
			{
				m_nCardsLayIn[i%3][SK_GetCardIndexEx(card[i])] ++;
			}
		}

		UwlLogFile(_T("原始牌："));
		CompareAndLogCardLays(NULL, m_nCardsLayIn, TOTAL_CHAIRS);
		
		if (TRUE == bMakeDeal)//根据配置做牌
			MakeDealByCfg(card, TOTAL_CARDS);
		
		{
			for (int i = 0; i < TOTAL_CARDS-BOTTOM_CARD; i++)
			{
				m_nCardsLayIn[i%3][SK_GetCardIndexEx(card[i])] ++;
			}
		}

		ZeroMemory(m_nCardsLayIn, sizeof(int)*SK_LAYOUT_NUM*TOTAL_CHAIRS);
	}
}

void CGameTable::MakeDealByCfg(int cards[], int length)
{
	MAKEDEALCFG dealCfg;
	ZeroMemory(&dealCfg, sizeof(MAKEDEALCFG));
	GetMakeDealCfg(&dealCfg);//获取做牌配置

	if (0 == dealCfg.nMakeDealType)
	{
		if (0 < dealCfg.nBeginMakeNum && dealCfg.nBeginMakeNum < CARDS_PER_CHAIR
			&& -1 != dealCfg.nFirstChairHandCount && -1 != dealCfg.nFirstChairBombCount && -1 != dealCfg.nFirstChairBigCardsCount
			&& -1 != dealCfg.nOtherChairHandCount && -1 != dealCfg.nOtherChairBombCount && -1 != dealCfg.nOtherChairBigCardsCount)
		{
			int nChairCards[TOTAL_CHAIRS][CARDS_PER_CHAIR];
			int *pReserveCards = new int[(CARDS_PER_CHAIR - dealCfg.nBeginMakeNum) * 3];

			if (NULL == pReserveCards)
				return;

			XygInitChairCards(nChairCards[0], CARDS_PER_CHAIR);
			XygInitChairCards(nChairCards[1], CARDS_PER_CHAIR);
			XygInitChairCards(nChairCards[2], CARDS_PER_CHAIR);
			XygInitChairCards(pReserveCards, (CARDS_PER_CHAIR - dealCfg.nBeginMakeNum) * 3);

			//复制前面正常的牌
			{
				for (int i = 0; i < dealCfg.nBeginMakeNum; i++)
				{
					nChairCards[0][i] = cards[i * 3];
					nChairCards[1][i] = cards[i * 3 + 1];
					nChairCards[2][i] = cards[i * 3 + 2];
				}
			}

			//复制后面可能需要变动的牌
			memcpy(pReserveCards, &cards[dealCfg.nBeginMakeNum * 3], sizeof(int)* (CARDS_PER_CHAIR - dealCfg.nBeginMakeNum) * 3);

			int nHandCount[TOTAL_CHAIRS];
			int nBombCount[TOTAL_CHAIRS];
			int nBigCardsCount[TOTAL_CHAIRS];
			int nCardLays[TOTAL_CHAIRS][SK_LAYOUT_NUM];
			ZeroMemory(nHandCount, sizeof(int)*TOTAL_CHAIRS);
			ZeroMemory(nBombCount, sizeof(int)*TOTAL_CHAIRS);
			ZeroMemory(nBigCardsCount, sizeof(int)*TOTAL_CHAIRS);
			ZeroMemory(nCardLays, sizeof(nCardLays));

			//计算出牌手数，统计炸弹、王和2的数量
			CalcHandCardsCount(nChairCards[0], dealCfg.nBeginMakeNum, nCardLays[0], nHandCount[0], nBombCount[0], nBigCardsCount[0]);
			CalcHandCardsCount(nChairCards[1], dealCfg.nBeginMakeNum, nCardLays[1], nHandCount[1], nBombCount[1], nBigCardsCount[1]);
			CalcHandCardsCount(nChairCards[2], dealCfg.nBeginMakeNum, nCardLays[2], nHandCount[2], nBombCount[2], nBigCardsCount[2]);

#ifdef _MAKEDEALINFO
			UwlLogFile(_T("统计牌型信息："));
			UwlLogFile(_T("FirstChair:%d nHandCount:%d, nBombCount:%d, nBigCardsCount:%d"), 0 == m_nBanker, nHandCount[0], nBombCount[0], nBigCardsCount[0]);
			UwlLogFile(_T("FirstChair:%d nHandCount:%d, nBombCount:%d, nBigCardsCount:%d"), 1 == m_nBanker, nHandCount[1], nBombCount[1], nBigCardsCount[1]);
			UwlLogFile(_T("FirstChair:%d nHandCount:%d, nBombCount:%d, nBigCardsCount:%d"), 2 == m_nBanker, nHandCount[2], nBombCount[2], nBigCardsCount[2]);

			{
				int *pReserveCardLay = new int[SK_LAYOUT_NUM];
				ZeroMemory(pReserveCardLay, sizeof(int)*SK_LAYOUT_NUM);
				for (int i = 0; i < (CARDS_PER_CHAIR - dealCfg.nBeginMakeNum) * 3; i++)
				{
					pReserveCardLay[SK_GetCardIndexEx(pReserveCards[i])]++;
				}

				UwlLogFile(_T("先打印剩余的%d张牌，不包括底牌："), (CARDS_PER_CHAIR - dealCfg.nBeginMakeNum) * 3);
				CompareAndLogCardLay(NULL, pReserveCardLay);

				delete[] pReserveCardLay;
			}

			UwlLogFile(_T("再打印随机分发的前%d张牌："), dealCfg.nBeginMakeNum);
			CompareAndLogCardLays(m_nCardsLayIn, nCardLays, TOTAL_CHAIRS);
#endif

			if (FALSE == DoMakeDeal(nChairCards, nCardLays, pReserveCards, (CARDS_PER_CHAIR - dealCfg.nBeginMakeNum) * 3, &dealCfg,
				nHandCount, nBombCount, nBigCardsCount))
			{
				return;
			}

#ifdef _MAKEDEALINFO
			UwlLogFile(_T("将剩余的牌分配给玩家，得到最终的牌"), dealCfg.nBeginMakeNum);
			CompareAndLogCardLays(m_nCardsLayIn, nCardLays, TOTAL_CHAIRS);
#endif

			{
				//将变动之后的牌赋值到原牌值数组
				for (int i = dealCfg.nBeginMakeNum; i < CARDS_PER_CHAIR; i++)
				{
					cards[i * 3] = nChairCards[0][i];
					cards[i * 3 + 1] = nChairCards[1][i];
					cards[i * 3 + 2] = nChairCards[2][i];
				}
			}

			//释放掉内存
			delete[] pReserveCards;
		}
	}
	else if (1 == dealCfg.nMakeDealType){
		//如果是满足用户做牌条件或者是机器人，采用用户做牌策略或机器人策略，不采用房间通用策略
		MAKEDEALCFG userdealCfg[3] = { 0 };
		for (int i = 0; i < 3; i++)
		{
			if (m_ptrPlayers[i]->IsRoboter())
			{
				ZeroMemory(&userdealCfg[i], sizeof(MAKEDEALCFG));
				GetMakeDealCfg(&userdealCfg[i], "robot");//获取做牌配置
			}
			else if (this->IsNeedMakeDealByUserBoutInfo(m_ptrPlayers[i]))
			{
				ZeroMemory(&userdealCfg[i], sizeof(MAKEDEALCFG));
				GetMakeDealCfg(&userdealCfg[i], "newuser");//获取做牌配置
			}
			else
			{
				userdealCfg[i] = dealCfg;
			}
			if (0 > userdealCfg[i].nBeginMakeNum || userdealCfg[i].nBeginMakeNum > CARDS_PER_CHAIR ||
				0 > userdealCfg[i].nBeginSelectBanker || userdealCfg[i].nBeginSelectBanker > CARDS_PER_CHAIR)
			{
				return;
			}
		}	
		
		std::vector<int> arrHandCardList[3];
		std::vector<int> arrReserveCards(&cards[0], &cards[length]);   //剩余未发的牌
		int nBottomCard[BOTTOM_CARD];
		for (int p = 0; p < CARDS_PER_CHAIR + 1; ++p)
		{
			for (int k = 0; k < 3; ++k)
			{
				if (p < userdealCfg[k].nBeginMakeNum)
				{
					//随机发牌
					arrHandCardList[k].push_back(*arrReserveCards.begin());
					arrReserveCards.erase(arrReserveCards.begin());
				}
				else if (p < userdealCfg[k].nBeginSelectBanker)
				{
					//根据组牌策略组牌
					//拆牌
					std::vector<CardGroupData> AllCardTypeArr;
					SpliteCard(arrHandCardList[k], AllCardTypeArr);
					int nHandCount = 0;
					int nHandCardValue = 0;
					CalHandCardValue(AllCardTypeArr, nHandCount, nHandCardValue);
					ComposeCardResult tmpRet = MakeDeal_ComposeCard(&userdealCfg[k], AllCardTypeArr, arrReserveCards, nHandCount> userdealCfg[k].nTargetRound || nHandCardValue < userdealCfg[k].nTargetValue);
					arrHandCardList[k].push_back(tmpRet.nRemoveCardID);

					m_nMakeDealTypes[k] = 3;
				}
				else if (p == userdealCfg[k].nBeginSelectBanker)
				{
					//选取地主牌
					//随机三张底牌
					int nLen = arrReserveCards.size();
					nBottomCard[k] = arrReserveCards[nLen - 1];
					arrReserveCards.erase(arrReserveCards.begin() + nLen - 1);
				}
				else{
					//随机发牌
					//剩余随机组牌
					int nLen = arrReserveCards.size();
					arrHandCardList[k].push_back(arrReserveCards[nLen - 1]);
					arrReserveCards.erase(arrReserveCards.begin() + nLen - 1);
				}
			}
		}

        if (arrHandCardList[0].size() != CARDS_PER_CHAIR
            ||arrHandCardList[1].size() != CARDS_PER_CHAIR
            ||arrHandCardList[2].size() != CARDS_PER_CHAIR)
        {
            return;
        }
        //做牌完成，处理做牌结果
        for (int i = 0; i < CARDS_PER_CHAIR; i++){
            cards[i * 3] = arrHandCardList[0][i];
            cards[i * 3 + 1] = arrHandCardList[1][i];
            cards[i * 3 + 2] = arrHandCardList[2][i];
        }
        cards[CARDS_PER_CHAIR * 3] = nBottomCard[0];
        cards[CARDS_PER_CHAIR * 3+1] = nBottomCard[1];
        cards[CARDS_PER_CHAIR * 3+2] = nBottomCard[2];
	}
}

//根据获取用户做牌规则判断是否需要做牌
bool CGameTable::IsNeedMakeDealByUserBoutInfo(CPlayer* pPlayer)
{
	if (CConfigManagerSys::m_jsoncfgobjmgr[MAKEDEAL_CONFIG]["PlayerMakeDealRule"]["IsEnable"].asInt() == 0)
	{
		return false;
	}

	TCHAR szRoomID[16];
	memset(szRoomID, 0, sizeof(szRoomID));
	_stprintf(szRoomID, _T("%ld"), m_nRoomID);
	//如果房间没有指定用户做牌规则，则不给新用户做牌
	if (!CConfigManagerSys::m_jsoncfgobjmgr[MAKEDEAL_CONFIG]["PlayerMakeDealRule"]["RoomRule"][szRoomID].isNull())
	{
		int nTargetNewUserBout = CConfigManagerSys::m_jsoncfgobjmgr[MAKEDEAL_CONFIG]["PlayerMakeDealRule"]["RoomRule"][szRoomID]["NewUserBout"].asInt();
		int nNewUserBout = pPlayer->m_nBout;
		if (nTargetNewUserBout >= nNewUserBout)
		{
			return true;
		}
		else
		{
			return false;
		}
	}
	else
	{
		return false;
	}
	
	return false;
}

//获取做牌配置
void CGameTable::GetMakeDealCfg(LPMAKEDEALCFG pCfg, std::string MakeDealStrategy)
{
	if (NULL == pCfg)
		return;
	TCHAR szRoomID[16];
	memset(szRoomID, 0, sizeof(szRoomID));
	_stprintf(szRoomID, _T("%ld"), m_nRoomID);
	CString strMakeDealStrategy = "default";
	if (MakeDealStrategy != "")
	{
		//针对指定做牌策略，优先查找有没有针对房间的做牌策略，如果没有，用相应通用做牌策略
		strMakeDealStrategy = (MakeDealStrategy + szRoomID).c_str();
		if (CConfigManagerSys::m_jsoncfgobjmgr[MAKEDEAL_CONFIG]["MakeDealStrategy"][strMakeDealStrategy].isNull())
		{
			//如果房间没有配置策略，采用机器人默认策略
			strMakeDealStrategy = MakeDealStrategy.c_str();
		}
	}
	else
	{
		//根据房间号获取做牌策略
		if (!CConfigManagerSys::m_jsoncfgobjmgr[MAKEDEAL_CONFIG]["MakeDeal"][szRoomID].isNull())
		{
			strMakeDealStrategy = CConfigManagerSys::m_jsoncfgobjmgr[MAKEDEAL_CONFIG]["MakeDeal"][szRoomID].asCString();
			if (!CConfigManagerSys::m_jsoncfgobjmgr[MAKEDEAL_CONFIG]["MakeDealStrategy"][strMakeDealStrategy + szRoomID].isNull())
			{
				//如果配置了指定房间的指定策略
				strMakeDealStrategy = strMakeDealStrategy + szRoomID;
			}
		}

		if (CConfigManagerSys::m_jsoncfgobjmgr[MAKEDEAL_CONFIG]["MakeDealStrategy"][strMakeDealStrategy].isNull())
		{
			//如果没有房间没有配置策略，采用default默认策略
			strMakeDealStrategy = "default";
		}
	}

	//特殊房间做牌配置,具体指定了做牌策略
	pCfg->nMakeDealType = CConfigManagerSys::m_jsoncfgobjmgr[MAKEDEAL_CONFIG]["MakeDealStrategy"][strMakeDealStrategy]["MakeDealType"].asInt();
	pCfg->nBeginMakeNum = CConfigManagerSys::m_jsoncfgobjmgr[MAKEDEAL_CONFIG]["MakeDealStrategy"][strMakeDealStrategy]["BeginMakeNum"].asInt();
	pCfg->nBeginSelectBanker = CConfigManagerSys::m_jsoncfgobjmgr[MAKEDEAL_CONFIG]["MakeDealStrategy"][strMakeDealStrategy]["BeginSelectBanker"].asInt();
	pCfg->nFirstChairHandCount = CConfigManagerSys::m_jsoncfgobjmgr[MAKEDEAL_CONFIG]["MakeDealStrategy"][strMakeDealStrategy]["FirstChairHandCount"].asInt();
	pCfg->nFirstChairBombCount = CConfigManagerSys::m_jsoncfgobjmgr[MAKEDEAL_CONFIG]["MakeDealStrategy"][strMakeDealStrategy]["FirstChairBombCount"].asInt();
	pCfg->nFirstChairBigCardsCount = CConfigManagerSys::m_jsoncfgobjmgr[MAKEDEAL_CONFIG]["MakeDealStrategy"][strMakeDealStrategy]["FirstChairBigCardsCount"].asInt();
	pCfg->nOtherChairHandCount = CConfigManagerSys::m_jsoncfgobjmgr[MAKEDEAL_CONFIG]["MakeDealStrategy"][strMakeDealStrategy]["OtherChairHandCount"].asInt();
	pCfg->nOtherChairBombCount = CConfigManagerSys::m_jsoncfgobjmgr[MAKEDEAL_CONFIG]["MakeDealStrategy"][strMakeDealStrategy]["OtherChairBombCount"].asInt();
	pCfg->nOtherChairBigCardsCount = CConfigManagerSys::m_jsoncfgobjmgr[MAKEDEAL_CONFIG]["MakeDealStrategy"][strMakeDealStrategy]["OtherChairBigCardsCount"].asInt();
	pCfg->nReserved[0] = CConfigManagerSys::m_jsoncfgobjmgr[MAKEDEAL_CONFIG]["MakeDealStrategy"][strMakeDealStrategy]["BigCardsTo"].asInt();
	float fTargetValue = CConfigManagerSys::m_jsoncfgobjmgr[MAKEDEAL_CONFIG]["MakeDealStrategy"][strMakeDealStrategy]["TargetValue"].asFloat();
	if (fTargetValue >= 0)
	{
		pCfg->nTargetValue = (int)(fTargetValue * CConfigManagerSys::m_jsoncfgobjmgr[MAKEDEAL_CONFIG]["MakeDealCommonArgs"]["MaxCardsValue"].asInt());       //手牌的总值最大106
	}
	else {
		pCfg->nTargetValue = -(int)(fTargetValue * CConfigManagerSys::m_jsoncfgobjmgr[MAKEDEAL_CONFIG]["MakeDealCommonArgs"]["MinCardsValue"].asInt());      //手牌的总值小-25
	}

	pCfg->nTargetRound = CConfigManagerSys::m_jsoncfgobjmgr[MAKEDEAL_CONFIG]["MakeDealStrategy"][strMakeDealStrategy]["TargetRound"].asInt();

	//获取一个随机凑牌策略
	srand(time(NULL));
	int nCouPaiStrategyCount = CConfigManagerSys::m_jsoncfgobjmgr[MAKEDEAL_CONFIG]["MakeDealStrategy"][strMakeDealStrategy]["CouPaiStrategy"].size();
	int nCouPaiStrategySelectIndex = rand() % nCouPaiStrategyCount;
	for (int i = 0; i < CConfigManagerSys::m_jsoncfgobjmgr[MAKEDEAL_CONFIG]["MakeDealStrategy"][strMakeDealStrategy]["CouPaiStrategy"][nCouPaiStrategySelectIndex].size(); i++)
	{
		pCfg->arrCouPaiStrategy.push_back(CConfigManagerSys::m_jsoncfgobjmgr[MAKEDEAL_CONFIG]["MakeDealStrategy"][strMakeDealStrategy]["CouPaiStrategy"][nCouPaiStrategySelectIndex][i].asInt());
	}
}

//根据配置调整相应玩家的手牌
BOOL CGameTable::DoMakeDeal(int (*pChairCards)[CARDS_PER_CHAIR], int (*pCardLays)[SK_LAYOUT_NUM], int nReserveCards[], int nReserveCount, 
							LPMAKEDEALCFG pCfg, int nHandCount[], int nBombCount[], int nBigCardsCount[])
{
	{
		CString strIniFile = GetINIFileName();
		TCHAR szRoomID[16];
		memset(szRoomID, 0, sizeof(szRoomID));
		_stprintf(szRoomID, _T("%ld"), m_nRoomID);

		BOOL bRobotNeedMakeDeal = GetPrivateProfileInt(
			_T("RobotNeedMakeDeal"),//机器人是否要做牌
			szRoomID,
			TRUE,
		strIniFile);

		for (int i = 0; i < CARDS_PER_CHAIR-pCfg->nBeginMakeNum; i++)
		{
			//首家做牌
			if (m_ptrPlayers[m_nBanker]->m_nUserType != USER_TYPE_ROBOT
				|| (m_ptrPlayers[m_nBanker]->m_nUserType == USER_TYPE_ROBOT && TRUE == bRobotNeedMakeDeal))
			{
				if (0 == i && nHandCount[m_nBanker] > pCfg->nFirstChairHandCount 
					&& nBombCount[m_nBanker] < pCfg->nFirstChairBombCount 
					&& nBigCardsCount[m_nBanker] < pCfg->nFirstChairBigCardsCount)
				{//第一轮做牌需要判断做牌条件
					MatchFirstChairCards(pChairCards[m_nBanker], pCardLays[m_nBanker], nReserveCards, nReserveCount, pCfg);
				}
				else if (i > 0)
				{//第二轮以上做牌不需要判断做牌条件
					MatchFirstChairCards(pChairCards[m_nBanker], pCardLays[m_nBanker], nReserveCards, nReserveCount, pCfg);
				}
			}
			
			//第二玩家做牌
			int nNextChairNo = GetNextChair(m_nBanker);
			if (m_ptrPlayers[nNextChairNo]->m_nUserType != USER_TYPE_ROBOT
				|| (m_ptrPlayers[nNextChairNo]->m_nUserType == USER_TYPE_ROBOT && TRUE == bRobotNeedMakeDeal))
			{
				if (0 == i && nHandCount[nNextChairNo] > pCfg->nOtherChairHandCount 
					&& nBombCount[nNextChairNo] < pCfg->nOtherChairBombCount 
					&& nBigCardsCount[nNextChairNo] < pCfg->nOtherChairBigCardsCount)
				{//第一轮做牌需要判断做牌条件
					MatchOtherChairCards(pChairCards[nNextChairNo], pCardLays[nNextChairNo], nReserveCards, nReserveCount, pCfg, 1);
				}
				else if (i > 0)
				{//第二轮及以上做牌不需要判断做牌条件
					MatchOtherChairCards(pChairCards[nNextChairNo], pCardLays[nNextChairNo], nReserveCards, nReserveCount, pCfg, 1);
				}
			}

			//第三玩家做牌
			int nNextNextChairNo = GetNextChair(nNextChairNo);
			if (m_ptrPlayers[nNextNextChairNo]->m_nUserType != USER_TYPE_ROBOT
				|| (m_ptrPlayers[nNextNextChairNo]->m_nUserType == USER_TYPE_ROBOT && TRUE == bRobotNeedMakeDeal))
			{
				if (0 == i && nHandCount[nNextNextChairNo] > pCfg->nOtherChairHandCount 
					&& nBombCount[nNextNextChairNo] < pCfg->nOtherChairBombCount 
					&& nBigCardsCount[nNextNextChairNo] < pCfg->nOtherChairBigCardsCount)
				{//第一轮做牌需要判断做牌条件
					MatchOtherChairCards(pChairCards[nNextNextChairNo], pCardLays[nNextNextChairNo], nReserveCards, nReserveCount, pCfg, 2);
				}
				else if (i > 0)
				{//第二轮及以上做牌不需要判断做牌条件
					MatchOtherChairCards(pChairCards[nNextNextChairNo], pCardLays[nNextNextChairNo], nReserveCards, nReserveCount, pCfg, 2);
				}
			}
		}
	}

#ifdef _MAKEDEALINFO
	UwlLogFile(_T("根据出牌手数、炸弹和大牌(王和2)做牌："));
	CompareAndLogCardLays(m_nCardsLayIn, pCardLays, TOTAL_CHAIRS);
#endif
		
	{//将剩余的牌按顺序分配给对应玩家
		for (int i = pCfg->nBeginMakeNum; i < CARDS_PER_CHAIR; i++)
		{
			if (-1 == pChairCards[0][i])
			{
				pChairCards[0][i] = GetOneReservedCard(nReserveCards, (CARDS_PER_CHAIR-pCfg->nBeginMakeNum)*3);
				pCardLays[0][SK_GetCardIndexEx(pChairCards[0][i])]++;
			}
			
			if (-1 == pChairCards[1][i])
			{
				pChairCards[1][i] = GetOneReservedCard(nReserveCards, (CARDS_PER_CHAIR-pCfg->nBeginMakeNum)*3);
				pCardLays[1][SK_GetCardIndexEx(pChairCards[1][i])]++;
			}

			if (-1 == pChairCards[2][i])
			{
				pChairCards[2][i] = GetOneReservedCard(nReserveCards, (CARDS_PER_CHAIR-pCfg->nBeginMakeNum)*3);
				pCardLays[2][SK_GetCardIndexEx(pChairCards[2][i])]++;
			}

			if (-1 == pChairCards[0][i] || -1 == pChairCards[1][i] || -1 == pChairCards[2][i])
				return FALSE;
		}
	}

	return TRUE;
}

//匹配首家手牌
void CGameTable::MatchFirstChairCards(int nChairCard[], int nCardLay[], int nReserveCards[], int nReserveCount, LPMAKEDEALCFG pCfg)
{
	for (int i = 0; i < CARDS_PER_CHAIR; i++)
	{
		if (-1 != nChairCard[i])
			continue;

		int nMatchedCardID = -1;
		nMatchedCardID = Match2OrKingCardType(nCardLay, nReserveCards, nReserveCount, pCfg);//匹配2或王
		if (TRUE == CopyMatchedCardID(nChairCard[i], nCardLay, nMatchedCardID, nReserveCards, nReserveCount)) break;

		nMatchedCardID = MatchBombCardType(nCardLay, nReserveCards, nReserveCount);//匹配炸弹
		if (TRUE == CopyMatchedCardID(nChairCard[i], nCardLay, nMatchedCardID, nReserveCards, nReserveCount)) break;

		nMatchedCardID = MatchThreeCardType(nCardLay, nReserveCards, nReserveCount);//匹配三张
		if (TRUE == CopyMatchedCardID(nChairCard[i], nCardLay, nMatchedCardID, nReserveCards, nReserveCount)) break;

		nMatchedCardID = MatchABTCardType(nCardLay, nReserveCards, nReserveCount);//匹配顺子
		if (TRUE == CopyMatchedCardID(nChairCard[i], nCardLay, nMatchedCardID, nReserveCards, nReserveCount)) break;

		nMatchedCardID = MatchABTCoupleCardType(nCardLay, nReserveCards, nReserveCount);//匹配连对
		if (TRUE == CopyMatchedCardID(nChairCard[i], nCardLay, nMatchedCardID, nReserveCards, nReserveCount)) break;
		
		nMatchedCardID = MatchCoupleCardType(nCardLay, nReserveCards, nReserveCount);//匹配对子
		if (TRUE == CopyMatchedCardID(nChairCard[i], nCardLay, nMatchedCardID, nReserveCards, nReserveCount)) break;
		
		return;//只匹配一轮，无论是否匹配上
	}

	m_nMakeDealTypes[0] = 2;
}

//匹配第二或第三玩家手牌
void CGameTable::MatchOtherChairCards(int nChairCard[], int nCardLay[], int nReserveCards[], int nReserveCount, LPMAKEDEALCFG pCfg, int nChairNO)
{
	for (int i = 0; i < CARDS_PER_CHAIR; i++)
	{
		if (-1 != nChairCard[i])
			continue;

		int nMatchedCardID = -1;
		nMatchedCardID = Match2OrKingCardType(nCardLay, nReserveCards, nReserveCount, pCfg);//匹配2或王
		if (TRUE == CopyMatchedCardID(nChairCard[i], nCardLay, nMatchedCardID, nReserveCards, nReserveCount)) break;

		nMatchedCardID = MatchBombCardType(nCardLay, nReserveCards, nReserveCount);//匹配炸弹
		if (TRUE == CopyMatchedCardID(nChairCard[i], nCardLay, nMatchedCardID, nReserveCards, nReserveCount)) break;

		nMatchedCardID = MatchThreeCardType(nCardLay, nReserveCards, nReserveCount);//匹配三张
		if (TRUE == CopyMatchedCardID(nChairCard[i], nCardLay, nMatchedCardID, nReserveCards, nReserveCount)) break;

		nMatchedCardID = MatchABTCardType(nCardLay, nReserveCards, nReserveCount);//匹配顺子
		if (TRUE == CopyMatchedCardID(nChairCard[i], nCardLay, nMatchedCardID, nReserveCards, nReserveCount)) break;
		
		nMatchedCardID = MatchABTCoupleCardType(nCardLay, nReserveCards, nReserveCount);//匹配连对
		if (TRUE == CopyMatchedCardID(nChairCard[i], nCardLay, nMatchedCardID, nReserveCards, nReserveCount)) break;
		
		nMatchedCardID = MatchCoupleCardType(nCardLay, nReserveCards, nReserveCount);//匹配对子
		if (TRUE == CopyMatchedCardID(nChairCard[i], nCardLay, nMatchedCardID, nReserveCards, nReserveCount)) break;
		
		return;//只匹配一轮，无论是否匹配上
	}

	if (nChairNO != -1) {
		m_nMakeDealTypes[nChairNO] = 2;
	}
}

//将匹配到的CardID分配给玩家，并从剩余牌中去除匹配到的CardID
BOOL CGameTable::CopyMatchedCardID(int &nPreCardID, int nCardLay[], int nMatchedCardID, int nReserveCards[], int nReserveCount)
{
	if (-1 == nMatchedCardID)
		return FALSE;

	for (int i = 0; i < nReserveCount; i++)
	{
		if (nReserveCards[i] == nMatchedCardID)
		{
			nReserveCards[i] = -1;
			nPreCardID = nMatchedCardID;
			nCardLay[SK_GetCardIndexEx(nMatchedCardID, 0)]++;
			return TRUE;
		}
	}

	return FALSE;
}

void CGameTable::CompareAndLogCardLay(int *pOriginalCardLay, int *pModifiedCardLay)
{
	UwlLogFile(_T("    B  S  2  A  K  Q  J  10 9  8  7  6  5  4  3"));
	int nOrder[SK_LAYOUT_NUM] = {0, 15, 14, 1, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2};
	if (NULL == pOriginalCardLay && NULL != pModifiedCardLay)
	{
		string strLog = "    ";
		for (int j = 1; j < SK_LAYOUT_NUM; j++)
		{
			char buffer[5];
			_itoa(pModifiedCardLay[nOrder[j]], buffer, 10);
			strLog += buffer[0];
			strLog += "  ";
		}
		UwlLogFile(_T("%s"), strLog.c_str());
	}
	else if (NULL != pOriginalCardLay && NULL != pModifiedCardLay)
	{
		string strLog = "";
		for (int j = 1; j < SK_LAYOUT_NUM; j++)
		{
			char buffer[5];
			_itoa(pModifiedCardLay[nOrder[j]], buffer, 10);
			if (pOriginalCardLay[nOrder[j]] != pModifiedCardLay[nOrder[j]])
			{
				strLog += "@";
				strLog += buffer[0];
				strLog += " ";
			}
			else
			{
				strLog += buffer[0];
				strLog += "  ";
			}
		}
		UwlLogFile(_T("%s"), strLog.c_str());
	}
}

void CGameTable::CompareAndLogCardLays(int (*pOriginalCardLays)[SK_LAYOUT_NUM], int (*pModifiedCardLays)[SK_LAYOUT_NUM], int nCount)
{
	UwlLogFile(_T("NC  B  S  2  A  K  Q  J  10 9  8  7  6  5  4  3"));
	int nOrder[SK_LAYOUT_NUM] = {0, 15, 14, 1, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2};
	if (NULL == pOriginalCardLays && NULL != pModifiedCardLays)
	{
		for (int i = 0; i < nCount; i++)
		{
			int nHandCount = 0;
			int nBombCount = 0;
			int nBigCardCount = 0;
			CalcHandCardsCount(pModifiedCardLays[i], nHandCount, nBombCount, nBigCardCount);

			string strLog = "";
			char buffer[5];

			if (0 == nHandCount/10)
			{
				_itoa(nHandCount, buffer, 10);
				strLog += buffer[0];
				strLog += "   ";
			}
			else if (1 == nHandCount/10)
			{
				_itoa(nHandCount, buffer, 10);
				strLog += buffer[0];
				strLog += buffer[1];
				strLog += "  ";
			}
			else
			{
				strLog += "XX  ";
			}

			for (int j = 1; j < SK_LAYOUT_NUM; j++)
			{
				_itoa(pModifiedCardLays[i][nOrder[j]], buffer, 10);
				strLog += buffer[0];
				strLog += "  ";
			}
			UwlLogFile(_T("%s"), strLog.c_str());
		}
	}
	else if (NULL != pOriginalCardLays && NULL != pModifiedCardLays)
	{
		for (int i = 0; i < nCount; i++)
		{
			int nHandCount = 0;
			int nBombCount = 0;
			int nBigCardCount = 0;
			CalcHandCardsCount(pModifiedCardLays[i], nHandCount, nBombCount, nBigCardCount);
			
			string strLog = "";
			char buffer[5];
			
			if (0 == nHandCount/10)
			{
				_itoa(nHandCount, buffer, 10);
				strLog += buffer[0];
				strLog += "   ";
			}
			else if (1 == nHandCount/10)
			{
				_itoa(nHandCount, buffer, 10);
				strLog += buffer[0];
				strLog += buffer[1];
				strLog += "  ";
			}
			else
			{
				strLog += "XX  ";
			}

			for (int j = 1; j < SK_LAYOUT_NUM; j++)
			{
				_itoa(pModifiedCardLays[i][nOrder[j]], buffer, 10);
				if (pOriginalCardLays[i][nOrder[j]] != pModifiedCardLays[i][nOrder[j]])
				{
					strLog += "@";
					strLog += buffer[0];
					strLog += " ";
				}
				else
				{
					strLog += buffer[0];
					strLog += "  ";
				}
			}
			UwlLogFile(_T("%s"), strLog.c_str());
		}
	}
}

BOOL CGameTable::CheckCards()
{
	int	nCardsLay[MAX_CARDS_LAYOUT_NUM];	// 
	ZeroMemory(nCardsLay, sizeof(nCardsLay));
	
	GAME_PUBLIC_INFO* pPublicInfo=GetPublicInfo();
	int i;
	for(i=0;i<TOTAL_CARDS;i++)
	{
		nCardsLay[pPublicInfo->GameCard[i].nCardID%54]++;
	}
	
	for (i=0;i<54;i++)
	{
		if(nCardsLay[i] != m_nTotalPacks){
			return FALSE;
		}
	}
	return TRUE;
}

BOOL CGameTable::ReadCardsFromFile()
{
	TCHAR szKey[32];
	TCHAR szCards[TOTAL_CHAIRS][256];
	///////////////////////////////////////////////
	int nIndex=0;
	GAME_PUBLIC_INFO* pPublicInfo=GetPublicInfo();

	int IsFixChair0Cards = GetPrivateProfileInt(_T("StartDeal"), _T("SoleRealPlayerOnRobotTableCatchChair0Cards"), 0, GetINIFileName());
	int realPlayerCount = 0;
	int cardsIndexForSwapOnRobotTable[TOTAL_CHAIRS] = {0, 0, 0};
	if (IsFixChair0Cards == 1)
	{
		for (int i = 0; i < TOTAL_CHAIRS; i++)
		{
			cardsIndexForSwapOnRobotTable[i] = i;
			if (this->IsRoboter(i) == FALSE)
			{
				cardsIndexForSwapOnRobotTable[i] = 0;
				cardsIndexForSwapOnRobotTable[0] = i;

				realPlayerCount += 1;
			}
		}
	}

	for(int i=0; i<TOTAL_CHAIRS; i++)
	{
		if (IsFixChair0Cards == 1 && realPlayerCount == 1)
		{
			sprintf(szKey, _T("Chair%d"), cardsIndexForSwapOnRobotTable[i]);
		}
		else
		{
			sprintf(szKey, _T("Chair%d"), i);
		}
		
		sprintf_s(szCards[i], "%s", CConfigManagerSys::m_jsoncfgobjmgr[MAKEDEAL_CONFIG]["StartDeal"][szKey].asCString());
		TCHAR *fields[128];
		TCHAR *p1, *p2;
		p1=szCards[i];
		int nCount=RetrieveFields ( p1, fields, 60, &p2 ) ;
		for(int x=0; x<nCount; x++)
		{
			pPublicInfo->GameCard[nIndex].nPositionIndex=nIndex;
			int nCardID=atoi(fields[x]);
			pPublicInfo->GameCard[nIndex].nCardID=nCardID;
			pPublicInfo->GameCard[nIndex].nCardIndex=SK_GetCardIndexEx(nCardID,0);
			pPublicInfo->GameCard[nIndex].nShape=SK_GetCardShapeEx(nCardID,0);
			pPublicInfo->GameCard[nIndex].nValue=SK_GetCardValueEx(nCardID,0);
			nIndex++;
			CatchOneCard(i);
		}
	}
	
	//底牌
	{
		sprintf(szKey,_T("Bottom"));
		TCHAR szBottomCards[256];
		sprintf_s(szBottomCards, "%s", CConfigManagerSys::m_jsoncfgobjmgr[MAKEDEAL_CONFIG]["StartDeal"][szKey].asCString());
		TCHAR *fields[128];
		TCHAR *p1, *p2;
		p1=szBottomCards;
		int nCount=RetrieveFields ( p1, fields, 60, &p2 ) ;
		for(int x=0;x<nCount;x++)
		{
			int nCardID=atoi(fields[x]);
			pPublicInfo->GameCard[nIndex].nCardID=nCardID;
			pPublicInfo->GameCard[nIndex].nCardIndex=SK_GetCardIndexEx(nCardID,0);
			pPublicInfo->GameCard[nIndex].nShape=SK_GetCardShapeEx(nCardID,0);
			pPublicInfo->GameCard[nIndex].nValue=SK_GetCardValueEx(nCardID,0);
			pPublicInfo->GameCard[nIndex].nCardStatus=CARD_STATUS_WAITDEAL;
			pPublicInfo->GameCard[nIndex].nChairNO=-1;
			pPublicInfo->GameCard[nIndex].nPositionIndex=nIndex;
			nIndex++;
			m_nBottomIDs[x] = nCardID;
		}
	}

	if (m_bIsRazzMode) {
		if (CConfigManagerSys::m_jsoncfgobjmgr[MAKEDEAL_CONFIG]["StartDeal"]["RazzValue"].isNumeric()) {
			m_nRazzCardValue = CConfigManagerSys::m_jsoncfgobjmgr[MAKEDEAL_CONFIG]["StartDeal"]["RazzValue"].asInt();
		}
	}
	
	return TRUE;
}

int CGameTable::RetrieveFields ( TCHAR *buf, TCHAR **fields, int maxfields, TCHAR**buf2 )
{
	if ( buf == NULL )
		return 0;
	
	TCHAR *p;
	p = buf;
	int count = 0;
	
	try{
		while ( 1 ) {
			fields [ count ++ ] = p;
			while ( *p != '|' && *p != '\0' ) p++;
			if ( *p == '\0' || count >= maxfields ) 
				break;
			*p = '\0';
			p++;
		}
	}catch(...)
	{
		buf2=NULL;
		return 0;
	}
	
	if ( *p == '\0' ) 
		*buf2 = NULL;
	else
		*buf2 = p+1;
	*p = '\0';
	
	return count;
}

CString CGameTable::GetINIFileName()
{
	CString sRet;
	TCHAR szFilePath[MAX_PATH];
	GetModuleFileName(NULL,szFilePath,MAX_PATH);
	
	*strrchr(szFilePath,'\\')=0;	
	strcat(szFilePath, "\\");
	strcat(szFilePath,PRODUCT_NAME);
	strcat(szFilePath,".ini");
	sRet.Format(_T("%s"),szFilePath);
	return sRet;
}


int  CGameTable::CatchOneCard(int chairno)
{
	if (GetPublicInfo()->nCurrentCatch>=m_nTotalCards)
		return -1;//没牌了
	
	GAME_PUBLIC_INFO* pPublicInfo=GetPublicInfo();
	pPublicInfo->GameCard[GetPublicInfo()->nCurrentCatch].nCardStatus=CARD_STATUS_INHAND;
	pPublicInfo->GameCard[GetPublicInfo()->nCurrentCatch].nChairNO=chairno;
	pPublicInfo->GameCard[GetPublicInfo()->nCurrentCatch].nPositionIndex=0;
	int nCardID=pPublicInfo->GameCard[GetPublicInfo()->nCurrentCatch].nCardID;
	//	m_GameTalbeInfo->nCurrentCatch=(m_GameTalbeInfo->nCurrentCatch+1)%m_nTotalCards;
	int nIndex=SK_GetCardIndexEx(nCardID, 0);
	m_nCardsLayIn[chairno][nIndex]++;
    GetPublicInfo()->nCurrentCatch++;
	
	return nCardID;
}

BOOL CGameTable::GiveCard(int chairno,int destchair,int nCardID)
{
	CARDINFO* card=GetCard(nCardID);
	if (!card) return FALSE;
	if (card->nChairNO!=chairno)return FALSE;
	card->nChairNO=destchair;
	m_nCardsLayIn[chairno][card->nCardIndex]--;
	m_nCardsLayIn[destchair][card->nCardIndex]++;
	return TRUE;
}

BOOL  CGameTable::ValidateThrow(CARDS_THROW* pCardsThrow)
{
	if (!IS_BIT_SET(m_dwStatus,TS_PLAYING_GAME))
		return FALSE;
	if (!IS_BIT_SET(m_dwStatus, TS_WAITING_THROW) && !IS_BIT_SET(m_dwStatus, TS_WAITING_DOUBLE))
		return FALSE;
	if (pCardsThrow->nChairNO!=GetCurrentChair())
		return FALSE;
	if (!IsCardInHand(pCardsThrow->nChairNO,pCardsThrow->unite.nCardIDs,pCardsThrow->unite.nCardCount))
        return FALSE;
	if (!IsCardIdUnique(pCardsThrow->unite.nCardIDs, pCardsThrow->unite.nCardCount))
	{
		return FALSE;
	}
	if (pCardsThrow->unite.nCardCount!=GetCardCount(pCardsThrow->unite.nCardIDs,MAX_CARDS_PER_CHAIR))
		return FALSE;
	
	int nThrowHand[MAX_CARDS_PER_CHAIR];
	XygInitChairCards(nThrowHand,MAX_CARDS_PER_CHAIR);
	memcpy(nThrowHand,pCardsThrow->unite.nCardIDs,sizeof(int)*MAX_CARDS_PER_CHAIR);
	CARD_UNITE unit_details;
	ZeroMemory(&unit_details,sizeof(CARD_UNITE));

	if(m_bIsRazzMode)
	{
		memcpy(m_nFirstRazzCardsAlter, m_nRazzCardsAlter, sizeof(int)*MAX_RAZZ_COUNT);
		m_nFirstChairNo = m_nSecondChairNo;
		m_dwFirstCardType = m_dwSecondCardType;
        
		memset(m_nRazzCardsAlter, 0, sizeof(int)*MAX_RAZZ_COUNT);
		m_dwGameFlags |= GAME_FLAGS_USE_JOKER;
		memcpy(m_nRazzCardsAlter, pCardsThrow->nReserved4, sizeof(int)*MAX_RAZZ_COUNT);

		m_nSecondChairNo = pCardsThrow->nChairNO;
		m_dwSecondCardType = pCardsThrow->unite.dwCardType;
	}
	else
	{
		m_dwGameFlags &= ~GAME_FLAGS_USE_JOKER;		//及时更新配置文件动态配置癞子与非癞子切换的状态
	}
	if (!GetUniteDetails(pCardsThrow->nChairNO,nThrowHand,MAX_CARDS_PER_CHAIR,unit_details,pCardsThrow->unite.dwCardType))
		return FALSE;
	
	if (GetPublicInfo()->nWaitChair!=-1
		&&!CompareCards(GetPublicInfo()->WaitCardUnite,pCardsThrow->unite))//不是第一手牌，必须比前一首牌大
		return FALSE;
	
	//这5种牌型，牌数必须一致
	if (GetPublicInfo()->nWaitChair!=-1
		&& GetPublicInfo()->WaitCardUnite.dwCardType == pCardsThrow->unite.dwCardType)
	{
		if ((pCardsThrow->unite.dwCardType == CARD_UNITE_TYPE_ABT_SINGLE
			|| pCardsThrow->unite.dwCardType == CARD_UNITE_TYPE_ABT_COUPLE
			|| pCardsThrow->unite.dwCardType == CARD_UNITE_TYPE_ABT_THREE
			|| pCardsThrow->unite.dwCardType == CARD_UNITE_TYPE_ABT_THREE_1
			|| pCardsThrow->unite.dwCardType == CARD_UNITE_TYPE_ABT_THREE_COUPLE)
			&& GetPublicInfo()->WaitCardUnite.nCardCount!=pCardsThrow->unite.nCardCount)
			return FALSE;
	}

	return TRUE;
}

void CGameTable::FillupGameResults(void* pData, int nLen, GAME_RESULT_EX GameResults[])
{
	LPGAME_WIN pGameWin = (LPGAME_WIN)pData;

	for (int i = 0; i < m_nTotalChairs; i++) { // 
		if (NULL == m_ptrPlayers[i])
			continue;
		CPlayer* ptrP = m_ptrPlayers[i];
		GameResults[i].nUserID = ptrP->m_nUserID;				// 用户ID
		GameResults[i].nTableNO = m_nTableNO;					// 桌号
		GameResults[i].nChairNO = i;							// 位置
		GameResults[i].nBaseScore = m_nBaseScore;					// 基本积分
		GameResults[i].nBaseDeposit = m_nBaseDeposit;				// 基本银子
		GameResults[i].nOldScore = pGameWin->nOldScores[i];		// 旧积分
		GameResults[i].nOldDeposit = pGameWin->nOldDeposits[i];	// 旧银子
		GameResults[i].nScoreDiff = pGameWin->nScoreDiffs[i];		// 积分增减	
		GameResults[i].nDepositDiff = pGameWin->nDepositDiffs[i];	// 银子输赢
		GameResults[i].nLevelID = pGameWin->nLevelIDs[i];		// 级别ID
		GameResults[i].nBout = 1;							// 总回合
		GameResults[i].nBreakOff = 0;							// 断线次数
		if (m_nBaseDeposit == 0 && m_nBaseScore > 0) {
			GameResults[i].nFee = 0;		// 积分房茶水费强制设置0，否则提交不了结算
		}
		else {
			GameResults[i].nFee = pGameWin->nWinFees[i];		// 茶水费
		}
		GameResults[i].nParentGameId = ptrP->m_nParentGameId;		// 宿主游戏ID
		GameResults[i].dwParentGameCode = ptrP->m_dwParentGameCode;		// 宿主游戏CODE
		lstrcpy(GameResults[i].szLevelName, pGameWin->szLevelNames[i]);	// 级别名称

		// 耗时(秒)	 经验值(分钟)
		(void)CalcTimeCost(GameResults[i].nTimeCost, GameResults[i].nExperience);

		// match
		if (m_bIsMatchGame) {
			if (pGameWin->nWinPoints[i] > 0) {
				GameResults[i].nWin = 1;							// 赢(次数)
			}
			else if (pGameWin->nWinPoints[i] == 0) {
				GameResults[i].nStandOff = 1;						// 和(次数)
			}
			else {
				GameResults[i].nLoss = 1;							// 输(次数)		
			}
		}
		else {
			if (pGameWin->nWinPoints[i] > 0) {
				GameResults[i].nWin = 1;							// 赢(次数)
			}
			else if (pGameWin->nWinPoints[i] == 0) {
				GameResults[i].nStandOff = 1;							// 和(次数)
			}
			else {
				GameResults[i].nLoss = 1;							// 输(次数)		
			}
		}
		

		{
			jsonAddInfo[i]["bout"] = 1;
			if (GameResults[i].nWin)
			{
				jsonAddInfo[i]["win"] = 1;
				if (i == m_nBanker) {
					jsonAddInfo[i]["banker_win"] = 1;
					if (m_boutDataCache.nSpring > 0) {
						jsonAddInfo[i]["banker_spring"] = 1;
					}
				}
				if (i != m_nBanker) {
					jsonAddInfo[i]["farmer_win"] = 1;
					if (m_boutDataCache.nSpring > 0) {
						jsonAddInfo[i]["farmer_spring"] = 1;
					}
				}
			}

			if (GetPrivateProfileInt("primary_room", xyIntToString(m_nRoomID), FALSE, GetINIFileName())) {
				jsonAddInfo[i]["primary_bout"] = 1;
			}
		}
	}
}

int  CGameTable::ThrowCards(CARDS_THROW* pCardsThrow)
{

	CString strIniFile = GetINIFileName();

	GetPlayerInfo(pCardsThrow->nChairNO)->nThrowCount++;

	GetPublicInfo()->nWaitChair=pCardsThrow->nChairNO;
	GetPublicInfo()->WaitCardUnite=pCardsThrow->unite;
	
	for(int i=0;i<pCardsThrow->unite.nCardCount;i++)
	{
		if (pCardsThrow->unite.nCardIDs[i]==-1) break;
		
		CARDINFO* card=GetCard(pCardsThrow->unite.nCardIDs[i]);
		if (!card) continue;
		card->nChairNO=pCardsThrow->nChairNO;
		card->nCardStatus=CARD_STATUS_THROWDOWN;
		card->nPositionIndex=i;
		card->nUniteCount=pCardsThrow->unite.nCardCount;
		int index=SK_GetCardIndexEx(pCardsThrow->unite.nCardIDs[i], 0);
		m_nCardsLayIn[pCardsThrow->nChairNO][index]--;
	}
	
	CalcBombInThrow(pCardsThrow);
	CalcChairThrowTime(pCardsThrow->nChairNO);
	
	if (this->IsRobotTable())
	{
		DWORD dwType = pCardsThrow->unite.dwCardType;
		if (IS_BIT_SET((CARD_UNITE_TYPE_MIXEDRAZZ_BOMB | CARD_UNITE_TYPE_PURERAZZ_BOMB), dwType)) {
			dwType = CARD_UNITE_TYPE_BOMB;
		}

		//JuniorRobotAI
		for(int i=0; i<m_nTotalChairs; i++)
			m_GameAI[i].OnThrowCards(pCardsThrow->nChairNO, pCardsThrow->unite.nCardIDs, pCardsThrow->unite.nCardCount);


		//RemoteRobotAI
		if (Robot_IsUseRemoteAI() == TRUE)
		{
			int nCardCount = pCardsThrow->unite.nCardCount;
			int nCardIDs[MAX_CARDS_PER_CHAIR + 1];
			memcpy(nCardIDs, pCardsThrow->unite.nCardIDs, nCardCount * sizeof(int));
			nCardIDs[nCardCount] = dwType;

			{
				Json::Value data(Json::objectValue);
				Json::Value cardIDs(Json::arrayValue);

				for (int i = 0; i < nCardCount; ++i) {
					Json::Value cardObj;
					cardObj["id"] = nCardIDs[i];
					cardObj["value"] = CardIDTobitIdx(nCardIDs[i]);
					cardIDs.append(cardObj);
				}

				data["cardIDs"] = cardIDs;

				data["cardType"] = (int)pCardsThrow->unite.dwCardType;

				Json::StreamWriterBuilder builder;
				const std::string json = Json::writeString(builder, data);

				LOG_INFO("%s json:%s", __FUNCTION__, json);


				std::vector<CAIEngineItem> vecAIEngineItems;
				vecAIEngineItems.push_back(CAIEngineItem{ pCardsThrow->nChairNO, CAI_Dll::e_AI_OnThrowCards, json });

				m_pGameServer->PostAIEngineAction(this, vecAIEngineItems);
			}
		}
	}
	BOOL bAuto = FALSE;
	if (IsOffline(pCardsThrow->nChairNO)
		|| IsAutoPlay(pCardsThrow->nChairNO))
	{
		bAuto = TRUE;
		GetPlayerInfo(pCardsThrow->nChairNO)->nAutoThrowCount++;
	}

	if (IsNeedRecord())
	{
		RecordAction(m_sinRecord, "Throw", bAuto, pCardsThrow->nChairNO, CoverCardIDsEx(pCardsThrow->unite.nCardIDs, pCardsThrow->unite.nCardCount), pCardsThrow->unite.dwCardType);
	}


	SetStatus(TS_PLAYING_GAME|TS_WAITING_THROW);

	m_boutVideo.RecordPlayerThrowCards(pCardsThrow->nUserID); //BoutVideo日志记录


	//王炸、炸弹、飞机
	if (pCardsThrow->unite.dwCardType == CARD_UNITE_TYPE_ABT_THREE_COUPLE || pCardsThrow->unite.dwCardType == CARD_UNITE_TYPE_ABT_THREE || pCardsThrow->unite.dwCardType == CARD_UNITE_TYPE_ABT_THREE_1)
	{
		jsonAddInfo[pCardsThrow->nChairNO]["abtthree"] = jsonAddInfo[pCardsThrow->nChairNO]["abtthree"].asInt() + 1;
	}
	if (pCardsThrow->unite.dwCardType == CARD_UNITE_TYPE_BOMB)
	{
		jsonAddInfo[pCardsThrow->nChairNO]["normalbomb"] = jsonAddInfo[pCardsThrow->nChairNO]["normalbomb"].asInt() + 1;
	}
	if (pCardsThrow->unite.dwCardType == CARD_UNITE_TYPE_2KING)
	{
		jsonAddInfo[pCardsThrow->nChairNO]["kingbomb"] = jsonAddInfo[pCardsThrow->nChairNO]["kingbomb"].asInt() + 1;
	}

	return 0;
}

CARDINFO*  CGameTable::GetCard(int nCardID)
{
	GAME_PUBLIC_INFO* pPublicInfo=GetPublicInfo();
	for(int i=0;i<m_nTotalCards;i++)
	{
		if (nCardID==pPublicInfo->GameCard[i].nCardID)
			return &pPublicInfo->GameCard[i];
	}
	return NULL;
}


BOOL   CGameTable::SetCardStatus(int nCardID,int chairno,int nStatus)
{
	GAME_PUBLIC_INFO* pPublicInfo=GetPublicInfo();
	for(int i=0;i<m_nTotalCards;i++)
	{
		if (pPublicInfo->GameCard[i].nCardID==nCardID)
		{
			pPublicInfo->GameCard[i].nCardStatus=nStatus;
			pPublicInfo->GameCard[i].nChairNO=chairno;
			return TRUE;
		}
	}
	
	return FALSE;
}


BOOL  CGameTable::CalcWinOnThrow(CARDS_THROW* pCardsThrow)
{
	int nMyChir=GetCurrentChair();
	int nFriend=GetNextChair(GetNextChair(GetCurrentChair()));

	if (!HaveCards(nMyChir))
	{   
		m_dwWinFlags=GW_NORMAL;
		pCardsThrow->Next_Chair=pCardsThrow->nChairNO;

		// 最后一手牌更新 操作时间信息。
		// 此时游戏已经结束了。
		if (isSupportChessFestival() && IsFriendRoom()) {
			int curChair = pCardsThrow->nChairNO;
			int nUserID = m_ptrPlayers[curChair]->m_nUserID;
			if (compManager->getPlayerType(nUserID) != -1) {
				updatePlayerOPCost(curChair);
			}
		}
		return TRUE;
	}
	else
	{
		int chairno=GetNextChair(GetCurrentChair());
        SetCurrentChair(chairno,m_nOperateTime);
		PutThrowCardsToCost(GetCurrentChair());

		while(!HaveCards(GetCurrentChair()))
		{
			int chairno=GetNextChair(GetCurrentChair());
			SetCurrentChair(chairno,m_nOperateTime);
			PutThrowCardsToCost(GetCurrentChair());
		}

		pCardsThrow->Next_Chair=GetCurrentChair();
		// 非最后一手更新 操作时间。
		if (isSupportChessFestival() && IsFriendRoom()) {
			int curChair = pCardsThrow->nChairNO;
			int nUserID = m_ptrPlayers[curChair]->m_nUserID;
			if (compManager->getPlayerType(nUserID) != -1) {
				updatePlayerOPCost(curChair);
			}

			int nextChair = pCardsThrow->Next_Chair;
			int nUserID2 = m_ptrPlayers[nextChair]->m_nUserID;
			if (compManager->getPlayerType(nUserID2) != -1) {
				SetPlayerLastOPTime(m_nBanker);
			}
		}
		return FALSE;
	}
}

BOOL  CGameTable::CalcWinOnPass(CARDS_PASS* pCardsPass)
{
	while(1)
	{
		int chairno=GetNextChair(GetCurrentChair());
        SetCurrentChair(chairno,m_nOperateTime);
		//SetCurrentChair(GetNextChair(GetCurrentChair()));
		PutThrowCardsToCost(GetCurrentChair());

		// 新一轮出牌
		if (GetCurrentChair()==GetPublicInfo()->nWaitChair)//转回当前位置了
		{
			pCardsPass->nWinChair=GetPublicInfo()->nWaitChair;
			GetPublicInfo()->nWaitChair=-1;
			memset(&GetPublicInfo()->WaitCardUnite,0,sizeof(UNITE_TYPE));
			PutAllCardsToCost();//把所有打出的牌都放回废牌区
			pCardsPass->bNextFirst=TRUE;
			m_nRoundCount++; //回合数
			// 先手出牌，第一手出牌。
			if (HaveCards(GetCurrentChair()))
			{
				pCardsPass->Next_Chair=GetCurrentChair();

				// 非最后一手，更新玩家 操作时间。
				if (isSupportChessFestival() && IsFriendRoom()) {
					int curChair = pCardsPass->nChairNO;
					int nUserID = m_ptrPlayers[curChair]->m_nUserID;
					if (compManager->getPlayerType(nUserID) != -1) {
						updatePlayerOPCost(curChair);
					}
					// 重设玩家的操作时间。
					setAllPlayerLastOPTime();
				}
			}
			// 不可能走入的情况。先手出牌，已经没牌了。
			else
 			{	
				// 最后一手更新
				if (isSupportChessFestival() && IsFriendRoom()) {
					int curChair = pCardsPass->nChairNO;
					int nUserID = m_ptrPlayers[curChair]->m_nUserID;
					if (compManager->getPlayerType(nUserID) != -1) {
						updatePlayerOPCost(curChair);
					}
				}
				//没牌，应该已经结束了	//reserved
			}
			return TRUE;
		}
		// 有牌，非先手
		else if (HaveCards(GetCurrentChair()))
		{
			pCardsPass->Next_Chair=GetCurrentChair();
			pCardsPass->bNextFirst=FALSE;

			// 过牌玩家时间更新，下一玩家起始时间更新
			if (isSupportChessFestival() && IsFriendRoom()) {
				int curChair = pCardsPass->nChairNO;
				int nUserID = m_ptrPlayers[curChair]->m_nUserID;
				if (compManager->getPlayerType(nUserID) != -1) {
					updatePlayerOPCost(curChair);
				}

				int nextChair = pCardsPass->Next_Chair;
				int nUserID2 = m_ptrPlayers[nextChair]->m_nUserID;
				if (compManager->getPlayerType(nUserID2) != -1) {
					SetPlayerLastOPTime(m_nBanker);
				}
			}

			return TRUE;
		}
	}

	return TRUE;
}

void   CGameTable::PutThrowCardsToCost(int chairno)
{

    GAME_PUBLIC_INFO* pPublicInfo=GetPublicInfo();
	for(int i=0;i<m_nTotalCards;i++)
	{
		if (pPublicInfo->GameCard[i].nCardStatus==CARD_STATUS_THROWDOWN
			&&pPublicInfo->GameCard[i].nChairNO==chairno)
		{
			pPublicInfo->GameCard[i].nCardStatus=CARD_STATUS_COST;
		}
	}
}

void   CGameTable::PutAllCardsToCost()
{
	GAME_PUBLIC_INFO* pPublicInfo=GetPublicInfo();
	for(int i=0;i<m_nTotalCards;i++)
	{
		if (pPublicInfo->GameCard[i].nCardStatus==CARD_STATUS_THROWDOWN)
		{
			pPublicInfo->GameCard[i].nCardStatus=CARD_STATUS_COST;
		}
	}
}

int  CGameTable::GetTributeCard(int chairno)
{
	int m=0;
	int temp=-1;
	GAME_PUBLIC_INFO* pPublicInfo=GetPublicInfo();
	for(int i=0;i<m_nTotalCards;i++)
	{
		if (pPublicInfo->GameCard[i].nChairNO==chairno
			&&pPublicInfo->GameCard[i].nCardStatus==CARD_STATUS_INHAND
			&&!IsJoker(pPublicInfo->GameCard[i].nCardID)//不能进贡逢人配
			&&SK_GetCardPRIEx(pPublicInfo->GameCard[i].nCardID,GetCurrentRank(),0)>m)
		{
			m=SK_GetCardPRIEx(pPublicInfo->GameCard[i].nCardID,GetCurrentRank(),0);
			temp=pPublicInfo->GameCard[i].nCardID;
		}
	}
	
	return temp;
}

int  CGameTable::GetInHandCard(int chairno,int nCardIDs[])
{
	int count=0;
	GAME_PUBLIC_INFO* pPublicInfo=GetPublicInfo();
	for(int i=0;i<m_nTotalCards;i++)
	{
		if (pPublicInfo->GameCard[i].nChairNO==chairno
			&&pPublicInfo->GameCard[i].nCardStatus==CARD_STATUS_INHAND
			&&count<m_nChairCards)
		{
			
			nCardIDs[count++]=pPublicInfo->GameCard[i].nCardID;
		}
	}
	return count;
}

void CGameTable::SetCurrentRank(int nRank)
{
	GetPublicInfo()->nCurrentRank=nRank;
}

int  CGameTable::GetCurrentRank()
{
	return GetPublicInfo()->nCurrentRank;
}

int  CGameTable::CalcChairThrowTime(int chairno)
{
	int nTotalCastTime=GetPlayerInfo(chairno)->nTotalThrowCost;
	nTotalCastTime+=(GetTickCount()-m_dwStatusBegin)/1000;

	GetPlayerInfo(chairno)->nThrowTime=m_nOperateTime;
	GetPlayerInfo(chairno)->nTotalThrowCost=nTotalCastTime;
	
	return m_nOperateTime;
}


void  CGameTable::CalcBombInThrow(CARDS_THROW* pCardsThrow)
{
	//记录炸弹奖励
	if (pCardsThrow->unite.dwCardType==CARD_UNITE_TYPE_BOMB
		|| pCardsThrow->unite.dwCardType==CARD_UNITE_TYPE_2KING
		|| pCardsThrow->unite.dwCardType==CARD_UNITE_TYPE_MIXEDRAZZ_BOMB
		|| pCardsThrow->unite.dwCardType==CARD_UNITE_TYPE_PURERAZZ_BOMB)
	{
		GetPublicInfo()->nBombFan+=1;
		GetPlayerInfo(pCardsThrow->nChairNO)->nBombCount++;
	}
}

BOOL  CGameTable::IsCardInHand(int nChairNO,int nCardIDs[],int nCount)
{
	for(int i=0;i<nCount;i++)
	{
		if (nCardIDs[i]!=-1)
		{
			CARDINFO* card=GetCard(nCardIDs[i]);
			//解决了可以反复出同样牌的外挂问题,没有校验打出牌
			if (!card || card->nChairNO!=nChairNO || card->nCardStatus!=CARD_STATUS_INHAND)
				return FALSE;
		}
	}
	
	return TRUE;
}

/*
应对复制牌攻击
*/
BOOL CGameTable::IsCardIdUnique(int aCard[], int nLen)
{
	for (int i = 1; i < nLen; i++)
	{
		for (int j = 0; j < i; j++)
		{
			if (aCard[j] == aCard[i])
			{
				return FALSE;
			}
		}
	}
	return TRUE;
}

void  CGameTable::OnPass(CARDS_PASS* pCardsPass)
{
	if (this->IsRobotTable())
	{
		//JuniorRobotAI
		for(int i=0; i<m_nTotalChairs; i++)
			m_GameAI[i].OnPassCards(pCardsPass->nChairNO);

		//RemoteRobotAI
		if (Robot_IsUseRemoteAI() == TRUE)
		{
			CString strIniFile = GetINIFileName();

			{
				Json::Value data(Json::objectValue);
				
				Json::StreamWriterBuilder builder;
				const std::string json = Json::writeString(builder, data);

				LOG_INFO("%s json:%s", __FUNCTION__, json);


				std::vector<CAIEngineItem> vecAIEngineItems;
				vecAIEngineItems.push_back(CAIEngineItem{ pCardsPass->nChairNO, CAI_Dll::e_AI_OnPassCards, json });

				m_pGameServer->PostAIEngineAction(this, vecAIEngineItems);
			}
		}
	}

	m_nPassTimes[pCardsPass->nChairNO]++; //过牌次数

	BOOL bAuto = FALSE;
	if (IsOffline(pCardsPass->nChairNO)
		|| IsAutoPlay(pCardsPass->nChairNO))
	{
		bAuto = TRUE;
	}
	if (IsNeedRecord())
	{
		RecordAction(m_sinRecord, "Pass", bAuto, pCardsPass->nChairNO);
	}

	SetStatus(TS_PLAYING_GAME|TS_WAITING_THROW);

	m_boutVideo.RecordPlayerPassCards(pCardsPass->nUserID); //BoutVideo日志记录
}

void  CGameTable::OnChat(LPCHAT_TO_TABLE chatInfo, const std::string& sRecord)
{
	if (!IS_BIT_SET(m_dwStatus, TS_PLAYING_GAME))
		return;

	if (this->IsRobotTable())
	{
		//JuniorRobotAI

		//RemoteRobotAI
		if (Robot_IsUseRemoteAI() == TRUE)
		{
			CString strIniFile = GetINIFileName();

			{
				Json::Value data(Json::objectValue);
				data["chatContent"] = sRecord.data();

				Json::StreamWriterBuilder builder;
				const std::string json = Json::writeString(builder, data);

				LOG_INFO("%s json:%s", __FUNCTION__, json);


				std::vector<CAIEngineItem> vecAIEngineItems;
				vecAIEngineItems.push_back(CAIEngineItem{ chatInfo->nChairNO, CAI_Dll::e_AI_Chat, json });

				m_pGameServer->PostAIEngineAction(this, vecAIEngineItems);
			}
		}
	}

	if (IsNeedRecord())
	{
		RecordAction(m_sinRecord, "Chat", FALSE, chatInfo->nChairNO, sRecord);
	}
}


BOOL  CGameTable::ValidatePass(CARDS_PASS* pCardsPass)
{
	if (!IS_BIT_SET(m_dwStatus,TS_PLAYING_GAME))
		return FALSE;
	if (!IS_BIT_SET(m_dwStatus,TS_WAITING_THROW))
		return FALSE;
	if (!ValidateChair(pCardsPass->nChairNO)
		|| !ValidateChair(GetCurrentChair()))
		return FALSE;
	if (pCardsPass->nChairNO!=GetCurrentChair())
		return FALSE;
	
	return TRUE;
}


void CGameTable::FillupNextBoutInfo(void* pData, int nLen, int chairno)
{
	CSkTable::FillupNextBoutInfo(pData,nLen,chairno);
	LPGAME_WIN_RESULT pGameWin = (LPGAME_WIN_RESULT)pData;
	pGameWin->nNextBaseScore = GetBaseScore();

	//Add on 20121011
	//读房间设置
	//如果是比赛房间固定基础银基础分，使用固定值
	CString strIniFile = GetINIFileName();
	{
		TCHAR szRoomID[16];
		memset(szRoomID, 0, sizeof(szRoomID));
		_stprintf(szRoomID, _T("%ld"), m_nRoomID);

		int base_score = GetPrivateProfileInt(
				_T("basescore"),		// section name
				szRoomID,		// key name
				0,	// default int
				strIniFile		// initialization file name
				);
			
		if (base_score>0)
			pGameWin->nNextBaseScore = base_score;
	}
	//Add end

	//无人叫庄，此局无效
	if (pGameWin->nReserved[0]!=0)
	{
		pGameWin->nReserved[1] = chairno;
	}
}	

BOOL CGameTable::CalcCardType_Bomb(int nCardIDs[],int nCardLen,int nCardCount,CARD_UNITE* CardDetail)
{
	if (nCardLen<=0)
		return FALSE;
	
	if (CardDetail->nTypeCount>=MAX_FIT_TYPE) return FALSE;
	if (nCardCount<4) return FALSE;                  //必须是四张或者5张相同的牌
	int nCardLay[SK_LAYOUT_NUM];
	memset(nCardLay,0,sizeof(nCardLay));
	int nJokerCount=0;
	PreDealCards(nCardIDs,nCardLen,nCardLay,SK_LAYOUT_NUM,nJokerCount);

	if (m_bIsRazzMode && nJokerCount>0)//非癞子炸弹
	{
		return FALSE;
	}
	
	int& index=CardDetail->nTypeCount;

	memset(&CardDetail->uniteType[index],0,sizeof(UNITE_TYPE));
	CardDetail->uniteType[index].nCardCount=nCardCount;
	//主值放前，财神放后
	memset(&CardDetail->uniteType[index],0,sizeof(UNITE_TYPE));
	if (!GetCardType_Bomb(nCardIDs,nCardLen,nCardLay,SK_LAYOUT_NUM,nJokerCount,CardDetail->uniteType[index],nCardCount))
		return FALSE;

	index++;
	return TRUE;
}

BOOL CGameTable::GetCardType_Bomb(int nCardIDs[],int nCardLen,int nCardLay[],int nLayLen,int nJokerCount,UNITE_TYPE& type,int nUseCount)
{
	if (nCardLen<=0 || nLayLen<=0)
		return FALSE;
	
	int nValue=0;

	if (type.dwCardType==CARD_UNITE_TYPE_BOMB)
	{
		nValue=type.nMainValue;
	}

	if (m_bIsRazzMode && !m_bIsRemind && nJokerCount != 0)	return FALSE;

	if (m_bIsRazzMode&&!m_bIsRemind)
	{
		for (int i=0;i<nLayLen;i++)
		{
			if (GetCardValueByIndex(i)==m_nRazzCardValue && nCardLay[i]==4)
				return FALSE;
		}
	}
	
	
	int nCardCount=0;
	int nCardIndex=-1;
	nJokerCount=0;
	if (nUseCount)
	{
		nCardCount=nUseCount;
		nCardIndex=GetSameCountEx(nCardLay,nLayLen,nUseCount,nJokerCount,nValue);
		if (nCardIndex==-1)
			return FALSE;
	}
	else
	{
		for(int i=4;i<=4*m_nTotalPacks;i++)
		{
			nCardIndex=GetSameCountEx(nCardLay,nLayLen,i,nJokerCount,nValue);
			if (nCardIndex!=-1)
			{
				nCardCount=i;
				break;
			}
		}
		
		if (nCardIndex==-1)
			return FALSE;
	}
	
	type.dwCardType=CARD_UNITE_TYPE_BOMB;
	type.dwComPareType=COMPARE_UNITE_TYPE_BOMB_EX;
	type.nMainValue=SK_GetIndexPRIEx(nCardIndex,GetCurrentRank(), 0);
	for(int k=0;k<nCardCount-4;k++)
	{
		type.nMainValue+=10000;
	}
	
	type.nCardCount=nCardCount;
	XygInitChairCards(type.nCardIDs,MAX_CARDS_PER_CHAIR);
	
	int temp[MAX_CARDS_PER_CHAIR];
	memset(temp,0,sizeof(temp));
	memcpy(temp,nCardIDs,sizeof(int)*nCardLen);
	PutCardToArray(type.nCardIDs,nCardLen,temp,nCardLen,nCardIndex,nCardCount);
	return TRUE;
}


BOOL  CGameTable::GetUniteDetails(int chairno, int nCardIDs[],int nCardsLen,CARD_UNITE& unit_detail,DWORD dwFlags)
{
	int count=GetCardCount(nCardIDs,nCardsLen);
	//首先清理CARD_UNITE
	
	if (IS_BIT_SET(dwFlags,CARD_UNITE_TYPE_SINGLE))
		CalcCardType_Single(nCardIDs,nCardsLen,count,&unit_detail);
	
	if (IS_BIT_SET(dwFlags,CARD_UNITE_TYPE_COUPLE))
		CalcCardType_Couple(nCardIDs,nCardsLen,count,&unit_detail);
	
	if (IS_BIT_SET(dwFlags,CARD_UNITE_TYPE_THREE))
		CalcCardType_Three(nCardIDs,nCardsLen,count,&unit_detail);

	if (IS_BIT_SET(dwFlags,CARD_UNITE_TYPE_THREE_1))
		CaclCardType_Three_1(nCardIDs,nCardsLen,count,&unit_detail);
	
	if (IS_BIT_SET(dwFlags,CARD_UNITE_TYPE_THREE_COUPLE))
		CaclCardType_Three_Couple(nCardIDs,nCardsLen,count,&unit_detail);
	
	if (IS_BIT_SET(dwFlags,CARD_UNITE_TYPE_ABT_SINGLE))
		CaclCardType_ABT_Single(nCardIDs,nCardsLen,count,&unit_detail);
	
	if (IS_BIT_SET(dwFlags,CARD_UNITE_TYPE_ABT_COUPLE))
		CaclCardType_ABT_Couple(nCardIDs,nCardsLen,count,&unit_detail);
	
	if (IS_BIT_SET(dwFlags,CARD_UNITE_TYPE_ABT_THREE))
		CaclCardType_ABT_Three(nCardIDs,nCardsLen,count,&unit_detail);

	if (IS_BIT_SET(dwFlags,CARD_UNITE_TYPE_ABT_THREE_1))
		CaclCardType_ABT_Three_1(nCardIDs,nCardsLen,count,&unit_detail);
	
	if (IS_BIT_SET(dwFlags,CARD_UNITE_TYPE_ABT_THREE_COUPLE))
		CaclCardType_ABT_Three_Couple(nCardIDs,nCardsLen,count,&unit_detail);
	
	//4带2张单
	if (IS_BIT_SET(dwFlags,CARD_UNITE_TYPE_FOUR_2))
		CalcCardType_Four_2(nCardIDs,nCardsLen,count,&unit_detail);

	//4带2张对子
	if (IS_BIT_SET(dwFlags,CARD_UNITE_TYPE_FOUR_2_COUPLE))
		CalcCardType_Four_2_Couple(nCardIDs,nCardsLen,count,&unit_detail);
	
	if (IS_BIT_SET(dwFlags, CARD_UNITE_TYPE_MIXEDRAZZ_BOMB) && m_bIsRazzMode)//癞子炸弹
		CalcCardType_BombMixedRazz(nCardIDs,nCardsLen,count,&unit_detail);

	if (IS_BIT_SET(dwFlags,CARD_UNITE_TYPE_BOMB))
		CalcCardType_Bomb(nCardIDs,nCardsLen,count,&unit_detail);
		
	if (IS_BIT_SET(dwFlags, CARD_UNITE_TYPE_PURERAZZ_BOMB) && m_bIsRazzMode)//纯癞子炸弹
		CalcCardType_BombPureRazz(nCardIDs,nCardsLen,count,&unit_detail);

	if (IS_BIT_SET(dwFlags,CARD_UNITE_TYPE_2KING))
		CalcCardType_BOMB_2King(nCardIDs,nCardsLen,count,&unit_detail);
	
	
	if (unit_detail.nTypeCount>0)
		return TRUE;
	else
		return FALSE;
}

DWORD CGameTable::SetStatusOnStart()
{
	//注意，不可使用SetStatus，否则会导致比赛出现无法开始的情况
	return AddStatus(TS_PLAYING_GAME|TS_WAITING_AUCTION);
}

int CGameTable::SetCurrentChairOnStart()
{
	SetCurrentChair(m_nBanker,m_nOperateTime);
	
	return GetCurrentChair();
}

BOOL CGameTable::OnAuctionBanker(LPAUCTION_BANKER pAuctionBanker, int& auction_finished, BOOL bAuto)
{
	int nUserId = pAuctionBanker->nUserID;
	int chairno = pAuctionBanker->nChairNO;
	BOOL passed = pAuctionBanker->bPassed;
	int gains = pAuctionBanker->nGains;

	if (!ValidateChair(chairno))
		return FALSE;
	//分不对
	if (!passed && (gains<1 || gains>3))
		return FALSE;

	int i;
	//已经叫过庄
	for (i=0;i<m_nAuctionCount;i++)
	{
		if (m_Auctions[i].nChairNO == chairno)
			return FALSE;
	}

	m_nAuctionCount++;
	
	assert(m_nAuctionCount <= MAX_AUCTION_COUNT);
	
	m_Auctions[m_nAuctionCount - 1].nChairNO = chairno;
	m_Auctions[m_nAuctionCount - 1].bPassed = passed;
	m_Auctions[m_nAuctionCount - 1].nGains = gains;
	
	if(!IS_BIT_SET(m_dwGameFlags, GF_AUCTION_REVERSE)){
		if(!passed && gains >= m_nMaxAuction){  // 叫了最高分
			m_nObjectGains = gains;
			m_nBanker = chairno;
			auction_finished = 1;
		}else if(IS_BIT_SET(m_dwGameFlags, GF_AUCTION_ONCE) 
			&& m_nAuctionCount >= m_nTotalChairs){
			auction_finished = 1;
			BOOL all_passed = TRUE;
			for(i = 0; i < m_nTotalChairs; i++){
				if(!m_Auctions[i].bPassed){
					all_passed = FALSE;
					if(m_Auctions[i].nGains >= m_nObjectGains){
						m_nObjectGains = m_Auctions[i].nGains;
						m_nBanker = m_Auctions[i].nChairNO;
					}
				}
			}
			if(all_passed){ // 所有人都放弃
				m_nObjectGains = m_nDefAuction;
//				auction_finished = 2;	//表示重新发牌
				setAuctionEndResult(auction_finished);
			}
		}else if(gains > m_nObjectGains){
			m_nObjectGains = gains;
		}
	}else{ // 叫分从大往小倒着叫
		if(!passed && gains <= m_nMinAuction){  // 叫了最低分
			m_nObjectGains = gains;
			m_nBanker = chairno;
			auction_finished = 1;
		}else if(IS_BIT_SET(m_dwGameFlags, GF_AUCTION_ONCE) 
			&& m_nAuctionCount >= m_nTotalChairs){
			auction_finished = 1;
			BOOL all_passed = TRUE;
			for(int i = 0; i < m_nTotalChairs; i++){
				if(!m_Auctions[i].bPassed){
					all_passed = FALSE;
					if(m_Auctions[i].nGains <= m_nObjectGains){
						m_nObjectGains = m_Auctions[i].nGains;
						m_nBanker = m_Auctions[i].nChairNO;
					}
				}
			}
			if(all_passed){ // 所有人都放弃
				m_nObjectGains = m_nDefAuction;
			}
		}else if(gains < m_nObjectGains){
		}
	}
	if(!auction_finished){
		SetCurrentChair(GetNextChair(GetCurrentChair()), m_nOperateTime);
	}

	if (this->IsRobotTable())
	{
		//JuniorRobotAI
		for(i=0; i<m_nTotalChairs; i++)
			m_GameAI[i].OnAuction(chairno, gains);

		//RemoteRobotAI
		if (Robot_IsUseRemoteAI() == TRUE)
		{

			CString strIniFile = GetINIFileName();

			{
				Json::Value data(Json::objectValue);
				data["isScore"] = gains;

				Json::StreamWriterBuilder builder;
				const std::string json = Json::writeString(builder, data);

				std::vector<CAIEngineItem> vecAIEngineItems;
				vecAIEngineItems.push_back(CAIEngineItem{ chairno, CAI_Dll::e_AI_OnAuction, json });

				m_pGameServer->PostAIEngineAction(this, vecAIEngineItems); // 该接口会自己通知本桌所有ai
			}
		}
	}

	if (IsNeedRecord())
	{
		RecordAction(m_sinRecord, "Auction", bAuto, pAuctionBanker->nChairNO, (pAuctionBanker->bPassed ? 0 : pAuctionBanker->nGains));
	}

	m_boutVideo.RecordPlayerCallScore(nUserId, gains); //BoutVideo日志记录

	return TRUE;
}

BOOL CGameTable::OnRobBanker(LPROB_BANKER pRobBanker, int& rob_finished, int noCallBankerForceType, BOOL bAuto)
{
	int nUserId = pRobBanker->nUserID;
	int chairno = pRobBanker->nChairNO;
	int nextchair = GetNextChair(pRobBanker->nChairNO);
	int prechair = GetPrevChair(pRobBanker->nChairNO);
	int nFirstAuctionChair = m_nBanker;
	int nSecAuctionChair = GetNextChair(nFirstAuctionChair);
	int nThirdAuctionChair = GetNextChair(nSecAuctionChair);;
	BOOL bNoCall = pRobBanker->bNoCall;
	BOOL bCalled = pRobBanker->bCalled;
	BOOL bNoRob = pRobBanker->bNoRob;
	BOOL bRobbed = pRobBanker->bRobbed;
	
	if (!ValidateChair(chairno))
		return FALSE;

	int nTrueCount = 0;
	if (bNoCall)
		nTrueCount++;
	if (bCalled)
		nTrueCount++;
	if (bNoRob)
		nTrueCount++;
	if (bRobbed)
		nTrueCount++;

	//抢地主状态不对
	if (nTrueCount != 1)
		return FALSE;

	//疯狂玩法最多只能叫一次地主，其它都是抢地主
	if ((m_nAuctionCount != 0 && m_nAuctionCount != 1)
		|| (bCalled && m_nAuctionCount > 0))
		return FALSE;

	// 更新操作玩家的时间。放到判断不叫之前，为不叫的玩家也记录操作时间。
	if (isSupportChessFestival() && IsFriendRoom()) {
		int nUserID = m_ptrPlayers[chairno]->m_nUserID;
		// 只统计参赛选手的信息
		if (compManager->getPlayerType(nUserID) != -1) {
			updatePlayerOPCost(chairno);
		}
	}

	//不叫的玩家不参加抢地主
	for (int i=0;i<TOTAL_CHAIRS;i++)
	{
		if (m_Rob[i].nChairNO == chairno && m_Rob[i].bNoCall)
			return FALSE;
	}
	
	if (IsNeedRecord())
	{
		RecordAction(m_sinRecord, m_nAuctionCount == 0 ? "Call" : "Rob", bAuto, pRobBanker->nChairNO, (m_nAuctionCount == 0 ? pRobBanker->bCalled : pRobBanker->bRobbed));
	}

	if (bNoCall)
	{
		m_boutVideo.RecordPlayerCallBanker(nUserId, FALSE); //BoutVideo日志记录
	}

	if (bCalled && 0 == m_nAuctionCount)
	{
		m_nAuctionCount = 1;//叫地主
		m_nObjectGains = 3;

		m_boutVideo.RecordPlayerCallBanker(nUserId, TRUE); //BoutVideo日志记录
	}
	else if (bNoRob || bRobbed)
	{
		m_nRobCount++;//抢地主
		if (bRobbed)
			m_nObjectGains *= 2;

		if (bRobbed)
		{
			m_boutVideo.RecordPlayerRobBanker(nUserId, TRUE); //BoutVideo日志记录
		}
		else if (bNoRob)
		{
			m_boutVideo.RecordPlayerRobBanker(nUserId, FALSE); //BoutVideo日志记录
		}
	}

	//不叫地主的自动算不抢
	if (m_Rob[nextchair].bNoCall)
	{
		m_nRobCount++;
		nextchair = GetNextChair(nextchair);
		if (m_Rob[nextchair].bNoCall)
		{
			m_nRobCount++;
			nextchair = GetNextChair(nextchair);
		}
	}
	
	m_Rob[chairno].nChairNO = chairno;
	m_Rob[chairno].bNoCall = bNoCall;
	m_Rob[chairno].bCalled = bCalled;
	m_Rob[chairno].bNoRob = bNoRob;
	m_Rob[chairno].bRobbed = bRobbed;

	if (m_Rob[0].bNoCall 
		&& m_Rob[1].bNoCall 
		&& m_Rob[2].bNoCall)
	{//3个玩家都不叫
		m_nBanker = nFirstAuctionChair;
		m_nObjectGains = 3;
		if (noCallBankerForceType == NOCALLBANKER_NOFORCE || noCallBankerForceType == NOCALLBANKER_LASTFORCE)
		{
			rob_finished = 2;
		}
		else
		{
			rob_finished = 1;
		}
	}
	else if (m_Rob[nFirstAuctionChair].bCalled
		&& m_Rob[nSecAuctionChair].bNoRob
		&& m_Rob[nThirdAuctionChair].bNoRob)
	{//第1个叫了地主，后2个不抢
		m_nBanker = nFirstAuctionChair;
		rob_finished = 1;
	}
	else if (m_Rob[nFirstAuctionChair].bNoCall
		&& m_Rob[nSecAuctionChair].bCalled
		&& m_Rob[nThirdAuctionChair].bNoRob)
	{//第1个不叫，第2个叫地主，第3个不抢
		m_nBanker = nSecAuctionChair;
		rob_finished = 1;
	}
	else if (m_Rob[nFirstAuctionChair].bNoCall 
		&& m_Rob[nSecAuctionChair].bNoCall
		&& m_Rob[nThirdAuctionChair].bCalled)
	{//前2个玩家不叫，第3个玩家叫了地主
		m_nBanker = nThirdAuctionChair;
		rob_finished = 1;
	}
	else if (m_nRobCount >= 3)
	{//抢地主数量已达上限，结束抢地主
		if (bRobbed) 
		{
			m_nBanker = chairno;
		}
		else
		{
			if (m_Rob[prechair].bRobbed || m_Rob[prechair].bCalled)
			{
				m_nBanker = prechair;
			}
			else if(m_Rob[GetPrevChair(prechair)].bRobbed || m_Rob[GetPrevChair(prechair)].bCalled)
			{
				m_nBanker = GetPrevChair(prechair);
			}
			else
			{
				m_nBanker = nFirstAuctionChair;
			}
		}
		
		rob_finished = 1;

		if(m_nRobCount > 3)
		{
			UwlLogFile(_T("Rob_Banker Error: rob[0]:chairno=%d, nocall=%d, called=%d, norob=%d, robbed=%d,\nrob[1]:chairno=%d, nocall=%d, called=%d, norob=%d, robbed=%d,\nrob[2]:chairno=%d, nocall=%d, called=%d, norob=%d, robbed=%d,\nbanker=%d, nRobCount=%d"), 
				m_Rob[0].nChairNO, m_Rob[0].bNoCall, m_Rob[0].bCalled, m_Rob[0].bNoRob, m_Rob[0].bRobbed,
				m_Rob[1].nChairNO, m_Rob[1].bNoCall, m_Rob[1].bCalled, m_Rob[1].bNoRob, m_Rob[1].bRobbed,
				m_Rob[2].nChairNO, m_Rob[2].bNoCall, m_Rob[2].bCalled, m_Rob[2].bNoRob, m_Rob[2].bRobbed, 
				m_nBanker, m_nRobCount);
		}
	}

	if(!rob_finished){
		SetCurrentChair(nextchair, m_nOperateTime);
	}

	// 更新下一玩家的起始时间。
	if (isSupportChessFestival() && IsFriendRoom()) {
		int nUserID = m_ptrPlayers[nextchair]->m_nUserID;
		// 只统计参赛选手的信息
		if (compManager->getPlayerType(nUserID) != -1) {
			SetPlayerLastOPTime(nextchair);
		}
	}

	if (IsRobotTable())
	{
		//JuniorRobotAI

		//RemoteRobotAI
		if (Robot_IsUseRemoteAI() == TRUE)
		{
			CString strIniFile = GetINIFileName();

			{	
				Json::Value data(Json::objectValue);
				data["isScore"] = m_nObjectGains;

				Json::StreamWriterBuilder builder;
				const std::string json = Json::writeString(builder, data);

				std::vector<CAIEngineItem> vecAIEngineItems;
				vecAIEngineItems.push_back(CAIEngineItem{ chairno, CAI_Dll::e_AI_OnAuction, json });
				
				m_pGameServer->PostAIEngineAction(this, vecAIEngineItems); // 该接口会自己通知本桌所有ai
			}
		}
	}

	return TRUE;
}


// 叫分结束后进入加倍阶段
BOOL CGameTable::OnAuctionFinished()
{
	RemoveStatus(TS_WAITING_AUCTION);
	if (GetDoubleType() != DoubleType::NO_DOUBLE)
	{
		AddStatus(TS_WAITING_DOUBLE);
		SetCurrentChair(m_nBanker, ReadDoubleCommonInfo().m_nDoubleWaitTime);

		// 更新操作玩家的起始时间。
		if (isSupportChessFestival() && IsFriendRoom()) {
			setAllPlayerLastOPTime();
		}
	}
	else
	{
		AddStatus(TS_WAITING_THROW);
		SetCurrentChair(m_nBanker, THROW_WAIT);

		// 设置地主玩家的起始时间。
		if (isSupportChessFestival() && IsFriendRoom()) {
			setAllPlayerLastOPTime();
		}
	}
	
	SetGroupsOnAuctionFinished();
	
	ResetAuctionWhenFinished();

	//给地主加上最后3张牌
	GAME_PUBLIC_INFO* pPublicInfo=GetPublicInfo();

	if (pPublicInfo->nCurrentCatch==m_nTotalCards
		&&m_nBottomCatch[0]>=0)
	{
		for (int i=0;i<BOTTOM_CARD;i++)
		{
			pPublicInfo->GameCard[m_nBottomCatch[i]].nCardStatus=CARD_STATUS_INHAND;
			pPublicInfo->GameCard[m_nBottomCatch[i]].nChairNO=m_nBanker;
			pPublicInfo->GameCard[m_nBottomCatch[i]].nPositionIndex=0;
			int nCardID=pPublicInfo->GameCard[m_nBottomCatch[i]].nCardID;
			int nIndex=SK_GetCardIndexEx(nCardID, 0);
			m_nCardsLayIn[m_nBanker][nIndex]++;
		}
	}
	else
	{
		for (int i=0;i<BOTTOM_CARD;i++)
		{
			CatchOneCard(m_nBanker);
		}
	}

	if (this->IsRobotTable())
	{
		//JuniorRobotAI
		for(int i=0; i<TOTAL_CHAIRS; i++)
		{
			m_GameAI[i].InitBankerChair(m_nBanker, m_nObjectGains);
			m_GameAI[i].InitBottomCards(m_nBottomIDs, BOTTOM_CARD);
		}

		//RemoteRobotAI
		if (Robot_IsUseRemoteAI() == TRUE)
		{
			CString strIniFile = GetINIFileName();

			{

				std::vector<CAIEngineItem> vecAIEngineItems;
				Json::Value data4InitBankerChair(Json::objectValue);
				//data4InitBankerChair["bankerChair"] = &m_nObjectGains;
				data4InitBankerChair["bankerChair"] = m_nBanker;
				data4InitBankerChair["isScore"] = m_nObjectGains;

				Json::StreamWriterBuilder builder;
				const std::string json4InitBankerChair = Json::writeString(builder, data4InitBankerChair);
				vecAIEngineItems.push_back(CAIEngineItem{ m_nBanker, CAI_Dll::e_AI_InitBankerChair, json4InitBankerChair });



				Json::Value data(Json::objectValue);
				Json::Value cardIDs(Json::arrayValue);
				for (int i = 0; i < BOTTOM_CARD; ++i) {
					Json::Value cardObj;
					cardObj["id"] = m_nBottomIDs[i];
					cardObj["value"] = CardIDTobitIdx(m_nBottomIDs[i]);
					cardIDs.append(cardObj);
				}

				data["cardIDs"] = cardIDs;

				const std::string json = Json::writeString(builder, data);
				
				vecAIEngineItems.push_back(CAIEngineItem{ 0, CAI_Dll::e_AI_InitBottomCards, json });

				m_pGameServer->PostAIEngineAction(this, vecAIEngineItems); // 该接口会自己通知本桌所有ai



			}

			

			if (m_bIsRazzMode) {
				int id = m_nRazzCardValue % SK_LAYOUT_MOD;

				{
					std::vector<CAIEngineItem> vecAIEngineItems;

					Json::Value data(Json::objectValue);
					Json::Value cardIDs(Json::arrayValue);
					
					Json::Value cardObj;
					cardObj["id"] = id;
					cardObj["value"] = CardIDTobitIdx(id);
					cardIDs.append(cardObj);
					

					data["cardIDs"] = cardIDs;

					Json::StreamWriterBuilder builder;
					const std::string json = Json::writeString(builder, data);

					vecAIEngineItems.push_back(CAIEngineItem{ 0, CAI_Dll::e_AI_InitVariantCard, json });

					m_pGameServer->PostAIEngineAction(this, vecAIEngineItems); // 该接口会自己通知本桌所有ai
				}
				
			}
		}
	}

	if (IsNeedRecord())
	{
		RecordAction(m_sinRecord, "Bottom", FALSE, m_nBanker, CoverCardIDsEx(m_nBottomIDs, BOTTOM_CARD));
	}

#if  REPLAY_TEXT_OPEN_VALUE > 0
	if (m_pReportrRecord)
	{
		m_pReportrRecord->SetStartInfoBanker(m_nBanker);     //本局庄家
		//当前某数值的牌的数量	
		for (int i = 0; i < m_nTotalChairs; i++)
		{
			User* pUser = new User;
			pUser->nID1 = m_ptrPlayers[i]->m_nUserID;
			pUser->nID2 = i;
			GetPlayerCardIDs(i, pUser->nCardIDs, PLAYER_CARDS_MAX_LENGTH);
			m_pReportrRecord->AddStartInfoUser(pUser);
		}
	}
#endif

	m_dwAuctionFinishTime = GetTickCount();
	return TRUE;
}

BOOL  CGameTable::GetCardType_Couple(int nCardIDs[],int nCardLen,int nCardLay[], int nLayLen,int nJokerCount,UNITE_TYPE& type)
{
	if (nCardLen<=0 || nLayLen<=0)
		return FALSE;

    if (nCardLen == nJokerCount)
	{
		nJokerCount = 0;
	}

	int nCardIndex=GetSameCountEx(nCardLay,nLayLen,2,nJokerCount,type.nMainValue);
	if (nCardIndex==-1 || nCardIndex==14 || nCardIndex==15) return FALSE;

	type.dwCardType=CARD_UNITE_TYPE_COUPLE;
	type.dwComPareType=CARD_UNITE_TYPE_COUPLE;
	type.nMainValue=SK_GetIndexPRIEx(nCardIndex,GetCurrentRank(), 0);
	type.nCardCount=2;

    if (m_bIsRazzMode && nJokerCount==1)
    {  
		//////////////////////癞子/////////////////////////////
		if (!CalcRazzValueInSame(nJokerCount, nCardIndex))
		{
			return FALSE;
		}
    }
	
	XygInitChairCards(type.nCardIDs,MAX_CARDS_PER_CHAIR);
	
	int temp[MAX_CARDS_PER_CHAIR];
	memset(temp,0,sizeof(temp));
	memcpy(temp,nCardIDs,sizeof(int)*nCardLen);
	PutCardToArray(type.nCardIDs,nCardLen,temp,nCardLen,nCardIndex,2);
	return TRUE;
}

BOOL CGameTable::GetCardType_Three(int nCardIDs[],int nCardLen,int nCardLay[], int nLayLen,int nJokerCount,UNITE_TYPE& type)
{
	if (nCardLen<=0 || nLayLen<=0)
		return FALSE;

    if (nCardLen == nJokerCount)
	{
		nJokerCount = 0;
	}

	int nCardIndex=GetSameCountEx(nCardLay,nLayLen,3,nJokerCount,type.nMainValue);
	if (nCardIndex==-1 || nCardIndex==14 || nCardIndex==15) return FALSE;


	type.dwCardType=CARD_UNITE_TYPE_THREE;
	type.dwComPareType=CARD_UNITE_TYPE_THREE;
	type.nMainValue=SK_GetIndexPRIEx(nCardIndex,GetCurrentRank(), 0);
	type.nCardCount=3;
	
	if (m_bIsRazzMode && (nJokerCount==1 || nJokerCount==2))
    { 
		//////////////////////癞子/////////////////////////////
		if (!CalcRazzValueInSame(nJokerCount, nCardIndex))
		{
			return FALSE;
		}		
    }
	
	XygInitChairCards(type.nCardIDs,MAX_CARDS_PER_CHAIR);

	int temp[MAX_CARDS_PER_CHAIR];
	memset(temp,0,sizeof(temp));
	memcpy(temp,nCardIDs,sizeof(int)*nCardLen);
	PutCardToArray(type.nCardIDs,nCardLen,temp,nCardLen,nCardIndex,3);
	return TRUE;
}

BOOL CGameTable::CaclCardType_Three_1(int nCardIDs[],int nCardLen,int nCardCount,CARD_UNITE* CardDetail)
{
	if (nCardLen<=0)
		return FALSE;
	
	//3带1
	if (CardDetail->nTypeCount>=MAX_FIT_TYPE) return FALSE;
	if (nCardCount!=4) return FALSE;                  
	int nCardLay[SK_LAYOUT_NUM];
	memset(nCardLay,0,sizeof(nCardLay));
	int nJokerCount=0;
	PreDealCards(nCardIDs,nCardLen,nCardLay,SK_LAYOUT_NUM,nJokerCount);

	int& index=CardDetail->nTypeCount;
	BOOL bnFind=FALSE;
	while(GetCardType_Three_1(nCardIDs,nCardLen,nCardLay,SK_LAYOUT_NUM,nJokerCount,CardDetail->uniteType[index]))
	{
		//迭代自身最大值
		bnFind=TRUE;
	}
	if (!bnFind) return FALSE;
	
	index++;
	return TRUE;
}

BOOL CGameTable::GetCardType_Three_1(int nCardIDs[],int nCardLen,int nCardLay[],int nLayLen,int nJokerCount,UNITE_TYPE& type)
{
	if (nCardLen<=0 || nLayLen<=0)
		return FALSE;
	
	int nMainIndex=-1;
	int nSecondIndex=-1;
	if (!GetDoubleCount(nCardLay,nLayLen,3,1,nJokerCount,type.nMainValue,nMainIndex,nSecondIndex))
		return FALSE;

	if (!m_bIsRemind&&nCardLay[nMainIndex]==3 && nJokerCount==1) //三张非癞子与1张癞子视为炸弹
	{
		return FALSE;
	}

	type.dwCardType=CARD_UNITE_TYPE_THREE_1;
	type.dwComPareType=COMPARE_UNITE_TYPE_THREE_1;
	type.nMainValue=SK_GetIndexPRIEx(nMainIndex,GetCurrentRank(), 0)*10000;
	if(!m_bIsRazzMode)
		type.nMainValue += SK_GetIndexPRIEx(nSecondIndex,GetCurrentRank(), 0);	//非癞子场带上副牌牌值

	type.nCardCount=4;
	if (m_bIsRazzMode && nJokerCount>0&&(!m_bIsRemind))
	{
		// 如果带牌是单王，则癞子只能是本身
		if (nSecondIndex != 14 && nSecondIndex != 15) {
			if (!CalcRazzValueInDoubleCount(nCardLay, nJokerCount, 3, 1, nMainIndex, nSecondIndex))
			{
				return FALSE;
			}
		}
	}
	
	XygInitChairCards(type.nCardIDs,MAX_CARDS_PER_CHAIR);
	
	int temp[MAX_CARDS_PER_CHAIR];
	memset(temp,0,sizeof(temp));
	memcpy(temp,nCardIDs,sizeof(int)*nCardLen);
	PutCardToArray(type.nCardIDs,nCardLen,temp,nCardLen,nMainIndex,3);
	PutCardToArray(type.nCardIDs,nCardLen,temp,nCardLen,nSecondIndex,1);
	return TRUE;
}

BOOL CGameTable::GetCardType_Three_Couple(int nCardIDs[],int nCardLen,int nCardLay[],int nLayLen,int nJokerCount,UNITE_TYPE& type)
{
	if (nCardLen<=0 || nLayLen<=0)
		return FALSE;

// 	if (m_bIsRazzMode && nJokerCount>0 && (nCardLay[14]!=0 || nCardLay[15]!=0))//癞子牌不能当大小王组成3张或对子
// 		return FALSE;

	int nMainIndex=-1;
	int nSecondIndex=-1;
	if (!GetDoubleCount(nCardLay,nLayLen,3,2,nJokerCount,type.nMainValue,nMainIndex,nSecondIndex))
		return FALSE;

	type.dwCardType=CARD_UNITE_TYPE_THREE_COUPLE;
	type.dwComPareType=COMPARE_UNITE_TYPE_THREE_COUPLE;
	type.nMainValue=SK_GetIndexPRIEx(nMainIndex,GetCurrentRank(), 0)*10000;
	if(!m_bIsRazzMode)
		type.nMainValue += SK_GetIndexPRIEx(nSecondIndex,GetCurrentRank(), 0);	//非癞子场带上副牌牌值

	type.nCardCount=5;

	if (m_bIsRazzMode && nJokerCount>0 && (!m_bIsRemind))
	{
		if (!CalcRazzValueInDoubleCount(nCardLay, nJokerCount, 3, 2, nMainIndex, nSecondIndex))
		{
			return FALSE;
		}
	}

	XygInitChairCards(type.nCardIDs,MAX_CARDS_PER_CHAIR);

	int temp[MAX_CARDS_PER_CHAIR];
	memset(temp,0,sizeof(temp));
	memcpy(temp,nCardIDs,sizeof(int)*nCardLen);
	PutCardToArray(type.nCardIDs,nCardLen,temp,nCardLen,nMainIndex,3);
	PutCardToArray(type.nCardIDs,nCardLen,temp,nCardLen,nSecondIndex,2);
	return TRUE;
}

BOOL  CGameTable::CaclCardType_ABT_Single(int nCardIDs[],int nCardLen,int nCardCount,CARD_UNITE* CardDetail)
{
	if (nCardLen<=0)
		return FALSE;

	if (CardDetail->nTypeCount>=MAX_FIT_TYPE) return FALSE;
	if (nCardCount<5) return FALSE;          

	int nCardLay[SK_LAYOUT_NUM];
	memset(nCardLay,0,sizeof(nCardLay));
	int nJokerCount=0;
	PreDealCards(nCardIDs,nCardLen,nCardLay,SK_LAYOUT_NUM,nJokerCount);
	int& index=CardDetail->nTypeCount;

	memset(&CardDetail->uniteType[index],0,sizeof(UNITE_TYPE));

	BOOL bnFind=FALSE;
	while(GetCardType_ABT_Single(nCardIDs,nCardLen,nCardLay,SK_LAYOUT_NUM,nJokerCount,CardDetail->uniteType[index],nCardCount))
	{
		//迭代自身最大值
		bnFind=TRUE;
	}
	if (!bnFind) return FALSE;
	if (CardDetail->uniteType[index].nCardCount!=nCardCount)	//此时牌数需要吻合，才能是此类型
		return FALSE;

	index++;
	return TRUE;
}

BOOL  CGameTable::GetCardType_ABT_Single(int nCardIDs[],int nCardLen,int nCardLay[],int nLayLen,int nJokerCount,UNITE_TYPE& type,int nMaxCount)
{
	if (nCardLen<=0 || nLayLen<=0)
		return FALSE;

	if (nMaxCount<5)
		return FALSE;

	int nStartIndex=-1;
	int nValue=0;
	int nShunCount = 5;		//至少5张是顺子
	BOOL  bFixed = FALSE;
	if (type.dwCardType==CARD_UNITE_TYPE_ABT_SINGLE)
	{
		if (type.nCardCount<5)
			return FALSE;
		nShunCount = type.nCardCount;
		nValue=type.nMainValue;
		bFixed = TRUE;	//找固定长度
	}

	int nMinValue=-1;
	int nLength = 12;
	if (bFixed)	//固定长度	//不固定，则尽可能延续到A
		nLength = nShunCount;
	int selectedCount = GetCardCount(nCardIDs,SK_CHAIR_CARD);
	int i;
	for(i=2;i<15-nShunCount;i++)
	{
		int abt_count=0;
		int nJokerUsed = 0;
		for(int j=0;j<nLength;j++)
		{
			if (i+j>13 || (nCardLay[i+j]==0 && nJokerUsed>=nJokerCount))
				break;

			if(nCardLay[i+j]==0 && nJokerUsed<nJokerCount)
			{
				nJokerUsed++;
				continue;
			}
			abt_count++;
		}

		///////////////////////////////癞子///////////////////////
		int nThisValue = SK_GetIndexPRIEx(i,-1, 0)+100;
		if (((bFixed && (abt_count+nJokerUsed==nShunCount)) || (!bFixed && (abt_count+nJokerUsed)>=5))
			&& nThisValue>nValue)
		{
			if (m_bIsRazzMode && m_bIsRemind)	//提示从小到大取值
			{
				if (nStartIndex==-1 || nThisValue<nMinValue)
				{
					nStartIndex=i;
					nMinValue=SK_GetIndexPRIEx(i,GetCurrentRank(), 0)+100;
					nMaxCount=abt_count+nJokerUsed;
				}
			}
			else if ( (nMinValue==-1 || (nThisValue<nMinValue && !m_bIsRazzMode))
				|| (m_bIsRazzMode && nThisValue>nMinValue))  /*********癞子场出顺子取最大顺子**********/
			{
				if (m_bIsRazzMode && !bFixed && (abt_count+nJokerUsed)!=selectedCount) //首家出带癞子的顺子中有重复的牌
				{
					continue;
				}
				nStartIndex=i;
				nMinValue=SK_GetIndexPRIEx(i,GetCurrentRank(), 0)+100;
				nMaxCount=abt_count+nJokerCount;
			}
		}
	}

	if (nStartIndex==-1)
		return FALSE;

	type.dwCardType=CARD_UNITE_TYPE_ABT_SINGLE;
	type.dwComPareType= COMPARE_UNITE_TYPE_ABT_SINGLE; 
	type.nMainValue=nMinValue;//顺子中级牌还原
	type.nCardCount=nMaxCount;

	if (m_bIsRazzMode && nJokerCount>0 && !m_bIsRemind)
	{
		if (!CalcRazzValueInAbtSingle(nCardLay, nJokerCount, nStartIndex, type.nCardCount))
		{
			return FALSE;
		}
	}

	XygInitChairCards(type.nCardIDs,MAX_CARDS_PER_CHAIR);

	int temp[MAX_CARDS_PER_CHAIR];
	memset(temp,0,sizeof(temp));
	memcpy(temp,nCardIDs,sizeof(int)*nCardLen);
	
	{
		for(i=nStartIndex;i<nStartIndex+nMaxCount;i++)
		{
			PutCardToArray(type.nCardIDs,nCardLen,temp,nCardLen,i,1);
		}
	}
	

	return TRUE;
}


BOOL  CGameTable::CaclCardType_ABT_Couple(int nCardIDs[],int nCardLen,int nCardCount,CARD_UNITE* CardDetail)
{
	if (nCardLen<=0)
		return FALSE;

	if (CardDetail->nTypeCount>=MAX_FIT_TYPE) return FALSE;
	if (nCardCount<6) return FALSE;           //两队起连
	if (nCardCount%2!=0) return FALSE;

	int nCardLay[SK_LAYOUT_NUM];
	memset(nCardLay,0,sizeof(nCardLay));
	int nJokerCount=0;
	PreDealCards(nCardIDs,nCardLen,nCardLay,SK_LAYOUT_NUM,nJokerCount);
	int& index=CardDetail->nTypeCount;

	
	memset(&CardDetail->uniteType[index],0,sizeof(UNITE_TYPE));
	if (!GetCardType_ABT_Couple(nCardIDs,nCardLen,nCardLay,SK_LAYOUT_NUM,nJokerCount,CardDetail->uniteType[index],nCardCount/2))
		return FALSE;
    index++;
	return TRUE;
}

BOOL CGameTable::GetCardType_ABT_Couple(int nCardIDs[],int nCardLen,int nCardLay[], int nLayLen,int nJokerCount,UNITE_TYPE& type,int nMaxPair)
{
	if (nCardLen<=0 || nLayLen<=0)
		return FALSE;

	if (nMaxPair<3)
		return FALSE;

	int nStartIndex=-1;
	int nValue=0;
	if (type.dwCardType==CARD_UNITE_TYPE_ABT_COUPLE)
	{
		if (type.nCardCount!=nMaxPair*2)//张数不同不能比较
			return FALSE;

		nValue=type.nMainValue;
	}

	int nMinValue=-1;
	int nBaseValue=1000*nMaxPair;
	int i;
	for(i=2;i<=14-nMaxPair;i++)//3到
	{
		int Joker_Need=0;
		for(int j=0;j<nMaxPair;j++)//最多3连对
		{
			if (nCardLay[i+j]<2)
				Joker_Need+=2-nCardLay[i+j];
		}
		//////////////////////////////////////////癞子//////////////////////////////
		int nThisValue = SK_GetIndexPRIEx(i,GetCurrentRank(), 0)+nBaseValue;
		if (Joker_Need<=nJokerCount && nThisValue>nValue)
		{
			if (m_bIsRazzMode && m_bIsRemind)
			{
				if (nMinValue==-1 || nThisValue<nMinValue)	//提示从小到大取值
				{
					nStartIndex=i;
					nMinValue=SK_GetIndexPRIEx(i,GetCurrentRank(), 0)+nBaseValue;
				}
			}
			else if ((nMinValue==-1 || (nThisValue<nMinValue && !m_bIsRazzMode))
				|| (m_bIsRazzMode&&nThisValue>nMinValue))//癞子场选最大连对
			{
				nStartIndex=i;
				nMinValue=SK_GetIndexPRIEx(i,GetCurrentRank(), 0)+nBaseValue;
			}
		}
	}

	if (nStartIndex==-1) return FALSE;

	type.dwCardType=CARD_UNITE_TYPE_ABT_COUPLE;
	type.dwComPareType= COMPARE_UNITE_TYPE_ABT_COUPLE; 
	type.nMainValue=nMinValue;//顺子中级牌还原
	type.nCardCount=nMaxPair*2;

	if (m_bIsRazzMode && nJokerCount>0 && !m_bIsRemind)
	{
		if (!CalcRazzValueInAbtCouple(nCardLay, nJokerCount, nStartIndex, nMaxPair))
		{
			return FALSE;
		}
	}

	XygInitChairCards(type.nCardIDs,MAX_CARDS_PER_CHAIR);

	int temp[MAX_CARDS_PER_CHAIR];
	memset(temp,0,sizeof(temp));
	memcpy(temp,nCardIDs,sizeof(int)*nCardLen);


    for(i=0;i<nMaxPair;i++)
		PutCardToArray(type.nCardIDs,nCardLen,temp,nCardLen,nStartIndex+i,2);

	return TRUE;
}

BOOL CGameTable::GetCardType_ABT_Three(int nCardIDs[],int nCardLen,int nCardLay[], int nLayLen,int nJokerCount,UNITE_TYPE& type,int nMaxPair)
{
	if (nCardLen<=0 || nLayLen<=0)
		return FALSE;

	
	if (nMaxPair<2)
		return FALSE;

	if (m_nCardTypeLimit && nJokerCount == 4) {
		return FALSE;
	}

	int nStartIndex=-1;
	int nValue=0;
	if (type.dwCardType==CARD_UNITE_TYPE_ABT_THREE)
	{
		if (type.nCardCount!=nMaxPair*3)//张数不同不能比较
			return FALSE;

		nValue=type.nMainValue;
	}
	int nMinValue=-1;
	int nBaseValue=1000*nMaxPair;
	int i;
	for(i=2;i<=14-nMaxPair;i++)//3到A
	{
		int Joker_Need=0;
		for(int j=0;j<nMaxPair;j++)//最多nMaxPair连
		{
			if (nCardLay[i+j]<3)
				Joker_Need+=3-nCardLay[i+j];
		}

		int nThisValue = SK_GetIndexPRIEx(i,-1, 0)+nBaseValue;
		if (Joker_Need<=nJokerCount && nThisValue>nValue)
		{
			if (m_bIsRazzMode && m_bIsRemind)
			{
				if (nStartIndex==-1 || nThisValue<nMinValue)	//提示从小到大取值
				{
					nStartIndex=i;
					nMinValue=nThisValue;
				}
			}
			else if ((nMinValue==-1||nThisValue<nMinValue&&!m_bIsRazzMode)
				||(m_bIsRazzMode&&nThisValue>nMinValue))//癞子场取最大飞机
			{
				nStartIndex=i;
				nMinValue=nThisValue;
			}
		}
	}
	
	if (nStartIndex==-1) return FALSE;

	type.dwCardType=CARD_UNITE_TYPE_ABT_THREE;
	type.dwComPareType= COMPARE_UNITE_TYPE_ABT_THREE; 
	type.nMainValue=nMinValue;//顺子中级牌还原
	type.nCardCount=nMaxPair*3;

	if (m_bIsRazzMode && nJokerCount>0 && !m_bIsRemind)
	{
		if (!CalcRazzValueInAbtThree(nCardLay, nJokerCount, 0, 0, nStartIndex, nMaxPair))
		{
			return FALSE;
		}
	}
	
	XygInitChairCards(type.nCardIDs,MAX_CARDS_PER_CHAIR);
	int temp[MAX_CARDS_PER_CHAIR];
	memset(temp,0,sizeof(temp));
	memcpy(temp,nCardIDs,sizeof(int)*nCardLen);
	
	for(i=0;i<nMaxPair;i++)
	 	PutCardToArray(type.nCardIDs,nCardLen,temp,nCardLen,nStartIndex+i,3);
	
	return TRUE;
}

BOOL CGameTable::CaclCardType_ABT_Three_Couple(int nCardIDs[],int nCardLen,int nCardCount,CARD_UNITE* CardDetail)
{
	if (nCardLen<=0)
		return FALSE;

	if (CardDetail->nTypeCount>=MAX_FIT_TYPE) return FALSE;
	if (nCardCount<10) return FALSE;           
	if (nCardCount%5!=0) return FALSE;
	
	int nCardLay[SK_LAYOUT_NUM];
	memset(nCardLay,0,sizeof(nCardLay));
	int nJokerCount=0;
	PreDealCards(nCardIDs,nCardLen,nCardLay,SK_LAYOUT_NUM,nJokerCount);
	int& index=CardDetail->nTypeCount;
	
	memset(&CardDetail->uniteType[index],0,sizeof(UNITE_TYPE));
	if (!GetCardType_ABT_Three_Couple(nCardIDs,nCardLen,nCardLay,SK_LAYOUT_NUM,nJokerCount,CardDetail->uniteType[index],nCardCount/5))
		return FALSE;

	index++;
	return TRUE;
}

BOOL   CGameTable::GetCardType_ABT_Three_Couple(int nCardIDs[],int nCardLen,int nCardLay[], int nLayLen,int nJokerCount,UNITE_TYPE& type,int nMaxPair)
{
	if (nCardLen<=0 || nLayLen<=0)
		return FALSE;

	if (nMaxPair<2)
		return FALSE;
	
	int nStartIndex=-1;
	int nValue=0;
	if (type.dwCardType==CARD_UNITE_TYPE_ABT_THREE_COUPLE)
	{
		if (type.nCardCount!=nMaxPair*5)//张数不同不能比较
			return FALSE;
		
		nValue=type.nMainValue;
	}

	if (m_nCardTypeLimit) {
		// 飞机不能组成炸弹
		auto bombIndex = GetSameCountEx(nCardLay, nLayLen, 4, 0, 0);
		auto singleIndex = GetSameCountEx(nCardLay, nLayLen, 1, 0, 0);
		if (bombIndex != -1 || nJokerCount == 4 || (singleIndex == 0 && nJokerCount == 1))
		{
			return FALSE;
		}
	}

	//注意支持癞子
	int nTempLay[SK_LAYOUT_NUM];
	int nMinValue=-1;
	int nThisValue = 0;
	int nBaseValue=10000*nMaxPair;
	int nCoupleLay[SK_LAYOUT_NUM];
	int nCoupleLayTmp[SK_LAYOUT_NUM];
	int nCoupleLayTmpTmp[SK_LAYOUT_NUM];
	int Joker_Need=0;
	memset(nCoupleLay,0,sizeof(nCoupleLay));


	int nRemindValue = -1;
	int joker_UsedAbt = 0;
	int joker_UsedCouple = 0;
	int nCoupleIdx[MAX_ABTTHREECOUPLE_COUNT];//飞机带的对子index
	memset(nCoupleIdx, 0, sizeof(int)*MAX_ABTTHREECOUPLE_COUNT);
	int i;
	for(i=2;i<=14-nMaxPair;i++)//3到A
	{
		memset(nTempLay, 0, sizeof(nTempLay));
		memcpy(nTempLay,nCardLay,sizeof(nTempLay));
		Joker_Need=0;
		int Joker_NeedInAbt = 0;
		int Joker_NeedInCouple = 0;
		int nTmpIndex[MAX_ABTTHREECOUPLE_COUNT];//临时存储飞机带的对子index
		memset(nTmpIndex, 0, sizeof(int)*MAX_ABTTHREECOUPLE_COUNT);
		for(int j=0;j<nMaxPair;j++)//最多nMaxPair连对
		{
			if (nCardLay[i+j]<3)
			{
				Joker_Need+=3-nCardLay[i+j];
				nTempLay[i+j]=0;
			}
			nTempLay[i+j] = 0;
		}

		if (Joker_Need>nJokerCount) continue;
		Joker_NeedInAbt = Joker_Need;

		int nTempStartIndex=i;

		int nCoupleCount = 0;
		memset(nCoupleLayTmp,0,sizeof(nCoupleLayTmp));

		int m;
		if (m_bIsRazzMode)
		{
			for (m=1; m<14; m++) //寻找最优的不拆牌的组合
			{
				if (nTempLay[m] == 2)
				{
					nTempLay[m] -= 2;
					nCoupleCount++;
					nCoupleLayTmp[m]++;
				}
				if (nCoupleCount >= nMaxPair)
				{
					break;
				}
			}

			if (nCoupleCount < nMaxPair) //没有找到最优的组合，需要拆非炸弹牌的组合
			{
				for (m=2; m<14; m++)	
				{
					if (nCoupleCount >= nMaxPair) break;
					if (nTempLay[m]==3)
					{
						nTempLay[m]-=2;
						nCoupleCount++;
						nCoupleLayTmp[m]++;
					}
				}
				if (m==14 && nCoupleCount<nMaxPair)
				{
					if (nTempLay[1]==3)
					{
						nTempLay[1]-=2;
						nCoupleCount++;
						nCoupleLayTmp[1]++;
					}
				}
			}

			if (nCoupleCount < nMaxPair) //没有找到最优的组合，拆炸弹
			{
				for (m=2;m<14;m++)
				{
					while (nCoupleCount < nMaxPair && nTempLay[m] >= 2)
					{
						nTempLay[m]-=2;
						nCoupleCount++;
						nCoupleLayTmp[m]++;
					}
				}
				if (m==14)
				{
					while (nCoupleCount < nMaxPair && nTempLay[1] >= 2)
					{
						nTempLay[1]-=2;
						nCoupleCount++;
						nCoupleLayTmp[1]++;
					}
				}
			}
		}
		else
		{
			for (m=1;m<14;m++)	//找出非癞子组成的对子
			{
				while (nCoupleCount < nMaxPair && nTempLay[m] >= 2)
				{
					nTempLay[m] -= 2;
					nCoupleCount++;
					nCoupleLayTmp[m]++;
				}
			}
		}
	
		if(m_bIsRazzMode && nJokerCount-Joker_Need>0 && nCoupleCount<nMaxPair)
		{
			for (m=2;m<14;m++)	//癞子与非癞子搭配成对子
			{
				if (nCoupleCount >= nMaxPair) break;

				auto limit = !m_nCardTypeLimit ? true : nCoupleLayTmp[m] == 0;
				if(nTempLay[m]==1 && Joker_NeedInCouple<nJokerCount-Joker_NeedInAbt && limit)
				{
					Joker_Need += 1;
					nTempLay[m] = 0;
					nCoupleCount++;
					nCoupleLayTmp[m]++;
					Joker_NeedInCouple++;
				}
			}
			if(nCoupleCount<nMaxPair && nTempLay[1]==1 && Joker_NeedInCouple<nJokerCount-Joker_NeedInAbt)//判断2和癞子组成对子
			{
				Joker_Need += 1;
				nTempLay[1] = 0;
				nCoupleCount++;
				nCoupleLayTmp[1]++;
				Joker_NeedInCouple++;
			}
			if (nCoupleCount < nMaxPair)
			{
				for (m = 1; m < 14; m++)
				{
					while (nTempLay[m] >= 2)
					{
						nTempLay[m] -= 2;
						nCoupleCount++;
						nCoupleLayTmp[m]++;
					}
					auto limit = !m_nCardTypeLimit ? true : nCoupleLayTmp[m] == 0;
					if (nCoupleCount < nMaxPair && nTempLay[m] == 1 && Joker_NeedInCouple < nJokerCount - Joker_NeedInAbt && limit)
					{
						Joker_Need++;
						Joker_NeedInCouple++;
						nTempLay[m] = 0;
						nCoupleCount++;
						nCoupleLayTmp[m]++;
					}
				}
			}
		}

		while(m_bIsRazzMode && nJokerCount>=Joker_Need+2 && nCoupleCount<nMaxPair)//还剩2张癞子作为对子
		{
		    int razzCardIndex = SK_GetCardIndexEx(m_nRazzCardValue, 0);
			nCoupleCount++;
			nCoupleLayTmp[razzCardIndex]++;
			Joker_Need += 2;
			Joker_NeedInCouple +=2;
		}

		/***********************************************************************
		* 当一共有nCoupleCount个对子，需要从中遍历nMaxPair个对子的所有组合
		* 利用位运算，遍历0～2的nCoupleCount次方中的数，如果该数的二进制有
		* nMaxPair个1，表示一种遍历情况
		************************************************************************/
		if (nCoupleCount>=nMaxPair)
		{
			//遍历所有对子组合,在若干对子中，取出nMaxPair个对子
			int nTotal = 1<<nCoupleCount;
			for (int j=3;j<nTotal;j++)
			{
				//有nMaxPair个对子
				if (GetBit1Count(j)==nMaxPair)
				{
					memset(nCoupleLayTmpTmp,0,sizeof(nCoupleLayTmpTmp));
					for (int k=0;k<nCoupleCount;k++)
					{
						//Lay中第k+1个数被随机组成nMaxPair个对子中的一对
						if ((j&(1<<k))!=0)
						{
							int nIndex = GetIndexByIndex(nCoupleLayTmp,SK_LAYOUT_NUM,k+1);
							if (nIndex>=0)
							{
								nCoupleLayTmpTmp[nIndex]++;
							}
						}
					}

					nThisValue = SK_GetIndexPRIEx(i,GetCurrentRank(), 0)*100 + nBaseValue;
					if(!m_bIsRazzMode)
						nThisValue += GetLayPri(nCoupleLayTmpTmp,SK_LAYOUT_NUM);	//非癞子场带上副牌牌值

					if(nThisValue > nValue)
					{
						if (m_bIsRazzMode && m_bIsRemind)	//提示从小到大取值
						{
							int nThisRemindValue = SK_GetIndexPRIEx(i,GetCurrentRank(), 0)*100+GetLayPri(nCoupleLayTmpTmp,SK_LAYOUT_NUM)+nBaseValue;	//提示带上翅膀值
							if (nRemindValue==-1 || nThisRemindValue<nRemindValue)
							{
								nRemindValue = nThisRemindValue;
								nStartIndex = i;
								nMinValue = nThisValue;
								memcpy(nCoupleLay,nCoupleLayTmpTmp,sizeof(nCoupleLay));
							}
						}
						else if ((nMinValue==-1||nThisValue<nMinValue&&!m_bIsRazzMode)
							||(m_bIsRazzMode&&nThisValue>nMinValue))
						{
							nStartIndex = i;
							nMinValue = nThisValue;
							memcpy(nCoupleLay,nCoupleLayTmpTmp,sizeof(nCoupleLay));
							joker_UsedCouple = Joker_NeedInCouple;
							joker_UsedAbt = Joker_NeedInAbt;
							memcpy(nCoupleIdx, nTmpIndex, sizeof(int)*MAX_ABTTHREECOUPLE_COUNT);
						}
					}
				}
			}
		}
	}

	if (nStartIndex==-1) return FALSE;

	type.dwCardType=CARD_UNITE_TYPE_ABT_THREE_COUPLE;
	type.dwComPareType= COMPARE_UNITE_TYPE_ABT_THREE_COUPLE; 
	type.nMainValue=nMinValue;
	type.nCardCount=nMaxPair*5;
	
	XygInitChairCards(type.nCardIDs,MAX_CARDS_PER_CHAIR);
	int temp[MAX_CARDS_PER_CHAIR];
	memset(temp,0,sizeof(temp));
	memcpy(temp,nCardIDs,sizeof(int)*nCardLen);
	
	for(i=0;i<nMaxPair;i++)
	{
		PutCardToArray(type.nCardIDs,nCardLen,temp,nCardLen,nStartIndex+i,3);
	}

	for (i = 0; i<nMaxPair; i++)
	{
		int nIndex = GetIndexByIndex(nCoupleLay, SK_LAYOUT_NUM, i + 1);
		if (nIndex >= 0)
		{
			PutCardToArray(type.nCardIDs, nCardLen, temp, nCardLen, nIndex, 2);
		}
	}
	
	return TRUE;
}

int CGameTable::GetIndexByIndex(int nCardLay[], int nLayLen, int nIndex)
{
	if (nIndex<1) 
		return -1;

	for (int i=0;i<nLayLen;i++)
	{
		int nCount = nCardLay[i];		//支持重复取牌，比如nCardLay[3]=2，允许取两次索引为3的牌
		while (nCount-- > 0)
		{
			if (--nIndex <= 0)
				return i;
		}
	}
	return -1;
}

int CGameTable::GetBit1Count(unsigned int bit)
{
	int nCount = 0;
	while (bit>0)
	{
		nCount++;
		bit&=(bit-1);
	}
	return nCount;
}

int CGameTable::GetLayPri(int nCardLay[], int nLayLen)
{
	int nPri=0;
	for (int i=0;i<nLayLen;i++)
	{
		if (nCardLay[i]>0)
		{
			nPri += SK_GetIndexPRIEx(i,GetCurrentRank(), 0);
		}
	}

	return nPri;
}


int CGameTable::GetLayPriEx(int nCardLay[], int nLayLen)
{
	int nPri=0;
	for (int i=0;i<nLayLen;i++)
	{
		if (nCardLay[i]>=0)
		{
			nPri += SK_GetIndexPRIEx(nCardLay[i],GetCurrentRank(), 0);
		}
	}
	
	return nPri;
}

BOOL    CGameTable::CaclCardType_ABT_Three_1(int nCardIDs[],int nCardLen,int nCardCount,CARD_UNITE* CardDetail)
{
	if (nCardLen<=0)
		return FALSE;

	if (CardDetail->nTypeCount>=MAX_FIT_TYPE) return FALSE;
	if (nCardCount<8) return FALSE;           
	if (nCardCount%4!=0) return FALSE;
	
	int nCardLay[SK_LAYOUT_NUM];
	memset(nCardLay,0,sizeof(nCardLay));
	int nJokerCount=0;
	PreDealCards(nCardIDs,nCardLen,nCardLay,SK_LAYOUT_NUM,nJokerCount);
	int& index=CardDetail->nTypeCount;
	
	memset(&CardDetail->uniteType[index],0,sizeof(UNITE_TYPE));
	if (!GetCardType_ABT_Three_1(nCardIDs,nCardLen,nCardLay,SK_LAYOUT_NUM,nJokerCount,CardDetail->uniteType[index],nCardCount/4))
		return FALSE;

	index++;
	return TRUE;
}

BOOL CGameTable::GetCardType_ABT_Three_1(int nCardIDs[], int nCardLen, int nCardLay[], int nLayLen,
										 int nJokerCount, UNITE_TYPE& type, int nMaxPair)
{
	if (nCardLen<=0 || nLayLen<=0)
		return FALSE;

	if (nMaxPair<2)
		return FALSE;
	
	int nValue = 0;
	int nStartIndex = -1;
	
	if (type.dwCardType==CARD_UNITE_TYPE_ABT_THREE_1)
	{
		if (type.nCardCount!=nMaxPair*4)//张数不同不能比较
			return FALSE;
		
		nValue=type.nMainValue;
	}

	if (m_nCardTypeLimit) {
		// 飞机不能组成炸弹
		auto bombIndex = GetSameCountEx(nCardLay,nLayLen,4,0,0);
		if (bombIndex != -1 || nJokerCount == 4)
		{
			return FALSE;
		}
	}

	//注意不支持财神
	
	int nMinValue=-1;
	int nThisValue = 0;
	int nBaseValue=10000*nMaxPair;
	
	int nTempLay[SK_LAYOUT_NUM];
	memset(nTempLay,-1,sizeof(nTempLay));
	int nSingleLay[CHAIR_CARDS];
	memset(nSingleLay,-1,sizeof(nSingleLay));
	int nSingleLayTmp[CHAIR_CARDS];
	memset(nSingleLayTmp,-1,sizeof(nSingleLayTmp));
	int nSingleLayTmpTmp[CHAIR_CARDS];
	memset(nSingleLayTmpTmp,-1,sizeof(nSingleLayTmpTmp));

	int Joker_UsedInAbt = 0;
	int Joker_UsedInSingle = 0;
	int nRemindValue = -1;
	int i;
	for(i=2; i<=14-nMaxPair; i++)//3到K
	{
		memcpy(nTempLay,nCardLay,sizeof(nTempLay));
		int Joker_NeedInAbt=0;
		int Joker_NeedInSingle = 0;
		for(int j=0;j<nMaxPair;j++)//最多nMaxPair连对
		{
			if (nCardLay[i+j]<3)
			{
				Joker_NeedInAbt+=3-nCardLay[i+j];
			}
			nTempLay[i + j] -= 3;
		}
		
		if (Joker_NeedInAbt>nJokerCount) 
			continue;
		
		int nSingleCount = 0;
		memset(nSingleLayTmp,-1,sizeof(nSingleLayTmp));

		if (m_bIsRazzMode)
		{
			for (int t=1; t<nLayLen; t++)  //寻找不拆牌的最优组合
			{
				if (nTempLay[t]==1)
				{
					if ((t==14 && nTempLay[15]) || (t==15 && nTempLay[14])) continue;
					nSingleLayTmp[nSingleCount++] = t;
				}
			}

			if (nMaxPair>nSingleCount)  //没有找到最优组合，拆非炸弹的牌
			{
				nSingleCount = 0;
	        	memset(nSingleLayTmp,-1,sizeof(nSingleLayTmp));
				for (int m=1; m<nLayLen; m++)
				{
					if (nTempLay[m]==4 || (m==14 && nTempLay[14] && nTempLay[15]) || (m==15 && nTempLay[14] && nTempLay[15])) continue;
					int nCount=nTempLay[m];
					if (nCount>=1)
					{
						while (nCount>=1)
						{
							nSingleLayTmp[nSingleCount++]=m;
							nCount -= 1;
						}
					}		
				}
			}

			if (nMaxPair>nSingleCount)  //没有找到最优组合，可拆炸弹牌
			{
				nSingleCount = 0;
				memset(nSingleLayTmp,-1,sizeof(nSingleLayTmp));
				for (int m=1; m<nLayLen; m++)
				{
					if ((m==14 && nTempLay[15] && nTempLay[14]) || (m==15 && nTempLay[14] && nTempLay[15])) continue;
					while (nTempLay[m]>=1)
					{
						nSingleLayTmp[nSingleCount++]=m;
						nTempLay[m] -= 1;
					}
				}
			}
		}
		else
		{
			for (int m=0; m<nLayLen; m++)
			{
				while (nTempLay[m]>=1)
				{
					nSingleLayTmp[nSingleCount++]=m;
					nTempLay[m] -= 1;
				}
			}
		}

	

		if(nMaxPair-nSingleCount==3 && m_bIsRazzMode && nJokerCount>=Joker_NeedInAbt+3)//3张癞子作为单张
		{
			int singleIndex = SK_GetCardIndexEx(m_nRazzCardValue, 0);
			nSingleLayTmp[nSingleCount++] = singleIndex;
			nSingleLayTmp[nSingleCount++] = singleIndex;
			nSingleLayTmp[nSingleCount++] = singleIndex;
			Joker_NeedInSingle += 3;
		}
		else if(nMaxPair-nSingleCount==2 && m_bIsRazzMode && nJokerCount>=Joker_NeedInAbt+2)//2张癞子作为单张
		{
			int singleIndex = SK_GetCardIndexEx(m_nRazzCardValue, 0);
			nSingleLayTmp[nSingleCount++] = singleIndex;
			nSingleLayTmp[nSingleCount++] = singleIndex;
			Joker_NeedInSingle += 2;
		}
		else if(nMaxPair-nSingleCount==1 && m_bIsRazzMode && nJokerCount>=Joker_NeedInAbt+1)//1张癞子作为单张
		{
			int singleIndex = SK_GetCardIndexEx(m_nRazzCardValue, 0);
			nSingleLayTmp[nSingleCount++] = singleIndex;
			Joker_NeedInSingle += 1;
		}
		else if(nMaxPair-nSingleCount==4 && m_bIsRazzMode && nJokerCount==Joker_NeedInAbt+4)//癞子都作为单张
		{
			int singleIndex = SK_GetCardIndexEx(m_nRazzCardValue, 0);
			nSingleLayTmp[nSingleCount++] = singleIndex;
			nSingleLayTmp[nSingleCount++] = singleIndex;
			nSingleLayTmp[nSingleCount++] = singleIndex;
			nSingleLayTmp[nSingleCount++] = singleIndex;
			Joker_NeedInSingle += 4;
		}

		/***********************************************************************
		* 当一共有nSingleCount个单张，需要从中遍历nMaxPair个单张的所有组合
		* 利用位运算，遍历0～2的nSingleCount次方中的数，如果该数的二进制有
		* nMaxPair个1，表示一种遍历情况
		************************************************************************/
		if (nSingleCount>=nMaxPair)
		{
			//遍历所有对子组合,在若干对子中，取出nMaxPair个单张
			int nTotal = 1<<nSingleCount;
			for (int j=3;j<nTotal;j++)
			{
				//有nMaxPair个对子
				if (GetBit1Count(j)==nMaxPair)
				{
					memset(nSingleLayTmpTmp,-1,sizeof(nSingleLayTmpTmp));
					for (int k=0;k<nSingleCount;k++)
					{	//nSingleLayTmp中第k个数被随机组成nMaxPair个单牌中的一个
						if ((j&(1<<k))!=0)
						{
							nSingleLayTmpTmp[k] = nSingleLayTmp[k];
						}
					}

					nThisValue =  SK_GetIndexPRIEx(i,GetCurrentRank(), 0)*100 + nBaseValue;
					if(!m_bIsRazzMode)
						nThisValue += GetLayPriEx(nSingleLayTmpTmp,SK_LAYOUT_NUM);	//非癞子场带上副牌牌值

					if(nThisValue>nValue)
					{
						if (m_bIsRazzMode && m_bIsRemind)	//提示从小到大取值
						{
							int nThisRemindValue = SK_GetIndexPRIEx(i,GetCurrentRank(), 0)*100 + GetLayPriEx(nSingleLayTmpTmp,SK_LAYOUT_NUM) + nBaseValue;	//提示带上翅膀值
							if (nRemindValue==-1 || nThisRemindValue<nRemindValue)
							{
								nRemindValue = nThisRemindValue;
								nStartIndex=i;
								nMinValue=nThisValue;
								memcpy(nSingleLay,nSingleLayTmpTmp,sizeof(nSingleLay));
							}
						}
						else if ((nMinValue==-1||nThisValue<nMinValue&&!m_bIsRazzMode)
							||(m_bIsRazzMode&&nThisValue>nMinValue)) //癞子场取最大
						{
							nStartIndex=i;
							nMinValue=nThisValue;
							memcpy(nSingleLay,nSingleLayTmpTmp,sizeof(nSingleLay));
							Joker_UsedInAbt = Joker_NeedInAbt;
							Joker_UsedInSingle = Joker_NeedInSingle;
						}
					}
				}
			}
		}
	}

	if (nStartIndex==-1) 
		return FALSE;

	type.dwCardType=CARD_UNITE_TYPE_ABT_THREE_1;
	type.dwComPareType= COMPARE_UNITE_TYPE_ABT_THREE_1; 
	type.nMainValue=nMinValue;
	type.nCardCount=nMaxPair*4;

	if (m_bIsRazzMode && nJokerCount>=Joker_UsedInAbt && (!m_bIsRemind)) //癞子做飞机，也可做翅膀
	{
		if (!CalcRazzValueInAbtThree(nCardLay, Joker_UsedInAbt, 0, Joker_UsedInSingle, nStartIndex, nMaxPair))
		{
			return FALSE;
		}
	}
	
	XygInitChairCards(type.nCardIDs,MAX_CARDS_PER_CHAIR);
	int temp[MAX_CARDS_PER_CHAIR];
	memset(temp,0,sizeof(temp));
	memcpy(temp,nCardIDs,sizeof(int)*nCardLen);
	
	for(i=0;i<nMaxPair;i++)
	{
		PutCardToArray(type.nCardIDs,nCardLen,temp,nCardLen,nStartIndex+i,3);
	}

	for (i=0;i<CHAIR_CARDS;i++)
	{
		if (nSingleLay[i]>=0)
		{
			PutCardToArray(type.nCardIDs,nCardLen,temp,nCardLen,nSingleLay[i],1);
		}
	}
	
	return TRUE;
}

BOOL    CGameTable::CalcCardType_Four_2(int nCardIDs[],int nCardLen,int nCardCount,CARD_UNITE* CardDetail)
{
	if (nCardLen<=0)
		return FALSE;

	if (CardDetail->nTypeCount>=MAX_FIT_TYPE) return FALSE;
	if (nCardCount!=6) return FALSE;           
	
	int nCardLay[SK_LAYOUT_NUM];
	memset(nCardLay,0,sizeof(nCardLay));
	int nJokerCount=0;
	PreDealCards(nCardIDs,nCardLen,nCardLay,SK_LAYOUT_NUM,nJokerCount);
	int& index=CardDetail->nTypeCount;
	
	memset(&CardDetail->uniteType[index],0,sizeof(UNITE_TYPE));
	if (!GetCardType_Four_2(nCardIDs,nCardLen,nCardLay,SK_LAYOUT_NUM,nJokerCount,CardDetail->uniteType[index]))
		return FALSE;
	if (CardDetail->uniteType[index].nCardCount!=nCardCount)	//此时牌数需要吻合，才能是此类型
		return FALSE;

	index++;
	return TRUE;
}

BOOL   CGameTable::GetCardType_Four_2(int nCardIDs[],int nCardLen,int nCardLay[], int nLayLen,int nJokerCount,UNITE_TYPE& type)
{
	if (nCardLen<=0 || nLayLen<=0)
		return FALSE;

	int nMaxPair=2;	//需要2个单张
	
	int nStartIndex=-1;
	int nSingleIndex1 = 0;
	int nSingleIndex2 = 0;
	int nValue=0;
	if (type.dwCardType==CARD_UNITE_TYPE_FOUR_2)
	{
		if (type.nCardCount!=6)//张数不同不能比较
			return FALSE;
		
		nValue=type.nMainValue;
	}

	//注意支持癞子
	int nTempLay[SK_LAYOUT_NUM];
	int nMinValue=-1;
	int nThisValue = 0;
	int nBaseValue=100000;
	int Joker_UsedInFour = 0;
	int Joker_UsedInSingle = 0;
	int nJokerIndex = 0;
	int nSingleLay[CHAIR_CARDS];
	int nSingleLayTmp[CHAIR_CARDS];
	int nSingleLayTmpTmp[CHAIR_CARDS];
	memset(nSingleLay,-1,sizeof(nSingleLay));
	int nRemindValue = -1;

	int i;
	for(i=1;i<14;i++)//3到2
	{
		if (m_nRazzCardValue == GetCardValueByIndex(i))//4张癞子+2张非癞子
		{
			nJokerIndex = i;
		}
		if (!nCardLay[i] && i!=nJokerIndex) continue;
		int Joker_NeedInFour=0;//补4张所需的癞子牌个数
		int Joker_NeedInSingle=0;//补单张所需的癞子牌个数
		int nTmpIdx1 = 0;
		int nTmpIdx2 = 0;
		memcpy(nTempLay,nCardLay,sizeof(nTempLay));
		if (nCardLay[i]<4)
		{
			Joker_NeedInFour += 4 - nTempLay[i];
		}
		nTempLay[i] = 0;

		if (Joker_NeedInFour > nJokerCount) continue;

		int nSingleCount = 0;
		memset(nSingleLayTmp,-1,sizeof(nSingleLayTmp));

        if (m_bIsRazzMode)
        {
			for (int t=1; t<nLayLen; t++) //寻找不拆牌的最优组合
			{
				if (nTempLay[t] == 1)
				{
					if ((t==14 && nTempLay[15]) || (t==15 && nTempLay[14])) continue;
					nSingleLayTmp[nSingleCount++] = t;
					if (nTmpIdx1 == 0)
						nTmpIdx1 = t;
					else if (nTmpIdx2 == 0)
						nTmpIdx2 = t;
				}
			}

			if (nTmpIdx2==0) //没有找到最优组合，拆非炸弹的牌
			{
				nTmpIdx1 = 0;
				nTmpIdx2 = 0;
				nSingleCount = 0;
				memset(nSingleLayTmp, -1, sizeof(nSingleLayTmp));
				for (int m=1; m<nLayLen; m++)
				{
					if (nTempLay[m]==4)		continue;
					if ((m==14 && nTempLay[15] && nTempLay[14]) || (m==15 && nTempLay[14] && nTempLay[15])) continue;

					int nCount=nTempLay[m];
					if (nCount>=1)
					{
						while (nCount >=1)  
						{						
							nSingleLayTmp[nSingleCount++]=m;
							nCount--;
							
							if (nTmpIdx1 == 0)
								nTmpIdx1 = m;
							else if (nTmpIdx2 == 0)
								nTmpIdx2 = m;
						}
					}
				}
			}

			if (nTmpIdx2==0) //没有找到最优组合，拆炸弹
			{
				nTmpIdx1 = 0;
				nTmpIdx2 = 0;
				nSingleCount = 0;
				memset(nSingleLayTmp, -1, sizeof(nSingleLayTmp));
				for (int m=1; m<nLayLen; m++)
				{
				//	if (nTempLay[m]!=4 && m<=13)		continue;
					if ((m==14 && nTempLay[14] && nTempLay[15]) || (m==15 && nTempLay[14] &&  nTempLay[15])) continue;
					while (nTempLay[m] >=1)  
					{						
						nSingleLayTmp[nSingleCount++]=m;
						nTempLay[m] -= 1;
						
						if (nTmpIdx1 == 0)
							nTmpIdx1 = m;
						else if (nTmpIdx2 == 0)
							nTmpIdx2 = m;						
					}
				}
			}
        }
		else
		{
			for (int m=0;m<nLayLen;m++)
			{
				if ((m == 14 && nTempLay[15] && nTempLay[14]) || (m == 15 && nTempLay[14] && nTempLay[15])) continue;
				while (nTempLay[m] >= 1)
				{
					nSingleLayTmp[nSingleCount++]=m;
					nTempLay[m] -= 1;
					if (nTmpIdx1 == 0)
					{
						nTmpIdx1 = m;
					}
					else if (nTmpIdx2 == 0)
					{
						nTmpIdx2 = m;
					}
				}
			}
		}
		
		if (m_bIsRazzMode && nMaxPair-nSingleCount==2 && nJokerCount>=Joker_NeedInFour+2)	//剩2张癞子做单张
		{
			int razzValueIndex = SK_GetCardIndexEx(m_nRazzCardValue, 0);
			nSingleLayTmp[nSingleCount++] = razzValueIndex;
			nSingleLayTmp[nSingleCount++] = razzValueIndex;
			Joker_NeedInSingle += 2;
			nTmpIdx1 = razzValueIndex;
			nTmpIdx2 = razzValueIndex;
		}
		
		if (m_bIsRazzMode && nMaxPair-nSingleCount==1 && nJokerCount>=Joker_NeedInFour+1)	//剩1张癞子做单张
		{
			int razzValueIndex = SK_GetCardIndexEx(m_nRazzCardValue, 0);
			nSingleLayTmp[nSingleCount++] = razzValueIndex;
			Joker_NeedInSingle +=1;
			if (nTmpIdx1 == 0)
			{
				nTmpIdx1 = razzValueIndex;
			}
			else if (nTmpIdx2 == 0)
			{
				nTmpIdx2 = razzValueIndex;
			}
		}

		/***********************************************************************
		* 当一共有nCoupleCount个对子，需要从中遍历nMaxPair个对子的所有组合
		* 利用位运算，遍历0～2的nCoupleCount次方中的数，如果该数的二进制有
		* nMaxPair个1，表示一种遍历情况
		************************************************************************/
		if (nSingleCount>=nMaxPair)
		{
			//遍历所有对子组合,在若干对子中，取出nMaxPair个对子
			int nTotal = 1<<nSingleCount;
			for (int j=3;j<nTotal;j++)
			{
				//有nMaxPair个对子
				if (GetBit1Count(j)==nMaxPair)
				{
					memset(nSingleLayTmpTmp,-1,sizeof(nSingleLayTmpTmp));
					CString tmp;
					for (int k=0;k<nSingleCount;k++)
					{
						//nSingleLayTmp中第k个数被随机组成nMaxPair个对子中的一对
						if ((j&(1<<k))!=0)
						{
							nSingleLayTmpTmp[k] = nSingleLayTmp[k];
						}
					}	
					
					nThisValue = SK_GetIndexPRIEx(i,GetCurrentRank(), 0)*100 + nBaseValue;
					if(!m_bIsRazzMode)
						nThisValue += GetLayPriEx(nSingleLayTmpTmp,SK_LAYOUT_NUM);	//非癞子场带上副牌牌值

					if(nThisValue>nValue)
					{
						if (m_bIsRazzMode && m_bIsRemind)	//提示从小到大取值
						{
							int nThisRemindValue = SK_GetIndexPRIEx(i,GetCurrentRank(), 0)*100 + GetLayPriEx(nSingleLayTmpTmp,SK_LAYOUT_NUM) + nBaseValue;
							if (nRemindValue==-1 || nThisRemindValue<nRemindValue)
							{
								nRemindValue = nThisRemindValue;
								nStartIndex=i;
								nMinValue=nThisValue;
								memcpy(nSingleLay,nSingleLayTmpTmp,sizeof(nSingleLay));
							}
						}
						else if ((nMinValue==-1||nThisValue<nMinValue&&!m_bIsRazzMode) 
							|| (m_bIsRazzMode&&nThisValue>nMinValue))
						{
							nStartIndex=i;
							nSingleIndex1 = nTmpIdx1;
							nSingleIndex2 = nTmpIdx2;
							nMinValue=nThisValue;
							memcpy(nSingleLay,nSingleLayTmpTmp,sizeof(nSingleLay));
						}
					}
				}
			}
		}
	}

	if (nStartIndex==-1) return FALSE;

	type.dwCardType=CARD_UNITE_TYPE_FOUR_2;
	type.dwComPareType= COMPARE_UNITE_TYPE_FOUR_2; 
	type.nMainValue=nMinValue;
	type.nCardCount=6;

	if (m_bIsRazzMode && nJokerCount>0 && !m_bIsRemind)
	{
		if (!CalcRazzValueInThreeCount(nCardLay, nJokerCount, 4, 1, 1, nStartIndex, nSingleIndex1, nSingleIndex2))
		{
			return FALSE;
		}
		if(nSingleIndex1==0 || nSingleIndex2==0) 
			return FALSE;
	}
	
	XygInitChairCards(type.nCardIDs,MAX_CARDS_PER_CHAIR);
	int temp[MAX_CARDS_PER_CHAIR];
	memset(temp,0,sizeof(temp));
	memcpy(temp,nCardIDs,sizeof(int)*nCardLen);
	
	PutCardToArray(type.nCardIDs,nCardLen,temp,nCardLen,nStartIndex,4);

	for (i=0;i<CHAIR_CARDS;i++)
	{
		if (nSingleLay[i]>=0)
		{
			PutCardToArray(type.nCardIDs,nCardLen,temp,nCardLen,nSingleLay[i],1);
		}
	}
	
	return TRUE;
}

BOOL    CGameTable::CalcCardType_Four_2_Couple(int nCardIDs[],int nCardLen,int nCardCount,CARD_UNITE* CardDetail)
{
	if (nCardLen<=0)
		return FALSE;

	if (CardDetail->nTypeCount>=MAX_FIT_TYPE) return FALSE;
	if (nCardCount!=8) return FALSE;           
	
	int nCardLay[SK_LAYOUT_NUM];
	memset(nCardLay,0,sizeof(nCardLay));
	int nJokerCount=0;
	PreDealCards(nCardIDs,nCardLen,nCardLay,SK_LAYOUT_NUM,nJokerCount);
	int& index=CardDetail->nTypeCount;
	
	memset(&CardDetail->uniteType[index],0,sizeof(UNITE_TYPE));
	if (!GetCardType_Four_2_Couple(nCardIDs,nCardLen,nCardLay,SK_LAYOUT_NUM,nJokerCount,CardDetail->uniteType[index]))
		return FALSE;
	if (CardDetail->uniteType[index].nCardCount!=nCardCount)	//此时牌数需要吻合，才能是此类型
		return FALSE;

	index++;
	return TRUE;
}

BOOL   CGameTable::GetCardType_Four_2_Couple(int nCardIDs[],int nCardLen,int nCardLay[], int nLayLen,int nJokerCount,UNITE_TYPE& type)
{
	if (nCardLen<=0 || nLayLen<=0)
		return FALSE;

	int nMaxPair=2;	//需要2个对子
	
	int nStartIndex=-1;
	int nCoupleIndex1 = 0;
	int nCoupleIndex2 = 0;
	int nValue=0;
	if (type.dwCardType==CARD_UNITE_TYPE_FOUR_2_COUPLE)
	{
		if (type.nCardCount!=8)//张数不同不能比较
			return FALSE;
		
		nValue=type.nMainValue;
	}

	if (m_nCardTypeLimit) {
		// 四代二不能有炸弹
		auto nBombCount = 0;
		auto nSingleCount = 0;
		auto doubleCount = 0;
		auto threeCount = 0;
		for (int i = 1; i < 14; i++) {
			if (nCardLay[i] == 4) { ++nBombCount; }
			else if (nCardLay[i] == 1)
			{
				++nSingleCount;
			}
			else if (nCardLay[i] == 2)
			{
				++doubleCount;
			}
			else if (nCardLay[i] == 3)
			{
				++threeCount;
			}
		}
		if (nBombCount > 1 || nJokerCount == 4 || (nJokerCount == 1 && nSingleCount == 0)) { return FALSE; }
		//if (nBombCount > 1 || nJokerCount == 4 || threeCount > 1 ||(nJokerCount == 1 && nSingleCount == 0 && doubleCount !=2)){ return FALSE;}
	
	}

	//注意支持癞子
	int nTempLay[SK_LAYOUT_NUM];
	int nMinValue=-1;
	int nThisValue = 0;
	int nBaseValue=100000;
	int nJokerIndex = 0;
	int nCoupleLay[SK_LAYOUT_NUM];
	int nCoupleLayTmp[SK_LAYOUT_NUM];
	int nCoupleLayTmpTmp[SK_LAYOUT_NUM];
	memset(nCoupleLay,0,sizeof(nCoupleLay));

	int nRemindValue = -1;

	for(int i=1;i<14;i++)//2到A
	{
		if (m_nRazzCardValue == GetCardValueByIndex(i))//4张癞子+2对非癞子
		{
			nJokerIndex = i;
		}
		if (!nCardLay[i] && i!=nJokerIndex) continue;
		int Joker_NeedInFour=0; //补4张需要的癞子个数
		int Joker_NeedInCouple=0; //补对子需要的癞子个数
		memcpy(nTempLay,nCardLay,sizeof(nTempLay));
		if (nCardLay[i]<4)
		{
			Joker_NeedInFour += 4 - nTempLay[i];
		}
		nTempLay[i] = 0;
		if (Joker_NeedInFour > nJokerCount) continue;

		int nCoupleCount = 0;
		memset(nCoupleLayTmp,0,sizeof(nCoupleLayTmp));

		int m;

        if (m_bIsRazzMode)
        {
			for (int t=2; t<14; t++)
			{
				if (nTempLay[t] == 2) //寻找不拆牌的组合
				{
					nTempLay[t] -= 2;
					nCoupleCount++;
					nCoupleLayTmp[t]++;
				}
				if (nCoupleCount >= nMaxPair)
				{
					break;
				}
			}
			if (nCoupleCount < nMaxPair)
			{
				if (nTempLay[1] == 2) //寻找不拆牌的组合
				{
					nTempLay[1] -= 2;
					nCoupleCount++;
					nCoupleLayTmp[1]++;
				}
			}
        }
		else
		{
			for (m = 2; m < 14; m++)
			{
				if (nTempLay[m] == 2) //寻找不拆牌的组合
				{
					nTempLay[m] -= 2;
					nCoupleCount++;
					nCoupleLayTmp[m]++;
				}
				if (nCoupleCount >= nMaxPair)
				{
					break;
				}
			}
			if (nCoupleCount < nMaxPair)
			{
				for (m=1;m<14;m++)
				{
					while (nTempLay[m] >= 2)
					{
						nTempLay[m] -= 2;
						nCoupleCount++;
						nCoupleLayTmp[m]++;
					}
				}
			}
		}
		
		if(m_bIsRazzMode && nJokerCount-Joker_NeedInFour>0 && nCoupleCount<nMaxPair) //癞子与非癞子搭配成对子
		{
			for (m=2; m<14; m++)
			{
				auto limit = !m_nCardTypeLimit ? true : nCoupleLayTmp[m] == 0;
				if(nTempLay[m]==1 && Joker_NeedInCouple<nJokerCount-Joker_NeedInFour)
				{
					Joker_NeedInCouple++;
					nTempLay[m] = 0;
					nCoupleCount++;
					nCoupleLayTmp[m]++;
				}
				if (nCoupleCount >= nMaxPair)
				{
					break;
				}
			}
			if (nCoupleCount < nMaxPair)
			{
				if (nCoupleLayTmp[1]==0 && nTempLay[1]==1 && Joker_NeedInCouple<nJokerCount-Joker_NeedInFour)
				{
					Joker_NeedInCouple++;
					nTempLay[1] = 0;
					nCoupleCount++;
					nCoupleLayTmp[1]++;
				}
			}
			if (nCoupleCount < nMaxPair)
			{
				for (m = 1; m < 14; m++)
				{
					while (nTempLay[m] >= 2)
					{
						nTempLay[m] -= 2;
						nCoupleCount++;
						nCoupleLayTmp[m]++;
					}
					if (nCoupleCount < nMaxPair && nTempLay[m] == 1 && Joker_NeedInCouple<nJokerCount - Joker_NeedInFour)
					{
						Joker_NeedInCouple++;
						nTempLay[m] = 0;
						nCoupleCount++;
						nCoupleLayTmp[m]++;
					}
				}
			}

		}

		if(m_bIsRazzMode && nJokerCount-Joker_NeedInFour-Joker_NeedInCouple >= 2 && nCoupleCount<nMaxPair)//还剩2张癞子作为对子
		{
			int razzCardIndex = SK_GetCardIndexEx(m_nRazzCardValue, 0);
			nCoupleCount++;
			nCoupleLayTmp[razzCardIndex]++;
			Joker_NeedInCouple +=2;
		}

		if (m_bIsRazzMode)
		{
			if (nCoupleCount < nMaxPair) //没有找到满足的对子组合，拆非炸弹牌
			{
				for (m=2; m<14; m++)
				{
					if (nTempLay[m] == 3)	//拆3张牌型
					{
						nTempLay[m] -= 2;
						nCoupleCount++;
						nCoupleLayTmp[m]++;
					}
					if (nCoupleCount >= nMaxPair)
					{
						break;
					}
				}
				if (nCoupleCount < nMaxPair)
				{
					if (nTempLay[1] == 3) //拆3个2
					{
						nTempLay[1] -= 2;
						nCoupleCount++;
						nCoupleLayTmp[1]++;
					}
				}
			}
			if (nCoupleCount < nMaxPair) //没有找到满足的对子组合，拆炸弹牌
			{
				for (m = 2; m<14; m++)
				{
					auto limit = !m_nCardTypeLimit ? true : nTempLay[m] != 4;
					while (nTempLay[m] >= 2 && limit)
					{
						nTempLay[m] -= 2;
						nCoupleCount++;
						nCoupleLayTmp[m]++;
					}
					if (nCoupleCount >= nMaxPair)
					{
						break;
					}
				}
				if (nCoupleCount < nMaxPair)
				{
					auto limit = !m_nCardTypeLimit ? true : nTempLay[1] != 4;
					while (nTempLay[1] >= 2 && limit) //拆4个2
					{
						nTempLay[1] -= 2;
						nCoupleCount++;
						nCoupleLayTmp[1]++;
					}
				}
			}
		}

		/***********************************************************************
		* 当一共有nCoupleCount个对子，需要从中遍历nMaxPair个对子的所有组合
		* 利用位运算，遍历0～2的nCoupleCount次方中的数，如果该数的二进制有
		* nMaxPair个1，表示一种遍历情况
		************************************************************************/
		if (nCoupleCount>=nMaxPair)
		{
			//遍历所有对子组合,在若干对子中，取出nMaxPair个对子
			int nTotal = 1<<nCoupleCount;
			for (int j=3;j<nTotal;j++)
			{
				//有nMaxPair个对子
				if (GetBit1Count(j)==nMaxPair)
				{
					memset(nCoupleLayTmpTmp,0,sizeof(nCoupleLayTmpTmp));
					for (int k=0;k<nCoupleCount;k++)
					{
						//Lay中第k+1个数被随机组成nMaxPair个对子中的一对
						if ((j&(1<<k))!=0)
						{
							int nIndex = GetIndexByIndex(nCoupleLayTmp,SK_LAYOUT_NUM,k+1);
							if (nIndex>=0)
							{
								nCoupleLayTmpTmp[nIndex]++;
							}
						}
					}

					nThisValue = SK_GetIndexPRIEx(i,GetCurrentRank(), 0)*100+nBaseValue;
					if(!m_bIsRazzMode)
						nThisValue += GetLayPriEx(nCoupleLayTmpTmp,SK_LAYOUT_NUM);	//非癞子场带上副牌牌值

					if(nThisValue>nValue)
					{
						if (m_bIsRazzMode && m_bIsRemind)	//提示从小到大取值
						{
							int nThisRemindValue = SK_GetIndexPRIEx(i,GetCurrentRank(), 0)*100 + GetLayPri(nCoupleLayTmpTmp,SK_LAYOUT_NUM) + nBaseValue;
							if (nRemindValue==-1 || nThisRemindValue<nRemindValue)
							{
								nRemindValue = nThisRemindValue;
								nStartIndex=i;
								nMinValue = nThisValue;
								memcpy(nCoupleLay,nCoupleLayTmpTmp,sizeof(nCoupleLay));
							}
						}
						else if ((nMinValue==-1||nThisValue<nMinValue&&!m_bIsRazzMode) 
							|| (m_bIsRazzMode&&nThisValue>nMinValue))
						{
							nStartIndex=i;
							nMinValue = nThisValue;
							memcpy(nCoupleLay,nCoupleLayTmpTmp,sizeof(nCoupleLay));
						}
					}
				}
			}
		}
	}

	if (nStartIndex==-1) return FALSE;

	type.dwCardType=CARD_UNITE_TYPE_FOUR_2_COUPLE;
	type.dwComPareType= COMPARE_UNITE_TYPE_FOUR_2_COUPLE; 
	type.nMainValue=nMinValue;
	type.nCardCount=8;


	XygInitChairCards(type.nCardIDs,MAX_CARDS_PER_CHAIR);
	int temp[MAX_CARDS_PER_CHAIR];
	memset(temp,0,sizeof(temp));
	memcpy(temp,nCardIDs,sizeof(int)*nCardLen);
	
	PutCardToArray(type.nCardIDs,nCardLen,temp,nCardLen,nStartIndex,4);


	for (int nPairIndex = 0; nPairIndex < 2; nPairIndex++)
	{
		int nIndex = GetIndexByIndex(nCoupleLay, SK_LAYOUT_NUM, nPairIndex + 1);
		if (nIndex >= 0)
		{
			PutCardToArray(type.nCardIDs, nCardLen, temp, nCardLen, nIndex, 2);
		}
	}

	return TRUE;
}

BOOL CGameTable::CalcCardType_BombMixedRazz(int nCardIDs[],int nCardLen,int nCardCount,CARD_UNITE* CardDetail)
{
	if (nCardLen<=0)
		return FALSE;

	if (CardDetail->nTypeCount>=MAX_FIT_TYPE) return FALSE;
	if (nCardCount != 4) return FALSE;      //必须是四张相同的牌
	int nCardLay[SK_LAYOUT_NUM];
	memset(nCardLay,0,sizeof(nCardLay));
	int nJokerCount=0;
	PreDealCards(nCardIDs,nCardLen,nCardLay,SK_LAYOUT_NUM,nJokerCount);
	if (nJokerCount<=0 || nJokerCount>=4) //至少1张并少于4张癞子
	{
		return FALSE;
	}

	int& index=CardDetail->nTypeCount;

	memset(&CardDetail->uniteType[index],0,sizeof(UNITE_TYPE));
	CardDetail->uniteType[index].nCardCount=nCardCount;
	//主值放前，财神放后
	memset(&CardDetail->uniteType[index],0,sizeof(UNITE_TYPE));
	if (!GetCardType_BombMixedRazz(nCardIDs,nCardLen,nCardLay,SK_LAYOUT_NUM,nJokerCount,CardDetail->uniteType[index],nCardCount))
		return FALSE;

	index++;
	return TRUE;
}

BOOL CGameTable::GetCardType_BombMixedRazz(int nCardIDs[],int nCardLen,int nCardLay[], int nLayLen,int nJokerCount,UNITE_TYPE& type,int nUseCount)
{
	if (nCardLen<=0 || nLayLen<=0)
		return FALSE;

	if (nCardLen == nJokerCount)
	{
		nJokerCount = 0;
	}
	
	if (m_bIsRazzMode && nJokerCount==0)
		return FALSE;

	int nValue=0;
	if (type.dwCardType==CARD_UNITE_TYPE_MIXEDRAZZ_BOMB)
	{
		nValue=type.nMainValue;
	}
	
	int nCardCount=0;
	int nCardIndex=-1;
	nCardCount=nUseCount;
	nCardIndex=GetSameCountEx(nCardLay,nLayLen,nUseCount,nJokerCount,nValue);
	if (nCardIndex==-1 || nCardIndex==14 || nCardIndex==15)
		return FALSE;
	
	type.dwCardType=CARD_UNITE_TYPE_MIXEDRAZZ_BOMB;
	type.dwComPareType=COMPARE_UNITE_TYPE_MIXEDRAZZ_BOMB;
	type.nMainValue=SK_GetIndexPRIEx(nCardIndex,GetCurrentRank(), 0);
	for(int k=0;k<nCardCount-4;k++)
	{
		type.nMainValue+=10000;
	}
	
	type.nCardCount=nCardCount;
	
	if (m_bIsRazzMode && (nJokerCount==1||nJokerCount==2||nJokerCount==3) && (!m_bIsRemind))
	{   
		if (!CalcRazzValueInSame(nJokerCount, nCardIndex))
		{ 
			return FALSE;
		}
	}
	
	XygInitChairCards(type.nCardIDs,MAX_CARDS_PER_CHAIR);
	
	int temp[MAX_CARDS_PER_CHAIR];
	memset(temp,0,sizeof(temp));
	memcpy(temp,nCardIDs,sizeof(int)*nCardLen);
	PutCardToArray(type.nCardIDs,nCardLen,temp,nCardLen,nCardIndex,nCardCount);
	return TRUE;
}

BOOL CGameTable::CalcCardType_BombPureRazz(int nCardIDs[],int nCardLen,int nCardCount,CARD_UNITE* CardDetail)
{
	if (nCardLen<=0)
		return FALSE;

	if (CardDetail->nTypeCount>=MAX_FIT_TYPE) return FALSE;
	if (nCardCount != 4) return FALSE;    //必须是四张相同的牌
	int nCardLay[SK_LAYOUT_NUM];
	memset(nCardLay,0,sizeof(nCardLay));
	int nJokerCount=0;
	PreDealCards(nCardIDs,nCardLen,nCardLay,SK_LAYOUT_NUM,nJokerCount);
// 	if (nJokerCount != 4)//4张癞子组成纯癞子炸弹//改PreDealCards时注释掉
// 	{
// 		return FALSE;
// 	}
	int& index=CardDetail->nTypeCount;

	memset(&CardDetail->uniteType[index],0,sizeof(UNITE_TYPE));
	CardDetail->uniteType[index].nCardCount=nCardCount;

	if (!GetCardType_BombPureRazz(nCardIDs,nCardLen,nCardLay,SK_LAYOUT_NUM,nJokerCount,CardDetail->uniteType[index],nCardCount))
		return FALSE;

	index++;
	return TRUE;
}

BOOL CGameTable::GetCardType_BombPureRazz(int nCardIDs[],int nCardLen,int nCardLay[], int nLayLen,int nJokerCount,UNITE_TYPE& type,int nUseCount)
{
	if (nCardLen<=0 || nLayLen<=0)
		return FALSE;

	if (m_bIsRazzMode && type.dwCardType==CARD_UNITE_TYPE_PURERAZZ_BOMB)//上次提示的纯癞子炸弹
		return FALSE;
	
	int nValue=0;
	
	int nCardCount=0;
	int nCardIndex=-1;
	nCardCount=nUseCount;

	int i;
	if (m_bIsRemind)
	{
		for(i=0;i<nLayLen;i++)
		{
			//手牌中有纯癞子炸弹 或者 手牌中只有纯癞子炸弹需要分别处理
			if ((GetCardValueByIndex(i)==m_nRazzCardValue && nCardLay[i]==0 && nJokerCount==4) || (GetCardValueByIndex(i)==m_nRazzCardValue && nCardLay[i]==4))
			{
				nCardIndex = i;
				break;
			}
		}
	}
	else
	{
		for(i=0;i<nLayLen;i++)
		{
			if (GetCardValueByIndex(i)==m_nRazzCardValue && nCardLay[i]==4)
			{
				nCardIndex = i;
				break;
			}
		}
	}
	if (nCardIndex==-1)	return FALSE;
 	
	
	type.dwCardType=CARD_UNITE_TYPE_PURERAZZ_BOMB;
	type.dwComPareType=COMPARE_UNITE_TYPE_PURERAZZ_BOMB;
	type.nMainValue=1;
	type.nCardCount=nCardCount;
	
	XygInitChairCards(type.nCardIDs,MAX_CARDS_PER_CHAIR);
	int temp[MAX_CARDS_PER_CHAIR];
	memset(temp,0,sizeof(temp));
	memcpy(temp,nCardIDs,sizeof(int)*nCardLen);
	PutCardToArray(type.nCardIDs,nCardLen,temp,nCardLen,nCardIndex,nCardCount);
	return TRUE;
}

BOOL  CGameTable::CalcCardType_BOMB_2King(int nCardIDs[],int nCardLen,int nCardCount,CARD_UNITE* CardDetail)
{
	if (nCardLen<=0)
		return FALSE;
	
	if (CardDetail->nTypeCount>=MAX_FIT_TYPE) return FALSE;
	if (nCardCount!=2) return FALSE;           //必须是2张大小王
	int nCardLay[SK_LAYOUT_NUM];
	memset(nCardLay,0,sizeof(nCardLay));
	int nJokerCount=0;
	PreDealCards(nCardIDs,nCardLen,nCardLay,SK_LAYOUT_NUM,nJokerCount);
	
	int& index=CardDetail->nTypeCount;
	memset(&CardDetail->uniteType[index],0,sizeof(UNITE_TYPE));
	if (!GetCardType_BOMB_2King(nCardIDs,nCardLen,nCardLay,SK_LAYOUT_NUM,nJokerCount,CardDetail->uniteType[index]))
		return FALSE;
	
	index++;
	return TRUE;
}

BOOL CGameTable::GetCardType_BOMB_2King(int nCardIDs[],int nCardLen,int nCardLay[], int nLayLen,int nJokerCount,UNITE_TYPE& type)
{
	if (nCardLen<=0 || nLayLen<=0)
		return FALSE;
	
	if (nCardLay[14]!=1  //小王			
		||nCardLay[15]!=1) //大王
		return FALSE;
	
	if (type.dwCardType==CARD_UNITE_TYPE_2KING
		&&type.nMainValue>0)
		return FALSE;
	
	type.dwCardType=CARD_UNITE_TYPE_2KING;
	type.dwComPareType=  COMPARE_UNITE_TYPE_2KING;
	type.nMainValue=1;//唯一
	type.nCardCount=2;
	
	int temp[MAX_CARDS_PER_CHAIR];
	memset(temp,0,sizeof(temp));
	memcpy(temp,nCardIDs,sizeof(int)*nCardLen);
	XygInitChairCards(type.nCardIDs,MAX_CARDS_PER_CHAIR);
	PutCardToArray(type.nCardIDs,nCardLen,temp,nCardLen,14,1);//大王
	PutCardToArray(type.nCardIDs,nCardLen,temp,nCardLen,15,1);//小王
	
	return TRUE;
	
}

BOOL CGameTable::IsGameMsg(UINT resquesID)
{
	if (resquesID>SYSMSG_BEGIN&&resquesID<SYSMSG_END)
		return TRUE;
	if (resquesID>LOCAL_GAME_MSG_BEGIN&&resquesID<LOCAL_GAME_MSG_END)
		return TRUE;
	
	if (resquesID>ZGDA_GAME_MSG_BEGIN&&resquesID<ZGDA_GAME_MSG_END)
		return TRUE;

	return FALSE;
}

BOOL CGameTable::IsTooManyAuctionRound()
{
	if(m_nAuctionRound >= MAX_AUCTION_ROUND)
		return TRUE;
	else
		return FALSE;
}

void CGameTable::AuctionRoundEnd()
{
	m_nAuctionRound++;
}

void CGameTable::InvalidResults(void* pData, int nLen)
{
	LPGAME_WIN_RESULT pGameWinResult = (LPGAME_WIN_RESULT)pData;
	
	pGameWinResult->nReserved[0] = 1;		//无人叫庄，此局无效
	
	//CTable::InvalidResults(pData,nLen);
	//GAME_WIN为GAME_WIN_RESULT的第一个字节要不让会有问题
	LPGAME_WIN pGameWin = (LPGAME_WIN)pData;
	
	for(int i = 0; i < m_nTotalChairs; i++){ 
		if (NULL==m_ptrPlayers[i])
			continue;
		CPlayer* ptrP	= m_ptrPlayers[i];
		pGameWin->nScoreDiffs[i]	= 0;
		pGameWin->nDepositDiffs[i]	= 0;
		pGameWin->nWinFees[i] = 0;
		pGameWin->nLevelIDs[i]	= ptrP->m_nLevelID;
	}
}

void CGameTable::ActuallizeResults(void* pData, int nLen)
{
	CTable::ActuallizeResults(pData,nLen);

	//记录每局结果
	int nRecordIndex = m_nBoutCount-1;
	nRecordIndex = (m_nBoutCount-1)%MAX_RESULT_COUNT;
	if(m_nBoutCount>0)
	{
		GAME_WIN* pGameWinEx=(GAME_WIN*)pData;
		if(m_nBaseDeposit)
		{
			//银子房间记录银子得失
			for(int i=0;i<TOTAL_CHAIRS;i++){			
				m_nResultDiff[i][nRecordIndex]=pGameWinEx->nDepositDiffs[i];
				m_nTotalResult[i]+=pGameWinEx->nDepositDiffs[i];
			}
		}
		else
		{
			//记录分得失
			for(int i=0;i<TOTAL_CHAIRS;i++){			
				m_nResultDiff[i][nRecordIndex]=pGameWinEx->nScoreDiffs[i];
				m_nTotalResult[i]+=pGameWinEx->nScoreDiffs[i];
			}
		}		
	}

}
void CGameTable::ResetAuctionOnStandOff()
{
	m_nAuctionRound = 0;
	m_nBoutCount--;
}

void CGameTable::ResetAuctionWhenFinished()
{
	m_nAuctionRound = 0;
}

void CGameTable::OnPlayerPassiveEvent(int chairno)
{
	if (!ValidateChair(chairno))
		return;
	m_nAutoPlayCount[chairno]++;
}

void CGameTable::OnPlayerActiveEvent(int chairno)
{
	if (!ValidateChair(chairno))
		return;
	m_nAutoPlayCount[chairno] = 0;
}

BOOL CGameTable::IsTooManyAutoPlay(int chairno)
{
	if (!ValidateChair(chairno))
		return FALSE;

	if (m_nAutoPlayCount[chairno]>=MAX_AUTO_COUNT)
		return TRUE;
	else
		return FALSE;
}

//逃跑倍数加上炸弹
int CGameTable::CalcDoubleOfScore(int chairno, int breakchair, int defdouble)
{
	if (m_bIsMatchGame)
	{
		return 0;
	}

	if (m_nObjectGains>0)
	{
		int nBombInHand = CalcBombInHandForBroken();
		int nBankerPunish = (breakchair==m_nBanker) ? 1 : 0;
	//	int nbreak = defdouble;
		defdouble*=m_nMaxAuction*UwlPow2(GetPublicInfo()->nBombFan+nBombInHand+nBankerPunish);
	//	UwlLogFile(_T("CalcBreakDeposit() breakchair %ld nBombFan:%d nBombInHand:%d nBankerPunish:%d m_nMaxAuction:%d nbreak:%d defdouble:%d"), breakchair,GetPublicInfo()->nBombFan,nBombInHand,nBankerPunish,m_nMaxAuction,nbreak,defdouble);

	}
	
	return CTable::CalcDoubleOfScore(chairno,breakchair,defdouble);
}

int CGameTable::CalcBreakDeposit(int breakchair, int breakdouble, int& cut)
{	
	if (m_nObjectGains>0)
	{
		int nBombInHand = CalcBombInHandForBroken();
		int nBankerPunish = (breakchair==m_nBanker) ? 1 : 0;

		breakdouble*=m_nMaxAuction*UwlPow2(GetPublicInfo()->nBombFan+nBombInHand+nBankerPunish);
	}
	
	return CTable::CalcBreakDeposit(breakchair,breakdouble,cut);
}

int     CGameTable::CalcBombInHandForBroken()
{
	int nBombCount = 0;
	for (int i=0;i<TOTAL_CHAIRS;i++)
	{
		//普通炸
		for(int j=0;j<14;j++)
		{
			if (m_nCardsLayIn[i][j]>=4)
				nBombCount++;
		}
		//大小王
		if (m_nCardsLayIn[i][14]==1
			&&m_nCardsLayIn[i][15]==1)
				nBombCount++;
	}

	return nBombCount;
}

BOOL  CGameTable::GetBestUnitType(UNITE_TYPE& first_card,CARD_UNITE& fight_card)
{
    DWORD dwDestType=first_card.dwCardType;
	DWORD dwDestMain=first_card.nMainValue;
	DWORD dwDestCompareType=fight_card.uniteType[0].dwComPareType;
	int nCardIDs[MAX_CARDS_PER_CHAIR];
	XygInitChairCards(nCardIDs,MAX_CARDS_PER_CHAIR);


 	int nTempIndex=0;
	
	BOOL bnFindBig=FALSE;
	for(int i=0;i<fight_card.nTypeCount;i++)
	{
		if (IS_BIT_SET(fight_card.uniteType[i].dwComPareType,dwDestType))
		{
			if (fight_card.uniteType[i].dwCardType==dwDestType)
			{
				if (fight_card.uniteType[i].nMainValue>dwDestMain)
				{
					if ((  dwDestType == CARD_UNITE_TYPE_ABT_SINGLE
						|| dwDestType == CARD_UNITE_TYPE_ABT_COUPLE
						|| dwDestType == CARD_UNITE_TYPE_ABT_THREE
						|| dwDestType == CARD_UNITE_TYPE_ABT_THREE_1
						|| dwDestType == CARD_UNITE_TYPE_ABT_THREE_COUPLE)
						&& fight_card.uniteType[i].nCardCount!=first_card.nCardCount)
					{
						//去掉这5种类型，牌不相等的情况
					}
					else
					{
						nTempIndex=i;
						bnFindBig=TRUE;
						dwDestType=fight_card.uniteType[i].dwCardType;
						dwDestMain=fight_card.uniteType[i].nMainValue;
						dwDestCompareType=fight_card.uniteType[i].dwComPareType;
						memcpy(nCardIDs,fight_card.uniteType[i].nCardIDs,sizeof(int)*MAX_CARDS_PER_CHAIR);
					}
				}
			}
			else
			{
				nTempIndex=i;
				bnFindBig=TRUE;
				dwDestType=fight_card.uniteType[i].dwCardType;
				dwDestMain=fight_card.uniteType[i].nMainValue;
				dwDestCompareType=fight_card.uniteType[i].dwComPareType;
				memcpy(nCardIDs,fight_card.uniteType[i].nCardIDs,sizeof(int)*MAX_CARDS_PER_CHAIR);
			}
		}	
	}
	
	if (bnFindBig)
	{
		memcpy(&m_razzCardsAlterValueUnit.razzCardsAlterValue[0],&m_razzCardsAlterValueUnit.razzCardsAlterValue[nTempIndex], sizeof(RAZZCARDS_ALTER_VALUE));

		fight_card.uniteType[0].dwCardType=dwDestType;
		fight_card.uniteType[0].nMainValue=dwDestMain;
		fight_card.uniteType[0].dwComPareType=dwDestCompareType;
		memcpy(fight_card.uniteType[0].nCardIDs,nCardIDs,sizeof(int)*MAX_CARDS_PER_CHAIR);
		
		fight_card.nTypeCount=1;
		return TRUE;
	}
	else
		return FALSE;
}


BOOL  CGameTable::GetBestUnitType(CARD_UNITE& fight_card)
{
	if (fight_card.nTypeCount<=1)
		return TRUE;
	
    DWORD dwDestType=fight_card.uniteType[0].dwCardType;
	DWORD dwDestMain=fight_card.uniteType[0].nMainValue;
	DWORD dwDestCompareType=fight_card.uniteType[0].dwComPareType;
	int nCardIDs[MAX_CARDS_PER_CHAIR];
	XygInitChairCards(nCardIDs,MAX_CARDS_PER_CHAIR);
	
	BOOL bnFindBig=FALSE;
	for(int i=1;i<fight_card.nTypeCount;i++)
	{
		if (IS_BIT_SET(fight_card.uniteType[i].dwComPareType,dwDestType))
		{
			if (fight_card.uniteType[i].dwCardType==dwDestType)
			{
				if (fight_card.uniteType[i].nMainValue>dwDestMain)
				{
					bnFindBig=TRUE;
					dwDestType=fight_card.uniteType[i].dwCardType;
					dwDestMain=fight_card.uniteType[i].nMainValue;
					dwDestCompareType=fight_card.uniteType[i].dwComPareType;
					memcpy(nCardIDs,fight_card.uniteType[i].nCardIDs,sizeof(int)*MAX_CARDS_PER_CHAIR);
				}
			}
			else
			{
				bnFindBig=TRUE;
				dwDestType=fight_card.uniteType[i].dwCardType;
				dwDestMain=fight_card.uniteType[i].nMainValue;
				dwDestCompareType=fight_card.uniteType[i].dwComPareType;
				memcpy(nCardIDs,fight_card.uniteType[i].nCardIDs,sizeof(int)*MAX_CARDS_PER_CHAIR);
			}
		}	
	}
	
	if (bnFindBig)
	{
		fight_card.uniteType[0].dwCardType=dwDestType;
		fight_card.uniteType[0].nMainValue=dwDestMain;
		fight_card.uniteType[0].dwComPareType=dwDestCompareType;
		memcpy(fight_card.uniteType[0].nCardIDs,nCardIDs,sizeof(int)*MAX_CARDS_PER_CHAIR);
		fight_card.nTypeCount=1;
	}
	return TRUE;
}

void CGameTable::SoftResetMember()
{
	m_nObjectGains = 0;
	InitialGameTableInfo(m_GameTalbeInfo);
	memset(m_nAutoPlayCount,0,sizeof(m_nAutoPlayCount));

	m_dwLastClockStop=0;
	ZeroMemory(m_nBottomIDs, sizeof(m_nBottomIDs));	// 底牌ID
	ZeroMemory(m_nCardsLayIn, sizeof(m_nCardsLayIn));// 每个人手里的牌
	
	m_nAuctionCount = 0;							// 叫庄计数
	m_nRobCount = 0;
	ZeroMemory(m_Auctions, sizeof(m_Auctions));		// 叫庄情况记录
	ZeroMemory(m_Rob, sizeof(m_Rob));				// 抢庄情况记录
	memset(m_nBombHadDeal,-1,sizeof(m_nBombHadDeal));
	memset(m_nBottomCatch,-1,sizeof(m_nBottomCatch));
	m_nOperateTime = THROW_WAIT;

	int i = 0;
	for (i = 0; i < TOTAL_CHAIRS; i++)
	{
		m_nUseCardMaster[i] = 0;
	}

	memset(m_peeredBottomPlayer, 0, sizeof(m_peeredBottomPlayer));

	m_bNoShuffMakeDeal = FALSE;
	m_b2K = FALSE;
}

int CGameTable::CompensateDeposits(int nOldDeposits[], int nDepositDiffs[])
{
	/*if (m_nRoomSilverLimit > 0) {
		return CompensateDepositsForSilverLimit(nOldDeposits, nDepositDiffs);
	}
	else
	{
		return CompensateDepositsOriginal(nOldDeposits, nDepositDiffs);
	}*/
	return CompensateDepositsForSilverLimit(nOldDeposits, nDepositDiffs);
}

int CGameTable::SetCurrentChair(int chairno,int nWaitSecond)
{
	if(INVALID_OBJECT_ID != m_nCurrentChair && m_dwActionStart){
		m_dwCostTime[m_nCurrentChair] += (GetTickCount() - m_dwActionStart);
	}
	m_nCurrentChair = chairno;
	m_dwActionBegin = GetTickCount();
	m_dwActionStart = GetTickCount();
	
	m_dwCheckBreakTime[chairno]=GetTickCount();
	
	m_dwWaitOperateTick=(nWaitSecond+2)*1000;//2000豪秒缓冲时间
	return chairno;
}

BOOL CGameTable::IsJoker(int nCardID)
{
	if(nCardID%SK_LAYOUT_MOD==0 && nCardID!=52)
	{
		return m_nRazzCardValue == SK_LAYOUT_MOD; //2的牌值
	}
	else if(nCardID!=53&&nCardID!=52)
	{
		return m_nRazzCardValue == nCardID%SK_LAYOUT_MOD;
	}
	else//大小王不能做癞子
	{
		return FALSE;
	}
}

int CGameTable::GetCardValueById(int nCardID)
{ 
	nCardID=nCardID%54;

	int nValue = 0;
	
	if (52 == nCardID)
	{
		//小王
		nValue = SK_LAYOUT_MOD + 1;
	}
	else if (53 == nCardID)
	{
		//大王
		nValue = SK_LAYOUT_MOD + 2;
	}
	else if (0 == nCardID%SK_LAYOUT_MOD)
	{
		//牌值2
		nValue = SK_LAYOUT_MOD;
	}
	else
	{
		nValue = nCardID%SK_LAYOUT_MOD;
	}

	return nValue;
}

int CGameTable::GetCardValueByIndex(int nCardIndex)
{
	int nValue = 0;
	if(nCardIndex == 1)
	{
		nValue = SK_LAYOUT_MOD;
	}
	else if(nCardIndex==14 || nCardIndex==15)
	{
		nValue = nCardIndex;
	}
	else
	{
		nValue = nCardIndex - 1;
	}
	return nValue;
}

BOOL CGameTable::ValidateDoubleCountForRazz(int nCardIDs[], int nCardLen, int nCardLay[], int nJokerCount, int count1, int count2, int nMainIndex, int nSecondeIndex)
{
	BOOL razzAlterInCount1Ok = FALSE;
	BOOL razzAlterInCount2Ok = FALSE;
	int nCardLayTmp[SK_LAYOUT_NUM];
	if(nMainIndex<0 || nSecondeIndex<0)
	{
		return FALSE;
	}
	memset(nCardLayTmp, 0, SK_LAYOUT_NUM*sizeof(int));
	memcpy(nCardLayTmp, nCardLay, SK_LAYOUT_NUM*sizeof(int));
	
	for(int i=0; i<nJokerCount; i++)
	{
		for(int j=0; j<nCardLen; j++)
		{
			if (m_nRazzCardsAlter[i] == GetCardValueById(nCardIDs[j]))
			{
				int index=SK_GetCardIndexEx(nCardIDs[j], 0);
				nCardLayTmp[index]++;
				break;
			}
		}
	}

	if(nCardLayTmp[nMainIndex] == count1)
	{
		razzAlterInCount1Ok = TRUE;
	}
	if(nCardLayTmp[nSecondeIndex] == count2)
	{
		razzAlterInCount2Ok = TRUE;
	}
	if(!razzAlterInCount1Ok || !razzAlterInCount2Ok)
	{
		return FALSE;
	}
	return TRUE;
}

BOOL CGameTable::ValidateThreeCountForRazz(int nCardIDs[], int nCardLen, int nCardLay[], int nJokerCount, int count1, int count2, int count3, int nMainIndex, int nSecondIndex, int nThirdIndex)
{
	BOOL razzAlterInCount1Ok = FALSE;
	BOOL razzAlterInCount2Ok = FALSE;
	BOOL razzAlterInCount3Ok = FALSE;
	int nLayTmp[SK_LAYOUT_NUM];
	memset(nLayTmp, 0, SK_LAYOUT_NUM*sizeof(int));
	memcpy(nLayTmp, nCardLay, SK_LAYOUT_NUM*sizeof(int));
	for(int i=0; i<nJokerCount; i++)
	{
		for(int j=0; j<nCardLen; j++)
		{
			if (m_nRazzCardsAlter[i] == GetCardValueById(nCardIDs[j]))
			{
				int index=SK_GetCardIndexEx(nCardIDs[j], 0);
				nLayTmp[index]++;
				break;
			}
		}
	}

	if(nLayTmp[nMainIndex] == count1)
		 razzAlterInCount1Ok = TRUE;

	if (nSecondIndex==nThirdIndex && count2==1 && count3==1)
	{
		if(nLayTmp[nSecondIndex] == count2+count3)
		{
			razzAlterInCount2Ok = TRUE;
			razzAlterInCount3Ok = TRUE;
		}
	}
	else
	{
		if(nLayTmp[nSecondIndex] == count2)
			razzAlterInCount2Ok = TRUE;
			
		if(nLayTmp[nThirdIndex] == count3)
			razzAlterInCount3Ok = TRUE;
	}

	if(!razzAlterInCount1Ok || !razzAlterInCount2Ok || !razzAlterInCount3Ok)
		return FALSE;

	return TRUE;
}

BOOL CGameTable::ValidateAbtSingleForRazz(int nCardIDs[], int nCardLen, int nCardLay[], int nJokerCount, int nStartIndex)
{
	int razzUsedCount = 0;
	int razzAlterTmp[MAX_RAZZ_COUNT];
	memcpy(razzAlterTmp, m_nRazzCardsAlter, sizeof(razzAlterTmp));
	for(int i=0; i<nJokerCount; i++)
	{
		for(int j=0; j<nCardLen; j++)
		{
			if(nCardLay[nStartIndex+j]==0&&nStartIndex+j<14)
			{
				if(razzAlterTmp[i] == GetCardValueByIndex(nStartIndex+j))
				{
					razzAlterTmp[i] = -1;
					razzUsedCount++;
				}
			}
		}
	}

	if(razzUsedCount != nJokerCount)
	{
		return FALSE;
	}
	return TRUE;
}

BOOL CGameTable::ValidateAbtCoupleForRazz(int nCardIDs[], int nCardLen, int nCardLay[], int nJokerCount, int nStartIndex)
{
	int razzUsedCount = 0;
	int razzAlterTmp[MAX_RAZZ_COUNT];
	memcpy(razzAlterTmp, m_nRazzCardsAlter, sizeof(razzAlterTmp));
	for(int j=0; j<nCardLen/2; j++)
	{
		if(nCardLay[nStartIndex+j]<2 && nStartIndex+j<14)
		{
			int num = 2 - nCardLay[nStartIndex+j];
			for (int i=0; i<num; i++)
			{
				for(int k=0; k<nJokerCount; k++)
				{
					if(razzAlterTmp[k] == GetCardValueByIndex(nStartIndex+j))
					{
						razzUsedCount++;
						razzAlterTmp[k] = -1;
						break;
					}
				}
			}
		}
		if(razzUsedCount >= nJokerCount)
		{
			break;
		}
	}

	if(razzUsedCount != nJokerCount)
	{
		return FALSE;
	}
	return TRUE;
}

BOOL CGameTable::ValidateAbtThreeForRazz(int nCardIDs[], int nCardLen, int nCardLay[], int joker_UsedAbt, int joker_UsedCouple, int joker_UsedSingle, int nStartIndex, int nMaxPair, int coupleIndex[])
{
	int razzUsedInAbt = 0;
	BOOL razzAlterInAbtOk = FALSE;
	BOOL razzAlterInCoupleOk = FALSE;
	BOOL razzAlterInSingleOk = FALSE;
	int nLayTmp[SK_LAYOUT_NUM];
	memset(nLayTmp, 0, SK_LAYOUT_NUM*sizeof(int));
	memcpy(nLayTmp, nCardLay, SK_LAYOUT_NUM*sizeof(int));

	for(int i=0; i<joker_UsedAbt+joker_UsedCouple+joker_UsedSingle; i++)
	{
		for(int j=1; j<SK_LAYOUT_NUM; j++)
		{
			if (m_nRazzCardsAlter[i] == GetCardValueByIndex(j))
			{
				int index = j;
				nLayTmp[index]++;
				break;
			}
		}
	}

	for(int j=0; j<nMaxPair; j++)
	{
		if(nCardLay[nStartIndex+j]<3 && nStartIndex+j<14)
		{
			if(nLayTmp[nStartIndex+j] >= 3)
			{
				razzUsedInAbt += 3-nCardLay[nStartIndex+j];
				nLayTmp[nStartIndex+j] -= 3;
			}
		}
	}
	if(razzUsedInAbt == joker_UsedAbt)
		razzAlterInAbtOk = TRUE;

	if(joker_UsedCouple>0) //验证所带对子中的癞子牌
	{
		razzAlterInSingleOk = TRUE;
		for(int i=0; i<nMaxPair; i++)
		{
			if(coupleIndex[i]<=0)
			{
				razzAlterInCoupleOk = FALSE;
				break;
			}
			else if(nLayTmp[coupleIndex[i]] != 2)
			{
				razzAlterInCoupleOk = FALSE;
				break;
			}
			razzAlterInCoupleOk = TRUE;
		}
	}
	else if(joker_UsedSingle>0)//验证所带单张中的癞子牌
	{
		razzAlterInCoupleOk = TRUE;
		int razzCardIndex = SK_GetCardIndexEx(m_nRazzCardValue, 0);
		if(nLayTmp[razzCardIndex] == joker_UsedSingle)
			razzAlterInSingleOk = TRUE;
	}
	else if(joker_UsedCouple==0 && joker_UsedSingle==0)
	{
		razzAlterInCoupleOk = TRUE;
		razzAlterInSingleOk = TRUE;
	}
	if(!razzAlterInAbtOk || !razzAlterInCoupleOk || !razzAlterInSingleOk)
		return FALSE;

	return TRUE;
}

BOOL CGameTable::ValidateSameCountForRazz(int nCardIDs[], int nCardLen, int nCardLay[], int nJokerCount)
{
	int razzUsedCount = 0;

	for(int i=0; i<nJokerCount; i++)
	{
		for(int j=0; j<nCardLen; j++)
		{
			if(m_nRazzCardsAlter[i] == GetCardValueById(nCardIDs[j]))
			{
				razzUsedCount++;
				break;
			}
		}
	}
	if(razzUsedCount != nJokerCount)
	{
		return FALSE;
	}
	return TRUE;
}

BOOL  CGameTable::GetDoubleCount(int nCardLay[], int nLayLen,int nCount1,int nCount2,int nJokerCount,int nDestValue,int& nMainIndex,int& nSecondeIndex)
{
	nMainIndex=-1;
	nSecondeIndex=-1;
	int nValue=0;
	int nJokerIndex=-1;
	for(int i=0;i<nLayLen;i++)
	{
		if (m_nRazzCardValue==GetCardValueByIndex(i) && nJokerCount==nCount1 && nCount2==2)
		{
			nJokerIndex = i;
		}
		if (!nCardLay[i] && i!=nJokerIndex) continue;

		if (i==14 || i==15) continue;  //不能与大小王搭配成三张

		int nRest=nJokerCount;
		if (nRest+nCardLay[i]>=nCount1)
		{
			int temp=nCardLay[i];
			if (nCount1>nCardLay[i])
			{
				nRest-=nCount1-nCardLay[i];//去掉财神
				nCardLay[i]=0;
			}
			else
			{
				nCardLay[i]-=nCount1;
			}

			for(int j=0;j<nLayLen;j++)
			{
				if (i==j) continue;

				if (!nCardLay[j]) continue;

				if (nRest+nCardLay[j]>=nCount2)
				{
					if (nRest>0&&nCount2==2 && (j==14||j==15))//癞子牌不能与大小王成对
					{
						continue;
					}
					int nThisValue=SK_GetIndexPRIEx(i,GetCurrentRank(), 0)*10000;
					if(!m_bIsRazzMode)
						nThisValue += SK_GetIndexPRIEx(j,GetCurrentRank(), 0);	//非癞子场带上副牌牌值

					if (nThisValue > nDestValue)
					{
						if (m_bIsRazzMode && m_bIsRemind)
						{
							if (nMainIndex==-1 || nThisValue<nValue) //提示从小到大取值
							{
								if (j==1) continue; //不从2开始判断
								if (nCardLay[j] != nCount2)
								{
									int k;
									for (k=j+1; k<14; k++) //寻找最优的不拆牌的组合
									{
										if (k==i) continue;
										if (!nCardLay[k]) continue;
										if ((k==14 && nCardLay[14] && nCardLay[15]) || (k==15 && nCardLay[14] && nCardLay[15])) continue;
										if (nCardLay[k] == nCount2)
										{
											nMainIndex = i;
											nSecondeIndex = k;
											nValue = nThisValue;
											break;
										}
									}
									if (k==14) //没有找到最优组合，判断2能否满足要求
									{
										if (nCardLay[1] && 1!=i && (nCardLay[1]==nCount2)) //找到最优组合，不拆牌
										{
											nMainIndex=i;
											nSecondeIndex=1;
											nValue=nThisValue;
										}
										else if (nCardLay[14] && (nCardLay[14]==nCount2) && !nCardLay[15]) //找到最优组合，不拆牌
										{
											nMainIndex=i;
											nSecondeIndex=14;
											nValue=nThisValue;
										}
										else if (nCardLay[15] && (nCardLay[15]==nCount2) && !nCardLay[14]) //找到最优组合，不拆牌
										{
											nMainIndex=i;
											nSecondeIndex=15;
											nValue=nThisValue;
										}
										else //没有找到含癞子的不拆非癞子牌的最优组合，寻找含癞子的不拆非癞子牌的组合
										{
											int t;
											for (t=j; t<14; t++)
											{
												if (t==i) continue;
												if (!nCardLay[t]) continue;
												if (nRest+nCardLay[t] >= nCount2 && nCardLay[t]<nCount2)
												{
													nMainIndex = i;
													nSecondeIndex = t;
													nValue = nThisValue;
													break;
												}
											}
											if (t==14)
											{
												if (nCardLay[1] && 1!=i && (nRest+nCardLay[1]>=nCount2) && nCardLay[1] < nCount2)
												{
													nMainIndex=i;
													nSecondeIndex=1;
										        	nValue=nThisValue;
												}
												else
												{
													int n;
													for (n=j; n<14; n++)  //拆非炸弹牌
													{
														if (n==i) continue;
														if (!nCardLay[n]) continue;
														if (nCardLay[n]==4)  continue;
														if (nRest+nCardLay[n] >= nCount2)
														{
															nMainIndex = i;
															nSecondeIndex = n;
															nValue = nThisValue;
															break;
														}
													}
													if (n==14)
													{
														if (nCardLay[1] && 1!=i && (nCardLay[1]!=4) && nCardLay[1]>nCount2)
														{
															nMainIndex=i;
															nSecondeIndex=1;
															nValue=nThisValue;
														}
														else //拆炸弹
														{
															nMainIndex=i;
															nSecondeIndex=j;
															nValue=nThisValue;
														}
													}
												}
											}
										}
									}
								}
								else //最优组合不拆牌
								{
									if ((j==14 || j==15) && 1!=i && ((nCardLay[1]==nCount2 ) || (nCardLay[14] && nCardLay[15] && nCardLay[1]) ))
									{
										nMainIndex=i;
										nSecondeIndex=1;
							        	nValue=nThisValue;
									}
									else
									{
										nMainIndex=i;
										nSecondeIndex=j;
							        	nValue=nThisValue;
									}
								}
							}
						}
						else if (((nMainIndex==-1 || nThisValue<nValue && !m_bIsRazzMode)
							|| (m_bIsRazzMode && nThisValue>nValue))) //癞子场取最大值
						{
							nMainIndex=i;
							nSecondeIndex=j;
							nValue=nThisValue;
						}
					}
				}
			}

			if (m_bIsRazzMode/* && nMainIndex==-1*/ && nRest==nCount2 && nCount2==2)//支持带癞子对子
			{
				for(int j=0;j<nLayLen;j++)
				{
					if (i==j) continue;
					if (j==14 || j==15) continue;
					if (m_nRazzCardValue == GetCardValueByIndex(j))
					{
						int nThisValue=SK_GetIndexPRIEx(i,GetCurrentRank(), 0)*10000;
						if (nThisValue>nDestValue)
						{
							if (m_bIsRemind)
							{
								if (nMainIndex==-1 || nThisValue<nValue) //提示从小到大取值
								{
									nMainIndex=i;
									nSecondeIndex=j;
									nValue=nThisValue;
								}
								
							}
							else if (nThisValue>nValue) //癞子场取最大值
							{
								nMainIndex=i;
								nSecondeIndex=j;
								nValue=nThisValue;
							}
						}
					}
				}
			}
			
			nCardLay[i]=temp;//还原
		}
	}

	if(m_bIsRazzMode && (nJokerCount==3 || nJokerCount==4) && nCount2==2)//三张癞子带1对，或四张癞子+1张单
	{
		for(int i=0; i<nLayLen; i++)
		{
			if( m_nRazzCardValue != GetCardValueByIndex(i))
			{
				continue;
			}
			for(int j=0; j<nLayLen; j++)
			{
				if (j == i) continue;
				if (j==14 || j==15) continue;
				if ( (nCardLay[j]==2&&nJokerCount==3) || (nCardLay[j]==1&&nJokerCount==4)  )
				{
            		int nThisValue=SK_GetIndexPRIEx(i,GetCurrentRank(), 0)*10000;
					if (nThisValue > nDestValue)
					{
						if (m_bIsRemind)
						{
							if (nMainIndex==-1 || nThisValue<nValue)
							{
								nMainIndex=i;
								nSecondeIndex=j;
						    	nValue=nThisValue;
							}
						}
						else if ( nThisValue>nValue) //癞子场取最大值
						{
							nMainIndex=i;
							nSecondeIndex=j;
							nValue=nThisValue;
						}
					}
				}
			}
		}
	}

	if (m_bIsRazzMode && (nJokerCount == 3) && nCount2 == 1)//三张癞子带1王
	{
		for (int i = 0; i < nLayLen; i++)
		{
			if (m_nRazzCardValue != GetCardValueByIndex(i))
			{
				continue;
			}

			for (int j = 0; j < nLayLen; j++)
			{
				if (j == i) continue;
				if (j == 14 || j == 15) {
					if ((nCardLay[j] == 1 && nJokerCount == 3))
					{
						int nThisValue = SK_GetIndexPRIEx(i, GetCurrentRank(), 0) * 10000;
						if (nThisValue > nDestValue)
						{
							if (m_bIsRemind)
							{
								if (nMainIndex == -1 || nThisValue < nValue)
								{
									nMainIndex = i;
									nSecondeIndex = j;
									nValue = nThisValue;
								}
							}
							else if (nThisValue > nValue) //癞子场取最大值
							{
								nMainIndex = i;
								nSecondeIndex = j;
								nValue = nThisValue;
							}
						}
					}
				};
			}
		}
	}

	if (nMainIndex==-1)
		return FALSE;
	else 
		return TRUE;
}

int CGameTable::PreDealCards(int nCardIDs[],int nCardLen,int nCardLay[], int nLayLen,int& nJokerCount)
{
	nJokerCount=0;
	int nCardCount=0;
	for(int i=0;i<nCardLen;i++)
	{
		if (nCardIDs[i]>=0&&nCardIDs[i]<m_nTotalCards)
		{
			if (IsJoker(nCardIDs[i])
				&&IS_BIT_SET(m_dwGameFlags,GAME_FLAGS_USE_JOKER))
			{
				nJokerCount++;//提出财神
			}
			else
			{
				int index=SK_GetCardIndexEx(nCardIDs[i], 0);
				nCardLay[index]++;
			}
			nCardCount++;
		}
	}
	
	if (nJokerCount==nCardCount)
	{
		for(int i=0;i<nCardLen;i++)
		{
			if (nCardIDs[i]>=0&&nCardIDs[i]<m_nTotalCards)
			{
				int index=SK_GetCardIndexEx(nCardIDs[i], 0);
				nCardLay[index]++;
			}
		}
// 		if(!m_bIsRazzMode)
// 		{
// 			nJokerCount=0;
// 		}
		nJokerCount=0;
	}
	
	return nCardCount;
}

int CGameTable::GetChairCards(int nChairNO, int nCardIDs[], int nLen)
{
	memset(nCardIDs, INVALID_OBJECT_ID, sizeof(int) * nLen);
	
	int count=0;
	for(int i=0; i<m_nTotalCards; i++)
	{
		if(GetPublicInfo()->GameCard[i].nChairNO==nChairNO
			&& GetPublicInfo()->GameCard[i].nCardStatus==CARD_STATUS_INHAND)		
			nCardIDs[count++]=GetPublicInfo()->GameCard[i].nCardID;
		
		if(count == nLen)
			break;
	}
	
	return count;
}

void CGameTable::OnAutoAuction(int nChairNO, AUCTION_BANKER* pAuctionBanker, 
	BOOL bRemote, std::function<std::remove_pointer_t<decltype(CAI_Dll::m_AI_AutoAuction)>> cbRmote)
{
	if (bRemote && !cbRmote)
	{
		m_pGameServer->PostAIEngineAI(this, nChairNO, {
			{ nChairNO, CAI_Dll::e_AI_AutoAuction }
		});
		return;
	}

	BOOL isUseRemoteAI = FALSE;
	if (Robot_IsUseRemoteAI() && cbRmote)
	{
		isUseRemoteAI = TRUE;
	}
	else if (Robot_IsUseRemoteAI() && !cbRmote)
	{
		LOG_ERROR(_T("[RobotAI] OnAutoAuction, useRemoteAI true but cbRemote is NULL!!!, userId=%d, roomId=%d, tableNo=%d"),
			pAuctionBanker->nUserID, m_nRoomID, m_nTableNO);
	}
	else if (!Robot_IsUseRemoteAI() && cbRmote)
	{
		LOG_ERROR(_T("[RobotAI] OnAutoAuction, useRemoteAI false but cbRemote not NULL!!!, userId=%d, roomId=%d, tableNo=%d"),
			pAuctionBanker->nUserID, m_nRoomID, m_nTableNO);
	}

	if (isUseRemoteAI == TRUE)
	{
		pAuctionBanker->nGains = cbRmote(-1);
		if (pAuctionBanker->nGains == 0)
			pAuctionBanker->bPassed = TRUE;

#ifdef DEBUG
		LOG_INFO(_T("[RobotAIRemote] OnRobotAuction, roomId=%d, tableId=%d, userId=%d"), m_nRoomID, m_nTableNO, pAuctionBanker->nUserID);
#endif
	}
	else
	{
		pAuctionBanker->nGains = m_GameAI[nChairNO].AutoAuction();
		if (pAuctionBanker->nGains == 0)
			pAuctionBanker->bPassed = TRUE;

#ifdef DEBUG
		LOG_INFO(_T("[RobotAIJunior] OnRobotAuction, roomId=%d, tableId=%d, userId=%d"), m_nRoomID, m_nTableNO, pAuctionBanker->nUserID);
#endif
	}

	UwlTrace(_T("OnAutoAuction nChairNO=%d, gains=%d"), nChairNO, pAuctionBanker->nGains);
}

void CGameTable::OnAutoDouble(int nChairNO, PLAYER_DOUBLE* pAuctionBanker,
	BOOL bRemote, std::function<std::remove_pointer_t<decltype(CAI_Dll::m_AI_AutoAuction)>> cbRmote)
{
	if (bRemote && !cbRmote)
	{
		m_pGameServer->PostAIEngineAI(this, nChairNO, {
			{ nChairNO, CAI_Dll::e_AI_AutoDouble }
		});
		return;
	}

	BOOL isUseRemoteAI = FALSE;
	if (Robot_IsUseRemoteAI() && cbRmote)
	{
		isUseRemoteAI = TRUE;
	}
	else if (Robot_IsUseRemoteAI() && !cbRmote)
	{
		LOG_ERROR(_T("[RobotAI] OnAutoDouble, useRemoteAI true but cbRemote is NULL!!!, userId=%d, roomId=%d, tableNo=%d"),
			pAuctionBanker->nUserID, m_nRoomID, m_nTableNO);
	}
	else if (!Robot_IsUseRemoteAI() && cbRmote)
	{
		LOG_ERROR(_T("[RobotAI] OnAutoDouble, useRemoteAI false but cbRemote not NULL!!!, userId=%d, roomId=%d, tableNo=%d"),
			pAuctionBanker->nUserID, m_nRoomID, m_nTableNO);
	}

	if (isUseRemoteAI == TRUE)
	{
		pAuctionBanker->nDoubleType = cbRmote(-1);

#ifdef DEBUG
		LOG_INFO(_T("[RobotAIRemote] OnAutoDouble, roomId=%d, tableId=%d, userId=%d"), m_nRoomID, m_nTableNO, pAuctionBanker->nUserID);
#endif
	}
	else
	{
		pAuctionBanker->nDoubleType = 0;

#ifdef DEBUG
		LOG_INFO(_T("[RobotAIJunior] OnAutoDouble, roomId=%d, tableId=%d, userId=%d"), m_nRoomID, m_nTableNO, pAuctionBanker->nUserID);
#endif
	}

	UwlTrace(_T("OnAutoDouble nChairNO=%d, doubleType=%d"), nChairNO, pAuctionBanker->nDoubleType);
}

void CGameTable::OnAutoRob(int nChairNO, ROB_BANKER* pRobBanker, 
	BOOL bRemote, std::function<std::remove_pointer_t<decltype(CAI_Dll::m_AI_AutoRob)>> cbRmote)
{
	if (bRemote && !cbRmote)
	{
		if (0 == m_nAuctionCount) {
			m_pGameServer->PostAIEngineAI(this, nChairNO, {
				{ nChairNO, CAI_Dll::e_AI_AutoAuction }
			});
		}
		else if (m_nAuctionCount > 0) {
			m_pGameServer->PostAIEngineAI(this, nChairNO, {
				{ nChairNO, CAI_Dll::e_AI_AutoRob }
			});
		}
		return;
	}

	BOOL isUseRemoteAI = FALSE;
	if (Robot_IsUseRemoteAI() && cbRmote)
	{
		isUseRemoteAI = TRUE;
	}
	else if (Robot_IsUseRemoteAI() && !cbRmote)
	{
		LOG_ERROR(_T("[RobotAI] OnAutoRob, useRemoteAI true but cbRemote is NULL!!!, userId=%d, roomId=%d, tableNo=%d"),
			pRobBanker->nUserID, m_nRoomID, m_nTableNO);
	}
	else if (!Robot_IsUseRemoteAI() && cbRmote)
	{
		LOG_ERROR(_T("[RobotAI] OnAutoRob, useRemoteAI false but cbRemote not NULL!!!, userId=%d, roomId=%d, tableNo=%d"),
			pRobBanker->nUserID, m_nRoomID, m_nTableNO);
	}

	if (isUseRemoteAI == TRUE)
	{
		if (0 == m_nAuctionCount){
			if (cbRmote(-1) > 0)
			{
				pRobBanker->bCalled = TRUE;
			}
			else {
				pRobBanker->bNoCall = TRUE;
			}
		}
		else if (m_nAuctionCount > 0){
			if (cbRmote(-1) > 0)
			{
				pRobBanker->bRobbed = TRUE;
			}
			else{
				pRobBanker->bNoRob = TRUE;
			}
		}

#ifdef DEBUG
		LOG_INFO(_T("[RobotAIRemote] OnRobotRob, roomId=%d, tableId=%d, userId=%d"), m_nRoomID, m_nTableNO, pRobBanker->nUserID);
#endif
	}
	else
	{
		int nWeight = m_GameAI[nChairNO].CalcHandCardWeight();
#if _DEBUG
		UwlLogFile(_T("ChairNo=%ld, Weight=%ld"), nChairNO, nWeight);
#endif
		if (m_nCurRobotAIType == ROBOTAI_JUNIOR)
		{
			for (int i = 0; i < TOTAL_CHAIRS; i++)
			{
				if (m_Rob[i].bCalled == TRUE || m_Rob[i].bRobbed == TRUE)
				{
					if (0 == m_nAuctionCount)
						pRobBanker->bNoCall = TRUE;
					else if (m_nAuctionCount > 0)
						pRobBanker->bNoRob = TRUE;
					return;
				}
			}
			if (0 == m_nAuctionCount)
				pRobBanker->bCalled = TRUE;
			else if (m_nAuctionCount > 0)
				pRobBanker->bRobbed = TRUE;
			return;
		}
		if (1 == GetRobotCount())
		{
			CString strIniFile = GetINIFileName();
			TCHAR szRoomID[16];
			memset(szRoomID, 0, sizeof(szRoomID));
			_stprintf(szRoomID, _T("%ld"), m_nRoomID);

			BOOL bRobotSpecialAuctionMode = GetPrivateProfileInt(
				_T("RobotSpecialAuctionMode"),//是否是机器人特殊叫地主模式
				szRoomID,
				FALSE,
				strIniFile);

			if (TRUE == bRobotSpecialAuctionMode && nWeight <= 4)
			{
				if (0 == m_nAuctionCount)
					pRobBanker->bCalled = TRUE;
				else if (m_nAuctionCount > 0)
					pRobBanker->bRobbed = TRUE;

				return;
			}
		}

		if (0 == m_nAuctionCount)
		{//叫地主
			if (nWeight >= ROBOT_ROB_WEIGHT_1)
				pRobBanker->bCalled = TRUE;
			else
				pRobBanker->bNoCall = TRUE;
		}
		else if (m_nAuctionCount > 0)
		{//抢地主
			srand(time(NULL) + nWeight);
			if (nWeight >= ROBOT_ROB_WEIGHT_3)//100%抢地主
				pRobBanker->bRobbed = TRUE;
			else if (nWeight >= ROBOT_ROB_WEIGHT_2 && nWeight<ROBOT_ROB_WEIGHT_3 && 0 == rand() % 2)//50%抢地主
				pRobBanker->bRobbed = TRUE;
			else
				pRobBanker->bNoRob = TRUE;
		}
		else
		{
			UwlLogFile(_T("OnAutoRob failed. m_nAuctionCount(%ld) is wrong."), m_nAuctionCount);
		}

#ifdef DEBUG
		LOG_INFO(_T("[RobotAIJunior] OnRobotRob, roomId=%d, tableId=%d, userId=%d"), m_nRoomID, m_nTableNO, pRobBanker->nUserID);
#endif
	}
}

void CGameTable::OnAutoThrow(int nChairNO, BOOL &bThrow, CARDS_THROW* pThrowCards, 
	BOOL bRemote, std::function<std::remove_pointer_t<decltype(CAI_Dll::m_AI_AutoThrow)>> cbRmote)
{
	BOOL bFirstHand = (GetPublicInfo()->nWaitChair==INVALID_OBJECT_ID);

	if (bRemote && !cbRmote)
	{
		LOG_INFO(_T("[RobotAI] OnAutoThrow, useRemoteAI true and PostAIEngineAI, userId=%d, roomId=%d, tableNo=%d, chairNo=%d"),
			pThrowCards->nUserID, m_nRoomID, m_nTableNO, nChairNO);

		CString strIniFile = GetINIFileName();

		{
			Json::Value data(Json::objectValue);
			data["firstHand"] = bFirstHand;

			Json::StreamWriterBuilder builder;
			const std::string json = Json::writeString(builder, data);

			LOG_INFO("%s json:%s", __FUNCTION__, json);


			std::vector<CAIEngineItem> vecAIEngineItems;
			vecAIEngineItems.push_back(CAIEngineItem{ nChairNO, CAI_Dll::e_AI_AutoThrow, json });

			m_pGameServer->PostAIEngineAI(this, nChairNO, vecAIEngineItems);
		}
		return;
	}

	BOOL isUseRemoteAI = FALSE;
	if (Robot_IsUseRemoteAI() && cbRmote)
	{
		isUseRemoteAI = TRUE;
	}
	else if (Robot_IsUseRemoteAI() && !cbRmote)
	{
		LOG_ERROR(_T("[RobotAI] OnAutoThrow, useRemoteAI true but cbRemote is NULL!!!, userId=%d, roomId=%d, tableNo=%d"),
			pThrowCards->nUserID, m_nRoomID, m_nTableNO);
	}
	else if (!Robot_IsUseRemoteAI() && cbRmote)
	{
		LOG_ERROR(_T("[RobotAI] OnAutoThrow, useRemoteAI false but cbRemote not NULL!!!, userId=%d, roomId=%d, tableNo=%d"),
			pThrowCards->nUserID, m_nRoomID, m_nTableNO);
	}

	if (isUseRemoteAI == TRUE)
	{
		DWORD dwType = CARD_UNITE_TYPE_TOTAL_EX;
		pThrowCards->unite.nCardCount = cbRmote(-1, bFirstHand, pThrowCards->unite.nCardIDs, CHAIR_CARDS, &dwType);
		if (dwType == CARD_UNITE_TYPE_BOMB) {
			dwType |= CARD_UNITE_TYPE_MIXEDRAZZ_BOMB | CARD_UNITE_TYPE_PURERAZZ_BOMB;
		}

		if (pThrowCards->unite.nCardCount > 0)
		{
			bThrow = TRUE;

			CARD_UNITE unit_details;
			ZeroMemory(&unit_details, sizeof(CARD_UNITE));
			int nTempIndex = m_razzCardsAlterValueUnit.nTypeCount;
			if (!GetUniteDetails(nChairNO, pThrowCards->unite.nCardIDs, pThrowCards->unite.nCardCount, unit_details, dwType)
				&& !GetUniteDetails(nChairNO, pThrowCards->unite.nCardIDs, pThrowCards->unite.nCardCount, unit_details, CARD_UNITE_TYPE_TOTAL_EX))
			{
				TCHAR szCardIds[MAX_CARDS_PER_CHAIR] = { 0 };
				int nPos = 0;
				for (int i = 0; i < pThrowCards->unite.nCardCount; i++)
				{
					nPos += _sntprintf(szCardIds + nPos, sizeof(szCardIds), "%d ", pThrowCards->unite.nCardIDs[i]);
				}
				CString szLogMsg = "";
				szLogMsg.Format("[RobotAI] roomid %d AutoThrow Err, CardIds[%s], Type[%d], isRazzMode[%d],razzValue[%d]", this->m_nRoomID, szCardIds, dwType, m_bIsRazzMode, m_nRazzCardValue);
				//CConfigManagerSys::GetInstance()->SendDingDing(szLogMsg.GetBuffer());
				//UwlLogFile(szLogMsg.GetBuffer());
				//szLogMsg.ReleaseBuffer();
				LOG_ERROR(std::string(CT2A(szLogMsg.GetString())).c_str());
			}
			pThrowCards->unite = unit_details.uniteType[0];

			memcpy(pThrowCards->nReserved4, m_razzCardsAlterValueUnit.razzCardsAlterValue[nTempIndex].nRazzAlterValue, MAX_RAZZ_COUNT * sizeof(int));
			m_razzCardsAlterValueUnit.nTypeCount = nTempIndex;
		}
		else//pass
			bThrow = FALSE;

#ifdef DEBUG
		LOG_INFO(_T("[RobotAIRemote] OnRobotThrow, roomId=%d, tableId=%d, userId=%d"), m_nRoomID, m_nTableNO, pThrowCards->nUserID);
#endif
	}
	else
	{
		pThrowCards->unite.nCardCount = m_GameAI[nChairNO].AutoThrow(bFirstHand, pThrowCards->unite.nCardIDs, CHAIR_CARDS);

		if (pThrowCards->unite.nCardCount > 0)
		{
			bThrow = TRUE;
			CARD_UNITE unit_details;
			ZeroMemory(&unit_details, sizeof(CARD_UNITE));
			GetUniteDetails(nChairNO, pThrowCards->unite.nCardIDs, pThrowCards->unite.nCardCount, unit_details, CARD_UNITE_TYPE_TOTAL_EX);
			pThrowCards->unite = unit_details.uniteType[0];
		}
		else//pass
			bThrow = FALSE;

#ifdef DEBUG
		LOG_INFO(_T("[RobotAIJunior] OnRobotThrow, roomId=%d, tableId=%d, userId=%d"), m_nRoomID, m_nTableNO, pThrowCards->nUserID);
#endif
	}
}

int CGameTable::AI_GetCurrentActionSeq(BOOL bSkipChat)
{
	for (auto i = m_objAIActions.size(); i > 0; i--)
	{
		if (m_objAIActions[i - 1].nType != CAI_Dll::e_AI_Chat) {
			return i;
		}
	}

	return 0;
}

void  CGameTable::OnAutoChat(int nChairNO, CHAT_TO_TABLE* pChatToTable,
	BOOL bRemote, std::function<std::remove_pointer_t<decltype(CAI_Dll::m_AI_AutoChat)>> cbRmote)
{
	if (bRemote && !cbRmote)
	{
		m_pGameServer->PostAIEngineAI(this, nChairNO, {
			{ nChairNO, CAI_Dll::e_AI_Chat }
			});
		return;
	}

	BOOL isUseRemoteAI = FALSE;
	if (Robot_IsUseRemoteAI() && cbRmote)
	{
		isUseRemoteAI = TRUE;
	}
	else if (Robot_IsUseRemoteAI() && !cbRmote)
	{
		LOG_ERROR(_T("[RobotAI] OnAutoChat, useRemoteAI true but cbRemote is NULL!!!, userId=%d, roomId=%d, tableNo=%d"),
			pChatToTable->nUserID, m_nRoomID, m_nTableNO);
	}
	else if (!Robot_IsUseRemoteAI() && cbRmote)
	{
		LOG_ERROR(_T("[RobotAI] OnAutoChat, useRemoteAI false but cbRemote not NULL!!!, userId=%d, roomId=%d, tableNo=%d"),
			pChatToTable->nUserID, m_nRoomID, m_nTableNO);
	}

	if (isUseRemoteAI == TRUE)
	{
		cbRmote(-1);

#ifdef DEBUG
		LOG_INFO(_T("[RobotAIRemote] OnAutoChat, roomId=%d, tableId=%d, userId=%d"), m_nRoomID, m_nTableNO, pChatToTable->nUserID);
#endif
	}

	UwlTrace(_T("OnAutoChat nChairNO=%d, msg=%s"), nChairNO, pChatToTable->szChatMsg);
}


int CGameTable::GetPlayerCountOnTable()
{
	int nCount = 0;
	
	for (int i=0;i<m_nTotalChairs;i++)
	{
		if (m_ptrPlayers[i]&&m_ptrPlayers[i]->m_nUserID!=0)
			nCount++;
	}
	
	return nCount;
}

/////////////////////////////////////////////////////////癞子/////////////////////////////////////////////////////
BOOL CGameTable::CalcRazzValueInSame(int nJokerCount, int nCardIndex)
{
	if (nCardIndex<1)
		return FALSE;
	
	int nRazzUsedCount = 0;

	int &nTypeIndex=m_razzCardsAlterValueUnit.nTypeCount;
	for(int i=0; i<nJokerCount, nRazzUsedCount<nJokerCount; i++)
	{
		m_razzCardsAlterValueUnit.razzCardsAlterValue[nTypeIndex].nRazzAlterValue[nRazzUsedCount++] = GetCardValueByIndex(nCardIndex);
	}
	
	if(nRazzUsedCount != nJokerCount)
		return FALSE;

	nTypeIndex++;

	return TRUE;
}

BOOL CGameTable::CalcRazzValueInDoubleCount(int nCardLay[], int nJokerCount, int count1, int count2, int nMainIndex, int nSecondIndex)
{
	if (nMainIndex<1 || nSecondIndex<1)
		return FALSE;

	int &nTypeIndex=m_razzCardsAlterValueUnit.nTypeCount;

	int nRazzUsedCount = 0;
	int nLayTmp[SK_LAYOUT_NUM];
	memcpy(nLayTmp, nCardLay, sizeof(nLayTmp));
	for(int i=0; i<nJokerCount; i++)
	{
		if (nLayTmp[nMainIndex] < count1)//计算三张中癞子牌改变的牌值
		{
			m_razzCardsAlterValueUnit.razzCardsAlterValue[nTypeIndex].nRazzAlterValue[i] = GetCardValueByIndex(nMainIndex);
			nLayTmp[nMainIndex]++;
			nRazzUsedCount++;
			continue;
		}
		if (nLayTmp[nSecondIndex] < count2)//计算单张或对子中癞子牌改变的牌值
		{
			m_razzCardsAlterValueUnit.razzCardsAlterValue[nTypeIndex].nRazzAlterValue[i] = GetCardValueByIndex(nSecondIndex);
			nLayTmp[nSecondIndex]++;
			nRazzUsedCount++;
		}
	}
	
	if(nRazzUsedCount != nJokerCount)
		return FALSE;
	
	nTypeIndex++;

	return TRUE;
}

BOOL CGameTable::CalcRazzValueInThreeCount(int nCardLay[], int nJokerCount, int count1, int count2, int count3, int nMainIndex, int nSecondIndex, int nThirdIndex)
{
	if(nMainIndex<1 || nSecondIndex<1 || nThirdIndex<1)
		return FALSE;

	int &nTypeIndex=m_razzCardsAlterValueUnit.nTypeCount;
	
	int nRazzUsedCount = 0;
	int nLayTmp[SK_LAYOUT_NUM];
	int nRazzAlterIndex = 0;
	memcpy(nLayTmp, nCardLay, sizeof(nLayTmp));
	for(int i=0; i<nJokerCount; i++)
	{
		if (nLayTmp[nMainIndex] < count1)//4张中补癞子
		{
			m_razzCardsAlterValueUnit.razzCardsAlterValue[nTypeIndex].nRazzAlterValue[nRazzAlterIndex]=GetCardValueByIndex(nMainIndex);
			nLayTmp[nMainIndex]++;
			nRazzUsedCount++;
			nRazzAlterIndex++;
			continue;
		}
		if (nSecondIndex==nThirdIndex && count2==1 && count3==1) //带2张相同的牌时，补2张癞子
		{
			if (nLayTmp[nSecondIndex] < count2+count3)
			{
				m_razzCardsAlterValueUnit.razzCardsAlterValue[nTypeIndex].nRazzAlterValue[nRazzAlterIndex++] = GetCardValueByIndex(nSecondIndex);
				m_razzCardsAlterValueUnit.razzCardsAlterValue[nTypeIndex].nRazzAlterValue[nRazzAlterIndex++] = GetCardValueByIndex(nSecondIndex);
				nLayTmp[nSecondIndex] += 2;
				nRazzUsedCount += 2;
				continue;
			}
		}
		else
		{
			if (nLayTmp[nSecondIndex] < count2)//带的第1个部分的单张或对子中补癞子
			{
				m_razzCardsAlterValueUnit.razzCardsAlterValue[nTypeIndex].nRazzAlterValue[nRazzAlterIndex]=GetCardValueByIndex(nSecondIndex);
				nRazzAlterIndex++;
				nLayTmp[nSecondIndex]++;
				nRazzUsedCount++;
				continue;
			}
			if (nLayTmp[nThirdIndex] < count3)//带的第2个部分的单张或对子中补癞子
			{
				m_razzCardsAlterValueUnit.razzCardsAlterValue[nTypeIndex].nRazzAlterValue[nRazzAlterIndex] = GetCardValueByIndex(nThirdIndex);
				nRazzAlterIndex++;
				nLayTmp[nThirdIndex]++;
				nRazzUsedCount++;
				continue;
			}
		}
	}
	
	if(nRazzUsedCount != nJokerCount)
		return FALSE;

	nTypeIndex++;
	
	return TRUE;
}

BOOL CGameTable::CalcRazzValueInAbtSingle(int nCardLay[], int nJokerCount, int nStartIndex, int nCardCount)
{
	int &nTypeIndex=m_razzCardsAlterValueUnit.nTypeCount;

	int nRazzUsedCount = 0;
	int nLayTmp[SK_LAYOUT_NUM];
	memcpy(nLayTmp, nCardLay, sizeof(nLayTmp));
	for(int j=0; j<nCardCount; j++)
	{
		if(nLayTmp[nStartIndex+j]<1 && nStartIndex+j<14)
		{
			m_razzCardsAlterValueUnit.razzCardsAlterValue[nTypeIndex].nRazzAlterValue[nRazzUsedCount] = GetCardValueByIndex(nStartIndex+j);
			nRazzUsedCount++;
		}
		if (nRazzUsedCount>=nJokerCount)
			break;
	}
	
	if(nRazzUsedCount != nJokerCount)
		return FALSE;

	nTypeIndex++;
	
	return TRUE;
}

BOOL CGameTable::CalcRazzValueInAbtCouple(int nCardLay[], int nJokerCount, int nStartIndex, int nMaxPair)
{
	int &nTypeIndex=m_razzCardsAlterValueUnit.nTypeCount;

	int nRazzUsedCount = 0;
	int nLayTmp[SK_LAYOUT_NUM];
	memcpy(nLayTmp, nCardLay, sizeof(nLayTmp));
	for(int j=0; j<nMaxPair; j++)
	{
		if(nLayTmp[nStartIndex+j]<2 && nStartIndex+j<14)
		{
			int num = 2 - nLayTmp[nStartIndex+j];
			for (int i=0; i<num; i++)
			{
				m_razzCardsAlterValueUnit.razzCardsAlterValue[nTypeIndex].nRazzAlterValue[nRazzUsedCount]=GetCardValueByIndex(nStartIndex+j);
				nRazzUsedCount++;
			}
		}
		if (nRazzUsedCount>=nJokerCount)
			break;
	}
	
	if(nRazzUsedCount != nJokerCount)
		return FALSE;
	
	nTypeIndex++;

	return TRUE;
}

BOOL CGameTable::CalcRazzValueInAbtThree(int nCardLay[], int nJokerCountAbt, int nJokerCountCouple, int nJokerCountSingle, int nStartIndex, int nMaxPair, int coupleIndex[])
{
	if (nJokerCountAbt==0 && nJokerCountCouple==0 && nJokerCountSingle==0)
		return TRUE;

	int &nTypeIndex=m_razzCardsAlterValueUnit.nTypeCount;  //一手牌中，计算不同的癞子牌型中第几种牌型
	
	int nRazzAlterIndex = 0;
	int nRazzUsedInAbt = 0;
	int nRazzUsedInCouple = 0;
	int nRazzUsedInSingle = 0;
	int nLayTmp[SK_LAYOUT_NUM];
	memset(nLayTmp, 0, SK_LAYOUT_NUM*sizeof(int));
	memcpy(nLayTmp, nCardLay, SK_LAYOUT_NUM*sizeof(int));
	
	for(int j=0; j<nMaxPair; j++)
	{
		if(nLayTmp[nStartIndex+j]<3 && nStartIndex+j<14)
		{
			int num = 3 - nLayTmp[nStartIndex+j];
			for (int i=0; i<num; i++)
			{
				m_razzCardsAlterValueUnit.razzCardsAlterValue[nTypeIndex].nRazzAlterValue[nRazzAlterIndex]=GetCardValueByIndex(nStartIndex+j);
				nRazzUsedInAbt++;
				nRazzAlterIndex++;
			}
		}
		if (nRazzUsedInAbt>=nJokerCountAbt)
			break;
	}
	
	if (nJokerCountCouple > 0)//计算所带对子中的癞子牌牌值
	{
		for(int i=0; i<nJokerCountCouple; i++)
		{
			if (nLayTmp[coupleIndex[0]]<2)
			{
				m_razzCardsAlterValueUnit.razzCardsAlterValue[nTypeIndex].nRazzAlterValue[nRazzAlterIndex]=GetCardValueByIndex(coupleIndex[0]);
				nRazzUsedInCouple++;
				nRazzAlterIndex++;
				nLayTmp[coupleIndex[0]]++;
			}
			else if (nLayTmp[coupleIndex[1]]<2)
			{
				m_razzCardsAlterValueUnit.razzCardsAlterValue[nTypeIndex].nRazzAlterValue[nRazzAlterIndex]=GetCardValueByIndex(coupleIndex[1]);
				nRazzUsedInCouple++;
				nRazzAlterIndex++;
				nLayTmp[coupleIndex[1]]++;
			}
			else if (coupleIndex[2]>0 && nLayTmp[coupleIndex[2]]<2)
			{
				m_razzCardsAlterValueUnit.razzCardsAlterValue[nTypeIndex].nRazzAlterValue[nRazzAlterIndex]=GetCardValueByIndex(coupleIndex[2]);
				nRazzUsedInCouple++;
				nRazzAlterIndex++;
				nLayTmp[coupleIndex[2]]++;
			}
			else if (coupleIndex[3]>0 && nLayTmp[coupleIndex[3]]<2)
			{
				m_razzCardsAlterValueUnit.razzCardsAlterValue[nTypeIndex].nRazzAlterValue[nRazzAlterIndex]=GetCardValueByIndex(coupleIndex[3]);
				nRazzUsedInCouple++;
				nRazzAlterIndex++;
				nLayTmp[coupleIndex[3]]++;
			}
			if (nRazzUsedInCouple >= nJokerCountCouple)
				break;
		}
	}
	else if (nJokerCountSingle > 0)//计算所带单张中的癞子牌牌值
	{
		for (int i=0; i<nJokerCountSingle; i++)
		{
			m_razzCardsAlterValueUnit.razzCardsAlterValue[nTypeIndex].nRazzAlterValue[nRazzAlterIndex] = m_nRazzCardValue;
			nRazzAlterIndex++;
			nRazzUsedInSingle++;
		}
	}
	
	if(nRazzUsedInAbt!=nJokerCountAbt || nRazzUsedInCouple!=nJokerCountCouple || nRazzUsedInSingle!=nJokerCountSingle)
		return FALSE;

	nTypeIndex++;
	
	return TRUE;
}

void CGameTable::SetSuppressChairNo(int nChairNo)
{
	m_nSuppRessChairNo = nChairNo;
}

BOOL CGameTable::IsSuppress(int nChairNo)
{
	BOOL bIsSuppress = FALSE;
	if(m_nSuppRessChairNo == nChairNo && m_nSuppRessChairNo != -1)
	{
		bIsSuppress = TRUE;
		m_nSuppRessChairNo = -1;
	}	
	return bIsSuppress;
}


int	CGameTable::GetPlayerCardIDs(int chairno, int nCardIDs[], int len)
{
	//TOTAL_CARDS
	GAME_PUBLIC_INFO* pPublicInfo = GetPublicInfo();
	if (!pPublicInfo) return -1;
	if (len == 0) return -1;
	
	int cnt = 0;
	for (int i = 0; i < TOTAL_CARDS; i++)
	{
		if (pPublicInfo->GameCard[i].nChairNO == chairno)
		{
			nCardIDs[cnt] = pPublicInfo->GameCard[i].nCardID;
			cnt++;
			if (cnt == len)
			{
				break;
			}
		}
	}

	return 0;
}

CPlayer* CGameTable::GetPlayerByUserID(int userid)
{
	for (int i = 0; i < TOTAL_CHAIRS; i++)
	{
		if (m_ptrPlayers[i] && m_ptrPlayers[i]->m_nUserID == userid)
		{
			return m_ptrPlayers[i];
		}
	}
	return NULL;
}
void CGameTable::SetPlayerReportedStatus(int userid, int reportedUserid, int status)
{
	CPlayer* pSubmitReportPlayer = GetPlayerByUserID(userid); //提交举报信息玩家（发起）
	CPlayer* pReportedPlayer = GetPlayerByUserID(reportedUserid);	//被举报玩家
	if (pReportedPlayer == NULL || pSubmitReportPlayer == NULL)
	{
		return;
	}
	//根据玩家的chairNO确定在数组中的位置
	PLAYER_REPORT_STATUS& playerReport = m_nPlayerReportedStatus[pSubmitReportPlayer->m_nChairNO];
	playerReport.nUserID = userid;

	//同样使用chairNO确定在数组中的位置
	REPORTED_STATUS& reportStatus = playerReport.playerReportedStatus[pReportedPlayer->m_nChairNO];
	reportStatus.nUserID = reportedUserid;
	reportStatus.nReportedStatus = status;
}

void CGameTable::ResetPlayerReportedStatus()
{
	ZeroMemory(m_nPlayerReportedStatus, sizeof(PLAYER_REPORT_STATUS) * TOTAL_CHAIRS);
}

BOOL CGameTable::FillPlayerReportedStatus(void* pData, int dataLen, int userid)
{
	if (pData == NULL)
	{
		return FALSE;
	}
	try
	{
		CPlayer* pReportPlayer = GetPlayerByUserID(userid); //提交举报信息玩家（发起）
		if (pReportPlayer == NULL)
		{
			UwlLogFile("断线续玩玩家信息指针为空");
			return FALSE;
		}
		PLAYER_REPORT_STATUS& statusData = m_nPlayerReportedStatus[pReportPlayer->m_nChairNO];
		memcpy(pData, &statusData, dataLen);
	}
	catch (...)
	{
		UwlLogFile("断线续玩填充玩家数据失败！");
		return FALSE;
	}
	return TRUE;
}

int CGameTable::GetPlayerReportedStatusSize()
{
	return sizeof(PLAYER_REPORT_STATUS);
}

void CGameTable::FillupHandCardsInfo(LPHANDCARDS_INFO pHandcards)
{
	ZeroMemory(pHandcards, sizeof(HANDCARDS_INFO));
	for (int i = 0; i < TOTAL_CHAIRS; ++i)
	{
		GetInHandCard(i, pHandcards->nHandID[i]);
	}
}

void CGameTable::InitPlayerReportedStatusByValue(int val)
{
	for (int i = 0; i < TOTAL_CHAIRS; i++)
	{
		CPlayer* pPlayer = m_ptrPlayers[i];
		//自己的userid
		m_nPlayerReportedStatus[i].nUserID = pPlayer->m_nUserID;
		//其他玩家状态
		for (int j = 0; j < TOTAL_CHAIRS; j++)
		{
			if (j == i)
			{
				continue;
			}
			m_nPlayerReportedStatus[i].playerReportedStatus[j].nReportedStatus = val;
			m_nPlayerReportedStatus[i].playerReportedStatus[j].nUserID = m_ptrPlayers[j]->m_nUserID;
		}
	}
}

int CGameTable::GetOtherUserID(int userid, int reportedUserid)
{
	for (int i = 0; i < TOTAL_CHAIRS; i++)
	{
		if (m_nPlayerReportedStatus[i].nUserID == userid)
		{
			PLAYER_REPORT_STATUS& playerReportStatus = m_nPlayerReportedStatus[i];

			for (int k = 0; k < TOTAL_CHAIRS; k++)
			{
				REPORTED_STATUS& reportStatus = playerReportStatus.playerReportedStatus[k];
				if (reportStatus.nUserID != reportedUserid && reportStatus.nUserID != 0)
				{
					return reportStatus.nUserID;
				}
			}
			break;
		}
	}
	return 0;
}

int CGameTable::GetBaseDeposit(int deposit_mult)
{
	// 计算这一局的基本银子
	int deposits[MAX_CHAIRS_PER_TABLE];
	ZeroMemory(deposits, sizeof(deposits));
	for (int i = 0; i < m_nTotalChairs; i++)
	{
		if (m_ptrPlayers[i])
		{
			deposits[i] = m_ptrPlayers[i]->m_nDeposit;
		}
	}
	int tableno = (m_bTableEqual) ? 0 : m_nTableNO;
	int result = CalcBaseDeposit(deposits, tableno) * deposit_mult;

	if (FEE_MODE_FIXED == m_nFeeMode || FEE_MODE_FREE == m_nFeeMode)
	{
		// 指定基础银
		if (m_nBaseSilver && result > m_nBaseSilver) {
			result = m_nBaseSilver;
		}
	}
	else if (FEE_MODE_SERVICE_FIXED == m_nFeeMode
		|| FEE_MODE_SERVICE_MINDEPOSIT == m_nFeeMode
		|| FEE_MODE_SERVICE_SELFDEPOSIT == m_nFeeMode)
	{
		int depositMin = 0;
		for (int i = 0; i < m_nTotalChairs; i++)
		{
			if (deposits[i] > 0)
			{
				if ((0 == depositMin)
					|| (deposits[i] < depositMin))
					depositMin = deposits[i];
			}
		}

		//验证配置基础银是否超过最低银两
		if (m_nBaseSilver && (depositMin >= m_nBaseSilver))
		{
			result = m_nBaseSilver;
		}
	}
	int realBaseSliver = 0;
	int realfee = 0;
	m_pGameServer->OnGetFeerange(m_nRoomID, this, realfee, realBaseSliver);
	if (realBaseSliver != 0)
	{
		result = realfee;
	}
	return result;
}

std::string CGameTable::CoverCardIDsEx(int nCardIDs[], int nLen)
{
	static char shape[] = "DCHS?J";
	static char value[] = "3456789XJQKA2LB";
	std::string s;
	for (int i = 0; i < nLen; i++)
	{
		int nID = nCardIDs[i];
		if (nID == INVALID_OBJECT_ID) continue;

		s += shape[SK_GetCardShape(nID)];
		s += value[GetCardValueById(nID) - 1];
	}
	return s;
}

BOOL CGameTable::ConstructGameResults(void* pData, int nLen, int roomid, int gameid,
	LPREFRESH_RESULT_EX lpRefreshResult, GAME_RESULT_EX GameResults[])
{
	if(IsNeedRecord())
	{
		static std::mutex m;
		std::lock_guard<std::mutex> l(m);

		time_t ts = m_nBoutStartSeconds;
		tm tm = { 0 };
		localtime_s(&tm, &ts);
		TCHAR szDate[MAX_PATH];
		_stprintf(szDate, _T("%d_%04d%02d%02d"), roomid, tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday);

		TCHAR szFilePath[MAX_PATH] = { 0 };
		GetModuleFileName(NULL, szFilePath, MAX_PATH);
		*_tcsrchr(szFilePath, _T('\\')) = 0;
		lstrcat(szFilePath, _T("\\Record\\"));
		lstrcat(szFilePath, szDate);
		lstrcat(szFilePath, _T(".record"));

		if (auto f = _tfopen(szFilePath, _T("at")))
		{
			_ftprintf(f, _T("%s"), m_sinRecord.str().c_str());
			fclose(f);
		}

		if (IsNeedGameReferee())
		{
			Report2GameReferee(m_sinRecord.str().c_str());
		}
	}
	return __super::ConstructGameResults(pData, nLen, roomid, gameid, lpRefreshResult, GameResults);
}

int CGameTable::Report2GameReferee(const char* record)
{
	int nStatusCode = -1;

	CHttpClient http;
	http.addHeaders("content-type", "application/json");
	Json::Value jstring;
	jstring["gameid"] = GAME_ID;
	jstring["gamecode"] = STR_SERVICE_NAME;
	jstring["record"] = record;

	http.setBodyJson(jstring.toStyledString().c_str());
	CString strReq;
	strReq.Format("%s/api/gamereport", m_strGameRefereeHost.c_str());
	CString strRet = http.doPost(strReq);
	strRet.TrimRight();
	Json::Reader reader;
	Json::Value value;
	int nRetLen = strRet.GetLength();
	if (reader.parse(strRet.GetBuffer(nRetLen + 1), value, false))
	{
		if (!value.isNull())
		{
			Json::Value s = value["status"];
			if (!s.isNull())
			{
				nStatusCode = s.asInt();
			}
		}
	}
	if (nStatusCode != 0)
	{
		LOG_WARN("Report2GameReferee url:%s\nparam:%s\nret:%s", strReq, jstring.toStyledString().c_str(), strRet);
	}
	return nStatusCode;
}

BOOL CGameTable::IsRobotTable()
{
	int robotCount = this->GetRobotCount();
	if (robotCount > 0)
	{
		return TRUE;
	}
	return FALSE;
}

int CGameTable::GetRobotCount()
{
	int robotCount = 0;
	for (int i = 0; i < TOTAL_CHAIRS; i++)
	{
		if (IsRoboter(i))
		{
			robotCount += 1;
		}
	}
	return robotCount;
}

BOOL CGameTable::Robot_IsUseRemoteAI()
{
	if (m_nCurRobotAIType == ROBOTAI_REMOTE)
	{
		int maxRemoteAITimeout = GetPrivateProfileInt(_T("RobotAI"), _T("MaxRemoteAITimeout"), 3, GetINIFileName());
		if (maxRemoteAITimeout > 0 && m_nRemoteAITimeout >= maxRemoteAITimeout)
		{
			return FALSE; //超时3次之后，不再使用RemoteAI，切换到使用本地AI
		}

		return TRUE;
	}

	return FALSE;
}

BOOL CGameTable::Robot_InitRobotAITypeOnBoutStart(int aiLevel)
{
	m_nCurRobotAIType = ROBOTAI_JUNIOR;
	if (aiLevel >=0 && m_pGameServer->RoomRemoteRobot(this) == TRUE)
	{
		m_nCurRobotAIType = ROBOTAI_REMOTE;
	}
	m_nAILevel[3] = aiLevel;
	return TRUE;
}

void CGameTable::Robot_IncRemoteAITimeout()
{
	m_nRemoteAITimeout += 1;

	//超时3次之后，确定陪玩AI有问题，则停止陪玩机器人匹配一段时间
	int maxRemoteAITimeout = GetPrivateProfileInt(_T("RobotAI"), _T("MaxRemoteAITimeout"), 3, GetINIFileName());
	if (maxRemoteAITimeout > 0 && m_nRemoteAITimeout >= maxRemoteAITimeout)
	{
		m_pGameServer->OrderRoomStopMatingRobotOnTimeout(m_nRoomID);
	}

	//钉钉报警
	{
		CString dingMsg;
#ifdef DEBUG
		dingMsg.Format(_T("斗地主报警\n内网陪玩机器人AI超时未响应第%d次，房间号%d，桌子号%d"), 
			m_nRemoteAITimeout, m_nRoomID, m_nTableNO);
#else
		dingMsg.Format(_T("斗地主报警\n外网陪玩机器人AI超时未响应第%d次，房间号%d，桌子号%d"),
			m_nRemoteAITimeout, m_nRoomID, m_nTableNO);
#endif
		m_pGameServer->SendDingMsgOfRobot(std::string(CT2A(dingMsg.GetString())));
	}
}

void CGameTable::Robot_IncRemoteAITimeoutWithUserId(int userId, int chairNo, int seq, int startTimestamp, CString aiEngineUrl)
{
	m_nRemoteAITimeout += 1;

	//超时3次之后，确定陪玩AI有问题，则停止陪玩机器人匹配一段时间
	int maxRemoteAITimeout = GetPrivateProfileInt(_T("RobotAI"), _T("MaxRemoteAITimeout"), 3, GetINIFileName());
	if (maxRemoteAITimeout > 0 && m_nRemoteAITimeout >= maxRemoteAITimeout)
	{
		m_pGameServer->OrderRoomStopMatingRobotOnTimeout(m_nRoomID);
	}

	//钉钉报警
	{
		CString dingMsg;
#ifdef DEBUG
		dingMsg.Format(_T("斗地主报警\n内网陪玩机器人AI超时未响应第%d次，房间号%d，桌子号%d, 用户ID%d, 座位号%d, seq:%d, 时间戳:%d, 唯一id:%d_%d, 机器人URL:%s"),
			m_nRemoteAITimeout, m_nRoomID, m_nTableNO, userId, chairNo, seq, startTimestamp, startTimestamp, seq, aiEngineUrl);
#else
		dingMsg.Format(_T("斗地主报警\n外网陪玩机器人AI超时未响应第%d次，房间号%d，桌子号%d, 用户ID%d, 座位号%d, seq:%d, 时间戳:%d, 唯一id:%d_%d, 机器人URL:%s"),
			m_nRemoteAITimeout, m_nRoomID, m_nTableNO, userId, chairNo, seq, startTimestamp, startTimestamp, seq, aiEngineUrl);
#endif
		LOG_ERROR("alert dingMsg:%s", dingMsg);
		m_pGameServer->SendDingMsgOfRobot(std::string(CT2A(dingMsg.GetString())));
		
	}
}

void CGameTable::Robot_SetRemoteAIRobotPeerBottomEnable()
{
	CString keyName;
	keyName.Format(_T("RemoteAIRobotPeerBottomEnable_%d"), m_nRoomID);
	int isPeerBottom = GetPrivateProfileInt(_T("RobotAI"), keyName, FALSE, GetINIFileName());
	if (isPeerBottom == 1)
	{
		m_bIsRemoteAIRobotPeerBottomEnable = TRUE;
	}
	else
	{
		m_bIsRemoteAIRobotPeerBottomEnable = FALSE;
	}
}

void CGameTable::SetPlayerPeeredBottom(int userId)
{
	for (int i = 0; i < TOTAL_CHAIRS; ++i)
	{
		if (m_peeredBottomPlayer[i] <= 0)
		{
			m_peeredBottomPlayer[i] = userId;
			break;
		}
	}
}

BOOL CGameTable::IsPlayerPeeredBottom(int userId)
{
	for (int i = 0; i < TOTAL_CHAIRS; ++i)
	{
		if (m_peeredBottomPlayer[i] == userId)
		{
			return TRUE;
		}
	}
	return FALSE;
}

int CGameTable::GetPeeredBottomPlayerCount()
{
	int count = 0;
	for (int i = 0; i < TOTAL_CHAIRS; ++i)
	{
		if (m_peeredBottomPlayer[i] > 0)
		{
			count++;
		}
	}
	return count;
}

BOOL CGameTable::SetBoutDataCacheOfGameWinData(GAME_WIN_RESULT* pGameWinResult)
{
	if (pGameWinResult == nullptr)
	{
		return FALSE;
	}

	for (int i = 0; i < TOTAL_CHAIRS; ++i)
	{
		if (m_ptrPlayers[i] != nullptr)
		{
			m_boutDataCache.nUserType[i] = m_ptrPlayers[i]->m_nUserType;
			m_boutDataCache.nDepositDiffs[i] = pGameWinResult->gamewin.nDepositDiffs[i];
		}
	}
	m_boutDataCache.nBombFan = pGameWinResult->nBombFan;
	m_boutDataCache.nSpring = pGameWinResult->nSpring;

	return TRUE;
}

BOOL CGameTable::CanAllThrow(int nChairNO, CARD_UNITE& unit_details)
{
	int nInHand[CHAIR_CARDS];
	XygInitChairCards(nInHand, CHAIR_CARDS);
	int nCardCount = GetInHandCard(nChairNO, nInHand);

	if (nCardCount < 1)
		return FALSE;

	if (!GetUniteDetails(nChairNO, nInHand, CHAIR_CARDS, unit_details, CARD_UNITE_TYPE_TOTAL_EX))
		return FALSE;

	if (GetPublicInfo()->nWaitChair == -1)
	{
		GetBestUnitType(unit_details);
		return TRUE;
	}
	else
	{
		if (GetBestUnitType(GetPublicInfo()->WaitCardUnite, unit_details))
		{
			return TRUE;
		}
	}

	return FALSE;
}

int CGameTable::SelectMinUnite(int nInHand[], int nInHandCount, int nPrompt[], int nPromptCount)
{
	int nSelectCount = 0;

	int nCardLay[SK_LAYOUT_NUM];
	memset(nCardLay, 0, sizeof(nCardLay));
	int nJokerCount = 0;
	PreDealCards(nInHand, nInHandCount, nCardLay, SK_LAYOUT_NUM, nJokerCount);

	int nIndex = 0;
	int nPri = 1000000;

	for (int j = 0; j < SK_LAYOUT_NUM; j++)
	{
		if (nCardLay[j] > 0 && nCardLay[j] < 4)
		{
			if (14 == j && nCardLay[15] > 0) //双王不拆
			{
				continue;
			}
			if (SK_GetIndexPRIEx(j, GetCurrentRank(), 0) < nPri)
			{
				nPri = SK_GetIndexPRIEx(j, GetCurrentRank(), 0);
				nIndex = j;
			}
		}
	}

	if (nIndex > 0)
	{
		nSelectCount = nCardLay[nIndex];
		PutCardToArray(nPrompt, CHAIR_CARDS, nInHand, CHAIR_CARDS, nIndex, nSelectCount);
		return nSelectCount;
	}

	for (int j = 0; j < SK_LAYOUT_NUM; j++)
	{
		if (nCardLay[j] > 0)
		{
			if (14 == j && nCardLay[15] > 0)
			{
				PutCardToArray(nPrompt, CHAIR_CARDS, nInHand, CHAIR_CARDS, 14, 1);
				PutCardToArray(nPrompt, CHAIR_CARDS, nInHand, CHAIR_CARDS, 15, 1);
				return 2;
			}else{
				PutCardToArray(nPrompt, CHAIR_CARDS, nInHand, CHAIR_CARDS, j, nCardLay[j]);
				return nCardLay[j];
			}
		}
	}

	return nSelectCount;
}

void CGameTable::FillMatchGameWinResult(void* pData)
{
	if (m_bIsMatchGame)
	{
		LPGAME_WIN pGameWin = (LPGAME_WIN)pData;
		for (int i = 0; i < m_nTotalChairs; i++)
		{
			pGameWin->nScoreDiffs[i] = m_jsonMatchInfo[i]["score"].asInt();
		}
	}
}

int CGameTable::GetBaseScore(int base_score)
{
	if (m_bIsMatchGame)
	{
		int nMatchBase = GetPrivateProfileInt(_T("MatchGame"), "basescore", 1, GetINIFileName());
		return nMatchBase;
	}

	return __super::GetBaseScore(base_score);
}


BOOL CGameTable::readJSONCard(int& cardid, Json::Value& card)
{
	if (!card["id"].isInt()) {
		return FALSE;
	}
	cardid = card["id"].asInt();
	return TRUE;
}

BOOL CGameTable::OnPlayerDouble(int nChair, int nDoubleType, BOOL &bDoubleFinished, BOOL bAuto)
{

	if (!ValidateChair(nChair))
	{
		return FALSE;
	}

	DoubleType eDoubleType = static_cast<DoubleType>(nDoubleType);

	m_PlayerDouble[nChair] = eDoubleType == DoubleType::NO_DOUBLE ? 1 : 2;

	// 如果有超级加倍,以它为准
	if (eDoubleType == DoubleType::SUPER_DOUBLE)
	{
		m_PlayerDouble[nChair] = 4;
	}

	bDoubleFinished = TRUE;
	for (int i = 0; i < TOTAL_CHAIRS; i++)
	{
		if (!m_PlayerDouble[i]){
			bDoubleFinished = FALSE;
		}
	}

	if (IsNeedRecord())
	{
		RecordAction(m_sinRecord, "Double", bAuto, nChair, nDoubleType);
	}

	if (IsRobotTable())
	{
		CString strIniFile = GetINIFileName();
		{
			Json::Value data(Json::objectValue);
			data["isDouble"] = nDoubleType;

			Json::StreamWriterBuilder builder;
			const std::string json = Json::writeString(builder, data);

			std::vector<CAIEngineItem> vecAIEngineItems;
			vecAIEngineItems.push_back(CAIEngineItem{ nChair, CAI_Dll::e_AI_AutoDouble, json });

			m_pGameServer->PostAIEngineAction(this, vecAIEngineItems); // 该接口会自己通知本桌所有ai
		}
	}
	return TRUE;
}



// 老客户端可能已经出牌了
BOOL CGameTable::OnPlayerDoubleFinished()
{

	RemoveStatus(TS_WAITING_DOUBLE);

	AddStatus(TS_PLAYING_GAME | TS_WAITING_THROW);
	
	// 需要重置下时钟停止值
	m_dwLastClockStop = GetTickCount();

	// 如果已经出牌过了,不再设置座位号,防止打乱出牌顺序
	if (m_bAlreadThrowed == TRUE)
	{
		return TRUE;
	}

	SetCurrentChair(m_nBanker, GetPlayerInfo(m_nBanker)->nThrowTime);

	return TRUE;
}

DOUBLE_COMMON_INFO CGameTable::ReadDoubleCommonInfo()
{
	CString strIniFile = GetINIFileName();

	std::string strModifyTime = GetFileModifyTime(std::string(strIniFile));
	if (strModifyTime != m_strModifyTime)
	{
		m_strModifyTime = strModifyTime;
		TCHAR szRoomID[16];
		memset(szRoomID, 0, sizeof(szRoomID));
		_stprintf(szRoomID, _T("%ld"), m_nRoomID);
		m_nDoubleType = GetPrivateProfileInt(
			_T("DoubleType"),		// section name
			szRoomID,				// key name
			0,		// default int
			strIniFile				// initialization file name
			);
		m_nDoubleTime = GetPrivateProfileInt(
			_T("DoubleTime"),		// section name
			szRoomID,				// key name
			5,		// default int
			strIniFile				// initialization file name
			);
		m_nSuperDoubleCost = GetPrivateProfileInt(
			_T("SuperDoubleCost"),		// section name
			szRoomID,				// key name
			500,		// default int
			strIniFile				// initialization file name
			);
	}

	return DOUBLE_COMMON_INFO{
		m_nDoubleType,
		m_nDoubleTime,
		m_nSuperDoubleCost
	};
}

DoubleType CGameTable::GetDoubleType()
{
	return static_cast<DoubleType>(ReadDoubleCommonInfo().m_nDoubleType);
}

bool CGameTable::CheckResultTask(int charno)
{
	if (charno < 0 || charno >= TOTAL_CHAIRS) {
		return false;
	}

	Json::Value& root = m_jsonDdzTaskInfo[charno];
	if (!root.isArray()) return false;

	bool needNotify = false;
	for (auto& task : root) {
		auto ttype = (TaskKind)(task["type"].asInt());
		if (ttype == TaskKind::ThrowNoNotify && CheckTaskFinished(task)) {
			needNotify = true;
		}

		if (ttype == TaskKind::Result || ttype == TaskKind::Accumulate) {
			TaskType taskType = (TaskType)(task["id"].asInt() / 1000);
			if (UpdateTaskStatus(task, charno) && taskType == TaskType::Turn) {
				if (ttype != TaskKind::ThrowNoNotify) {
					needNotify = true;
				}
			}
		}
	}
	return needNotify;
}

bool CGameTable::CheckThrowTask(CARDS_THROW* pCardsThrow, int preWaitChar)
{
	int charno = pCardsThrow->nChairNO;
	if (charno < 0 || charno >= TOTAL_CHAIRS) {
		return false;
	}

	auto needNotify = false;
	Json::Value& root = m_jsonDdzTaskInfo[charno];
	if (!root.isArray()) return false;

	for (auto& task : root) {
		auto ttype = (TaskKind)(task["type"].asInt());

		if (ttype == TaskKind::Throw || ttype == TaskKind::ThrowNoNotify) {
			TaskType taskType = (TaskType)(task["id"].asInt() / 1000);
			if (UpdateTaskStatus(task, charno, pCardsThrow, preWaitChar) && taskType == TaskType::Turn) {
				if (ttype != TaskKind::ThrowNoNotify) {
					needNotify = true;
				}
			}
		}
	}

	return needNotify;
}

// 更新任务的条件状态
bool CGameTable::UpdateTaskStatus(Json::Value& root, int charno, CARDS_THROW* pCardsThrow, int preWaitChar)
{
	if ((IsOffline(charno) || CheckTaskFinished(root))) {
		return false;
	}

	Json::Value& conditions = root["conditions"];
	auto startTm = root["starttime"].asInt64();
	if (!conditions.isArray() || startTm != m_startTimeStamp) return false;

	int i = 0;
	std::vector<bool> rets;
	for (auto& con : conditions) {
		if (con.isNull()) continue;

		// 有任意一个没有完成则不算完成
		if (i != rets.size()) return false;

		auto cid = con["type"].asInt();
		auto data = con["data"];

		if (!data.isArray()) continue;
		++i;

		switch (cid) {
			case (int)ConditionID::Role:
			{
				auto cd = data[0].asInt();
				bool isLandroid = m_nBanker == charno;
				// 0 不限制身份， 1 地主  2 农民
				if (!cd || (isLandroid && cd == 1) || (isLandroid == false && cd == 2)) {
					con["finish"] = con["finish"].asInt() + 1;
					rets.push_back(true);
				}
				break;
			}
			case (int)ConditionID::Result:
			{
				auto cd = data[0].asInt();
				if (abs(m_nMagnificationTheory[charno]) >= cd) {
					con["finish"] = con["finish"].asInt() + 1;
					rets.push_back(true);
				}
				break;
			}
			case (int)ConditionID::ThrowCard:
			{
				if (pCardsThrow) {
					std::vector<int> vcs;
					for (int f = 0; f < data.size(); ++f) {
						int v = data[f].asInt();
						vcs.push_back(v);
					}

					if (pCardsThrow->unite.nCardCount != vcs.size()) {
						break;
					}

					std::vector<int> cards;
					for (int k = 0; k < pCardsThrow->unite.nCardCount; ++k) {
						//int value = SK_GetCardValueEx(pCardsThrow->unite.nCardIDs[k], 0);
						int value = SK_GetCardValueEx(pCardsThrow->unite.nCardIDs[k], 0);
						cards.push_back(value);
					}

					std::sort(cards.begin(), cards.end());
					std::sort(vcs.begin(), vcs.end());

					bool isEquail = true;
					for (int k = 0; k < cards.size(); ++k) {
						auto ck = cards[k];
						auto cv = vcs[k];
						// 两套转换规则，无语死
						auto razz = m_nRazzCardValue + 1;
						if ((razz != ck) && (ck != cv))
							isEquail = false;
					}

					if (isEquail) {
						con["finish"] = con["finish"].asInt() + 1;
						rets.push_back(true);
					}
					break;
				}
			case (int)ConditionID::ThrowCardType:
			{
				if (pCardsThrow) {
					auto ctype = data[0].asInt();
					if (IS_BIT_SET(ctype, pCardsThrow->unite.dwCardType)) {
						con["finish"] = con["finish"].asInt() + 1;
						rets.push_back(true);
					}
				}
				break;
			}
			case (int)ConditionID::Ya:
			{
				int waitChar = GetPublicInfo()->nWaitChair;
				if (preWaitChar != -1 && preWaitChar != charno && waitChar != -1) {
					con["finish"] = con["finish"].asInt() + 1;
					rets.push_back(true);
				}
				break;
			}
			case (int)ConditionID::LastHand:
			{
				if (!HaveCards(charno)) {
					con["finish"] = con["finish"].asInt() + 1;
					rets.push_back(true);
				}
				break;
			}
			case (int)ConditionID::Round:
			{
				auto cd = data[0].asInt();
				auto round = GetPlayerInfo(charno)->nThrowCount;
				if (round > 0 && cd >= round) {
					con["finish"] = con["finish"].asInt() + 1;
					rets.push_back(true);
				}
				break;
			}
			case (int)ConditionID::Win:
			{
				auto cd = data[0].isInt() ? data[0].asInt() : 0;
				auto isWin = m_boutDataCache.nDepositDiffs[charno] > 0;
				if (isWin) {
					con["finish"] = con["finish"].asInt() + 1;
					rets.push_back(true);
				}
				break;
			}
			case (int)ConditionID::GetMoney:
			{
				auto cd = data[0].asInt();
				if (m_boutDataCache.nDepositDiffs[charno] > 0) {
					con["finish"] = con["finish"].asInt() + m_boutDataCache.nDepositDiffs[charno];
					if (con["finish"] >= cd) {
						rets.push_back(true);
					}
				}
				break;
			}
			case (int)ConditionID::Double:
			{
				auto cd = data[0].asInt();
				auto db = m_PlayerDouble[charno];
				if (cd && cd == db) {
					con["finish"] = con["finish"].asInt() + 1;
					rets.push_back(true);
				}
				break;
			}
			case (int)ConditionID::Spring:
			{
				auto isWin = m_boutDataCache.nDepositDiffs[charno] > 0;
				if (isWin && m_boutDataCache.nSpring) {
					con["finish"] = con["finish"].asInt() + 1;
					rets.push_back(true);
				}
				break;
			}
			case (int)ConditionID::Count:
			{
				con["finish"] = con["finish"].asInt() + 1;
				rets.push_back(true);
				break;
			}
		}
		}
	}

#ifdef _DEBUG //_RS125
	std::string str;
	if (pCardsThrow) {
		str += toString(pCardsThrow->nUserID) + ": " ;
	}

	for (int j = 0; j < rets.size(); ++j) {
		str += "true_";
	}
	UwlLogFile("[ddztask] charno: %d, %s", charno, str.c_str());
#endif

	return rets.size() == conditions.size();
}

bool CGameTable::CheckTaskFinished(const Json::Value& root)
{
	auto ttype = (TaskKind)root["type"].asInt();
	if (ttype == TaskKind::Accumulate) {
		// 累计类型得任务可以一直累计
		return false;
	}

	const Json::Value& conditions = root["conditions"];
	if (!conditions.isArray()) return false;

	int i = 0;
	std::vector<bool> rets;
	for (auto& con : conditions) {
		if (con.isNull()) continue;

		// 有任意一个没有完成则不算完成
		if (i != rets.size()) return false;

		if (con["finish"].asInt() > 0) {
			rets.push_back(true);
		}
		++i;
	}
	return rets.size() == conditions.size();
}

int CGameTable::CompensateDepositsOriginal(int nOldDeposits[], int nDepositDiffs[])
{
	int totalwin = 0;
	double dblDeposits[MAX_CHAIRS_PER_TABLE];
	ZeroMemory(dblDeposits, sizeof(dblDeposits));

	int i;
	for (i = 0; i < m_nTotalChairs; i++) {
		dblDeposits[i] = nDepositDiffs[i];
		if (nDepositDiffs[i] > 0) {
			totalwin += nDepositDiffs[i];
		}
	}
	if (IS_BIT_SET(m_dwGameFlags, GF_LEVERAGE_ALLOWED)) { // 允许以小博大
		for (i = 0; i < m_nTotalChairs; i++) {
			int depositDiff = nDepositDiffs[i];
			int depositOld = nOldDeposits[i];
			if (depositDiff < 0) { // 输家
				if (depositOld + depositDiff < 0) { // 银子不够输
					nDepositDiffs[i] = -depositOld; // 输光已有银子
					for (int j = 0; j < m_nTotalChairs; j++) {
						if (dblDeposits[j] > 0) {
							double dblReturn = (-depositDiff - depositOld);
							nDepositDiffs[j] -= ceil(dblReturn / totalwin * dblDeposits[j]);
							if (nDepositDiffs[j] < 0) {
								nDepositDiffs[j] = 0;
							}
						}
					}
				}
			}
		}
	}
	else { // 不允许以小博大
		BOOL nBankerWin = FALSE;
		int nFarmer1ChairNo = GetNextChair(m_nBanker);
		int nFarmer2ChairNo = GetNextChair(nFarmer1ChairNo);
		if (nDepositDiffs[m_nBanker] > 0)
			nBankerWin = TRUE;

		if (nBankerWin)
		{//地主赢
			int nBankerWinDeposit = nDepositDiffs[m_nBanker];
			int nFarmer1LoseDeposit = -nDepositDiffs[nFarmer1ChairNo];
			int nFarmer2LoseDeposit = -nDepositDiffs[nFarmer2ChairNo];

			if (nOldDeposits[m_nBanker] - nDepositDiffs[m_nBanker] < 0)
			{//按照倍数的赢取银两超过了地主的携带银两
				nBankerWinDeposit = nOldDeposits[m_nBanker];
				nFarmer1LoseDeposit = nBankerWinDeposit / 2;
				nFarmer2LoseDeposit = nBankerWinDeposit / 2;
				m_nCompensate |= m_nBanker + 1;               //默认值是0,此处座位号+1处理
				m_nCompensate |= COMPENSTATE_BANKER_BANKER_NOT_ENOUGH;
			}

			if (nOldDeposits[nFarmer1ChairNo] - nFarmer1LoseDeposit < 0)
			{//农民1的携带银两不够输
				nFarmer1LoseDeposit = nOldDeposits[nFarmer1ChairNo];
				m_nCompensate |= m_nBanker + 1;
				m_nCompensate |= COMPENSTATE_BANKER_FARMER_NOT_ENOUGH;
			}

			if (nOldDeposits[nFarmer2ChairNo] - nFarmer2LoseDeposit < 0)
			{//农民2的携带银两不够输
				nFarmer2LoseDeposit = nOldDeposits[nFarmer2ChairNo];
				m_nCompensate |= m_nBanker + 1;
				m_nCompensate |= COMPENSTATE_BANKER_FARMER_NOT_ENOUGH;
			}

			if (nFarmer1LoseDeposit > nFarmer2LoseDeposit)
			{
				m_nCompensate |= ((nFarmer1ChairNo + 1) << 8);
				m_nCompensate |= COMPENSTATE_FARMER1_FARMER_NOT_EQUAL;
			}
			else if (nFarmer1LoseDeposit < nFarmer2LoseDeposit)
			{
				m_nCompensate |= ((nFarmer2ChairNo + 1) << 16);
				m_nCompensate |= COMPENSTATE_FARMER2_FARMER_NOT_EQUAL;
			}

			nBankerWinDeposit = nFarmer1LoseDeposit + nFarmer2LoseDeposit;

			nDepositDiffs[m_nBanker] = nBankerWinDeposit;
			nDepositDiffs[nFarmer1ChairNo] = -nFarmer1LoseDeposit;
			nDepositDiffs[nFarmer2ChairNo] = -nFarmer2LoseDeposit;
		}
		else
		{//农民赢
			int nBankerLoseDeposit = -nDepositDiffs[m_nBanker];
			int nFarmer1WinDeposit = nDepositDiffs[nFarmer1ChairNo];
			int nFarmer2WinDeposit = nDepositDiffs[nFarmer2ChairNo];

			if (nOldDeposits[m_nBanker] + nDepositDiffs[m_nBanker] < 0)
			{//按照倍数的输掉银两超过了地主的携带银两
				nBankerLoseDeposit = nOldDeposits[m_nBanker];
				nFarmer1WinDeposit = nBankerLoseDeposit / 2;
				nFarmer2WinDeposit = nBankerLoseDeposit / 2;
				m_nCompensate |= ((nFarmer1ChairNo + 1) << 8);
				m_nCompensate |= ((nFarmer2ChairNo + 1) << 16);
				m_nCompensate |= COMPENSTATE_FARMER1_BANKER_NOT_ENOUGH;
				m_nCompensate |= COMPENSTATE_FARMER2_BANKER_NOT_ENOUGH;
			}

			if (nOldDeposits[nFarmer1ChairNo] - nFarmer1WinDeposit < 0)
			{//农民1的赢取的银两超过了携带银两
				nFarmer1WinDeposit = nOldDeposits[nFarmer1ChairNo];
				m_nCompensate |= ((nFarmer1ChairNo + 1) << 8);
				m_nCompensate |= COMPENSTATE_FARMER1_FARMER_NOT_ENOUGH;
			}

			if (nOldDeposits[nFarmer2ChairNo] - nFarmer2WinDeposit < 0)
			{//农民2的赢取的银两超过了携带银两
				nFarmer2WinDeposit = nOldDeposits[nFarmer2ChairNo];
				m_nCompensate |= ((nFarmer2ChairNo + 1) << 16);
				m_nCompensate |= COMPENSTATE_FARMER2_FARMER_NOT_ENOUGH;
			}

			if (nFarmer1WinDeposit < nFarmer2WinDeposit)
			{
				m_nCompensate |= ((nFarmer1ChairNo + 1) << 8);
				m_nCompensate |= COMPENSTATE_FARMER1_FARMER_NOT_EQUAL;
			}
			else if (nFarmer1WinDeposit > nFarmer2WinDeposit)
			{
				m_nCompensate |= ((nFarmer2ChairNo + 1) << 16);
				m_nCompensate |= COMPENSTATE_FARMER2_FARMER_NOT_EQUAL;
			}

			nBankerLoseDeposit = nFarmer1WinDeposit + nFarmer2WinDeposit;

			nDepositDiffs[m_nBanker] = -nBankerLoseDeposit;
			nDepositDiffs[nFarmer1ChairNo] = nFarmer1WinDeposit;
			nDepositDiffs[nFarmer2ChairNo] = nFarmer2WinDeposit;
		}
	}

	return CalcSurplus(nDepositDiffs);
}

int CGameTable::CompensateDepositsForSilverLimit(int nOldDeposits[], int nDepositDiffs[])
{
	int totalwin = 0;
	double dblDeposits[MAX_CHAIRS_PER_TABLE];
	ZeroMemory(dblDeposits, sizeof(dblDeposits));

	int i;
	for (i = 0; i < m_nTotalChairs; i++) {
		dblDeposits[i] = nDepositDiffs[i];
		if (nDepositDiffs[i] > 0) {
			totalwin += nDepositDiffs[i];
		}
	}
	if (IS_BIT_SET(m_dwGameFlags, GF_LEVERAGE_ALLOWED)) { // 允许以小博大
		for (i = 0; i < m_nTotalChairs; i++) {
			int depositDiff = nDepositDiffs[i];
			int depositOld = nOldDeposits[i];
			if (depositDiff < 0) { // 输家
				if (depositOld + depositDiff < 0) { // 银子不够输
					nDepositDiffs[i] = -depositOld; // 输光已有银子
					for (int j = 0; j < m_nTotalChairs; j++) {
						if (dblDeposits[j] > 0) {
							double dblReturn = (-depositDiff - depositOld);
							nDepositDiffs[j] -= ceil(dblReturn / totalwin * dblDeposits[j]);
							if (nDepositDiffs[j] < 0) {
								nDepositDiffs[j] = 0;
							}
						}
					}
				}
			}
		}
	}
	else { // 不允许以小博大
		BOOL nBankerWin = FALSE;
		int nFarmer1ChairNo = GetNextChair(m_nBanker);
		int nFarmer2ChairNo = GetNextChair(nFarmer1ChairNo);
		if (nDepositDiffs[m_nBanker] > 0)
			nBankerWin = TRUE;

		float bankRate = abs(m_nMagnificationTheory[m_nBanker]);
		float frame1 = abs(m_nMagnificationTheory[nFarmer1ChairNo]);

		if (nBankerWin)
		{//地主赢
			int nBankerWinDeposit = abs(nDepositDiffs[m_nBanker]);
			int nFarmer1LoseDeposit = abs(nDepositDiffs[nFarmer1ChairNo]);
			int nFarmer2LoseDeposit = abs(nDepositDiffs[nFarmer2ChairNo]);

			if (m_nRoomSilverLimit > 0 && nBankerWinDeposit > m_nRoomSilverLimit) {
				// 触发房间封顶
				nBankerWinDeposit = m_nRoomSilverLimit;
			}

			if (nOldDeposits[m_nBanker] - nBankerWinDeposit < 0)
			{//按照倍数的赢取银两超过了地主的携带银两，按照比率分配
				nBankerWinDeposit = nOldDeposits[m_nBanker];
				m_nCompensate |= m_nBanker + 1;               //默认值是0,此处座位号+1处理
				m_nCompensate |= COMPENSTATE_BANKER_BANKER_NOT_ENOUGH;
			}

			nFarmer1LoseDeposit = std::round(nBankerWinDeposit * (frame1 / bankRate));
			nFarmer2LoseDeposit = nBankerWinDeposit - nFarmer1LoseDeposit;

			if (m_nRoomSilverLimit > 0 && nFarmer1LoseDeposit > m_nRoomSilverLimit) {
				// 触发房间封顶
				nFarmer1LoseDeposit = m_nRoomSilverLimit;
			}
			if (nOldDeposits[nFarmer1ChairNo] - nFarmer1LoseDeposit < 0)
			{
				//农民1的携带银两不够输
				nFarmer1LoseDeposit = nOldDeposits[nFarmer1ChairNo];
				m_nCompensate |= m_nBanker + 1;
				m_nCompensate |= COMPENSTATE_BANKER_FARMER_NOT_ENOUGH;
			}

			if (m_nRoomSilverLimit > 0 && abs(nFarmer2LoseDeposit) > m_nRoomSilverLimit) {
				// 触发房间封顶
				nFarmer2LoseDeposit = m_nRoomSilverLimit;
			}
			if (nOldDeposits[nFarmer2ChairNo] - nFarmer2LoseDeposit < 0)
			{//农民2的携带银两不够输
				nFarmer2LoseDeposit = nOldDeposits[nFarmer2ChairNo];
				m_nCompensate |= m_nBanker + 1;
				m_nCompensate |= COMPENSTATE_BANKER_FARMER_NOT_ENOUGH;
			}

			if (nFarmer1LoseDeposit > nFarmer2LoseDeposit)
			{
				m_nCompensate |= ((nFarmer1ChairNo + 1) << 8);
				m_nCompensate |= COMPENSTATE_FARMER1_FARMER_NOT_EQUAL;
			}
			else if (nFarmer1LoseDeposit < nFarmer2LoseDeposit)
			{
				m_nCompensate |= ((nFarmer2ChairNo + 1) << 16);
				m_nCompensate |= COMPENSTATE_FARMER2_FARMER_NOT_EQUAL;
			}

			nBankerWinDeposit = nFarmer1LoseDeposit + nFarmer2LoseDeposit;

			nDepositDiffs[m_nBanker] = nBankerWinDeposit;
			nDepositDiffs[nFarmer1ChairNo] = -nFarmer1LoseDeposit;
			nDepositDiffs[nFarmer2ChairNo] = -nFarmer2LoseDeposit;
		}
		else
		{//农民赢
			int nBankerLoseDeposit = abs(nDepositDiffs[m_nBanker]);
			int nFarmer1WinDeposit = abs(nDepositDiffs[nFarmer1ChairNo]);
			int nFarmer2WinDeposit = abs(nDepositDiffs[nFarmer2ChairNo]);

			if (m_nRoomSilverLimit > 0 && nBankerLoseDeposit > m_nRoomSilverLimit) {
				// 触发房间封顶
				nBankerLoseDeposit = m_nRoomSilverLimit;
			}

			if (nOldDeposits[m_nBanker] - nBankerLoseDeposit < 0)
			{//按照倍数的输掉银两超过了地主的携带银两
				nBankerLoseDeposit = nOldDeposits[m_nBanker];

				m_nCompensate |= ((nFarmer1ChairNo + 1) << 8);
				m_nCompensate |= ((nFarmer2ChairNo + 1) << 16);
				m_nCompensate |= COMPENSTATE_FARMER1_BANKER_NOT_ENOUGH;
				m_nCompensate |= COMPENSTATE_FARMER2_BANKER_NOT_ENOUGH;
			}

			if (m_nRoomSilverLimit > 0 && nFarmer1WinDeposit > m_nRoomSilverLimit) {
				// 触发房间封顶
				nFarmer1WinDeposit = m_nRoomSilverLimit;
			}
			if (nOldDeposits[nFarmer1ChairNo] - nFarmer1WinDeposit < 0)
			{//农民1的赢取的银两超过了携带银两
				nFarmer1WinDeposit = nOldDeposits[nFarmer1ChairNo];
				m_nCompensate |= ((nFarmer1ChairNo + 1) << 8);
				m_nCompensate |= COMPENSTATE_FARMER1_FARMER_NOT_ENOUGH;
			}

			if (m_nRoomSilverLimit > 0 && nFarmer2WinDeposit > m_nRoomSilverLimit) {
				// 触发房间封顶
				nFarmer2WinDeposit = m_nRoomSilverLimit;
			}
			if (nOldDeposits[nFarmer2ChairNo] - nFarmer2WinDeposit < 0)
			{//农民2的赢取的银两超过了携带银两
				nFarmer2WinDeposit = nOldDeposits[nFarmer2ChairNo];
				m_nCompensate |= ((nFarmer2ChairNo + 1) << 16);
				m_nCompensate |= COMPENSTATE_FARMER2_FARMER_NOT_ENOUGH;
			}


			if (nBankerLoseDeposit < nFarmer1WinDeposit + nFarmer2WinDeposit) {
				nFarmer1WinDeposit = std::round(nBankerLoseDeposit * (frame1 / bankRate));
				nFarmer2WinDeposit = nBankerLoseDeposit - nFarmer1WinDeposit;

				if (nOldDeposits[nFarmer1ChairNo] - nFarmer1WinDeposit < 0)
				{
					nFarmer1WinDeposit = nOldDeposits[nFarmer1ChairNo];
				}

				if (nOldDeposits[nFarmer2ChairNo] - nFarmer2WinDeposit < 0)
				{
					nFarmer2WinDeposit = nOldDeposits[nFarmer2ChairNo];
				}
			}

			if (nFarmer1WinDeposit < nFarmer2WinDeposit)
			{
				m_nCompensate |= ((nFarmer1ChairNo + 1) << 8);
				m_nCompensate |= COMPENSTATE_FARMER1_FARMER_NOT_EQUAL;
			}
			else if (nFarmer1WinDeposit > nFarmer2WinDeposit)
			{
				m_nCompensate |= ((nFarmer2ChairNo + 1) << 16);
				m_nCompensate |= COMPENSTATE_FARMER2_FARMER_NOT_EQUAL;
			}

			nBankerLoseDeposit = nFarmer1WinDeposit + nFarmer2WinDeposit;

			nDepositDiffs[m_nBanker] = -nBankerLoseDeposit;
			nDepositDiffs[nFarmer1ChairNo] = nFarmer1WinDeposit;
			nDepositDiffs[nFarmer2ChairNo] = nFarmer2WinDeposit;
		}
	}

	return CalcSurplus(nDepositDiffs);
}

int CGameTable::GetUserCardPoolId()
{
	int nMyChairNo = -1;
	for (int m = 0; m < TOTAL_CHAIRS; m++)
	{
		if (!m_ptrPlayers[m]->IsRoboter())
		{
			nMyChairNo = m;
			break;
		}
	}

	auto szRoomID = std::to_string(m_nRoomID);
	int nCurBout = m_ptrPlayers[nMyChairNo]->m_nBout;
	int userid = m_ptrPlayers[nMyChairNo]->m_nUserID;
	int gameModle = m_pGameServer->GetGameModeByRoomID(m_nRoomID);

	if (CConfigManagerSys::m_jsoncfgobjmgr[MAKEDEAL_CONFIG]["PlayerMakeDealRule"]["RoomRule"][szRoomID]["ManualMakeDealBout"].isNull())
	{
		return FALSE;
	}

	int nManualMakeDealBout = CConfigManagerSys::m_jsoncfgobjmgr[MAKEDEAL_CONFIG]["PlayerMakeDealRule"]["RoomRule"][szRoomID]["ManualMakeDealBout"].asInt();

	// 读取新手牌型配置，当前局数对应的牌型如果没有配置则不读取
	Json::Value newUserSpecCards;
	newUserSpecCards = CConfigManagerSys::m_jsoncfgobjmgr[MAKEDEAL_CONFIG]["PlayerMakeDealRule"]["NewUserSpecCards"];

	auto road = std::to_string(m_boutDataCache.nProtectCard[0]);
	auto idStr = std::to_string(gameModle) + "_" + road + "_" + std::to_string(nCurBout);

	auto SpecCards = newUserSpecCards[idStr];
	if (SpecCards.isNull() || !SpecCards.isArray()) {
		m_boutDataCache.nProtectCard[3] = 2;
		m_boutDataCache.nProtectCard[2] = gameModle*10000+ m_boutDataCache.nProtectCard[0]*1000+nCurBout;
		return FALSE;
	}

	auto NewUserDealCardGroups = CConfigManagerSys::m_jsoncfgobjmgr[MAKEDEAL_CONFIG]["PlayerMakeDealRule"]["NewUserDealCardGroups2"];
	if (NewUserDealCardGroups.isNull())
	{
		m_boutDataCache.nProtectCard[3] = 6;
		return FALSE;
	}

	if (m_pGameServer->m_mapNewUserCardsIdRecord.size() > 300000) {
		m_pGameServer->m_mapNewUserCardsIdRecord.clear();
	}

	std::vector<int> vecIds;
	auto vCardIds = m_pGameServer->m_mapNewUserCardsIdRecord.find(userid);
	for(int i = 0; i < SpecCards.size(); i++)
	{
		int cardid = SpecCards[i].asInt();
		if (NewUserDealCardGroups[std::to_string(cardid)].isNull()) {
			continue;
		}

		if (vCardIds == m_pGameServer->m_mapNewUserCardsIdRecord.end())
		{
			vecIds.push_back(cardid);
		}
		else {
			auto it = std::find(vCardIds->second.begin(), vCardIds->second.end(), cardid);
			if (it == vCardIds->second.end()) {
				vecIds.push_back(cardid);
			}
		}
	}

	int id = 0;
	if (vecIds.size() > 0) {
		int randomIndex = rand() % vecIds.size();
		id = vecIds[randomIndex];
		if (m_pGameServer->m_mapNewUserCardsIdRecord[userid].empty()) {
			m_pGameServer->m_mapNewUserCardsIdRecord[userid].reserve(8);
		}
		m_pGameServer->m_mapNewUserCardsIdRecord[userid].push_back(id);

		LOG_DEBUG("GetUserCardPoolId: userid = %d  nCurBout = %d, gameModle = %d, cardid = %d", userid, nCurBout, gameModle, id);
	}
	else {
		m_boutDataCache.nProtectCard[3] = 5;
		LOG_DEBUG("GetUserCardPoolId: userid = %d  nCurBout = %d, gameModle = %d, no use cardid", userid, nCurBout, gameModle);
	}
	return id;
}

bool CGameTable::ReadNewUserAiLevel()
{
	m_boutDataCache.nProtectCard[0] = -1;
	if (CConfigManagerSys::m_jsoncfgobjmgr[MAKEDEAL_CONFIG]["PlayerMakeDealRule"]["IsEnable"].asInt() == 0)
	{
		return FALSE;
	}

	int nPlayerForNoRobot = 0;
	int iPlayerForForNoRobot = -1;
	for (int i = 0; i < TOTAL_CHAIRS; i++)
	{
		if (m_ptrPlayers == NULL || m_ptrPlayers[i] == NULL)
		{
			return false;
		}

		if (!m_ptrPlayers[i]->IsRoboter())
		{
			++nPlayerForNoRobot;
			iPlayerForForNoRobot = i;
		}
	}

	if (nPlayerForNoRobot != 1 || iPlayerForForNoRobot == -1)
	{
		return FALSE;
	}

	auto szRoomID = std::to_string(m_nRoomID);
	int bount = m_ptrPlayers[iPlayerForForNoRobot]->m_nBout;
	int nNoviceBountLimit = CConfigManagerSys::m_jsoncfgobjmgr[MAKEDEAL_CONFIG]["PlayerMakeDealRule"]["RoomRule"][szRoomID]["NewUserBout"].asInt();
	m_boutDataCache.nProtectCard[4] = bount;

	if (bount > nNoviceBountLimit)
	{
		return false;
	}

	int nManualMakeDealBout = CConfigManagerSys::m_jsoncfgobjmgr[MAKEDEAL_CONFIG]["PlayerMakeDealRule"]["RoomRule"][szRoomID]["ManualMakeDealBout"].asInt();
	if (bount <= nManualMakeDealBout)
	{
		m_boutDataCache.nProtectCard[0] = 0;
		return false;
	}

	auto NewUserSpecDeposit = CConfigManagerSys::m_jsoncfgobjmgr[MAKEDEAL_CONFIG]["PlayerMakeDealRule"]["NewUserSpecDeposit"];

	if (NewUserSpecDeposit.isNull())
	{
		m_boutDataCache.nProtectCard[3] = 1;
		return false;
	}

	int deposit = m_ptrPlayers[iPlayerForForNoRobot]->m_nDeposit;

	for(int i = 1; i <= NewUserSpecDeposit.size(); i++)
	{
		auto item = NewUserSpecDeposit[std::to_string(i)];
		if (!item.isNull() && item["min"].asInt() <= deposit && deposit < item["max"].asInt())
		{
			m_boutDataCache.nProtectCard[0] = i;

			m_nAILevel[0] = item["callFlag"].isNumeric() ? item["callFlag"].asInt() : 0;
			m_nAILevel[1] = item["robFlag"].isNumeric() ? item["robFlag"].asInt() : 0;
			m_nAILevel[2] = item["doubleFlag"].isNumeric() ? item["doubleFlag"].asInt() : 0;
			m_nAILevel[3] = item["throwTileFlag"].isNumeric() ? item["throwTileFlag"].asInt() : 0;

			UwlLogFile("[ReadNewUserAiLevel]: userid = %d, deposit = %d, road = %d, bout = %d, roomid = %d",
				m_ptrPlayers[iPlayerForForNoRobot]->m_nUserID, deposit, i, bount, m_nRoomID);
			return true;
		}
	}

	m_boutDataCache.nProtectCard[3] = 1; // 没有匹配的AI策略

	return false;
}

bool CGameTable::IsNeedNewUserAiCfg()
{
	if (!IsNewUserM2())
	{
		return FALSE;
	}

	for (int i = 0; i < 4; ++i) {
		if (m_nAILevel[i]) {
			return true;
		}
	}
	return false;
}

bool CGameTable::IsNewUserM()
{
	if (CConfigManagerSys::m_jsoncfgobjmgr[MAKEDEAL_CONFIG]["PlayerMakeDealRule"]["IsEnable"].asInt() == 0)
	{
		return false;
	}

	int nPlayerForNoRobot = 0;
	int iPlayerForForNoRobot = -1;
	for (int i = 0; i < TOTAL_CHAIRS; i++)
	{
		if (m_ptrPlayers == NULL || m_ptrPlayers[i] == NULL)
		{
			return false;
		}

		if (!m_ptrPlayers[i]->IsRoboter())
		{
			++nPlayerForNoRobot;
			iPlayerForForNoRobot = i;
		}
	}

	if (nPlayerForNoRobot != 1 || iPlayerForForNoRobot == -1)
	{
		return FALSE;
	}

	auto szRoomID = std::to_string(m_nRoomID);
	if (CConfigManagerSys::m_jsoncfgobjmgr[MAKEDEAL_CONFIG]["PlayerMakeDealRule"]["RoomRule"][szRoomID]["ManualMakeDealBout"].isNull())
	{
		return false;
	}

	int bount = m_ptrPlayers[iPlayerForForNoRobot]->m_nBout;

	int nManualMakeDealBout = CConfigManagerSys::m_jsoncfgobjmgr[MAKEDEAL_CONFIG]["PlayerMakeDealRule"]["RoomRule"][szRoomID]["ManualMakeDealBout"].asInt();
	if (bount > nManualMakeDealBout)
	{
		return false;
	}

	return true;
}

bool CGameTable::IsNewUserM2()
{
	if (CConfigManagerSys::m_jsoncfgobjmgr[MAKEDEAL_CONFIG]["PlayerMakeDealRule"]["IsEnable"].asInt() == 0)
	{
		return false;
	}

	int nPlayerForNoRobot = 0;
	int iPlayerForForNoRobot = -1;
	for (int i = 0; i < TOTAL_CHAIRS; i++)
	{
		if (m_ptrPlayers == NULL || m_ptrPlayers[i] == NULL)
		{
			return false;
		}

		if (!m_ptrPlayers[i]->IsRoboter())
		{
			++nPlayerForNoRobot;
			iPlayerForForNoRobot = i;
		}
	}

	if (nPlayerForNoRobot != 1 || iPlayerForForNoRobot == -1)
	{
		return FALSE;
	}

	auto szRoomID = std::to_string(m_nRoomID);

	//判断是否开启手动做牌，如果没有开启，使用自动做牌
	if (CConfigManagerSys::m_jsoncfgobjmgr[MAKEDEAL_CONFIG]["PlayerMakeDealRule"]["RoomRule"][szRoomID]["ManualMakeDealBout"].isNull())
	{
		return false;
	}

	int nNoviceBountLimit = CConfigManagerSys::m_jsoncfgobjmgr[MAKEDEAL_CONFIG]["PlayerMakeDealRule"]["RoomRule"][szRoomID]["NewUserBout"].asInt();

	int bount = m_ptrPlayers[iPlayerForForNoRobot]->m_nBout;

	int nManualMakeDealBout = CConfigManagerSys::m_jsoncfgobjmgr[MAKEDEAL_CONFIG]["PlayerMakeDealRule"]["RoomRule"][szRoomID]["ManualMakeDealBout"].asInt();
	if (nManualMakeDealBout < bount && bount <= nNoviceBountLimit)
	{
		return true;
	}

	return false;
}

bool CGameTable::InitEvaluateSysForClassic() {
	bool ret = false;
	auto handCardScore = CConfigManagerSys::m_jsoncfgobjmgr[MAKEDEAL_CONFIG]["HandCardScore"];
	if (m_bIsRazzMode ||  handCardScore.isNull()) {
		return ret;
	}

	auto baseScore = handCardScore["BaseScore"];
	if (!baseScore.isNull() && baseScore.size() > 0) {
		for (int i = 0; i < landlord::RANK_COUNT; ++i) {
			auto key = landlord::Rank_Str[i];
			if (baseScore[key].isInt()) {
				landlord::setRankBaseValue(i, baseScore[key].asInt());
			}
		}
		ret = true;
	}

	auto shuffleStrategy = handCardScore["ShuffleStrategy"];
	if (!shuffleStrategy.isNull() && shuffleStrategy.size() > 0) {
		auto& cfg = landlord::shuffleConfig();
		
		auto names = shuffleStrategy.getMemberNames();
		for (const auto& name : names) {
			const Json::Value& v = shuffleStrategy[name];

			if (name == "enabled") {
				if (v.isBool()) cfg.enabled = v.asBool();
				else if (v.isInt()) cfg.enabled = (v.asInt() != 0);
			}
			else if (name == "lowerThreshold") {
				if (v.isNumeric()) cfg.lowerThreshold = v.asDouble();
			}
			else if (name == "upperThreshold") {
				if (v.isNumeric()) cfg.upperThreshold = v.asDouble();
			}
			else if (name == "maxSpread") {
				if (v.isNumeric()) cfg.maxSpread = v.asDouble();
			}
			else if (name == "maxReshuffleTimes") {
				if (v.isNumeric()) cfg.maxReshuffleTimes = v.asInt();
			}
			else if (name == "maxPotentialLandlordScore") {
				if (v.isNumeric()) cfg.maxPotentialLandlordScore = v.asDouble();
			}
			else if (name == "maxLandlordAdvantage") {
				if (v.isNumeric()) cfg.maxLandlordAdvantage = v.asDouble();
			}
			else if (name == "maxSinglesPerHand") {
				if (v.isNumeric()) cfg.maxSinglesPerHand = v.asInt();
			}
			else if (name == "maxBombsPerHand") {
				if (v.isNumeric()) cfg.maxBombsPerHand = v.asInt();
			}
			else if (name == "thresholdRelaxStep") {
				if (v.isNumeric()) cfg.thresholdRelaxStep = v.asDouble();
			}
		}
		ret = true;
	}

	return ret;
}

int CGameTable::GetBombCnt(int chairno, bool isFinal) {
	int cnt = 0;
	if (chairno < 0 || chairno > 2) return cnt;

	int handCnt = CHAIR_CARDS;

	if (isFinal && chairno == m_nBanker)
	{
		int nCardIDs[CARDS_PER_CHAIR];
		XygInitChairCards(nCardIDs, CARDS_PER_CHAIR);
		
		std::memcpy(nCardIDs, m_initHandCards[chairno], sizeof(nCardIDs));

		for (int i = 0; i < m_nBottomCards; ++i) {
			nCardIDs[CARDS_PER_CHAIR + i] = m_nBottomIDs[i];
		}

		Calc2KingHandCount(nCardIDs, handCnt, cnt);
		CalcBombHandCount(nCardIDs, handCnt, cnt);
	}
	else {
		Calc2KingHandCount(m_initHandCards[chairno], handCnt, cnt);
		CalcBombHandCount(m_initHandCards[chairno], handCnt, cnt);
	}

	return cnt;
}

int CGameTable::GetHandPower(int chairno, bool isFinal) {
	int power = 0;
	if (chairno < 0 || chairno > 2) return power;

	int cnt = CARDS_PER_CHAIR;
	int nCardIDs[CHAIR_CARDS];
	XygInitChairCards(nCardIDs, CHAIR_CARDS);
	std::memcpy(nCardIDs, m_initHandCards[chairno], sizeof(nCardIDs));
	if (isFinal && chairno == m_nBanker)
	{
		for (int i = 0; i < m_nBottomCards; ++i) {
			nCardIDs[cnt++] = m_nBottomIDs[i];
		}
	}

	landlord::ShuffleDealStrategy strategy;
	auto handCards = landlord::HandCardUtils::convertZgdaIdToCard(nCardIDs, cnt);
	power = strategy.calcHandStrengthScores(handCards);

	return power;
}