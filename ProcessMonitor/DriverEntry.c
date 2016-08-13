#include <ntifs.h>
#include "ProcessMgr.h"

VOID CreateProcessNotifyEx(
	_Inout_  PEPROCESS              Process,
	_In_     HANDLE                 ProcessId,
	_In_opt_ PPS_CREATE_NOTIFY_INFO CreateInfo
)
{
	if (NULL != CreateInfo)
	{
		WCHAR wzProcessPath[512] = { 0 };
		GetProcessPathBySectionObject(CreateInfo->ParentProcessId, wzProcessPath);

		KdPrint(("CreateProcess begin ----------------------"));
		KdPrint(("ProcessId: %ld", ProcessId));
		KdPrint(("ProcessName: %s", GetProcessNameByProcessId(ProcessId)));
		KdPrint(("ProcessPath: %wZ", CreateInfo->ImageFileName));
		KdPrint(("ProcessCommandLine: %wZ", CreateInfo->CommandLine));
		KdPrint(("ParentProcessId: %ld", CreateInfo->ParentProcessId));
		KdPrint(("ParentProcessPath: %ws", wzProcessPath));
		KdPrint(("CreateProcess end ----------------------"));

		/*KdPrint(("[CreateProcessNotifyEx] [%04ld] %ws 创建进程：[%04ld] %wZ 参数：%wZ",
			CreateInfo->ParentProcessId,
			wzProcessPath,
			ProcessId,
			CreateInfo->ImageFileName,
			CreateInfo->CommandLine));*/
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
		KdPrint(("ExitProcess begin ----------------------"));
		KdPrint(("ProcessId: %ld", ProcessId));
		KdPrint(("ProcessName: %s", GetProcessNameByProcessId(ProcessId)));
		KdPrint(("ProcessPath: %ws", wzProcessPath));
		KdPrint(("ParentProcessId: %ld", ParentId));
		KdPrint(("ParentProcessPath: %ws", wzParentProcessPath));
		KdPrint(("ExitProcess end ----------------------"));
		/*KdPrint(("[CreateProcessNotify] [%04ld] %ws 进程退出，父进程：[%04ld] %ws",
			ProcessId, wzProcessPath, ParentId, wzParentProcessPath));*/
	}
}

VOID DriverUnload(
	_In_ struct _DRIVER_OBJECT *DriverObject
)
{
	PsSetCreateProcessNotifyRoutineEx(CreateProcessNotifyEx, TRUE);
	PsSetCreateProcessNotifyRoutine(CreateProcessNotify, TRUE);
	KdPrint(("DriverUnload..."));
}

NTSTATUS DriverEntry(
	_In_ PDRIVER_OBJECT  DriverObject,
	_In_ PUNICODE_STRING RegistryPath
)
{
	NTSTATUS status;

	DriverObject->DriverUnload = DriverUnload;

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

	return status;
}

