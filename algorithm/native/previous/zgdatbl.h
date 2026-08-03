#pragma once

#include "AI.h"
#include "commonreq.h"
#include "Replay.h"
#include "AI_Dll.h"
#include "BoutVideo.h"

enum{

	//初级机器人AI，应用于保护机器人场景；JuniorAI总是打开状态（对局过程中持续搜集对局数据）
	ROBOTAI_JUNIOR = 1,

	//远程机器人AI，应用于陪玩机器人场景；RemoteAI在配置开关开启时才运行；
	ROBOTAI_REMOTE = 2,

	// 远程保护机器人
	ROBOTAI_PROTECT_REMOTE = 3,
};

enum class TaskKind {
	None = 0,
	Throw,                  // 1 出牌任务
	Result,                 // 2 局内结束任务
	Accumulate,             // 3 累计任务
	Share,                  // 4 分享
	ThrowNoNotify,          // 5 出牌任务，完成结算通知客户端
};

enum class ConditionID {
	None = 0,
	Result,                 // 1 结算倍率
	ThrowCard,              // 2 打出指定牌
	ThrowCardType,          // 3 打出牌型
	Ya,						// 4 打出牌型并压制
	LastHand,               // 5 最后一手
	Round,                   // 6 回合任务  number
	Win,                    // 7 获得胜利，有三种情况： 0：获得胜利， 1：以地主的身份获胜， 2：春天/反春天获胜
	GetMoney,                // 8 获取银子
	Role,                    // 9 角色身份
	Double,                  // 10 加倍
	Spring,                  // 11 春天
	Count,                  // 12 局数
};

class CGameServer;

extern void InitialGameTableInfo(GAME_TABLE_INFO* table);
extern int  My_GetIndexPRI(int nCardID,int nRank, DWORD gameflags); //大小

class CGameTable : public CSkTable{
public:
	CGameTable(int roomid = INVALID_OBJECT_ID, int tableno = INVALID_OBJECT_ID, int score_mult = 1, 
			int totalchairs = TOTAL_CHAIRS, DWORD gameflags = GAME_FLAGS,
			DWORD gameflags2 = 0, 
			int max_asks = MAX_ASK_REPLYS,
			int totalcards = TOTAL_CARDS, 
			int totalpacks = CARD_PACKS, int chaircards = CHAIR_CARDS, int bottomcards = 0,
			int layoutnum = LAYOUT_NUM, int layoutmod = SK_LAYOUT_MOD, int layoutnumex = 0, 
			int abtpairs[] = NULL,
			int throwwait = DEF_THROW_WAIT, int maxautothrow = MAX_AUTO_THROW,
			int entrustwait = DEF_ENTRUST_WAIT,
			int max_auction = 3, int min_auction = MIN_AUCTION_GAINS,
			int def_auction = 1,
			FP_SK_GetXXX fpSKGetCardIndex = SK_GetCardIndex,
			FP_SK_GetXXX fpSKGetCardShape = SK_GetCardShape,
			FP_SK_GetXXX fpSKGetCardValue = SK_GetCardValue,
			FP_SK_GetXXX fpSKGetCardScore = SK_GetCardScore,
			FP_SK_GetXXXEx fpSKGetCardPRI = SK_GetCardPRI,
			FP_SK_GetXXXEx fpSKGetIndexPRI = My_GetIndexPRI);
	virtual ~CGameTable();

public:
	//////////////////////////////////////////////////////////////////////////
	//重载区
	virtual void ResetMembers(BOOL bResetAll = TRUE);
	//各种fill，需要重载
	virtual int				GetGameTableInfoSize();
	virtual int				GetGameStartSize();
	virtual int				GetGameWinSize();
	virtual int				GetEnterGameInfoSize();
	virtual void			FillupEnterGameInfo(void* pData, int nLen, int chairno, BOOL lookon = FALSE);
	virtual void			FillupGameTableInfo(void* pData, int nLen, int chairno, BOOL lookon = FALSE);
	virtual void			FillupGameStart(void* pData, int nLen, int chairno, BOOL lookon = FALSE);
	virtual int				FillupGameWin(void* pData, int nLen, int chairno);
	virtual void			FillupStartData(void* pData, int nLen);
	//牌操作相关，需要重载
	virtual int				CatchOneCard(int chairno);					 
	virtual BOOL			GiveCard(int chairno,int destchair,int nCardID);	 			 
	virtual BOOL			SetCardStatus(int nCardID,int chairno,int nStatus);		 
	virtual void			PutThrowCardsToCost(int chairno);					 
	virtual void			PutAllCardsToCost();								 
	virtual int				GetTributeCard(int chairno);	
	virtual CARDINFO*		GetCard(int nCardID);								 
	virtual int				GetInHandCard(int chairno,int nCardIDs[]);		 
	virtual BOOL			IsCardInHand(int nChairNO,int nCardIDs[],int nCount);
			BOOL			IsCardIdUnique(int aCard[], int nLen);
	//出牌过程，需要重载
	virtual BOOL			ValidatePass(CARDS_PASS* pCardsPass);
	virtual BOOL			ValidateThrow(CARDS_THROW* pCardsThrow);		 
	virtual int				ThrowCards(CARDS_THROW* pCardsThrow);			 
	virtual int				CalcChairThrowTime(int chairno);				 
	virtual void			CalcBombInThrow(CARDS_THROW* pCardsThrow);	 				 
	virtual void			OnPass(CARDS_PASS* pCardsPass);
	virtual BOOL			CalcWinOnThrow(CARDS_THROW* pCardsThrow);			 
	virtual BOOL			CalcWinOnPass(CARDS_PASS* pCardsPass);			 
	virtual void			SetCurrentRank(int nRank);						 
	virtual int				GetCurrentRank();	

	void  OnChat(LPCHAT_TO_TABLE chatInfo, const std::string& sRecord);

	//选择重载
	virtual int				CalcBaseDeposit(int nDeposits[], int tableno);
	virtual int				CalcWinFeesEx(int nOldDeposits1[], int nOldDeposits2[], int nDepositDiffs[], int nWinFees[]);
	virtual int				CalcBankerChairBefore();
	virtual int				CalcBankerChairAfter(void* pData, int nLen);
	virtual int				CalcResultDiffs(void* pData, int nLen, int nScoreDiffs[], int nDepositDiffs[]);
	virtual BOOL			CalcWinPoints(void* pData, int nLen, int chairno, int nWinPoints[]);
	virtual void			FillupNextBoutInfo(void* pData, int nLen, int chairno);
	virtual DWORD			SetStatusOnStart();
	virtual int				SetCurrentChairOnStart();
	virtual int				SetCurrentChair(int chairno,int nWaitSecond);
	virtual BOOL			OnAuctionFinished();
	virtual BOOL			OnAuctionBanker(LPAUCTION_BANKER pAuctionBanker, int& auction_finished, BOOL bAuto=FALSE);
	virtual void			setAuctionEndResult(int& auction_finished) { return; };		// 设置三个玩家不叫的结果。
	virtual BOOL			IsGameMsg(UINT resquesID);//查询是不是游戏消息

	virtual BOOL			CaclCardType_ABT_Couple(int nCardIDs[],int nCardLen,int nCardCount,CARD_UNITE* CardDetail);
	virtual BOOL			GetCardType_ABT_Couple(int nCardIDs[],int nCardLen,int nCardLay[], int nLayLen,int nJokerCount,UNITE_TYPE& type,int nMaxPair=3);
	virtual BOOL			CaclCardType_ABT_Single(int nCardIDs[],int nCardLen,int nCardCount,CARD_UNITE* CardDetail);
	virtual BOOL			GetCardType_ABT_Single(int nCardIDs[],int nCardLen,int nCardLay[], int nLayLen,int nJokerCount,UNITE_TYPE& type,int nMaxCount=12);
	virtual BOOL			CalcCardType_Bomb(int nCardIDs[],int nCardLen,int nCardCount,CARD_UNITE* CardDetail);
	virtual BOOL			GetCardType_Bomb(int nCardIDs[],int nCardLen,int nCardLay[], int nLayLen,int nJokerCount,UNITE_TYPE& type,int nUseCount=0);
	virtual BOOL            GetUniteDetails(int chairno, int nCardIDs[],int nCardsLen,CARD_UNITE& unit_detail,DWORD dwFlags=CARD_UNITE_TYPE_TOTAL_EX);
	virtual BOOL			CaclCardType_ABT_Three_Couple(int nCardIDs[],int nCardLen,int nCardCount,CARD_UNITE* CardDetail);
	virtual BOOL			GetCardType_ABT_Three_Couple(int nCardIDs[],int nCardLen,int nCardLay[], int nLayLen,int nJokerCount,UNITE_TYPE& type,int nMaxPair=2);
			int				GetLayPri(int nCardLay[], int nLayLen);	//lay中有值的权值,（nCardLay值表示个数）
			int				GetBit1Count(unsigned int bit);			//得到数值二进制里1的个数
			int				GetIndexByIndex(int nCardLay[], int nLayLen, int nIndex);		//得到第nIndex个数的index
			int				GetLayPriEx(int nCardLay[], int nLaylen);	//lay中有值的权值,（nCardLay值表示这个值）

	//对子
	virtual BOOL			GetCardType_Couple(int nCardIDs[],int nCardLen,int nCardLay[], int nLayLen,int nJokerCount,UNITE_TYPE& type);

	//三张
	virtual BOOL			GetCardType_Three(int nCardIDs[],int nCardLen,int nCardLay[], int nLayLen,int nJokerCount,UNITE_TYPE& type);

	//三带一
	virtual BOOL			CaclCardType_Three_1(int nCardIDs[],int nCardLen,int nCardCount,CARD_UNITE* CardDetail);
	virtual BOOL			GetCardType_Three_1(int nCardIDs[],int nCardLen,int nCardLay[], int nLayLen, int nJokerCount, UNITE_TYPE& type);

	//三带对
	virtual BOOL			GetCardType_Three_Couple(int nCardIDs[], int nCardLen, int nCardLay[], int nLayLen, int nJokerCount, UNITE_TYPE& type);

	//飞机
	virtual BOOL			GetCardType_ABT_Three(int nCardIDs[],int nCardLen,int nCardLay[], int nLayLen,int nJokerCount,UNITE_TYPE& type,int nMaxPair);

	virtual BOOL			CaclCardType_ABT_Three_1(int nCardIDs[],int nCardLen,int nCardCount,CARD_UNITE* CardDetail);
	virtual BOOL			GetCardType_ABT_Three_1(int nCardIDs[],int nCardLen,int nCardLay[], int nLayLen,int nJokerCount,UNITE_TYPE& type,int nMaxPair=2);

	//4带2张单
	virtual BOOL			CalcCardType_Four_2(int nCardIDs[],int nCardLen,int nCardCount,CARD_UNITE* CardDetail);
	virtual BOOL			GetCardType_Four_2(int nCardIDs[],int nCardLen,int nCardLay[], int nLayLen,int nJokerCount,UNITE_TYPE& type);

	//4带2对子
	virtual BOOL			CalcCardType_Four_2_Couple(int nCardIDs[],int nCardLen,int nCardCount,CARD_UNITE* CardDetail);
	virtual BOOL			GetCardType_Four_2_Couple(int nCardIDs[],int nCardLen,int nCardLay[], int nLayLen,int nJokerCount,UNITE_TYPE& type);

	//癞子炸弹
	virtual BOOL			CalcCardType_BombMixedRazz(int nCardIDs[],int nCardLen,int nCardCount,CARD_UNITE* CardDetail);
	virtual BOOL			GetCardType_BombMixedRazz(int nCardIDs[],int nCardLen,int nCardLay[], int nLayLen,int nJokerCount,UNITE_TYPE& type,int nUseCount=0);

	//纯癞子炸弹
	virtual BOOL			CalcCardType_BombPureRazz(int nCardIDs[],int nCardLen,int nCardCount,CARD_UNITE* CardDetail);
	virtual BOOL			GetCardType_BombPureRazz(int nCardIDs[],int nCardLen,int nCardLay[], int nLayLen,int nJokerCount,UNITE_TYPE& type,int nUseCount=0);

	//火箭
	virtual BOOL			CalcCardType_BOMB_2King(int nCardIDs[],int nCardLen,int nCardCount,CARD_UNITE* CardDetail);
	virtual BOOL			GetCardType_BOMB_2King(int nCardIDs[],int nCardLen,int nCardLay[], int nLayLen,int nJokerCount,UNITE_TYPE& type);

	virtual BOOL			GetBestUnitType(UNITE_TYPE& first_card,CARD_UNITE& fight_card);
	virtual BOOL			GetBestUnitType(CARD_UNITE& fight_card);

	//叫庄补充
	virtual	BOOL			IsTooManyAuctionRound();
			void			AuctionRoundEnd();
			void			ResetAuctionOnStandOff();
	virtual void			InvalidResults(void* pData, int nLen);
	virtual	void			ActuallizeResults(void* pData, int nLen);
			void			ResetAuctionWhenFinished();
			void			SoftResetMember();	//重新发牌时，稍微初始化下

	//积分玩法
			void		    SetScoreFee(int nScoreFee);
			virtual int		CalcWinFeesScore(int nOldScores1[], int nOldScores2[], int nScoreDiffs[], int nWinFees[]);
			virtual int		CompensateScores(int nOldScores[], int nScoreDiffs[]);
			BOOL		    CheckScoreResults(int nScoreDiffs[], int nWinFees[], int totalfee);

	//疯狂玩法
			BOOL			OnRobBanker(LPROB_BANKER pRobBanker, int& rob_finished, int noCallBankerForceType, BOOL bAuto=FALSE);

	//不能以小博大限制修改
	virtual int				CompensateDeposits(int nOldDeposits[], int nDepositDiffs[]);
			
	//断线处理：4次掉线出牌，断桌子
			void			OnPlayerPassiveEvent(int chairno);
			void			OnPlayerActiveEvent(int chairno);
			BOOL			IsTooManyAutoPlay(int chairno);
	virtual int				CalcDoubleOfScore(int chairno, int breakchair, int defdouble);
	virtual int				CalcBreakDeposit(int breakchair, int breakdouble, int& cut);
	virtual void			ResetTable();//清除桌子游戏，局数重新开始

	//做炸弹牌
			BOOL			MakeDealCards(int nMakeChance);
			int				GetRandomBombNum(int seed);
			void			MakeCardsLayIn(int nChair,int nBombNum,int& nCurIndex,int nBombValue[]);
			void			DealCardNormal();
			void			DealCardMakeMode();
			int				IsMakedCard(int nCardID);
			int				CalcBombInHandForBroken();
			
	//做牌相关
			void			MakeDealByCfg(int cards[], int length);
			bool			IsNeedMakeDealByUserBoutInfo(CPlayer* pPlayer);
			void			GetMakeDealCfg(LPMAKEDEALCFG pCfg, std::string MakeDealStrategy = "");
			BOOL			DoMakeDeal(int (*pChairCards)[CARDS_PER_CHAIR], int (*pCardLays)[SK_LAYOUT_NUM], int nReserveCards[], int nReserveCount, 
										LPMAKEDEALCFG pCfg, int nHandCount[], int nBombCount[], int nBigCardsCount[]);
			void			MatchFirstChairCards(int nChairCard[], int nCardLay[], int nReserveCards[], int nReserveCount, LPMAKEDEALCFG pCfg);
			void			MatchOtherChairCards(int nChairCard[], int nCardLay[], int nReserveCards[], int nReserveCount, LPMAKEDEALCFG pCfg, int nChairNO = -1);
			BOOL			CopyMatchedCardID(int &nPreCardID, int nCardLay[], int nMatchedCardID, int nReserveCards[], int nReserveCount);
			void			CompareAndLogCardLays(int (*pOriginalCardLays)[SK_LAYOUT_NUM], int (*pModifiedCardLays)[SK_LAYOUT_NUM], int nCount);
			void			CompareAndLogCardLay(int *pOriginalCardLay, int *pModifiedCardLay);
			void			SimulateStartDeal(CGameServer* pGameServer);
			BOOL			IsNeedMakeDealForNovice();
			BOOL			ReadNoviceCardsFromFile();
	//////////////////////////////////////////////////////////////////////////


	GAME_TABLE_INFO*  GetGameTableInfo();
	GAME_PUBLIC_INFO* GetPublicInfo();
	GAME_PLAYER_INFO* GetPlayerInfo(int chairno);

	void		  ConstructGameData();
	virtual void  StartDeal();
	virtual	BOOL  CheckCards();	//发完牌后，检查下牌
	virtual void  FillupGameResults(void* pData, int nLen, GAME_RESULT_EX GameResults[]);

	virtual BOOL  IsJoker(int nCardID);
	virtual int   PreDealCards(int nCardIDs[],int nCardLen,int nCardLay[], int nLayLen,int& nJokerCount);
	virtual BOOL  GetDoubleCount(int nCardLay[], int nLayLen,int nCount1,int nCount2,int nJokerCount,int nDestValue,int& nMainIndex,int& nSecondeIndex);
	virtual int   GetPlayerCountOnTable();					//桌子上的玩家数

			int	  CalcBanker(BOOL isFixBankerToSoleRealPlayer);
			int	  CalcRazzCardValue();	//选癞子牌，计算癞子牌牌值
			int   GetCardValueById(int nCardID);	//根据cardId计算牌值
			int   GetCardValueByIndex(int nCardIndex);	//根据cardIndex计算牌值

	/************************************验证癞子牌改变的大小**********************************************/    
			BOOL  ValidateDoubleCountForRazz(int nCardIDs[], int nCardLen, int nCardLay[], int nJokerCount, int count1, int count2, int nMainIndex, int nSecondeIndex);
			BOOL  ValidateThreeCountForRazz(int nCardIDs[], int nCardLen, int nCardLay[], int nJokerCount, int count1, int count2, int count3, int nMainIndex, int nSecondIndex, int nThirdIndex);
			BOOL  ValidateAbtSingleForRazz(int nCardIDs[], int nCardLen, int nCardLay[], int nJokerCount, int nStartIndex);
			BOOL  ValidateAbtCoupleForRazz(int nCardIDs[], int nCardLen, int nCardLay[], int nJokerCount, int nStartIndex);
			BOOL  ValidateAbtThreeForRazz(int nCardIDs[], int nCardLen, int nCardLay[], int joker_UsedAbt, int joker_UsedCouple, int joker_UsedSingle, int nStartIndex, int nMaxPair=2, int coupleIndex[]=0);
			BOOL  ValidateSameCountForRazz(int nCardIDs[], int nCardLen, int nCardLay[], int nJokerCount);
	/****************************************************************************************************************/

			int	  GetChairCards(int nChairNO,int nCardIDs[], int nLen = CHAIR_CARDS);
			void  OnAutoAuction(int nChairNO, AUCTION_BANKER* pAuctionBanker, 
				BOOL bRemote, std::function<std::remove_pointer_t<decltype(CAI_Dll::m_AI_AutoAuction)>> cbRmote = nullptr);
			void  OnAutoDouble(int nChairNO, PLAYER_DOUBLE* pAuctionBanker,
				BOOL bRemote, std::function<std::remove_pointer_t<decltype(CAI_Dll::m_AI_AutoAuction)>> cbRmote = nullptr);
			void  OnAutoRob(int nChairNO, ROB_BANKER* pRobBanker, 
				BOOL bRemote, std::function<std::remove_pointer_t<decltype(CAI_Dll::m_AI_AutoRob)>> cbRmote = nullptr);
			void  OnAutoThrow(int nChairNO, BOOL &bThrow, CARDS_THROW* pThrowCards, 
				BOOL bRemote, std::function<std::remove_pointer_t<decltype(CAI_Dll::m_AI_AutoThrow)>> cbRmote = nullptr);
			void  OnAutoChat(int nChairNO, CHAT_TO_TABLE* pChatToTable,
				BOOL bRemote, std::function<std::remove_pointer_t<decltype(CAI_Dll::m_AI_AutoChat)>> cbRmote = nullptr);

			virtual int AI_GetCurrentActionSeq(BOOL bSkipChat);

			BOOL CGameTable::readJSONCard(int &cardid, Json::Value &card);

	//razz begin
	/************************************计算癞子牌改变的大小**********************************************/    
		    BOOL  CalcRazzValueInSame(int nJokerCount, int nCardIndex);
			BOOL  CalcRazzValueInDoubleCount(int nCardLay[], int nJokerCount, int count1, int count2, int nMainIndex, int nSecondIndex);
            BOOL  CalcRazzValueInThreeCount(int nCardLay[], int nJokerCount, int count1, int count2, int count3, int nMainIndex, int nSecondIndex, int nThirdIndex);
			BOOL  CalcRazzValueInAbtSingle(int nCardLay[], int nJokerCount, int nStartIndex, int nCardCount);
			BOOL  CalcRazzValueInAbtCouple(int nCardLay[], int nJokerCount, int nStartIndex, int nMaxPair);
			BOOL  CalcRazzValueInAbtThree(int nCardLay[], int nJokerCountAbt, int nJokerCountCouple, int nJokerCountSingle, int nStartIndex, int nMaxPair, int coupleIndex[]=0);
    /********************************************************************************************************/    
    // razz end

	// match begin
	BOOL	m_bIsMatchGame;
	int		m_nMatchAbtWinCount[TOTAL_CHAIRS];
	Json::Value m_jsonMatchInfo[TOTAL_CHAIRS];

	Json::Value m_jsonDdzTaskInfo[TOTAL_CHAIRS];

	// virtual int	GetPlace(int nPlace);//返回名次为第几的人
	void	FillMatchGameWinResult(void* pData);
	virtual int GetBaseScore(int base_score = 0);
	// int CalcDoubleOfScore(int chairno, int breakchair, int defdouble);
	// match end

	int			  m_nScoreFee;// 服务费数量
	int			  m_nAuctionRound;
	GAME_TABLE_INFO* m_GameTalbeInfo;
	int			  m_nAutoPlayCount[TOTAL_CHAIRS];
	int			  m_nResultDiff[MAX_CHAIR_COUNT][MAX_RESULT_COUNT];
	int			  m_nTotalResult[MAX_CHAIR_COUNT];
	PRE_DEAL	  m_nBombHadDeal[LAYOUT_NUM];
	int			  m_nBottomCatch[BOTTOM_CARD];
	int			  m_nOperateTime;		//游戏操作时间，包括打牌和叫地主
	BOOL		  m_bIsCrazyMode;
	ROB			  m_Rob[TOTAL_CHAIRS];	// 抢庄情况记录
	int			  m_nRobCount;				// 抢庄计数
	LONGLONG	  m_nBoutBeginTime; //对局开始时间

	TCHAR		  m_szUsername[TOTAL_CHAIRS][MAX_USERNAME_LEN];
	int			  m_nRazzCardValue;
	BOOL		  m_bIsRazzMode;
	int			  m_nRazzCardsAlter[MAX_RAZZ_COUNT];

	CLandlordsAI  m_GameAI[TOTAL_CHAIRS];

	//razz begin
	BOOL                          m_bIsRemind;                  //提示
	RAZZCARDS_ALTERVALUE_UNIT     m_razzCardsAlterValueUnit;    //一手牌中不同牌型中癞子变化值

	int			  m_nFirstRazzCardsAlter[MAX_RAZZ_COUNT];  //记录上上手牌的癞子的改变的值
	int           m_nFirstChairNo;
	int           m_nSecondChairNo;
	DWORD         m_dwFirstCardType;
	DWORD         m_dwSecondCardType;
	//razz end

    int           m_nCompensate;
	int           m_nCardType[2][5];
	int           m_nThrowCardCounts[TOTAL_CHAIRS][5];

	int			  m_nSectionNum;	//银子区间序号

	int           m_nContinueWin[TOTAL_CHAIRS];
	DWORD         m_nLastWaterTime[TOTAL_CHAIRS];
	int           m_nUseCardMaster[TOTAL_CHAIRS];

    CGameServer*            m_pGameServer;


public://通讯相关
	DWORD   m_dwLastClockStop;

//读取配置文件
	BOOL    ReadCardsFromFile();
	CString GetINIFileName();
	int     RetrieveFields ( TCHAR *buf, TCHAR **fields, int maxfields, TCHAR**buf2 );

	int     m_nSuppRessChairNo;
	void	SetSuppressChairNo(int nChairNo);
	BOOL	IsSuppress(int nChairNo);


	/**********************************************
	*举报记录
	*/
	CReplayRecord	m_replayRecord;
	PLAYER_REPORT_STATUS m_nPlayerReportedStatus[TOTAL_CHAIRS];				//玩家举报记录

	BOOL m_bNoShuffMakeDeal;
	BOOL m_b2K;
	DWORD m_dwAuctionFinishTime;
	Json::Value jsonAddInfo[TOTAL_CHAIRS];
	
public:
	//通过座位获取玩家的所有手牌数值
	int		GetPlayerCardIDs(int chairno, int nCardIDs[], int len);
	CPlayer* GetPlayerByUserID(int userid);

	//设置玩家的举报状态
	//param：userid--发起举报玩家id,reportedUserid--被举报玩家id,status--举报状态
	void SetPlayerReportedStatus(int userid, int reportedUserid, int status);
	//重置举报记录数据
	void ResetPlayerReportedStatus();

	BOOL FillPlayerReportedStatus(void* pData, int dataLen, int userid);
	int GetPlayerReportedStatusSize();
	void FillupHandCardsInfo(LPHANDCARDS_INFO pHandcards);
	void InitPlayerReportedStatusByValue(int val);
	int GetOtherUserID(int userid, int reportedUserid);
	int GetBaseDeposit(int deposit_mult = 1);

public:
	std::stringstream m_sinRecord;
	BOOL m_bNeedRecord = FALSE;
	void SetNeedRecord(BOOL bNeed) { m_bNeedRecord = bNeed; }
	BOOL IsNeedRecord() const { return m_bNeedRecord; }
	std::string CoverCardIDsEx(int nCardIDs[], int nLen);
	virtual BOOL ConstructGameResults(void* pData, int nLen, int roomid, int gameid,
	LPREFRESH_RESULT_EX lpRefreshResult, GAME_RESULT_EX GameResults[]);

	BOOL m_bNeedGameReferee = FALSE;
	void SetNeedGameReferee(BOOL bNeed) { m_bNeedGameReferee = bNeed; }
	BOOL IsNeedGameReferee() const { return m_bNeedGameReferee; }	 
	std::string m_strGameRefereeHost = "";
	int Report2GameReferee(const char* record);

	//新版机器人----------------------------------------
public:
	BOOL IsRobotTable();
	int GetRobotCount();
	BOOL Robot_IsUseRemoteAI();
	BOOL Robot_InitRobotAITypeOnBoutStart(int aiLevel = 0);
	void Robot_IncRemoteAITimeout();
	void Robot_IncRemoteAITimeoutWithUserId(int userId, int chairNo, int seq, int startTimestamp, CString aiEngineUrl);
	void Robot_SetRemoteAIRobotPeerBottomEnable();
	void SetPlayerPeeredBottom(int userId);
	BOOL IsPlayerPeeredBottom(int userId);
	int GetPeeredBottomPlayerCount();
	BOOL SetBoutDataCacheOfGameWinData(GAME_WIN_RESULT* pGameWinResult);

private:
	int m_nCurRobotAIType; //当前机器人AI类型（JuniorRobotAI对应保护机器人AI，SeniorRobotAI对应陪玩机器人AI）
	int m_nRemoteAITimeout; //本次对局远程AI超时次数
	BOOL m_bIsRemoteAIRobotPeerBottomEnable; //本局是否机器人也看底牌
public:
	int m_nPassTimes[TOTAL_CHAIRS]; //过牌次数
	int m_initHandCards[TOTAL_CHAIRS][CHAIR_CARDS]; //初始手牌
	int m_nRoundCount; //回合数
	int m_peeredBottomPlayer[TOTAL_CHAIRS];
	BOUTDATACACHE m_boutDataCache; //对局数据缓存
	//------------------------------------------------------------------

	//新版不洗牌做牌-------------------------------------------
public:
	BOOL IsNeedMakeDealForNoShuff(TCHAR *pRoomID);
	BOOL MakeDealForNoShuff(int nOrder, int nBomb, int nThree, int nStraight, int cards[], std::vector<int> &vecBomb, std::vector<int> &vecThree, std::vector<int> &vecStraight, std::vector<int> &vecCouple, std::vector<int> &vecSingle, int &nDFS);

	void FindBomb(int nBomb, int nChooseArr[], int nCardLays[], std::vector<int> &vecBomb);
	void FindThree(int nThree, int nChooseArr[], int nCardLays[], std::vector<int> &vecThree, std::vector<int> &vecCouple, std::vector<int> &vecSingle);
	void FindStraight(int nStraight, int nChooseArr[], int nCardLays[], std::vector<int> &vecStraight);

	void DelChooseLayer(int nCardLays[], ADJUST_CARDS &stBestFind);
	void DealChooseCards(int cards[], int nCardCount[][CARDS_PER_CHAIR + 3], int nCounts[], std::vector<int> &vecBomb, std::vector<int> &vecThree, std::vector<int> &vecStraight, std::vector<int> &vecCouple, std::vector<int> &vecSingle);
	int FindOneCardByIndex(int cards[], int nCardIndex);
	void AddLeftLayout(int nCardLays[], std::vector<int> vecBomb, std::vector<int> vecThree, std::vector<int> vecStraight, std::vector<int> vecCouple, std::vector<int> vecSingle);
	void FindDFS(int nCardLays[], DWORD &dwFlag, int nCountLimit, ADJUST_CARDS &stFind, ADJUST_CARDS &stBestFind, int &nDeep, int &nDFS);
	void FindCoupleSingle(int nCardLays[], ADJUST_CARDS &stFind);
	void FitChooseCards(ADJUST_CARDS stBestFind, int nChairCards[][CARDS_PER_CHAIR + 3], int nChairNo, int cards[], int nCardLays[], int nCounts[]);
	void OpelChooseCard(int nChairCards[][CARDS_PER_CHAIR + 3], int cards[]);
	void ClearAdjustCards(ADJUST_CARDS &stFind);

	void CalcFindCountAndValue(ADJUST_CARDS &stFind);
	//------------------------------------------------------------

public:
	CBoutVideo m_boutVideo;

public:
	BOOL CanAllThrow(int nChairNO, CARD_UNITE& unit_details);
	int SelectMinUnite(int nInHand[], int nInHandCount, int nPrompt[], int nPromptCount);

public:
	// 加倍相关
	BOOL			m_bAlreadThrowed; // 是否出过牌了
	int				m_PlayerDouble[TOTAL_CHAIRS]; //玩家加倍情况
	int				m_nDoubleType;
	int				m_nDoubleTime;
	int				m_nSuperDoubleCost;
	std::string		m_strModifyTime; // 文件修改时间 
	BOOL OnPlayerDouble(int nChair, int nDoubleType, BOOL &bDoubleFinished, BOOL bAuto=FALSE);
	BOOL OnPlayerDoubleFinished();
	DOUBLE_COMMON_INFO ReadDoubleCommonInfo();
	DoubleType GetDoubleType();

	bool CheckResultTask(int charno);

	bool CheckThrowTask(CARDS_THROW* pCardsThrow, int preWaitChar);

	bool UpdateTaskStatus(Json::Value& root, int charno, CARDS_THROW* pCardsThrow = nullptr, int preWaitChar = -1);

	bool CheckTaskFinished(const Json::Value& root);

	// 结算矫正原逻辑
	int CGameTable::CompensateDepositsOriginal(int nOldDeposits[], int nDepositDiffs[]);
	// 结算矫正新逻辑-房间输赢上限
	int CGameTable::CompensateDepositsForSilverLimit(int nOldDeposits[], int nDepositDiffs[]);

	int GetUserCardPoolId();

	bool ReadNewUserAiLevel();
	bool IsNeedNewUserAiCfg();

	bool IsNewUserM();

	bool IsNewUserM2();

	bool InitEvaluateSysForClassic();

	int GetBombCnt(int chairno, bool isFinal = false);

	int GetHandPower(int chairno, bool isFinal = false);


	// ip数据上报新增
	TCHAR m_nPlayerIpData[TOTAL_CHAIRS][MAX_SERVERIP_LEN];
	// 理论倍数
	int	m_nMagnificationTheory[TOTAL_CHAIRS];
	int m_nCardTypeLimit;		// 20251231版本策划要求炸弹牌型限制，开关是为了兼容三端外放
	int m_nRoomSilverLimit;	// 房间银子输赢上限
	int m_nMakeDealTypes[TOTAL_CHAIRS];// 记录做牌类型上报  0:不优化牌型， 1:新手优化牌型， 2:老的优化牌型nMakeDealType = 0  3新的优化牌型 nMakeDealType = 1
	bool m_bIsProtected; // 是否已经开始过游戏
public:
	// 好友房新增字段。
	int	m_nMultiScore;
	int	m_nMinMode;
	int	m_nTotalScores[TOTAL_CHAIRS];

	int m_nAILevel[4];	// ai等级  callFlag，robFlag，doubleFlag，throwTileFlag

// 棋牌文化节新增
public:
	long long m_startTimeStamp;
	virtual BOOL IsFriendRoom() { return false; };

	// 统计玩家操作耗时（并非开局到结束的 对局时长）。
	DWORD nPlayerOPCost[TOTAL_CHAIRS];
	DWORD nLastOPTime[TOTAL_CHAIRS];

	void ClearCostTime() {
		memset(nPlayerOPCost, 0x00, sizeof(int) * TOTAL_CHAIRS);
		memset(nLastOPTime, 0x00, sizeof(int) * TOTAL_CHAIRS);
	}
	// 客户端发完牌之后就叫地主，服务端自行为其添加一定的时延。
	void SetPlayerLastOPTime(int nChairNo,DWORD nTickCnt) {
		// 仅0-2是合法范围。
		if (nChairNo < 0 || nChairNo >= 3)	return;
		nLastOPTime[nChairNo] = nTickCnt;
	}
	void SetPlayerLastOPTime(int nChairNo) {
		SetPlayerLastOPTime(nChairNo, GetTickCount());
	}
	// 先手情形下，重置上次操作时间。
	void setAllPlayerLastOPTime() {
		for (int i = 0; i < 3; i++) {
			int nUserID = m_ptrPlayers[i]->m_nUserID;
			// 只统计参赛选手的信息
			if (compManager->getPlayerType(nUserID) != -1) {
				SetPlayerLastOPTime(i);
			}
		}
	}
	void updatePlayerOPCost(int nChairNo) {
		if (nLastOPTime[nChairNo] == 0) {
			SetPlayerLastOPTime(nChairNo);
		}
		
		auto nowTick = GetTickCount();
		auto TickDiff = nowTick - nLastOPTime[nChairNo];
		nPlayerOPCost[nChairNo] += TickDiff;

		SetPlayerLastOPTime(nChairNo, GetTickCount());
	}

	void testCollect830Data(int nChairNo) {
		int curChair		= nChairNo;
		long long startTime	= m_startTimeStamp;
		DWORD opCostTime	= nPlayerOPCost[curChair]/1000;

		UwlLogFile("830Competition:CurChair:%d,startTime %lld,costTime %lu",curChair,startTime,opCostTime);
	}
};


inline void SvrReversalMoreByValue(int array[],int value[],int length)
{
	
	int i,j,temp; 
	for(i=0;i<length-1;i++) 
		for(j=i+1;j<length;j++) /*注意循环的上下限*/ 
			if(value[i]<value[j]) 
			{ 
				temp=array[i]; 
				array[i]=array[j]; 
				array[j]=temp;
				temp=value[i]; 
				value[i]=value[j]; 
				value[j]=temp;
			}
}
//以Seed为随机数，对数组array随机排序
inline void  SvrXygRandomSort(int array[],int length,int seed)
{
	srand(seed);
	int* value=new int[length];
	int s=length*1000;
	for(int i=0;i<length;i++)
		value[i]=rand()%s;
	SvrReversalMoreByValue(array,value,length);
	delete []value;
}

//统计王炸牌型信息
inline void Calc2KingHandCount(int nCardLay[], int &nHandCount, int &nBombCount)
{
	int nKingCount = nCardLay[14]+nCardLay[15];

	if (2 == nKingCount)
	{
		nBombCount++;
		nCardLay[14] = 0;
		nCardLay[15] = 0;
	}

	nHandCount += 0;
}

//统计炸弹牌型信息
inline void CalcBombHandCount(int nCardLay[], int &nHandCount, int &nBombCount)
{
	for (int i = 0; i < SK_LAYOUT_NUM; i++)
	{
		if (4 == nCardLay[i])
		{
			nBombCount++;
			nHandCount += -1;
			nCardLay[i] = 0;
		}
	}
}

//统计飞机牌型信息
inline void CalcABTThreeHandCount(int nCardLay[], int &nHandCount, int &nBombCount)
{
	for (int i = 0; i < SK_LAYOUT_NUM; i++)
	{
		if (nCardLay[i] >= 3)
		{
			if (i <= 1 || i >= 13)//A以上特殊处理
				continue;

			int j = i;
			int nThreeCount = 1;
			while ((++j <= SK_LAYOUT_MOD) && nCardLay[j] >= 3)
			{
				nThreeCount++;
			}

			if (nThreeCount <= 1)//未能构成飞机牌型
				continue;

			nHandCount += 1 - nThreeCount;

			while (nThreeCount)
			{
				nCardLay[i+nThreeCount-1] -= 3;
				nThreeCount--;
			}
		}
	}
}

//统计三张牌型信息
inline void CalcThreeHandCount(int nCardLay[], int &nHandCount, int &nBombCount)
{
	for (int i = 0; i < SK_LAYOUT_NUM; i++)
	{
		if (3 == nCardLay[i])
		{
			nHandCount += 0;
			nCardLay[i] = 0;
		}
	}
}

//统计连对牌型信息
inline void CalcABTCoupleHandCount(int nCardLay[], int &nHandCount, int &nBombCount)
{
	for (int i = 0; i < SK_LAYOUT_NUM; i++)
	{
		if (nCardLay[i] >= 2)
		{
			if (i <= 1 || i >= 13)//A以上特殊处理
				continue;
			
			int j = i;
			int nCoupleCount = 1;
			while ((++j <= SK_LAYOUT_MOD) && nCardLay[j] >= 2)
			{
				nCoupleCount++;
			}
			
			if (nCoupleCount <= 2)//未能构成连对牌型
				continue;
			
			nHandCount += 1;
			
			while (nCoupleCount)
			{
				nCardLay[i+nCoupleCount-1] -= 2;
				nCoupleCount--;
			}
		}
	}
}

//统计顺子牌型信息
inline void CalcABTHandCount(int nCardLay[], int &nHandCount, int &nBombCount)
{
	for (int i = 0; i < SK_LAYOUT_NUM; i++)
	{
		if (nCardLay[i] >= 1)
		{
			if (i <= 1 || i >= 13)//A以上特殊处理
				continue;
			
			int j = i;
			int nSingleCount = 1;
			while ((++j <= SK_LAYOUT_MOD) && nCardLay[j] >= 1)
			{
				nSingleCount++;
			}
			
			if (nSingleCount <= 4)//未能构成顺子牌型
				continue;
			
			nHandCount += 1;
			
			while (nSingleCount)
			{
				nCardLay[i+nSingleCount-1] -= 1;
				nSingleCount--;
			}
		}
	}
}

//统计对子牌型信息
inline void CalcCoupleHandCount(int nCardLay[], int &nHandCount, int &nBombCount)
{
	for (int i = 0; i < SK_LAYOUT_NUM; i++)
	{
		if (2 == nCardLay[i])
		{
			nHandCount += 1;
			nCardLay[i] = 0;
		}
	}
}

//统计单张牌型信息
inline void CalcSingleHandCount(int nCardLay[], int &nHandCount, int &nBombCount)
{
	for (int i = 0; i < SK_LAYOUT_NUM; i++)
	{
		if (1 == nCardLay[i])
		{
			nHandCount += 1;
			nCardLay[i] = 0;
		}
	}
}

//统计出牌手数(已知CardLay)
inline void CalcHandCardsCount(int nCardsID[], int nCardsCount, int nCardLay[], int &nHandCount, int &nBombCount, int &nBigCardCount)
{
	for(int i=0;i<nCardsCount;i++)
	{
		if (nCardsID[i]>=0&&nCardsID[i]<TOTAL_CARDS)
		{
			int index=SK_GetCardIndexEx(nCardsID[i], 0);
			nCardLay[index]++;
		}
	}

	int nCardLayTemp[SK_LAYOUT_NUM];
	memcpy(nCardLayTemp, nCardLay, sizeof(int)*SK_LAYOUT_NUM);

	nBigCardCount = nCardLay[1] + nCardLay[14] + nCardLay[15];

	//统计相关牌型信息，不能调换顺序或者合并
	Calc2KingHandCount(nCardLayTemp, nHandCount, nBombCount);
	CalcBombHandCount(nCardLayTemp, nHandCount, nBombCount);
	CalcABTThreeHandCount(nCardLayTemp, nHandCount, nBombCount);
	CalcABTCoupleHandCount(nCardLayTemp, nHandCount, nBombCount);
	CalcABTHandCount(nCardLayTemp, nHandCount, nBombCount);
	CalcThreeHandCount(nCardLayTemp, nHandCount, nBombCount);
	CalcCoupleHandCount(nCardLayTemp, nHandCount, nBombCount);
	CalcSingleHandCount(nCardLayTemp, nHandCount, nBombCount);
}

//统计出牌手数(未知CardLay)
inline void CalcHandCardsCount(int nCardLay[], int &nHandCount, int &nBombCount, int &nBigCardCount)
{
	int nCardLayTemp[SK_LAYOUT_NUM];
	memcpy(nCardLayTemp, nCardLay, sizeof(int)*SK_LAYOUT_NUM);
	
	nBigCardCount = nCardLay[1] + nCardLay[14] + nCardLay[15];
	
	//统计相关牌型信息，不能调换顺序或者合并
	Calc2KingHandCount(nCardLayTemp, nHandCount, nBombCount);
	CalcBombHandCount(nCardLayTemp, nHandCount, nBombCount);
	CalcABTThreeHandCount(nCardLayTemp, nHandCount, nBombCount);
	CalcABTCoupleHandCount(nCardLayTemp, nHandCount, nBombCount);
	CalcABTHandCount(nCardLayTemp, nHandCount, nBombCount);
	CalcThreeHandCount(nCardLayTemp, nHandCount, nBombCount);
	CalcCoupleHandCount(nCardLayTemp, nHandCount, nBombCount);
	CalcSingleHandCount(nCardLayTemp, nHandCount, nBombCount);
}

//匹配顺子
inline int MatchABTCardType(int nCardLay[], int nReserveCards[], int nReserveCount)
{
	{//寻找5缺1的单牌顺子
		for (int i = 0; i < SK_LAYOUT_NUM; i++)
		{
			if (i <= 1 || i >= 10)//从3到10
				continue;

			int nMatchedCardIndex = 0;
			int nIndexDiff = 0;
			int nDiffCardCount = 0;
			int nDoubleTypeCardIndex1 = 0;
			int nDoubleTypeCardIndex2 = 0;

			{//匹配连续5种牌值
				for (int j = 0; j < 5; j++)
				{
					if (1 == nCardLay[i+j] || 2 == nCardLay[i+j] || 3 == nCardLay[i+j])
					{
						nIndexDiff ++;
						nDiffCardCount += nCardLay[i+j];

						if (2 == nCardLay[i+j] && 0 == nDoubleTypeCardIndex1)//记录第一个对子
							nDoubleTypeCardIndex1 = i+j;
						else if (2 == nCardLay[i+j] && 0 == nDoubleTypeCardIndex2)//记录第二个对子
							nDoubleTypeCardIndex2 = i+j;
					}
					else if (0 == nCardLay[i+j] && 0 == nMatchedCardIndex)
						nMatchedCardIndex = i+j; 
				}
			}

			if (FALSE == (4 == nIndexDiff && 0 != nMatchedCardIndex && (nDiffCardCount == 4 || nDiffCardCount == 5 || nDiffCardCount == 6)))
				continue;//只匹配4张单牌、3张单+对子、2张单+2个对、3张单+1个三张

			if (nDiffCardCount == 6 && nDoubleTypeCardIndex1 != 0 && nDoubleTypeCardIndex2 != 0 && nDoubleTypeCardIndex2 - nDoubleTypeCardIndex1 <= 2)
				continue;//除去2张单+2个对的情况下，对子差值<=2

			{
				for (int j = 0; j < nReserveCount; j++)
				{
					if (nMatchedCardIndex == SK_GetCardIndexEx(nReserveCards[j], 0))
						return nReserveCards[j];//返回找到的CardID
				}
			}
		}
	}

	return -1;//找不到
}

//匹配连对
inline int MatchABTCoupleCardType(int nCardLay[], int nReserveCards[], int nReserveCount)
{
	int nMatchedCardIndex = 0;
	int nIndexDiff = 0;
	
	{//寻找3缺1的连对
		for (int i = 0; i < SK_LAYOUT_NUM; i++)
		{
			if (i <= 1 || i >= 12)//从3到Q
				continue;
			
			nMatchedCardIndex = 0;
			nIndexDiff = 0;
			
			if (2 == nCardLay[i])//第1对
				nIndexDiff ++;
			else if (1 == nCardLay[i] && 0 == nMatchedCardIndex)
				nMatchedCardIndex = i; 
			
			if (2 == nCardLay[i+1])//第2对
				nIndexDiff ++;
			else if (1 == nCardLay[i+1] && 0 == nMatchedCardIndex)
				nMatchedCardIndex = i+1; 
			
			if (2 == nCardLay[i+2])//第3对
				nIndexDiff ++;
			else if (1 == nCardLay[i+2] && 0 == nMatchedCardIndex)
				nMatchedCardIndex = i+2; 
			
			if (2 == nIndexDiff && 0 != nMatchedCardIndex)
			{
				for (int j = 0; j < nReserveCount; j++)
				{
					if (nMatchedCardIndex == SK_GetCardIndexEx(nReserveCards[j], 0))
						return nReserveCards[j];//返回找到的CardID
				}
			}
		}
	}
	
	return -1;//找不到
}

//匹配对子
inline int MatchCoupleCardType(int nCardLay[], int nReserveCards[], int nReserveCount)
{
	int nMatchedCardIndex = 0;
	
	{//寻找单牌
		for (int i = 0; i < SK_LAYOUT_NUM; i++)
		{
			if (i <= 0 || i >= 14)//从2到A
				continue;
			
			nMatchedCardIndex = 0;
			
			if (1 == nCardLay[i])//单牌
			{
				nMatchedCardIndex = i;

				for (int j = 0; j < nReserveCount; j++)
				{
					if (nMatchedCardIndex == SK_GetCardIndexEx(nReserveCards[j], 0))
						return nReserveCards[j];//返回找到的CardID
				}
			}
		}
	}
	
	return -1;//找不到
}

//匹配三张
inline int MatchThreeCardType(int nCardLay[], int nReserveCards[], int nReserveCount)
{
	int nMatchedCardIndex = 0;
	
	{//寻找对子
		for (int i = 0; i < SK_LAYOUT_NUM; i++)
		{
			if (i <= 0 || i >= 14)//从2到A
				continue;
			
			nMatchedCardIndex = 0;
			
			if (2 == nCardLay[i])//对子
			{
				nMatchedCardIndex = i;
				
				for (int j = 0; j < nReserveCount; j++)
				{
					if (nMatchedCardIndex == SK_GetCardIndexEx(nReserveCards[j], 0))
						return nReserveCards[j];//返回找到的CardID
				}
			}
		}
	}
	
	return -1;//找不到
}

//匹配2和大小王
inline int Match2OrKingCardType(int nCardLay[], int nReserveCards[], int nReserveCount, LPMAKEDEALCFG pCfg)
{
	if (nCardLay[1] + nCardLay[14] + nCardLay[15] >= pCfg->nReserved[0])//2和王的数量不超过配置个数的时候补牌
		return -1;

	int nMatchedCardIndex = 0;
	
	{//寻找2或大小王
		for (int j = 0; j < nReserveCount; j++)
		{
			if (14==SK_GetCardIndexEx(nReserveCards[j], 0)
				|| 15==SK_GetCardIndexEx(nReserveCards[j], 0)
				|| 1==SK_GetCardIndexEx(nReserveCards[j], 0))
			{
				return nReserveCards[j];//返回找到的CardID
			}
		}
	}
	
	return -1;//找不到
}

//匹配炸弹
inline int MatchBombCardType(int nCardLay[], int nReserveCards[], int nReserveCount)
{
	int nMatchedCardIndex = 0;
	
	{//寻找三张
		for (int i = 0; i < SK_LAYOUT_NUM; i++)
		{
			if (i <= 0 || i >= 14)//从2到A
				continue;
			
			nMatchedCardIndex = 0;
			
			if (3 == nCardLay[i])//三张
			{
				nMatchedCardIndex = i;
				
				for (int j = 0; j < nReserveCount; j++)
				{
					if (nMatchedCardIndex == SK_GetCardIndexEx(nReserveCards[j], 0))
						return nReserveCards[j];//返回找到的CardID
				}
			}
		}
	}
	
	return -1;//找不到
}

//从数组中寻找ID不为-1的牌
inline int GetOneReservedCard(int array[], int length)
{
	int nTempID = -1;
	for (int i = 0; i < length; i++)
	{
		if (array[i] != -1)
		{
			nTempID = array[i];
			array[i] = -1;
			break;
		}
	}

	return nTempID;
}

int My_GetRandomBetweenEx(int nMin, int nMax);


// 定义一个接受不定参数的函数模板
template<typename... Args>
void RecordAction(std::stringstream& m_sinRecord, const std::string& action, BOOL bAuto, Args... args) {
	std::string finalAction = bAuto ? ("Auto" + action) : action;
	m_sinRecord << finalAction << " ";
	// 递归模板展开来输出所有参数
	(void)std::initializer_list<int>{(
		m_sinRecord << args << " ",
		0 
		)...};

	m_sinRecord << GetCurTimeStampSec() << std::endl;
}