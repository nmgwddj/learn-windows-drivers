#include <ntifs.h>
#include "ProcessMgr.h"
#include "IODeviceControl.h"

VOID CreateProcessNotifyEx(
	_Inout_  PEPROCESS              Process,
	_In_     HANDLE                 ProcessId,
	_In_opt_ PPS_CREATE_NOTIFY_INFO CreateInfo
)
{
	if (NULL != CreateInfo)
	{
		LARGE_INTEGER		unCurrentSystemTime;
		LARGE_INTEGER		unCurrentLocalTime;
		WCHAR				wzProcessPath[MAX_STRING_LENGTH] = { 0 };

		// 时间
		KeQuerySystemTime(&unCurrentSystemTime);
		ExSystemTimeToLocalTime(&unCurrentSystemTime, &unCurrentLocalTime);

		// 进程路径
		GetProcessPathBySectionObject(CreateInfo->ParentProcessId, wzProcessPath);

		PPROCESSNODE pNode = InitListNode();
		if (pNode != NULL)
		{
			ULONG	ulParentProcessLength = wcslen(wzProcessPath) * sizeof(WCHAR) + sizeof(WCHAR);
			ULONG	ulProcessLength = CreateInfo->ImageFileName->Length + sizeof(WCHAR);
			ULONG	ulCommandLineLength = CreateInfo->CommandLine == NULL ? 0 : CreateInfo->CommandLine->Length + sizeof(WCHAR);

			SIZE_T	ulNumberOfBytes = sizeof(PROCESSINFO) + ulParentProcessLength + ulProcessLength + ulCommandLineLength;

			pNode->pProcessInfo = ExAllocatePoolWithTag(NonPagedPool, ulNumberOfBytes, MEM_TAG);

			pNode->pProcessInfo->bIsCreate					= TRUE;
			pNode->pProcessInfo->hParentProcessId			= CreateInfo->ParentProcessId;
			pNode->pProcessInfo->ulParentProcessLength		= ulParentProcessLength;
			pNode->pProcessInfo->hProcessId					= ProcessId;
			pNode->pProcessInfo->ulProcessLength			= ulProcessLength;
			pNode->pProcessInfo->ulCommandLineLength		= ulCommandLineLength;

			RtlTimeToTimeFields(&unCurrentLocalTime, &pNode->pProcessInfo->time);

			RtlCopyBytes(pNode->pProcessInfo->uData, wzProcessPath, ulParentProcessLength);
			RtlCopyBytes(pNode->pProcessInfo->uData + ulParentProcessLength, CreateInfo->ImageFileName->Buffer, CreateInfo->ImageFileName->Length);
			pNode->pProcessInfo->uData[ulParentProcessLength + CreateInfo->ImageFileName->Length + 0] = '\0';
			pNode->pProcessInfo->uData[ulParentProcessLength + CreateInfo->ImageFileName->Length + 1] = '\0';
			RtlCopyBytes(pNode->pProcessInfo->uData + ulParentProcessLength + ulProcessLength, CreateInfo->CommandLine->Buffer, CreateInfo->CommandLine->Length);
			pNode->pProcessInfo->uData[ulParentProcessLength + ulProcessLength + CreateInfo->CommandLine->Length + 0] = '\0';
			pNode->pProcessInfo->uData[ulParentProcessLength + ulProcessLength + CreateInfo->CommandLine->Length + 1] = '\0';

			ExInterlockedInsertTailList(&g_ListHead, (PLIST_ENTRY)pNode, &g_Lock);
			KeSetEvent(&g_Event, 0, FALSE);
		}
	}
	else
	{
		// 退出时仅能取到进程 Id
		//KdPrint(("[CreateProcessNotifyEx] [%04ld] 退出", ProcessId));
	}
}

VOID CreateProcessNotify(
	IN HANDLE   ParentId,
	IN HANDLE   ProcessId,
	IN BOOLEAN  Create
)
{
	WCHAR			wzParentProcessPath[512] = { 0 };
	WCHAR			wzProcessPath[512] = { 0 };
	UNICODE_STRING	usProcessParameters;

	GetProcessPathBySectionObject(ParentId, wzParentProcessPath);
	GetProcessPathBySectionObject(ProcessId, wzProcessPath);

	if (Create)
	{	
		/*KdPrint(("[CreateProcessNotify] [%04ld] %ws 创建进程：[%04ld] %ws",
			ParentId, wzParentProcessPath, ProcessId, wzProcessPath));*/
	}
	else
	{
		/*KdPrint(("ExitProcess begin ----------------------"));
		KdPrint(("ProcessId: %ld", ProcessId));
		KdPrint(("ProcessName: %s", GetProcessNameByProcessId(ProcessId)));
		KdPrint(("ProcessPath: %ws", wzProcessPath));
		KdPrint(("ParentProcessId: %ld", ParentId));
		KdPrint(("ParentProcessPath: %ws", wzParentProcessPath));
		KdPrint(("ExitProcess end ----------------------"));*/
		/*KdPrint(("[CreateProcessNotify] [%04ld] %ws 进程退出，父进程：[%04ld] %ws",
			ProcessId, wzProcessPath, ParentId, wzParentProcessPath));*/
	}
}

VOID DriverUnload(PDRIVER_OBJECT pDriverObject)
{
	PsSetCreateProcessNotifyRoutineEx(CreateProcessNotifyEx, TRUE);
	PsSetCreateProcessNotifyRoutine(CreateProcessNotify, TRUE);

	RemoveDevice(pDriverObject);
	DestroyList();
	
	KdPrint(("DriverUnload..."));
}

NTSTATUS DriverEntry(
	_In_ PDRIVER_OBJECT  pDriverObject,
	_In_ PUNICODE_STRING RegistryPath
)
{
	NTSTATUS status;

	// 创建进程回调
	status = PsSetCreateProcessNotifyRoutineEx(CreateProcessNotifyEx, FALSE);
	if (!NT_SUCCESS(status))
	{
		KdPrint(("Failed to call PsSetCreateProcessNotifyRoutineEx, error code = 0x%08X", status));
	}
	status = PsSetCreateProcessNotifyRoutine(CreateProcessNotify, FALSE);
	if (!NT_SUCCESS(status))
	{
		KdPrint(("Failed to call PsSetCreateProcessNotifyRoutine, error code = 0x%08X", status));
	}

	// 初始化事件、锁、链表头
	KeInitializeEvent(&g_Event, SynchronizationEvent, TRUE);
	KeInitializeSpinLock(&g_Lock);
	InitializeListHead(&g_ListHead);

	// 创建设备
	CreateDevice(pDriverObject);

	// 处理不同的 IRP 请求
	pDriverObject->MajorFunction[IRP_MJ_CREATE] = CreateCompleteRoutine;
	pDriverObject->MajorFunction[IRP_MJ_CLOSE] = CloseCompleteRoutine;
	pDriverObject->MajorFunction[IRP_MJ_READ] = ReadCompleteRoutine;
	pDriverObject->MajorFunction[IRP_MJ_WRITE] = WriteCompleteRoutine;
	pDriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] = DeviceControlCompleteRoutine;

	// 指定卸载函数
	pDriverObject->DriverUnload = DriverUnload;

	return status;
}

