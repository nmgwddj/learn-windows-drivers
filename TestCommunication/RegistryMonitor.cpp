#include "stdafx.h"
#include "RegistryMonitor.h"

struct UNICODE_STRING
{
	WORD Length;
	WORD MaximumLength;
	PWSTR Buffer;
};

typedef struct __PUBLIC_OBJECT_TYPE_INFORMATION {
	UNICODE_STRING TypeName;
	ULONG Reserved[22];    // reserved for internal use
} PUBLIC_OBJECT_TYPE_INFORMATION, *PPUBLIC_OBJECT_TYPE_INFORMATION;

CRegistryMonitor::CRegistryMonitor()
	: m_hDevice(NULL)
	, m_bIsRun(FALSE)
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
	HKEY	hTestKey;
	DWORD	dwError;

	dwError = RegOpenCurrentUser(KEY_READ, &hTestKey);
	if (dwError == ERROR_SUCCESS)
	{
		NTSTATUS	status;
		DWORD		dwRequiredLength;
		PPUBLIC_OBJECT_TYPE_INFORMATION t;

		typedef DWORD(WINAPI *pNtQueryObject)(HANDLE, DWORD, VOID*, DWORD, VOID*);
		pNtQueryObject NtQueryObject = (pNtQueryObject)GetProcAddress(GetModuleHandle(L"ntdll.dll"), (LPCSTR)"NtQueryObject");

		status = NtQueryObject(hTestKey, 1, NULL, 0, &dwRequiredLength);

		if (status == STATUS_INFO_LENGTH_MISMATCH)
		{
			t = (PPUBLIC_OBJECT_TYPE_INFORMATION)VirtualAlloc(NULL, dwRequiredLength, MEM_COMMIT, PAGE_READWRITE);
			if (status != NtQueryObject(hTestKey, 1, t, dwRequiredLength, &dwRequiredLength))
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
	// 打开驱动设备
	m_hDevice = CreateFile(
		_T("\\??\\_RegistryMonitor"), 
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
		m_objNameMap.insert(pair<wstring, wstring>(strCurrentUserKey, L"HKEY_CURRENT_USER"));
		m_objNameMap.insert(pair<wstring, wstring>(L"\\REGISTRY\\MACHINE", L"HKEY_LOCAL_MACHINE"));
		m_objNameMap.insert(pair<wstring, wstring>(L"\\Registry\\Machine", L"HKEY_LOCAL_MACHINE"));
	}
}

std::wstring CRegistryMonitor::ConvertRegObjectNameToCurrentUserName(wstring& wstrRegObjectName)
{
	map<wstring, wstring>::iterator iter = m_objNameMap.begin();

	for (; iter != m_objNameMap.end(); iter++)
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
	WCHAR	wzRegistryEventClass[MAX_PATH] = { 0 };
	WCHAR	wzRegistryKeyValueType[MAX_PATH] = { 0 };

	while (pRegistryMonitorObj->m_bIsRun)
	{
		bRet = DeviceIoControl(pRegistryMonitorObj->m_hDevice, CWK_DVC_RECV_STR, NULL, 0, NULL, 0, &ulResult, 0);
		if (bRet && ulResult != 0)
		{
			PREGISTRY_EVENT pstRegistryEvent = (PREGISTRY_EVENT)HeapAlloc(GetProcessHeap(), 0, ulResult);
			bRet = DeviceIoControl(pRegistryMonitorObj->m_hDevice, CWK_DVC_RECV_STR, NULL, 0, pstRegistryEvent, ulResult, &ulResult, 0);
			if (bRet)
			{
				switch (pstRegistryEvent->enRegistryNotifyClass)
				{
				case RegNtPreCreateKeyEx:
					wcsncpy_s(wzRegistryEventClass, L"RegNtPreCreateKeyEx", MAX_PATH);
					break;
				case RegNtPreDeleteKey:
					wcsncpy_s(wzRegistryEventClass, L"RegNtPreDeleteKey", MAX_PATH);
					break;
				case RegNtPreSetValueKey:
					wcsncpy_s(wzRegistryEventClass, L"RegNtPreSetValueKey", MAX_PATH);
					break;
				case RegNtDeleteValueKey:
					wcsncpy_s(wzRegistryEventClass, L"RegNtDeleteValueKey", MAX_PATH);
					break;
				}

				switch (pstRegistryEvent->ulKeyValueType)
				{
				case REG_NONE:
					wcsncpy_s(wzRegistryKeyValueType, L"REG_NONE", MAX_PATH);
					break;
				case REG_SZ:
					wcsncpy_s(wzRegistryKeyValueType, L"REG_SZ", MAX_PATH);
					break;
				case REG_DWORD:
					wcsncpy_s(wzRegistryKeyValueType, L"REG_DWORD", MAX_PATH);
					break;
				default:
					break;
				}

				pwzProcessPath = (WCHAR *)HeapAlloc(GetProcessHeap(), 0, pstRegistryEvent->ulProcessPathLength);
				pwzRegistryPath = (WCHAR *)HeapAlloc(GetProcessHeap(), 0, pstRegistryEvent->ulRegistryPathLength);
				pwzRegistryData = (WCHAR *)HeapAlloc(GetProcessHeap(), 0, pstRegistryEvent->ulDataLength == 0 ?  sizeof(WCHAR) : pstRegistryEvent->ulDataLength);

				ZeroMemory(pwzRegistryData, pstRegistryEvent->ulDataLength == 0 ? sizeof(WCHAR) : pstRegistryEvent->ulDataLength);

				CopyMemory(pwzProcessPath, pstRegistryEvent->uData, pstRegistryEvent->ulProcessPathLength);
				CopyMemory(pwzRegistryPath, pstRegistryEvent->uData + pstRegistryEvent->ulProcessPathLength, pstRegistryEvent->ulRegistryPathLength);
				CopyMemory(pwzRegistryData, pstRegistryEvent->uData + pstRegistryEvent->ulProcessPathLength + pstRegistryEvent->ulRegistryPathLength,
					pstRegistryEvent->ulDataLength);

				wstring wstrRegistryPath = pwzRegistryPath;
				wstrRegistryPath = pRegistryMonitorObj->ConvertRegObjectNameToCurrentUserName(wstrRegistryPath);

				wcout << "--------------------" << endl
					<< "ProcessId = " << (LONG)(LONG_PTR)pstRegistryEvent->hProcessId << endl
					<< "ProcessPath = " << pwzProcessPath << endl
					<< "RegistryPath = " << wstrRegistryPath.c_str() << endl
					<< "EventClass = " << wzRegistryEventClass << endl
					<< "RegistryData = " << pwzRegistryData << endl
					<< "RegValueKeyType = " << wzRegistryKeyValueType << endl
					<< "--------------------" << endl;

				HeapFree(GetProcessHeap(), 0, pwzProcessPath);
				HeapFree(GetProcessHeap(), 0, pwzRegistryPath);
				HeapFree(GetProcessHeap(), 0, pwzRegistryData);

			}
			else
			{
				printf("Failed to call DeviceIoControl, GetLastError = %ld, ulResult = %ld\r\n", GetLastError(), ulResult);
			}
			HeapFree(GetProcessHeap(), 0, pstRegistryEvent);
		}
	}
}
