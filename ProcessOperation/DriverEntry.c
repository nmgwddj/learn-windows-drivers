#include <Ntifs.h>

typedef unsigned long DWORD;

NTKERNELAPI UCHAR* PsGetProcessImageFileName(IN PEPROCESS Process);
NTKERNELAPI HANDLE PsGetProcessInheritedFromUniqueProcessId(IN PEPROCESS Process);

VOID ZwKillProcess(HANDLE hPid)
{
	HANDLE				hProcess = NULL;
	CLIENT_ID			ClientId;
	OBJECT_ATTRIBUTES	ObjectAttributes;

	ClientId.UniqueProcess = hPid;
	ClientId.UniqueThread = 0;

	InitializeObjectAttributes(&ObjectAttributes, NULL, 0, NULL, NULL);

	ZwOpenProcess(&hProcess, GENERIC_ALL, &ObjectAttributes, &ClientId);
	if (NULL != hProcess)
	{
		ZwTerminateProcess(hProcess, 0);
		ZwClose(hProcess);
	}
}

PETHREAD LookupThread(HANDLE Tid)
{
	PETHREAD ethread;
	if (NT_SUCCESS(PsLookupThreadByThreadId(Tid, &ethread)))
	{
		return ethread;
	}
	else
	{
		return NULL;
	}
}

VOID EnumThread(PEPROCESS Process)
{
	ULONG			i = 0;
	ULONG			c = 0;
	PETHREAD		ethread = NULL;
	PEPROCESS		eprocess = NULL;

	for (i = 4; i < 262144; i += 4)
	{
		ethread = LookupThread((HANDLE)i);
		if (ethread != NULL)
		{
			// 获取进程所属线程
			eprocess = IoThreadToProcess(ethread);
			if (eprocess)
			{
				UCHAR *ucProcessName = PsGetProcessImageFileName(eprocess);
				HANDLE PsGetThreadId();
				if (strcmp("notepad.exe", ucProcessName) == 0)
				{
					KdPrint(("\tETHREAD = %p, TID = %ld",
						ethread,
						(UINT64)PsGetThreadId(ethread)));
				}
			}
			ObDereferenceObject(ethread);
		}
	}

}

PEPROCESS LookupProcess(HANDLE Pid)
{
	PEPROCESS eprocess = NULL;
	if (NT_SUCCESS(PsLookupProcessByProcessId(Pid, &eprocess)))
	{
		return eprocess;
	}

	return NULL;
}

VOID EnumProcess()
{
	ULONG i = 0;
	PEPROCESS eproc = NULL;

	for (i = 4; i < 262144; i += 4)
	{
		eproc = LookupProcess((HANDLE)i);
		if (NULL != eproc)
		{
			UCHAR *ucProcessName = PsGetProcessImageFileName(eproc);
			HANDLE hPid = PsGetProcessId(eproc);
			DbgPrint("EPROCESS = %p, PID = %ld, PPID = %ld, Name = %s",
				eproc,
				(UINT64)hPid,
				(UINT64)PsGetProcessInheritedFromUniqueProcessId(eproc),
				ucProcessName);
			if (strcmp(ucProcessName, "notepad.exe") == 0)
			{
				// 枚举线程
				EnumThread(eproc);
				// 结束进程
				ZwKillProcess(hPid);
			}
		}
	}
}

VOID DriverUnload(IN PDRIVER_OBJECT pDriverObject)
{
	KdPrint(("[MemoryOperation] Unload..."));
	return;
}

NTSTATUS DriverEntry(IN PDRIVER_OBJECT pDriverObject, IN PUNICODE_STRING pRegistryPath)
{
	pDriverObject->DriverUnload = DriverUnload;
	KdPrint(("[MemoryOperation] Load..."));
	// 枚举进程
	EnumProcess();
	return STATUS_SUCCESS;
}