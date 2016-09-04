#pragma once
#include <map>
#include <string>
#include <thread>
#include <iostream>
#include "MonitorListCtrl.h"

using namespace std;

class CRegistryMonitor
{
public:
	CRegistryMonitor(CMonitorListCtrl* MonitorListCtrlObj);
	~CRegistryMonitor();

	BOOL	GetCurrentUserKey(LPTSTR lpCurrentUserKey);
	BOOL	OpenDevice();
	BOOL	CloseDevice();
	void	InitialiseObjectNameMap();
	wstring	ConvertRegObjectNameToCurrentUserName(wstring& wstrRegObjectName);

	BOOL	Run();
	BOOL	Stop();

protected:
	static void RegistryMonitorThread(CRegistryMonitor* pRegistryMonitorObj);

private:
	BOOL						m_bIsRun;
	HANDLE						m_hDevice;
	map<wstring, wstring>		m_mapNameMapObj;
	thread						m_Thread;
	CMonitorListCtrl*			m_MonitorListCtrlObj;
};

