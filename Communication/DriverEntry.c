#include <ntifs.h>

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

VOID DriverUnLoad(PDRIVER_OBJECT pDriverObject)
{
	UNICODE_STRING usSymbolicName;
	RtlInitUnicodeString(&usSymbolicName, L"\\??\\Communication");

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

	RtlInitUnicodeString(&usDeviceName, L"\\Device\\Communication");

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

	RtlInitUnicodeString(&usSymbolicName, L"\\??\\Communication");

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

NTSTATUS DeviceControlCompleteRoutine(PDEVICE_OBJECT pDeviceObject, PIRP pIrp)
{
	NTSTATUS			status = STATUS_SUCCESS;
	PIO_STACK_LOCATION	pIrpsp = IoGetCurrentIrpStackLocation(pIrp);
	ULONG				uLength = 0;

	KdPrint(("DeviceControl..."));
	PVOID pBuffer			= pIrp->AssociatedIrp.SystemBuffer;
	ULONG ulInputlength		= pIrpsp->Parameters.DeviceIoControl.InputBufferLength;
	ULONG ulOutputlength	= pIrpsp->Parameters.DeviceIoControl.OutputBufferLength;

	do 
	{
		switch (pIrpsp->Parameters.DeviceIoControl.IoControlCode)
		{
		case CWK_DVC_SEND_STR:
			{
				ASSERT(pBuffer != NULL);
				ASSERT(ulInputlength > 0);
				ASSERT(ulOutputlength == 0);

				KdPrint(("ulOutputlength = %ul, ulInputlength = %ul", ulOutputlength, ulInputlength));
				KdPrint(("pBuffer = %s", pBuffer));
			}
			break;
		case CWK_DVC_RECV_STR:
			{
				ASSERT(pBuffer != NULL);
				ASSERT(ulInputlength == 0);

				if (ulOutputlength < 512)
				{
					status = STATUS_INVALID_BUFFER_SIZE;
					break;
				}

				CHAR	szBuffer[] = "Hi, this is a leap.";
				KdPrint(("ulOutputlength = %ul, ulInputl`ength = %ul", ulOutputlength, ulInputlength));
				strncpy(pBuffer, szBuffer, sizeof(szBuffer));
				uLength = strnlen(szBuffer, sizeof(szBuffer)) + 1;
			}
			break;
		default:
		{
			status = STATUS_INVALID_PARAMETER;
		}
		break;
		}
	} while (FALSE);
	

	pIrp->IoStatus.Status = status;
	pIrp->IoStatus.Information = uLength;
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
	pDriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] = DeviceControlCompleteRoutine;

	pDriverObject->DriverUnload = DriverUnLoad;

	return STATUS_SUCCESS;
}