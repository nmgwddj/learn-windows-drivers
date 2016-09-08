#include "IODeviceControl.h"

NTSTATUS CreateDevice(PDRIVER_OBJECT pDriverObject)
{
	NTSTATUS status;
	PDEVICE_OBJECT pDeviceObject;
	UNICODE_STRING usDeviceName;
	UNICODE_STRING usSymbolicName;

	RtlInitUnicodeString(&usDeviceName, PROCESS_MONITOR_DEVICE);

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

	pDeviceObject->Flags &= ~DO_DEVICE_INITIALIZING;

	RtlInitUnicodeString(&usSymbolicName, PROCESS_MONITOR_SYMBOLIC);

	status = IoCreateSymbolicLink(&usSymbolicName, &usDeviceName);
	if (!NT_SUCCESS(status))
	{
		IoDeleteDevice(pDeviceObject);
		return status;
	}

	return STATUS_SUCCESS;
}

NTSTATUS RemoveDevice(PDRIVER_OBJECT pDriverObject)
{
	NTSTATUS		status;
	UNICODE_STRING	usSymbolicName;

	RtlInitUnicodeString(&usSymbolicName, PROCESS_MONITOR_SYMBOLIC);

	// 删除符号链接和设备对象
	if (NULL != pDriverObject->DeviceObject)
	{
		status = IoDeleteSymbolicLink(&usSymbolicName);
		IoDeleteDevice(pDriverObject->DeviceObject);
		KdPrint(("Unload driver success..\r\n"));
	}

	return status;
}

NTSTATUS CreateCompleteRoutine(PDEVICE_OBJECT pDeviceObject, PIRP pIrp)
{
	NTSTATUS status = STATUS_SUCCESS;

	pIrp->IoStatus.Status = status;
	pIrp->IoStatus.Information = 0;
	IoCompleteRequest(pIrp, IO_NO_INCREMENT);

	return status;
}

NTSTATUS CloseCompleteRoutine(PDEVICE_OBJECT pDeviceObject, PIRP pIrp)
{
	NTSTATUS status = STATUS_SUCCESS;

	pIrp->IoStatus.Status = status;
	pIrp->IoStatus.Information = 0;
	IoCompleteRequest(pIrp, IO_NO_INCREMENT);

	return status;
}

NTSTATUS ReadCompleteRoutine(PDEVICE_OBJECT pDeviceObject, PIRP pIrp)
{
	NTSTATUS status = STATUS_SUCCESS;

	pIrp->IoStatus.Status = status;
	pIrp->IoStatus.Information = 0;
	IoCompleteRequest(pIrp, IO_NO_INCREMENT);

	return status;
}

NTSTATUS WriteCompleteRoutine(PDEVICE_OBJECT pDeviceObject, PIRP pIrp)
{
	NTSTATUS status = STATUS_SUCCESS;

	pIrp->IoStatus.Status = status;
	pIrp->IoStatus.Information = 0;
	IoCompleteRequest(pIrp, IO_NO_INCREMENT);

	return status;
}

NTSTATUS DeviceControlCompleteRoutine(PDEVICE_OBJECT pDeviceObject, PIRP pIrp)
{
	NTSTATUS			status = STATUS_SUCCESS;
	PIO_STACK_LOCATION	pIrpsp = IoGetCurrentIrpStackLocation(pIrp);
	ULONG				uLength = 0;

	PVOID pBuffer = pIrp->AssociatedIrp.SystemBuffer;
	ULONG ulInputlength = pIrpsp->Parameters.DeviceIoControl.InputBufferLength;
	ULONG ulOutputlength = pIrpsp->Parameters.DeviceIoControl.OutputBufferLength;

	do
	{
		switch (pIrpsp->Parameters.DeviceIoControl.IoControlCode)
		{
		case CWK_DVC_SEND_STR:			// 接收到发送数据请求
			{
				ASSERT(pBuffer != NULL);
				ASSERT(ulInputlength > 0);
				ASSERT(ulOutputlength == 0);
			}
			break;
		case CWK_DVC_RECV_STR:			// 接收到获取数据请求
			{
				ASSERT(ulInputlength == 0);

				// 创建一个循环，不断从链表中拿是否有节点
				while (TRUE)
				{
					PPROCESSNODE pNode = (PPROCESSNODE)ExInterlockedRemoveHeadList(&g_ListHead, &g_Lock);

					// 如果拿到了节点，则传给应用层，直接想 pBuffer 里面赋值，应用层 DeviceIoControl 就能收到数据
					if (NULL != pNode)
					{
						PPROCESSINFO	pOutputBuffer = (PPROCESSINFO)pBuffer;
						SIZE_T			ulNumberOfBytes = sizeof(PROCESSINFO) +							// 结构体大小
							pNode->pProcessInfo->ulParentProcessLength +								// 父进程路径长度
							pNode->pProcessInfo->ulProcessLength +										// 子进程路径长度
							pNode->pProcessInfo->ulCommandLineLength;									// 命令行参数长度

						KdPrint(("ulNumberOfBytes = %ld, ulParentProcessLength = %ld, ulProcessLength = %ld, ulCommandLineLength = %ld\r\n",
							ulNumberOfBytes,
							pNode->pProcessInfo->ulParentProcessLength,
							pNode->pProcessInfo->ulProcessLength,
							pNode->pProcessInfo->ulCommandLineLength));

						if (NULL != pNode->pProcessInfo)
						{
							if (ulOutputlength >= ulNumberOfBytes)
							{
								RtlCopyBytes(pOutputBuffer, pNode->pProcessInfo, ulNumberOfBytes);
								ExFreePoolWithTag(pNode->pProcessInfo, MEM_TAG);
								ExFreePoolWithTag(pNode, MEM_TAG);
							}
							else
							{
								ExInterlockedInsertHeadList(&g_ListHead, (PLIST_ENTRY)pNode, &g_Lock);
							}
						}

						uLength = (ULONG)ulNumberOfBytes;
						break;
					}
					else
					{
						KeWaitForSingleObject(&g_Event, Executive, KernelMode, 0, 0);
					}
				}
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
	IoCompleteRequest(pIrp, IO_NO_INCREMENT);

	return status;
}
