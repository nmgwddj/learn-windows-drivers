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
	EnumProcess();
	return STATUS_SUCCESS;
}