
// SystemMonitorDlg.h : 头文件
//

#pragma once
#include "afxcmn.h"
#include "RegistryMonitor.h"
#include "ProcessMonitor.h"
#include "MonitorListCtrl.h"


// CSystemMonitorDlg 对话框
class CSystemMonitorDlg : public CDialogEx
{
// 构造
public:
	CSystemMonitorDlg(CWnd* pParent = NULL);	// 标准构造函数
	~CSystemMonitorDlg();

// 对话框数据
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_SYSTEMMONITOR_DIALOG };
#endif

	protected:
	virtual void DoDataExchange(CDataExchange* pDX);	// DDX/DDV 支持


// 实现
protected:
	HICON m_hIcon;

	// 生成的消息映射函数
	virtual BOOL OnInitDialog();
	afx_msg void OnSysCommand(UINT nID, LPARAM lParam);
	afx_msg void OnPaint();
	afx_msg HCURSOR OnQueryDragIcon();
	DECLARE_MESSAGE_MAP()

private:
	CMonitorListCtrl			m_listCtrl;
	CRegistryMonitor*			m_RegistryMonitor;
	CProcessMonitor*			m_ProcessMonitor;
public:
	afx_msg void OnSize(UINT nType, int cx, int cy);
};
