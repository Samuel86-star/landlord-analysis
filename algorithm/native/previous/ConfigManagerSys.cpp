#include "stdafx.h"
#include "ConfigManagerSys.h"
#include "HttpClient.h"
#include "CalcTime.h"
#include "Algorithm.h"

BOOL CConfigManagerSys::OnRequest(LPCONTEXT_HEAD lpContext, LPREQUEST lpRequest, CWorkerContext* pThreadCxt)
{
	switch (lpRequest->head.nRequest)
	{
		case 0:
			break;
		default:
			break;
	}
	return TRUE;
}

ZeromqWrap CConfigManagerSys::zmqApp;
ddz::threadsafe_unordered_map<std::string, Json::Value> CConfigManagerSys::m_jsoncfgobjmgr;
std::vector<std::string> CConfigManagerSys::m_filters;

DWORD CConfigManagerSys::SubThreadProc(LPVOID p)
{
	zmqApp.zmqPubSubRecieveMessage((ZeromqWrap::getDataFuncptr)&CConfigManagerSys::ProcessData);
	return 0;
}

DWORD CConfigManagerSys::MonitorSubThreadProc(LPVOID p)
{
	zmqApp.zmqMonitorPubSubSocket((ZeromqWrap::getMonitorDataFuncptr)&CConfigManagerSys::MonitorData);
	return 0;
}

BOOL CConfigManagerSys::ProcessData(std::string topic, std::string msg) {
	//接受到最新配置发布，更新本地内存中jsoncfgobj
	Json::Value value;
	if (ParseJsonConfig(msg, value))
	{
		m_jsoncfgobjmgr((LPCSTR)topic.c_str()) = value;
		UwlLogFile("CConfigManager %s Reload Parse Succesed! msg:%s", (LPCSTR)topic.c_str(), msg.c_str());
	}
	else
	{
		UwlLogFile("CConfigManager %s Reload Parse Failed! msg:%s", (LPCSTR)topic.c_str(), msg.c_str());
	}
	return true;
}

BOOL CConfigManagerSys::MonitorData(int event, std::string strMonitorMsg)
{
	UwlLogFile("CConfigManager ZMQ Monitor %d %s",event, strMonitorMsg.c_str());
	return TRUE;
}

BOOL CConfigManagerSys::ParseJsonConfig(IN std::string & sValue, OUT Json::Value& jsonobj)
{
	Json::Reader reader(Json::Features::strictMode());
	if (reader.parse(sValue, jsonobj, false))
	{
		return TRUE;
	}
	else
	{
		return FALSE;
	}
}

BOOL CConfigManagerSys::Initialize(TCHAR szIniFile[])
{
	m_szIniFile = szIniFile;
	ReadConfig();

	char szTempPubSubCon[48] = { 0 };
	sprintf_s(szTempPubSubCon, "tcp://%s:%d", m_strZgdbAssistSvrIP.c_str(), m_nZgdbAssistSvrPubSubPort);
	std::string conPubSubString = szTempPubSubCon;
	char szTempReqRepCon[48] = { 0 };
	sprintf_s(szTempReqRepCon, "tcp://%s:%d", m_strZgdbAssistSvrIP.c_str(), m_nZgdbAssistSvrReqRepPort);
	std::string conReqRepString = szTempReqRepCon;
	zmqApp.init(conPubSubString, ZMQ_SUB, conReqRepString, ZMQ_REQ);

	//指定本服务所需订阅的所有json配置文件
	m_filters = {"keeplive",SCORE2SLIVER_CONFIG, WXTASK_CONFIG, WXDAILYPK_CONFIG, MAMMONTASK_CONFIG, WXREALTIMEPK_CONFIG, MAKEDEAL_CONFIG, NOVICE_TASKINFO_CONFIG,WXINVITEPRIZE_CONFIG };
	for (std::vector<std::string>::iterator it = m_filters.begin(); it != m_filters.end(); it++)
	{
		zmqApp.zmqPubSubSetSockopt(ZMQ_SUBSCRIBE, it->c_str(), strlen(it->c_str())); //接收消息的前缀为filter1的消息
	}
	int  nReconnectInterval = 100; //ms
	zmqApp.zmqPubSubSetSockopt(ZMQ_RECONNECT_IVL, &nReconnectInterval, sizeof(nReconnectInterval));
	zmqApp.zmqReqRepSetSockopt(ZMQ_RECONNECT_IVL, &nReconnectInterval, sizeof(nReconnectInterval));
	int nHeartBeatInterval = 1000;
	zmqApp.zmqPubSubSetSockopt(ZMQ_HEARTBEAT_IVL, &nHeartBeatInterval, sizeof(nHeartBeatInterval));
	zmqApp.zmqReqRepSetSockopt(ZMQ_HEARTBEAT_IVL, &nHeartBeatInterval, sizeof(nHeartBeatInterval));
	int nHeartBeatTimeout = nHeartBeatInterval * 3;
	zmqApp.zmqPubSubSetSockopt(ZMQ_HEARTBEAT_TIMEOUT, &nHeartBeatTimeout, sizeof(nHeartBeatTimeout));
	zmqApp.zmqReqRepSetSockopt(ZMQ_HEARTBEAT_TIMEOUT, &nHeartBeatTimeout, sizeof(nHeartBeatTimeout));
	int nIsEnable = 1;
	zmqApp.zmqReqRepSetSockopt(ZMQ_REQ_CORRELATE, &nIsEnable, sizeof(nIsEnable));
	zmqApp.zmqReqRepSetSockopt(ZMQ_REQ_RELAXED, &nIsEnable, sizeof(nIsEnable));
	int nRcvTimeOut = 500;
	zmqApp.zmqReqRepSetSockopt(ZMQ_RCVTIMEO, &nRcvTimeOut, sizeof(nRcvTimeOut));

	zmqApp.zmqConnect();

	if (zmqApp.zmqPubSubConnIsInvalid())
	{
		UwlLogFile("init zeromq pubsub fail %s", conPubSubString.c_str());
		return FALSE;
	}

	if (zmqApp.zmqReqRepConnIsInvalid())
	{
		UwlLogFile("init zeromq reqrep fail %s", conReqRepString.c_str());
		return FALSE;
	}
		
	//开启订阅处理数据的线程
	m_hSubThread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)&CConfigManagerSys::SubThreadProc, NULL, 0, 0);

	//开启监控订阅线程
	m_hMonitorSubThread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)&CConfigManagerSys::MonitorSubThreadProc, NULL, 0, 0);

	//向发布者发送同步请求
	Json::Value oConfigJsonObjs;
//#if !(defined(_RS125) || defined(_DEBUG))
	BOOL bCConfigManagerSync = TRUE;
	bCConfigManagerSync = GetPrivateProfileInt(
		_T("CConfigManagerSync"),//是否是赖子玩法
		_T("Enable"),
		1,
		m_szIniFile);
	
	if (!bCConfigManagerSync){
		return TRUE;
	}
	do
	{
		UwlTrace("syncconfig...");
		UwlLogFile("syncconfig...");
		zmqApp.zmqReqRepSend("sync");
		std::string szConfigJson = zmqApp.zmqReqRepRecv();
		if (szConfigJson != "" && ParseJsonConfig(szConfigJson, oConfigJsonObjs))
		{
			oConfigJsonObjs.getMemberNames();
			for (size_t i = 0; i < m_filters.size(); i++)
			{
				if (!oConfigJsonObjs[m_filters[i]].isNull()) //是配置，加入mgr
				{
					Json::Value value;
					std::string strItem = oConfigJsonObjs[m_filters[i]].asCString();
					if (ParseJsonConfig(strItem, value))
					{
						m_jsoncfgobjmgr(m_filters[i]) = value;
					}
				}
			}
			LOG_INFO("CConfigManager Sync Parse Succesed! msg:%s", szConfigJson.c_str());
		}
		else
		{
			LOG_INFO("CConfigManager Sync Parse Failed! msg:%s", szConfigJson.c_str());
		}
	} while (m_jsoncfgobjmgr.size() == 0);
//#endif
	return TRUE;
}

bool CConfigManagerSys::SendDingDing(std::string msg)
{
	zmqApp.zmqReqRepSendMessage("dingding", msg);
	std::string szRetMsg = zmqApp.zmqReqRepRecv();
	if (szRetMsg == "dingdingok")
	{
		return true;
	}
	else{
		return false;
	}
}

void CConfigManagerSys::ReadConfig()
{
	CString strIP;
	GetPrivateProfileString(_T("ZgdbAssitsvr"), _T("IP"), "127.0.0.1", strIP.GetBuffer(255), 255, m_szIniFile);
	strIP.ReleaseBuffer();
	m_strZgdbAssistSvrIP = (LPCTSTR)strIP;
	m_nZgdbAssistSvrPubSubPort = GetPrivateProfileInt(_T("ZgdbAssitsvr"), _T("PubSubPort"), 60005, m_szIniFile);
	m_nZgdbAssistSvrReqRepPort = GetPrivateProfileInt(_T("ZgdbAssitsvr"), _T("ReqRepPort"), 60000, m_szIniFile);
}

BOOL CConfigManagerSys::ResetConnect()
{
	SuspendThread(m_hSubThread);
	SuspendThread(m_hMonitorSubThread);
	zmqApp.zmqDisConnect();
	zmqApp.zmqConnect();
	ResumeThread(m_hMonitorSubThread);
	ResumeThread(m_hSubThread);
	return TRUE;
}

void CConfigManagerSys::TimeFun(int nDate, int nHour, int nMin)
{
	if (nMin != m_nLastKeepAlive){
		time_t t = time(NULL);
		unsigned int nTimestamp = time(&t);
		if (zmqApp.zmqGetLastKeepliveTimeStamp() != 0 && nTimestamp - zmqApp.zmqGetLastKeepliveTimeStamp() > zmqApp.zmqGetKeepAliveInterval() * 3)
		{
			CString szLogMsg = "";
			szLogMsg.Format("KeepAlive Timeout %d %d", nTimestamp, zmqApp.zmqGetLastKeepliveTimeStamp());
			zmqApp.zmqDebugMessage(szLogMsg);
			this->ResetConnect();
		}
		m_nLastKeepAlive = nMin;
	}
	
}

BOOL CConfigManagerSys::GetConfigByName(IN std::string jsoncfgname, OUT Json::Value& jsoncfgobj)
{
	//不要用返回迭代器(可能会出现线程不安全)，用lookup形式
	auto bFound = m_jsoncfgobjmgr.find(jsoncfgname, jsoncfgobj);
	if (bFound)
	{
		return TRUE;
	}
	else
	{
		return FALSE;
	}
}
