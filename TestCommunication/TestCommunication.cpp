// TestCommunication.cpp : 定义控制台应用程序的入口点。
//

#include "stdafx.h"

#define SYMBOLIC_NAME _T("\\??\\Communication")

// 从应用层给驱动发送一个字符串。
#define  CWK_DVC_SEND_STR \
	(ULONG)CTL_CODE( \
	FILE_DEVICE_UNKNOWN, \
	0x911,METHOD_BUFFERED, \
	FILE_WRITE_DATA)

// 从驱动读取一个字符串
#define  CWK_DVC_RECV_STR \
	(ULONG)CTL_CODE( \
	FILE_DEVICE_UNKNOWN, \
	0x912,METHOD_BUFFERED, \
	FILE_READ_DATA)

int _tmain(int argc, _TCHAR* argv[])
{
	HANDLE		hDevice = NULL;
	CHAR		szBuffer[] = "Hello my first driver..";
	CHAR		szRecvBuffer[512] = { 0 };
	ULONG		ulResult = 0;
	BOOL		bRet = FALSE;

	hDevice = CreateFile(SYMBOLIC_NAME, GENERIC_READ | GENERIC_WRITE, 0, 0, OPEN_EXISTING, FILE_ATTRIBUTE_SYSTEM, 0);
	if (hDevice == INVALID_HANDLE_VALUE)
	{
		printf("Failed to Open device.\r\n");
		return -1;
	}
	else
	{
		printf("Open device successfully.\r\n");
	}

	bRet = DeviceIoControl(hDevice, CWK_DVC_SEND_STR, szBuffer, (DWORD)strlen(szBuffer) + 1, NULL, 0, &ulResult, 0);
	if (bRet)
	{
		printf("Send message successfully.\r\n");
	}

	system("PAUSE");

	bRet = DeviceIoControl(hDevice, CWK_DVC_RECV_STR, NULL, 0, szRecvBuffer, 512, &ulResult, 0);
	if (bRet)
	{
		printf("Recv message successfully. The result content = %s\r\n", szRecvBuffer);
	}

	CloseHandle(hDevice);

	system("PAUSE");
    return 0;
}

