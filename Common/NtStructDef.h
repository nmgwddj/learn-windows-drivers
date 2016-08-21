#pragma once

// 内存分配 TAG
#define MEM_TAG 'MEM'

// 字符串长度
#define MAX_STRING_LENGTH 512

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

typedef struct _PROCESSINFO
{
	HANDLE	hParentId;
	HANDLE	hProcessId;
	WCHAR	wsProcessPath[MAX_STRING_LENGTH];
	WCHAR	wsProcessCommandLine[MAX_STRING_LENGTH];
} PROCESSINFO, *PPROCESSINFO;
