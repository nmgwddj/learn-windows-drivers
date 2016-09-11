#include "stdafx.h"
#include "RegistryMonitor.h"

#include <winioctl.h>
#include "NtStructDef.h"

#ifndef _NTSTATUS_DEFINED
#define _NTSTATUS_DEFINED
typedef _Return_type_success_(return >= 0) long NTSTATUS;
#endif

#define STATUS_INFO_LENGTH_MISMATCH      ((NTSTATUS)0xC0000004L)

struct UNICODE_STRING
{
	WORD Length;
	WORD MaximumLength;
	PWSTR Buffer;
};

typedef struct __PUBLIC_OBJECT_TYPE_INFORMATION
{
	UNICODE_STRING TypeName;
	ULONG Reserved[22];    // reserved for internal use
} PUBLIC_OBJECT_TYPE_INFORMATION, *PPUBLIC_OBJECT_TYPE_INFORMATION;

CRegistryMonitor::CRegistryMonitor(CMonitorListCtrl* MonitorListCtrlObj)
	: m_bIsRun(FALSE)
	, m_hDevice(nullptr)
	, m_MonitorListCtrlObj(MonitorListCtrlObj)
{
	InitialiseObjectNameMap();
}


CRegistryMonitor::~CRegistryMonitor()
{
	CloseDevice();
}

BOOL CRegistryMonitor::GetCurrentUserKey(LPTSTR lpCurrentUserKey)
{
	BOOL	bRet = FALSE;
	HKEY	hCurrentKey;
	DWORD	dwError;

	dwError = RegOpenCurrentUser(KEY_READ, &hCurrentKey);
	if (dwError == ERROR_SUCCESS)
	{
		NTSTATUS	status;
		DWORD		dwRequiredLength;
		PPUBLIC_OBJECT_TYPE_INFORMATION t;

		typedef DWORD(WINAPI *pNtQueryObject)(HANDLE, DWORD, VOID*, DWORD, VOID*);
		pNtQueryObject NtQueryObject = (pNtQueryObject)GetProcAddress(GetModuleHandle(L"ntdll.dll"), (LPCSTR)"NtQueryObject");

		status = NtQueryObject(hCurrentKey, 1, NULL, 0, &dwRequiredLength);

		if (status == STATUS_INFO_LENGTH_MISMATCH)
		{
			t = (PPUBLIC_OBJECT_TYPE_INFORMATION)VirtualAlloc(NULL, dwRequiredLength, MEM_COMMIT, PAGE_READWRITE);
			if (status != NtQueryObject(hCurrentKey, 1, t, dwRequiredLength, &dwRequiredLength))
			{
				CopyMemory(lpCurrentUserKey, t->TypeName.Buffer, dwRequiredLength);
				bRet = TRUE;
			}
			VirtualFree(t, 0, MEM_RELEASE);
		}
	}

	return bRet;
}

BOOL CRegistryMonitor::OpenDevice()
{
	BOOL bRet = FALSE;

	m_hDevice = CreateFile(
		REGISTRY_MONITOR_SYMBOLIC, 
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
		LOG(ERROR) << _T("Failed to open device \\??\\_RegistryMonitor.");
	}

	return bRet;
}

BOOL CRegistryMonitor::CloseDevice()
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

void CRegistryMonitor::InitialiseObjectNameMap()
{
	WCHAR szCurrentUserKey[MAX_PATH] = { 0 };
	if (GetCurrentUserKey(szCurrentUserKey))
	{
		wstring strCurrentUserKey = szCurrentUserKey;
		m_mapNameObj.insert(pair<wstring, wstring>(strCurrentUserKey, L"HKEY_CURRENT_USER"));
		m_mapNameObj.insert(pair<wstring, wstring>(L"\\REGISTRY\\MACHINE", L"HKEY_LOCAL_MACHINE"));
		m_mapNameObj.insert(pair<wstring, wstring>(L"\\Registry\\Machine", L"HKEY_LOCAL_MACHINE"));
	}
}

std::wstring CRegistryMonitor::ConvertRegObjectNameToCurrentUserName(wstring& wstrRegObjectName)
{
	map<wstring, wstring>::iterator iter = m_mapNameObj.begin();

	for (; iter != m_mapNameObj.end(); iter++)
	{
		size_t position = wstrRegObjectName.rfind(iter->first, 0);
		if (position != std::wstring::npos)
		{
			return wstrRegObjectName.replace(position, iter->first.length(), iter->second, 0, iter->second.length());
		}
	}

	return wstrRegObjectName;
}

BOOL CRegistryMonitor::Run()
{
	BOOL bRet = FALSE;

	bRet = OpenDevice();
	if (bRet)
	{
		m_bIsRun = TRUE;
		m_Thread = thread(RegistryMonitorThread, this);
	}
	
	return bRet;
}

BOOL CRegistryMonitor::Stop()
{
	m_bIsRun = FALSE;
	m_Thread.join();
	return TRUE;
}

void CRegistryMonitor::RegistryMonitorThread(CRegistryMonitor* pRegistryMonitorObj)
{
	BOOL	bRet = FALSE;
	ULONG	ulResult = 0;
	WCHAR*	pwzProcessPath;
	WCHAR*	pwzRegistryPath;
	WCHAR*	pwzRegistryData;
	WCHAR*	pwzProcessInfo;
	DWORD	dwDataValue;
	DWORD64	dwDataValue64;
	WCHAR	wzRegistryEventClass[MAX_PATH] = { 0 };
	WCHAR	wzRegistryKeyValueType[MAX_PATH] = { 0 };
	WCHAR	wzRegistryTime[MAX_TIME_LENGTH] = { 0 };

	while (pRegistryMonitorObj->m_bIsRun)
	{
		bRet = DeviceIoControl(pRegistryMonitorObj->m_hDevice, CWK_DVC_RECV_STR, NULL, 0, NULL, 0, &ulResult, 0);
		if (bRet && ulResult != 0)
		{
			PREGISTRY_EVENT pstRegistryEvent = (PREGISTRY_EVENT)HeapAlloc(GetProcessHeap(), 0, ulResult);
			bRet = DeviceIoControl(pRegistryMonitorObj->m_hDevice, CWK_DVC_RECV_STR, NULL, 0, pstRegistryEvent, ulResult, &ulResult, 0);
			if (bRet)
			{
				// 操作类型处理
				switch (pstRegistryEvent->enRegistryNotifyClass)
				{
				case RegNtPreCreateKeyEx:
					wcsncpy_s(wzRegistryEventClass, L"创建项目", MAX_PATH);
					break;
				case RegNtPreDeleteKey:
					wcsncpy_s(wzRegistryEventClass, L"删除项目", MAX_PATH);
					break;
				case RegNtPreSetValueKey:
					wcsncpy_s(wzRegistryEventClass, L"修改键值", MAX_PATH);
					break;
				case RegNtDeleteValueKey:
					wcsncpy_s(wzRegistryEventClass, L"删除键值", MAX_PATH);
					break;
				}

				// 时间处理
				_stprintf_s(wzRegistryTime, MAX_PATH, _T("%04d-%02d-%02d %02d:%02d:%02d"),
					pstRegistryEvent->time.wYear,
					pstRegistryEvent->time.wMonth,
					pstRegistryEvent->time.wDay,
					pstRegistryEvent->time.wHour,
					pstRegistryEvent->time.wMinute,
					pstRegistryEvent->time.wSecond);


				// 进程路径和注册表路径及数据处理
				pwzProcessPath = (WCHAR *)HeapAlloc(GetProcessHeap(), 0, pstRegistryEvent->ulProcessPathLength);
				pwzRegistryPath = (WCHAR *)HeapAlloc(GetProcessHeap(), 0, pstRegistryEvent->ulRegistryPathLength);
				pwzRegistryData = (WCHAR *)HeapAlloc(GetProcessHeap(), 0, pstRegistryEvent->ulDataLength == 0 ? sizeof(WCHAR) : pstRegistryEvent->ulDataLength);
				pwzProcessInfo = (WCHAR *)HeapAlloc(GetProcessHeap(), 0, pstRegistryEvent->ulProcessPathLength + MAX_PID_LENGTH);

				CopyMemory(pwzProcessPath, pstRegistryEvent->uData, pstRegistryEvent->ulProcessPathLength);
				CopyMemory(pwzRegistryPath, pstRegistryEvent->uData + pstRegistryEvent->ulProcessPathLength, pstRegistryEvent->ulRegistryPathLength);
				ZeroMemory(pwzRegistryData, pstRegistryEvent->ulDataLength == 0 ? sizeof(WCHAR) : pstRegistryEvent->ulDataLength);

				_stprintf_s(pwzProcessInfo, pstRegistryEvent->ulProcessPathLength + MAX_PID_LENGTH,
					_T("[%I64d] %ws"), (LONG64)pstRegistryEvent->hProcessId, pwzProcessPath);

				switch (pstRegistryEvent->ulKeyValueType)
				{
				case REG_NONE:
					wcsncpy_s(wzRegistryKeyValueType, L"REG_NONE", MAX_PATH);
					break;
				case REG_SZ:
					wcsncpy_s(wzRegistryKeyValueType, L"REG_SZ", MAX_PATH);
					{
						CopyMemory(pwzRegistryData, pstRegistryEvent->uData + pstRegistryEvent->ulProcessPathLength + pstRegistryEvent->ulRegistryPathLength,
							pstRegistryEvent->ulDataLength);
					}
					break;
				case REG_DWORD:
					wcsncpy_s(wzRegistryKeyValueType, L"REG_DWORD", MAX_PATH);
					{
						pwzRegistryData = (WCHAR *)HeapReAlloc(GetProcessHeap(), 0, pwzRegistryData, MAX_PATH);
						CopyMemory(&dwDataValue, pstRegistryEvent->uData + pstRegistryEvent->ulProcessPathLength + pstRegistryEvent->ulRegistryPathLength,
							pstRegistryEvent->ulDataLength);
						ZeroMemory(pwzRegistryData, MAX_PATH);
						_stprintf_s(pwzRegistryData, MAX_PATH, _T("0x%08X"), dwDataValue);
					}
					break;
				case REG_QWORD:
					wcsncpy_s(wzRegistryKeyValueType, L"REG_DWORD", MAX_PATH);
					{
						pwzRegistryData = (WCHAR *)HeapReAlloc(GetProcessHeap(), 0, pwzRegistryData, MAX_PATH);
						CopyMemory(&dwDataValue64, pstRegistryEvent->uData + pstRegistryEvent->ulProcessPathLength + pstRegistryEvent->ulRegistryPathLength,
							pstRegistryEvent->ulDataLength);
						ZeroMemory(pwzRegistryData, MAX_PATH);
						_stprintf_s(pwzRegistryData, MAX_PATH, _T("0x%I64X"), dwDataValue64);
					}
					break;
				default:
					break;
				}				

				wstring wstrRegistryPath = pwzRegistryPath;
				wstrRegistryPath = pRegistryMonitorObj->ConvertRegObjectNameToCurrentUserName(wstrRegistryPath);

				pRegistryMonitorObj->m_MonitorListCtrlObj->InsertRegistryMonitorItem(
					wzRegistryTime,
					pwzProcessInfo,
					wstrRegistryPath.c_str(),
					wzRegistryEventClass,
					pwzRegistryData);

				HeapFree(GetProcessHeap(), 0, pwzProcessInfo);
				HeapFree(GetProcessHeap(), 0, pwzProcessPath);
				HeapFree(GetProcessHeap(), 0, pwzRegistryPath);
				HeapFree(GetProcessHeap(), 0, pwzRegistryData);
			}
			else
			{
				LOG(ERROR) << _T("Failed to call DeviceIoControl.");
			}
			HeapFree(GetProcessHeap(), 0, pstRegistryEvent);
		}
	}
}
