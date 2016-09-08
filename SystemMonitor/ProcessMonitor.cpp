#include "stdafx.h"
#include "ProcessMonitor.h"

#include <winioctl.h>
#include "NtStructDef.h"


CProcessMonitor::CProcessMonitor(CMonitorListCtrl* MonitorListCtrl)
	: m_MonitorListCtrlObj(MonitorListCtrl)
	, m_bIsRun(TRUE)
	, m_hDevice(nullptr)
{

}

CProcessMonitor::~CProcessMonitor()
{
}

BOOL CProcessMonitor::OpenDevice()
{
	BOOL bRet = FALSE;

	m_hDevice = CreateFile(
		PROCESS_MONITOR_SYMBOLIC,
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
	ULONG		uBufferSize = 0;
	ULONG		ulResult = 0;

	WCHAR		wzCreateProcessTime[MAX_TIME_LENGTH] = { 0 };
	WCHAR		wzParentProcessId[MAX_PID_LENGTH] = { 0 };
	WCHAR		wzParentProcessPath[MAX_PATH] = { 0 };
	WCHAR		wzProcessId[MAX_PID_LENGTH] = { 0 };
	WCHAR		wzProcessPath[MAX_PATH] = { 0 };
	WCHAR		wzCommandLine[MAX_STRING_LENGTH] = { 0 };

	while (pProcessMonitorObj->m_bIsRun)
	{
		bRet = DeviceIoControl(pProcessMonitorObj->m_hDevice, CWK_DVC_RECV_STR, NULL, 0, NULL, 0, &ulResult, 0);
		if (bRet && ulResult != 0)
		{
			PPROCESSINFO pstProcessEvent = (PPROCESSINFO)HeapAlloc(GetProcessHeap(), 0, ulResult);
			bRet = DeviceIoControl(pProcessMonitorObj->m_hDevice, CWK_DVC_RECV_STR, NULL, 0, pstProcessEvent, ulResult, &ulResult, 0);
			if (bRet)
			{
				_stprintf_s(wzParentProcessId, MAX_PATH, _T("%ld"), (LONG)(LONG_PTR)pstProcessEvent->hParentProcessId);
				_stprintf_s(wzProcessId, MAX_PATH, _T("%ld"), (LONG)(LONG_PTR)pstProcessEvent->hProcessId);

				// 时间处理
				_stprintf_s(wzCreateProcessTime, MAX_PATH, _T("%04d-%02d-%02d %02d:%02d:%02d"),
					pstProcessEvent->time.wYear,
					pstProcessEvent->time.wMonth,
					pstProcessEvent->time.wDay,
					pstProcessEvent->time.wHour,
					pstProcessEvent->time.wMinute,
					pstProcessEvent->time.wSecond);

				ZeroMemory(wzParentProcessPath, MAX_PATH);
				ZeroMemory(wzProcessPath, MAX_PATH);
				ZeroMemory(wzCommandLine, MAX_STRING_LENGTH);

				CopyMemory(wzParentProcessPath, pstProcessEvent->uData, pstProcessEvent->ulParentProcessLength);
				uBufferSize += pstProcessEvent->ulParentProcessLength;
				CopyMemory(wzProcessPath, pstProcessEvent->uData + uBufferSize, pstProcessEvent->ulProcessLength);
				uBufferSize += pstProcessEvent->ulProcessLength;
				CopyMemory(wzCommandLine, pstProcessEvent->uData + uBufferSize, pstProcessEvent->ulCommandLineLength);

				pProcessMonitorObj->m_MonitorListCtrlObj->InsertProcessMonitorItem(
					wzCreateProcessTime,
					wzParentProcessPath,
					wzProcessPath,
					_T("创建进程"),
					wzCommandLine);

				uBufferSize = 0;
			}
			else
			{
				OutputDebugString(_T("Failed to call DeviceIoControl.\r\n"));
			}
			HeapFree(GetProcessHeap(), 0, pstProcessEvent);
		}
	}
}
