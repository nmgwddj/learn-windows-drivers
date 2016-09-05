
// SystemMonitorDlg.cpp : 实现文件
//

#include "stdafx.h"
#include "SystemMonitor.h"
#include "SystemMonitorDlg.h"
#include "afxdialogex.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif


// 用于应用程序“关于”菜单项的 CAboutDlg 对话框

class CAboutDlg : public CDialogEx
{
public:
	CAboutDlg();

// 对话框数据
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_ABOUTBOX };
#endif

	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV 支持

// 实现
protected:
	DECLARE_MESSAGE_MAP()
};

CAboutDlg::CAboutDlg() : CDialogEx(IDD_ABOUTBOX)
{
}

void CAboutDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(CAboutDlg, CDialogEx)
END_MESSAGE_MAP()


// CSystemMonitorDlg 对话框



CSystemMonitorDlg::CSystemMonitorDlg(CWnd* pParent /*=NULL*/)
	: CDialogEx(IDD_SYSTEMMONITOR_DIALOG, pParent)
	, m_RegistryMonitor(nullptr)
	, m_ProcessMonitor(nullptr)
{
	m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);
}

CSystemMonitorDlg::~CSystemMonitorDlg()
{
	if (nullptr != m_RegistryMonitor)
	{
		m_RegistryMonitor->Stop();
		delete m_RegistryMonitor;
		m_RegistryMonitor = nullptr;
	}

	if (nullptr != m_ProcessMonitor)
	{
		m_ProcessMonitor->Stop();
		delete m_ProcessMonitor;
		m_ProcessMonitor = nullptr;
	}
}

void CSystemMonitorDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_LIST_MONITOR, m_listCtrl);
}

BEGIN_MESSAGE_MAP(CSystemMonitorDlg, CDialogEx)
	ON_WM_SYSCOMMAND()
	ON_WM_PAINT()
	ON_WM_QUERYDRAGICON()
	ON_WM_SIZE()
END_MESSAGE_MAP()


// CSystemMonitorDlg 消息处理程序

BOOL CSystemMonitorDlg::OnInitDialog()
{
	CDialogEx::OnInitDialog();

	// 将“关于...”菜单项添加到系统菜单中。

	// IDM_ABOUTBOX 必须在系统命令范围内。
	ASSERT((IDM_ABOUTBOX & 0xFFF0) == IDM_ABOUTBOX);
	ASSERT(IDM_ABOUTBOX < 0xF000);

	CMenu* pSysMenu = GetSystemMenu(FALSE);
	if (pSysMenu != NULL)
	{
		BOOL bNameValid;
		CString strAboutMenu;
		bNameValid = strAboutMenu.LoadString(IDS_ABOUTBOX);
		ASSERT(bNameValid);
		if (!strAboutMenu.IsEmpty())
		{
			pSysMenu->AppendMenu(MF_SEPARATOR);
			pSysMenu->AppendMenu(MF_STRING, IDM_ABOUTBOX, strAboutMenu);
		}
	}

	// 设置此对话框的图标。  当应用程序主窗口不是对话框时，框架将自动
	//  执行此操作
	SetIcon(m_hIcon, TRUE);			// 设置大图标
	SetIcon(m_hIcon, FALSE);		// 设置小图标

	// TODO: 在此添加额外的初始化代码
	m_listCtrl.Init();

	// 创建注册表监控对象
	m_RegistryMonitor  = new CRegistryMonitor(reinterpret_cast<CMonitorListCtrl*>(&m_listCtrl));
	m_RegistryMonitor->Run();
	m_ProcessMonitor = new CProcessMonitor(reinterpret_cast<CMonitorListCtrl*>(&m_listCtrl));
	m_ProcessMonitor->Run();

	/*int nItem = m_listCtrl.InsertItem(0, _T("test data"));
	m_listCtrl.SetItemText(nItem, 1, _T("C:\\Windows\\System32\\Regedit.exe"));
	m_listCtrl.SetItemText(nItem, 2, _T("删除注册表"));
	m_listCtrl.SetItemText(nItem, 3, _T("HKEY_LOCAL_MACHINE\\SOFTWARE\\AutoIt v3\\AutoIt"));
	m_listCtrl.SetItemText(nItem, 4, _T("about:blank"));*/

	return TRUE;  // 除非将焦点设置到控件，否则返回 TRUE
}

void CSystemMonitorDlg::OnSysCommand(UINT nID, LPARAM lParam)
{
	if ((nID & 0xFFF0) == IDM_ABOUTBOX)
	{
		CAboutDlg dlgAbout;
		dlgAbout.DoModal();
	}
	else
	{
		CDialogEx::OnSysCommand(nID, lParam);
	}
}

// 如果向对话框添加最小化按钮，则需要下面的代码
//  来绘制该图标。  对于使用文档/视图模型的 MFC 应用程序，
//  这将由框架自动完成。

void CSystemMonitorDlg::OnPaint()
{
	if (IsIconic())
	{
		CPaintDC dc(this); // 用于绘制的设备上下文

		SendMessage(WM_ICONERASEBKGND, reinterpret_cast<WPARAM>(dc.GetSafeHdc()), 0);

		// 使图标在工作区矩形中居中
		int cxIcon = GetSystemMetrics(SM_CXICON);
		int cyIcon = GetSystemMetrics(SM_CYICON);
		CRect rect;
		GetClientRect(&rect);
		int x = (rect.Width() - cxIcon + 1) / 2;
		int y = (rect.Height() - cyIcon + 1) / 2;

		// 绘制图标
		dc.DrawIcon(x, y, m_hIcon);
	}
	else
	{
		CDialogEx::OnPaint();
	}
}

//当用户拖动最小化窗口时系统调用此函数取得光标
//显示。
HCURSOR CSystemMonitorDlg::OnQueryDragIcon()
{
	return static_cast<HCURSOR>(m_hIcon);
}

void CSystemMonitorDlg::OnSize(UINT nType, int cx, int cy)
{
	CDialogEx::OnSize(nType, cx, cy);
	CRect rect;

	m_listCtrl.GetWindowRect(&rect);
	ScreenToClient(&rect);

	rect.top = 40;
	rect.left = 3;
	rect.right = cx - 3;
	rect.bottom = cy - 3;

	m_listCtrl.MoveWindow(&rect);
}
