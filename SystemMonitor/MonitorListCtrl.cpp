// MonitorListCtrl.cpp : 实现文件
//

#include "stdafx.h"
#include "SystemMonitor.h"
#include "MonitorListCtrl.h"
#include "CusMutex.h"


// CMonitorListCtrl

IMPLEMENT_DYNAMIC(CMonitorListCtrl, CListCtrl)

CMonitorListCtrl::CMonitorListCtrl()
{

}

CMonitorListCtrl::~CMonitorListCtrl()
{
}


void CMonitorListCtrl::Init()
{
	InsertColumn(0, _T("时间"), LVCFMT_LEFT, 140);
	InsertColumn(1, _T("进程"), LVCFMT_LEFT, 230);
	InsertColumn(2, _T("操作"), LVCFMT_LEFT, 100);
	InsertColumn(3, _T("目标"), LVCFMT_LEFT, 450);
	InsertColumn(4, _T("数据"), LVCFMT_LEFT, 200);
	DWORD dwStyle = GetExtendedStyle();
	dwStyle |= LVS_EX_GRIDLINES | LVS_EX_FULLROWSELECT;
	SetExtendedStyle(dwStyle);
}

void CMonitorListCtrl::InsertRegistryMonitorItem(LPCTSTR wzRegistryTime, LPCTSTR pwzProcessPath, LPCTSTR pwzRegistryPath, LPCTSTR wzRegistryEventClass, LPCTSTR pwzRegistryData)
{
	CCusMutex mutexTemp(_T("InsertUICtrlList"));

	//LockWindowUpdate();

	int nItem = InsertItem(GetItemCount(), wzRegistryTime);
	SetItemText(nItem, 1, pwzProcessPath);
	SetItemText(nItem, 2, wzRegistryEventClass);
	SetItemText(nItem, 3, pwzRegistryPath);
	SetItemText(nItem, 4, pwzRegistryData);

	SetItemData(nItem, RegistryItem);

	//UnlockWindowUpdate();
}

void CMonitorListCtrl::InsertProcessMonitorItem(LPCTSTR wzProcessTime, LPCTSTR pwzParentProcessPath, LPCTSTR pwzProcessPath, LPCTSTR pwzIsCreateProcess, LPCTSTR pwzCommandLine)
{
	CCusMutex mutexTemp(_T("InsertUICtrlList"));

	// LockWindowUpdate();
	
	int nItem = InsertItem(GetItemCount(), wzProcessTime);
	SetItemText(nItem, 1, pwzParentProcessPath);
	SetItemText(nItem, 2, pwzIsCreateProcess);
	SetItemText(nItem, 3, pwzProcessPath);
	SetItemText(nItem, 4, pwzCommandLine);

	SetItemData(nItem, ProcessItem);

	// UnlockWindowUpdate();
}

BEGIN_MESSAGE_MAP(CMonitorListCtrl, CListCtrl)
	ON_NOTIFY_REFLECT(NM_CUSTOMDRAW, &CMonitorListCtrl::OnNMCustomdraw)
END_MESSAGE_MAP()


// CMonitorListCtrl 消息处理程序
void CMonitorListCtrl::OnNMCustomdraw(NMHDR *pNMHDR, LRESULT *pResult)
{
	LPNMCUSTOMDRAW pNMCD = reinterpret_cast<LPNMCUSTOMDRAW>(pNMHDR);
	NMLVCUSTOMDRAW* pLVCD = reinterpret_cast<NMLVCUSTOMDRAW*>(pNMHDR);
	// TODO: 在此添加控件通知处理程序代码

	if (CDDS_PREPAINT == pLVCD->nmcd.dwDrawStage)
	{
		*pResult = CDRF_NOTIFYITEMDRAW;
	}
	else if (CDDS_ITEMPREPAINT == pLVCD->nmcd.dwDrawStage)
	{
		// This is the notification message for an item. We'll request  
		// notifications before each subitem's prepaint stage.  

		*pResult = CDRF_NOTIFYSUBITEMDRAW;
	}
	else if ((CDDS_ITEMPREPAINT | CDDS_SUBITEM) == pLVCD->nmcd.dwDrawStage)
	{
		int nItem = static_cast<int>(pLVCD->nmcd.dwItemSpec);
		COLORREF clrNewTextColor, clrNewBkColor;
		clrNewTextColor = RGB(0, 0, 0);
		
		switch (pNMCD->lItemlParam)
		{
		case RegistryItem:
			clrNewBkColor = RGB(175, 238, 238);
			pLVCD->clrText = clrNewTextColor;
			pLVCD->clrTextBk = clrNewBkColor;
			break;
		case ProcessItem:
			clrNewBkColor = RGB(255, 140, 0);
			pLVCD->clrText = clrNewTextColor;
			pLVCD->clrTextBk = clrNewBkColor;
			break;
		default:
			pLVCD->clrText = RGB(0, 0, 0);
			pLVCD->clrTextBk = RGB(255, 255, 255);
			break;
		}
		*pResult = CDRF_DODEFAULT;
	}
	//*pResult = 0;
}
