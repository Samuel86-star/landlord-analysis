#pragma once
#include "CalcTime.h"
#include "zeromq/zmqwrap.hpp"
#include "threadsafe_unordered_map.h"

#define WXTASK_CONFIG			   _T("wxtask.json")
#define WXDAILYPK_CONFIG           _T("wxdailypk.json")
#define MAMMONTASK_CONFIG          _T("MammonConfig.json")
#define WXREALTIMEPK_CONFIG        _T("wxRealTimeMatch.json")
#define MAKEDEAL_CONFIG            _T("makedeal.json")
#define NOVICE_TASKINFO_CONFIG		_T("NoviceTaskConfig.json")
#define WXINVITEPRIZE_CONFIG       _T("wxInvitePrize.json")

class CWorkerContext;
class CConfigManagerSys
{
public:
	static CConfigManagerSys* GetInstance()
	{
		static CConfigManagerSys instance;
		return &instance;
	}

	CConfigManagerSys(){}
	~CConfigManagerSys(){
		CloseHandle(m_hSubThread);
		CloseHandle(m_hMonitorSubThread);
	}

	BOOL OnRequest(LPCONTEXT_HEAD lpContext, LPREQUEST lpRequest, CWorkerContext* pThreadCxt);
	DWORD static SubThreadProc(LPVOID p);
	DWORD static MonitorSubThreadProc(LPVOID p);
	BOOL static ProcessData(std::string topic, std::string msg);
	BOOL static MonitorData(int event, std::string msg);

	static BOOL ParseJsonConfig(IN std::string & sValue, OUT Json::Value& jsonobj);
	BOOL Initialize(TCHAR szIniFile[]);
	bool SendDingDing(std::string msg);
	void ReadConfig();
	BOOL ResetConnect();
	void TimeFun(int nDate, int nHour, int nMin);
	BOOL GetConfigByName(IN std::string jsoncfgname, OUT Json::Value& jsoncfgobj);

	static std::vector<std::string> m_filters;
	static ddz::threadsafe_unordered_map<std::string, Json::Value> m_jsoncfgobjmgr;
protected:
	static ZeromqWrap zmqApp;                        //用来订阅配置管理zgdbassitsvr上的json配置文件
	HANDLE m_hSubThread;
	HANDLE m_hMonitorSubThread;
	unsigned int m_nLastKeepAlive;  //保存上一次激活的时间戳
	std::string m_strZgdbAssistSvrIP;
	int m_nZgdbAssistSvrPubSubPort;
	int m_nZgdbAssistSvrReqRepPort;
	CString m_szIniFile;
};