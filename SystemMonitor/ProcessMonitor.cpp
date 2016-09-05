#include "stdafx.h"
#include "ProcessMonitor.h"

#include <winioctl.h>
#include "NtStructDef.h"


CProcessMonitor::CProcessMonitor(CMonitorListCtrl* MonitorListCtrl)
	: m_MonitorListCtrlObj(MonitorListCtrl)
{

}

CProcessMonitor::~CProcessMonitor()
{
}

BOOL CProcessMonitor::OpenDevice()
{
	BOOL bRet = FALSE;

	m_hDevice = CreateFile(
		_T("\\??\\_ProcessMonitor"),
		GENERIC_READ | GENERIC_WRITE,
		0,
		0,
		OPEN_EXISTING,
		FILE_ATTRIBUTE_SYSTEM,
		0);
	if (m_hDevice != INVALID_HANDLE_VALUE)
	{
		bRet = TRUE;
	}
	else
	{
		OutputDebugString(_T("Open Device \\??\\_ProcessMonitor Error."));
	}

	return bRet;
}

BOOL CProcessMonitor::CloseDevice()
{
	BOOL bRet = FALSE;

	if (NULL != m_hDevice)
	{
		CloseHandle(m_hDevice);
		m_hDevice = NULL;
		bRet = TRUE;
	}

	return bRet;
}

BOOL CProcessMonitor::Run()
{
	BOOL bRet = FALSE;

	bRet = OpenDevice();
	if (bRet)
	{
		m_bIsRun = TRUE;
		m_Thread = thread(ProcessMonitorThread, this);
	}

	return bRet;
}

BOOL CProcessMonitor::Stop()
{
	m_bIsRun = FALSE;
	m_Thread.join();
	return TRUE;
}

void CProcessMonitor::ProcessMonitorThread(CProcessMonitor* pProcessMonitorObj)
{
	BOOL		bRet = FALSE;
	ULONG		ulResult = 0;
	PROCESSINFO stProcessInfo;

	WCHAR		wzParentId[MAX_PATH];
	WCHAR		wzProcessId[MAX_PATH];

	while (pProcessMonitorObj->m_bIsRun)
	{
		ZeroMemory(&stProcessInfo, sizeof(PROCESSINFO));
		bRet = DeviceIoControl(pProcessMonitorObj->m_hDevice, CWK_DVC_RECV_STR, NULL, 0, &stProcessInfo, sizeof(stProcessInfo), &ulResult, 0);
		if (bRet)
		{
			_stprintf_s(wzParentId, MAX_PATH, _T("%ld"), (LONG)(LONG_PTR)stProcessInfo.hParentId);
			_stprintf_s(wzProcessId, MAX_PATH, _T("%ld"), (LONG)(LONG_PTR)stProcessInfo.hProcessId);

			pProcessMonitorObj->m_MonitorListCtrlObj->InsertProcessMonitorItem(
				_T("2016-06-06 08:48:32"),
				wzParentId,
				stProcessInfo.wsProcessPath,
				_T("创建进程"),
				stProcessInfo.wsProcessCommandLine);
		}
	}
}
