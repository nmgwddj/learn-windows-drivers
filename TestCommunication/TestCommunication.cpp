// TestCommunication.cpp : 定义控制台应用程序的入口点。
//

#include "stdafx.h"
#include "RegistryMonitor.h"

int _tmain(int argc, _TCHAR* argv[])
{
	locale loc("chs");
	wcout.imbue(loc);

	CRegistryMonitor RegistryMonitorObj;

	if (RegistryMonitorObj.Run())
	{
		system("PAUSE");
		RegistryMonitorObj.Stop();
	}
	else
	{
		cout << "Failed to open device.." << endl;
		system("PAUSE");
	}

	return 0;
}

int _tmain1(int argc, _TCHAR* argv[])
{
	HANDLE		hStdHandle;
	HANDLE		hDevice = NULL;
	ULONG		ulResult = 0;
	BOOL		bRet = FALSE;

	// 设置控制台窗口大小，方便查看
	hStdHandle = GetStdHandle(STD_OUTPUT_HANDLE);
	SMALL_RECT rc = { 0, 0, 120 - 1, 25 - 1 };  
	SetConsoleWindowInfo(hStdHandle, TRUE, &rc);

	// 打开驱动设备
	hDevice = CreateFile(L"SYMBOLIC_NAME", GENERIC_READ | GENERIC_WRITE, 0, 0, OPEN_EXISTING, FILE_ATTRIBUTE_SYSTEM, 0);
	if (hDevice == INVALID_HANDLE_VALUE)
	{
		printf("Failed to Open device.\r\n");
		return -1;
	}

	// Receive message from driver
	PROCESSINFO stProcessInfo;
	while (TRUE)
	{
		memset(&stProcessInfo, 0, sizeof(PROCESSINFO));
		bRet = DeviceIoControl(hDevice, CWK_DVC_RECV_STR, NULL, 0, &stProcessInfo, sizeof(stProcessInfo), &ulResult, 0);
		if (bRet)
		{
			// 打印数据，wsProcessCommandLine 也是一个参数，如果需要可以自己放开，格式化字符串中增加一个 %ws
			//printf("PPID = %ld, PID = %ld, %ws\r\n",
			//	stProcessInfo.hParentId,
			//	stProcessInfo.hProcessId,
			//	stProcessInfo.wsProcessPath/*,
			//	stProcessInfo.wsProcessCommandLine*/);
		}
	}

	CloseHandle(hDevice);
	system("PAUSE");

    return 0;
}

