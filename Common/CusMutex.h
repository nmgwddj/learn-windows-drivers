// ConfigMutex.h: interface for the CConfigMutex class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_CONFIGMUTEX_H__07BFE34F_0B5E_4D73_8020_736CC12DFCD0__INCLUDED_)
#define AFX_CONFIGMUTEX_H__07BFE34F_0B5E_4D73_8020_736CC12DFCD0__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
#include <shlwapi.h>
#pragma comment(lib, "shlwapi.lib")
#include <string>
#include <tchar.h>


#ifndef tstring
#ifdef UNICODE 
#define tstring std::wstring
#else
#define tstring std::string
#endif
#endif

class CCusMutex
{
public:
	void Wait(LPCTSTR szMutexName)
	{
		if (StrCmp(szMutexName, _T("")) != 0)
		{
			m_strMutexName = szMutexName;

			m_hMutex = CreateMutex(NULL, FALSE, m_strMutexName.c_str());
		}

		BeginWait();
	}

	static BOOL IsRun(LPCTSTR szMutexName)
	{
		if (StrCmp(szMutexName, _T("")) != 0)
		{
			HANDLE hMutex = OpenMutex(MUTEX_ALL_ACCESS, FALSE, szMutexName);

			if (hMutex != NULL)
			{
				BOOL retValue = WaitForSingleObject(hMutex, 0x0) != WAIT_OBJECT_0;
				CloseHandle(hMutex);
				return retValue;
			}
		}

		return FALSE;
	}

	CCusMutex(LPCTSTR szMutexName/*, UINT IsOpen = 0*/)
	{
		m_hMutex = NULL;
		m_strMutexName = _T("");
		Wait(szMutexName);
	}

	CCusMutex()
	{
		m_hMutex = NULL;
		m_strMutexName = _T("");
	}
	virtual ~CCusMutex()
	{
		EndWait();

		if (m_hMutex != NULL)
		{
			CloseHandle(m_hMutex);
			m_hMutex = NULL;
		}
	}

private:
	HANDLE m_hMutex;
	tstring m_strMutexName;
	void EndWait()
	{
		if (m_hMutex != NULL)
		{
			ReleaseMutex(m_hMutex);
		}
	}
	void BeginWait()
	{
		if (m_hMutex != NULL)
		{
			/*DWORD dw = */WaitForSingleObject(m_hMutex, 0xFFFFFFFF);
		}
	}
};

#endif // !defined(AFX_CONFIGMUTEX_H__07BFE34F_0B5E_4D73_8020_736CC12DFCD0__INCLUDED_)
