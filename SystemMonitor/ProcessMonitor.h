#pragma once
#include <thread>
#include "MonitorListCtrl.h"
#include "MonitorListCtrl.h"

using namespace std;

class CProcessMonitor
{
public:
	CProcessMonitor(CMonitorListCtrl* MonitorListCtrl);
	~CProcessMonitor();

	BOOL	OpenDevice();
	BOOL	CloseDevice();

	BOOL	Run();
	BOOL	Stop();

protected:
	static void ProcessMonitorThread(CProcessMonitor* pProcessMonitorObj);

private:
	BOOL						m_bIsRun;
	HANDLE						m_hDevice;
	thread						m_Thread;
	CMonitorListCtrl*			m_MonitorListCtrlObj;
};

