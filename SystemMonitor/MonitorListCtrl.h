#pragma once


// CMonitorListCtrl

class CMonitorListCtrl : public CListCtrl
{
	DECLARE_DYNAMIC(CMonitorListCtrl)

public:
	CMonitorListCtrl();
	virtual ~CMonitorListCtrl();

	typedef enum _ITEMTYPE
	{
		RegistryItem,
		ProcessItem
	} ITEMTYPE;

	void Init();

	void InsertRegistryMonitorItem(
		LPCTSTR wzRegistryTime,
		LPCTSTR pwzProcessPath,
		LPCTSTR pwzRegistryPath,
		LPCTSTR wzRegistryEventClass,
		LPCTSTR pwzRegistryData);

	void InsertProcessMonitorItem(
		LPCTSTR wzProcessTime,
		LPCTSTR pwzParentProcessPath,
		LPCTSTR pwzProcessPath,
		LPCTSTR pwzIsCreateProcess,
		LPCTSTR pwzCommandLine);

protected:
	DECLARE_MESSAGE_MAP()

public:
	afx_msg void OnNMCustomdraw(NMHDR *pNMHDR, LRESULT *pResult);

private:
	ITEMTYPE	m_ItemType;
};


