#include <ntifs.h>

#pragma warning(disable: 4100)

NTKERNELAPI
NTSTATUS
ObReferenceObjectByName(
	__in PUNICODE_STRING ObjectName,
	__in ULONG Attributes,
	__in_opt PACCESS_STATE AccessState,
	__in_opt ACCESS_MASK DesiredAccess,
	__in POBJECT_TYPE ObjectType,
	__in KPROCESSOR_MODE AccessMode,
	__inout_opt PVOID ParseContext,
	__out PVOID *Object
);
extern POBJECT_TYPE *IoDriverObjectType;

PDRIVER_OBJECT g_FilterDriverObject;
PDRIVER_DISPATCH gfn_OrigReadCompleteRoutine;

NTSTATUS FilterReadComlpeteRouine(DEVICE_OBJECT *DeviceObject, IRP *Irp)
{
	KdPrint(("FilterReadComlpeteRouine is running.."));
	return gfn_OrigReadCompleteRoutine(DeviceObject, Irp);
}

NTSTATUS FilterDriverQuery()
{
	NTSTATUS		status;
	UNICODE_STRING	usObjectName;

	RtlInitUnicodeString(&usObjectName, L"\\Driver\\PCHunter64al");

	status = ObReferenceObjectByName(
		&usObjectName, 
		OBJ_CASE_INSENSITIVE,
		NULL,
		FILE_ALL_ACCESS,
		*IoDriverObjectType,
		KernelMode,
		NULL,
		(PVOID*)&g_FilterDriverObject);
	if (!NT_SUCCESS(status))
	{
		KdPrint(("ObReferenceObjectByName is failed.."));
		return status;
	}
	KdPrint(("ObReferenceObjectByName success.."));

	// 备份并跳转到我们的过滤函数
	gfn_OrigReadCompleteRoutine = g_FilterDriverObject->MajorFunction[IRP_MJ_QUERY_SECURITY];
	g_FilterDriverObject->MajorFunction[IRP_MJ_QUERY_SECURITY] = FilterReadComlpeteRouine;

	ObDereferenceObject(g_FilterDriverObject);

	return STATUS_SUCCESS;
}

VOID DriverUnLoad(PDRIVER_OBJECT pDriverObject)
{
	UNICODE_STRING usSymbolicName;
	RtlInitUnicodeString(&usSymbolicName, L"\\??\\FirstDevice");

	// 恢复监控
	if (MmIsAddressValid(g_FilterDriverObject))
	{
		g_FilterDriverObject->MajorFunction[IRP_MJ_QUERY_SECURITY] = gfn_OrigReadCompleteRoutine;
	}

	if (NULL != pDriverObject->DeviceObject)
	{
		IoDeleteSymbolicLink(&usSymbolicName);
		IoDeleteDevice(pDriverObject->DeviceObject);
		KdPrint(("Unload driver success.."));
	}
}

NTSTATUS CreateDevice(PDRIVER_OBJECT pDriverObject)
{
	NTSTATUS status;
	PDEVICE_OBJECT pDeviceObject;
	UNICODE_STRING usDeviceName;
	UNICODE_STRING usSymbolicName;

	RtlInitUnicodeString(&usDeviceName, L"\\Device\\FirstDriver");

	status = IoCreateDevice(
		pDriverObject, 
		0, 
		&usDeviceName, 
		FILE_DEVICE_UNKNOWN, 
		FILE_DEVICE_SECURE_OPEN, 
		TRUE, 
		&pDeviceObject);
	if (!NT_SUCCESS(status))
	{
		return status;
	}

	pDeviceObject->Flags |= DO_BUFFERED_IO;

	RtlInitUnicodeString(&usSymbolicName, L"\\??\\FirstDevice");

	status = IoCreateSymbolicLink(&usSymbolicName, &usDeviceName);
	if (!NT_SUCCESS(status))
	{
		IoDeleteDevice(pDeviceObject);
		return status;
	}

	return STATUS_SUCCESS;
}

NTSTATUS CreateCompleteRoutine(PDEVICE_OBJECT pDeviceObject, PIRP pIrp)
{
	NTSTATUS status = STATUS_SUCCESS;

	KdPrint(("Create..."));

	pIrp->IoStatus.Status = status;
	pIrp->IoStatus.Information = 0;
	// 设置 Irp 请求已经处理完成，不要再继续传递
	IoCompleteRequest(pIrp, IO_NO_INCREMENT);

	return status;
}

NTSTATUS CloseCompleteRoutine(PDEVICE_OBJECT pDeviceObject, PIRP pIrp)
{
	NTSTATUS status = STATUS_SUCCESS;

	KdPrint(("Close..."));

	pIrp->IoStatus.Status = status;
	pIrp->IoStatus.Information = 0;
	// 设置 Irp 请求已经处理完成，不要再继续传递
	IoCompleteRequest(pIrp, IO_NO_INCREMENT);

	return status;
}

NTSTATUS ReadCompleteRoutine(PDEVICE_OBJECT pDeviceObject, PIRP pIrp)
{
	NTSTATUS status = STATUS_SUCCESS;

	KdPrint(("Read..."));

	pIrp->IoStatus.Status = status;
	pIrp->IoStatus.Information = 0;
	// 设置 Irp 请求已经处理完成，不要再继续传递
	IoCompleteRequest(pIrp, IO_NO_INCREMENT);

	return status;
}

NTSTATUS WriteCompleteRoutine(PDEVICE_OBJECT pDeviceObject, PIRP pIrp)
{
	NTSTATUS status = STATUS_SUCCESS;

	KdPrint(("Write..."));

	pIrp->IoStatus.Status = status;
	pIrp->IoStatus.Information = 0;
	// 设置 Irp 请求已经处理完成，不要再继续传递
	IoCompleteRequest(pIrp, IO_NO_INCREMENT);

	return status;
}

NTSTATUS DriverEntry(PDRIVER_OBJECT pDriverObject, PUNICODE_STRING pRegistryPath)
{
	KdPrint(("pRegistryPath = %wZ", pRegistryPath));

	CreateDevice(pDriverObject);

	pDriverObject->MajorFunction[IRP_MJ_CREATE] = CreateCompleteRoutine;
	pDriverObject->MajorFunction[IRP_MJ_CLOSE] = CloseCompleteRoutine;
	pDriverObject->MajorFunction[IRP_MJ_READ] = ReadCompleteRoutine;
	pDriverObject->MajorFunction[IRP_MJ_WRITE] = WriteCompleteRoutine;

	pDriverObject->DriverUnload = DriverUnLoad;

	FilterDriverQuery();

	return STATUS_SUCCESS;
}