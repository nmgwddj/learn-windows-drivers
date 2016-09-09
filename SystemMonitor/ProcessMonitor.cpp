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
	CloseDevice();
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

	WCHAR		wchCreateProcessTime[MAX_TIME_LENGTH] = { 0 };
	WCHAR		wchParentProcessPath[MAX_PATH] = { 0 };
	WCHAR		wchProcessPath[MAX_PATH] = { 0 };
	WCHAR		wchCommandLine[MAX_STRING_LENGTH * 2] = { 0 };

	WCHAR		wchParentProcessInfo[MAX_PID_LENGTH + MAX_PATH] = { 0 };
	WCHAR		wchProcessInfo[MAX_PID_LENGTH + MAX_PATH] = { 0 };

	while (pProcessMonitorObj->m_bIsRun)
	{
		bRet = DeviceIoControl(pProcessMonitorObj->m_hDevice, CWK_DVC_RECV_STR, NULL, 0, NULL, 0, &ulResult, 0);
		if (bRet && ulResult != 0)
		{
			PPROCESSINFO pstProcessEvent = (PPROCESSINFO)HeapAlloc(GetProcessHeap(), 0, ulResult);
			bRet = DeviceIoControl(pProcessMonitorObj->m_hDevice, CWK_DVC_RECV_STR, NULL, 0, pstProcessEvent, ulResult, &ulResult, 0);
			if (bRet)
			{
				// 时间处理
				_stprintf_s(wchCreateProcessTime, MAX_PATH, _T("%04d-%02d-%02d %02d:%02d:%02d"),
					pstProcessEvent->time.wYear,
					pstProcessEvent->time.wMonth,
					pstProcessEvent->time.wDay,
					pstProcessEvent->time.wHour,
					pstProcessEvent->time.wMinute,
					pstProcessEvent->time.wSecond);

				ZeroMemory(wchParentProcessPath, MAX_PATH);
				ZeroMemory(wchProcessPath, MAX_PATH);
				ZeroMemory(wchCommandLine, MAX_STRING_LENGTH);
				ZeroMemory(wchParentProcessInfo, MAX_STRING_LENGTH);
				ZeroMemory(wchProcessInfo, MAX_STRING_LENGTH);

				CopyMemory(wchParentProcessPath, pstProcessEvent->uData, pstProcessEvent->ulParentProcessLength);
				uBufferSize += pstProcessEvent->ulParentProcessLength;
				CopyMemory(wchProcessPath, pstProcessEvent->uData + uBufferSize, pstProcessEvent->ulProcessLength);
				uBufferSize += pstProcessEvent->ulProcessLength;
				CopyMemory(wchCommandLine, pstProcessEvent->uData + uBufferSize, pstProcessEvent->ulCommandLineLength);

				_stprintf_s(wchParentProcessInfo, MAX_PATH + MAX_PID_LENGTH, _T("[%I64d] %ws"),
					(LONG64)pstProcessEvent->hParentProcessId, wchParentProcessPath);
				_stprintf_s(wchProcessInfo, MAX_PATH + MAX_PID_LENGTH, _T("[%I64d] %ws"),
					(LONG64)pstProcessEvent->hProcessId, wchProcessPath);

				pProcessMonitorObj->m_MonitorListCtrlObj->InsertProcessMonitorItem(
					wchCreateProcessTime,
					wchParentProcessInfo,
					wchProcessInfo,
					pstProcessEvent->bIsCreate ? _T("创建进程") : _T("退出进程"),
					wchCommandLine);

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
