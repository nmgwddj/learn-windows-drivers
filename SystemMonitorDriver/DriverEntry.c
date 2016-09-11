#include <ntifs.h>

VOID DriverUnLoad(PDRIVER_OBJECT pDriverObject)
{
	UNREFERENCED_PARAMETER(pDriverObject);
	KdPrint(("[SystemMonitor64] Driver unLoad.."));
}

NTSTATUS DriverEntry(PDRIVER_OBJECT pDriverObject, PUNICODE_STRING pRegistryPath)
{
	UNREFERENCED_PARAMETER(pDriverObject);
	UNREFERENCED_PARAMETER(pRegistryPath);

	NTSTATUS Status = STATUS_SUCCESS;

	KdPrint(("[SystemMonitor64] Driver unLoad.."));

	pDriverObject->DriverUnload = DriverUnLoad;

	return Status;
}