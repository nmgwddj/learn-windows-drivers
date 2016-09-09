#ifndef _NTSTRUCTDEF_H_
#define _NTSTRUCTDEF_H_

// 内存分配 TAG
#define MEM_TAG 'MEM'

// 字符串长度
#define MAX_STRING_LENGTH			512
#define MAX_PID_LENGTH				32
#define MAX_TIME_LENGTH				20

// 启动符号链接
#define REGISTRY_MONITOR_DEVICE		L"\\Device\\_RegistryMonitor"
#define REGISTRY_MONITOR_SYMBOLIC	L"\\??\\_RegistryMonitor"
#define PROCESS_MONITOR_DEVICE		L"\\Device\\_ProcessMonitor"
#define PROCESS_MONITOR_SYMBOLIC	L"\\??\\_ProcessMonitor"


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

#ifndef _NTIFS_

typedef unsigned short WORD;

typedef struct _TIME_FIELDS {
	WORD wYear;
	WORD wMonth;
	WORD wDay;
	WORD wHour;
	WORD wMinute;
	WORD wSecond;
	WORD wMilliseconds;
	WORD wWeekday;
} TIME_FIELDS;

typedef enum _REG_NOTIFY_CLASS {
	RegNtDeleteKey,
	RegNtPreDeleteKey = RegNtDeleteKey,
	RegNtSetValueKey,
	RegNtPreSetValueKey = RegNtSetValueKey,
	RegNtDeleteValueKey,
	RegNtPreDeleteValueKey = RegNtDeleteValueKey,
	RegNtSetInformationKey,
	RegNtPreSetInformationKey = RegNtSetInformationKey,
	RegNtRenameKey,
	RegNtPreRenameKey = RegNtRenameKey,
	RegNtEnumerateKey,
	RegNtPreEnumerateKey = RegNtEnumerateKey,
	RegNtEnumerateValueKey,
	RegNtPreEnumerateValueKey = RegNtEnumerateValueKey,
	RegNtQueryKey,
	RegNtPreQueryKey = RegNtQueryKey,
	RegNtQueryValueKey,
	RegNtPreQueryValueKey = RegNtQueryValueKey,
	RegNtQueryMultipleValueKey,
	RegNtPreQueryMultipleValueKey = RegNtQueryMultipleValueKey,
	RegNtPreCreateKey,
	RegNtPostCreateKey,
	RegNtPreOpenKey,
	RegNtPostOpenKey,
	RegNtKeyHandleClose,
	RegNtPreKeyHandleClose = RegNtKeyHandleClose,
	//
	// .Net only
	//    
	RegNtPostDeleteKey,
	RegNtPostSetValueKey,
	RegNtPostDeleteValueKey,
	RegNtPostSetInformationKey,
	RegNtPostRenameKey,
	RegNtPostEnumerateKey,
	RegNtPostEnumerateValueKey,
	RegNtPostQueryKey,
	RegNtPostQueryValueKey,
	RegNtPostQueryMultipleValueKey,
	RegNtPostKeyHandleClose,
	RegNtPreCreateKeyEx,
	RegNtPostCreateKeyEx,
	RegNtPreOpenKeyEx,
	RegNtPostOpenKeyEx,
	//
	// new to Windows Vista
	//
	RegNtPreFlushKey,
	RegNtPostFlushKey,
	RegNtPreLoadKey,
	RegNtPostLoadKey,
	RegNtPreUnLoadKey,
	RegNtPostUnLoadKey,
	RegNtPreQueryKeySecurity,
	RegNtPostQueryKeySecurity,
	RegNtPreSetKeySecurity,
	RegNtPostSetKeySecurity,
	//
	// per-object context cleanup
	//
	RegNtCallbackObjectContextCleanup,
	//
	// new in Vista SP2 
	//
	RegNtPreRestoreKey,
	RegNtPostRestoreKey,
	RegNtPreSaveKey,
	RegNtPostSaveKey,
	RegNtPreReplaceKey,
	RegNtPostReplaceKey,
	//
	// new to Windows 10
	//
	RegNtPreQueryKeyName,
	RegNtPostQueryKeyName,

	MaxRegNtNotifyClass //should always be the last enum
} REG_NOTIFY_CLASS;
#endif



typedef struct _PROCESSINFO
{
	TIME_FIELDS			time;						// 时间
	HANDLE				hParentProcessId;			// 父进程 ID
	ULONG				ulParentProcessLength;		// 父进程长度
	HANDLE				hProcessId;					// 子进程 ID
	ULONG				ulProcessLength;			// 子进程长度
	ULONG				ulCommandLineLength;		// 进程命令行参数长度
	UCHAR				uData[1];					// 数据域
} PROCESSINFO, *PPROCESSINFO;

typedef struct _REGISTRY_EVENT
{
	HANDLE				hProcessId;					// 操作注册表的进程
	TIME_FIELDS			time;						// 操作时间
	REG_NOTIFY_CLASS	enRegistryNotifyClass;		// 操作的类型，新建、删除、修改等
	ULONG				ulProcessPathLength;		// 操作注册表进程的路径长度
	ULONG				ulRegistryPathLength;		// 操作的注册表路径长度
	ULONG				ulKeyValueType;				// 如果是修改值则显示修改的数据键的数据类型
	ULONG				ulDataLength;				// 如果是修改值则代表修改的数据的长度
	UCHAR				uData[1];					// 存储路径和修改的值的空间
} REGISTRY_EVENT, *PREGISTRY_EVENT;

#endif
