#include <ntifs.h>
#include <ntstrsafe.h>

#include "NtStructDef.h"
#include "ProcessMgr.h"

LARGE_INTEGER	g_Cookie;		// 用于注册表监控
KSPIN_LOCK		g_Lock;			// 用于链表的锁
KEVENT			g_Event;		// 用于通知的事件
LIST_ENTRY		g_ListHead;		// 链表头

typedef struct _EVENT_DATA_NODE
{
	LIST_ENTRY			stListEntry;
	PREGISTRY_EVENT		pstRegistryEvent;
} EVENT_DATA_NODE, *PEVENT_DATA_NODE;

PEVENT_DATA_NODE InitListNode()
{
	PEVENT_DATA_NODE pNode = NULL;

	pNode = (PEVENT_DATA_NODE)ExAllocatePoolWithTag(NonPagedPool, sizeof(EVENT_DATA_NODE), MEM_TAG);
	if (pNode == NULL)
	{
		return NULL;
	}

	return pNode;
}

NTSTATUS CreateDevice(PDRIVER_OBJECT pDriverObject)
{
	NTSTATUS status;
	PDEVICE_OBJECT pDeviceObject;
	UNICODE_STRING usDeviceName;
	UNICODE_STRING usSymbolicName;

	RtlInitUnicodeString(&usDeviceName, L"\\Device\\_RegistryMonitor");

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
		KdPrint(("Failed to Create device.."));
		return status;
	}

	pDeviceObject->Flags &= ~DO_DEVICE_INITIALIZING;

	RtlInitUnicodeString(&usSymbolicName, L"\\??\\_RegistryMonitor");

	status = IoCreateSymbolicLink(&usSymbolicName, &usDeviceName);
	if (!NT_SUCCESS(status))
	{
		KdPrint(("Failed to create usSymbolicName."));
		IoDeleteSymbolicLink(&usSymbolicName);
		IoDeleteDevice(pDeviceObject);
		return status;
	}

	KdPrint(("Create device success."));
	return STATUS_SUCCESS;
}

NTSTATUS CreateCompleteRoutine(PDEVICE_OBJECT pDeviceObject, PIRP pIrp)
{
	UNREFERENCED_PARAMETER(pDeviceObject);

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
	UNREFERENCED_PARAMETER(pDeviceObject);

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
	UNREFERENCED_PARAMETER(pDeviceObject);

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
	UNREFERENCED_PARAMETER(pDeviceObject);

	NTSTATUS status = STATUS_SUCCESS;

	KdPrint(("Write..."));

	pIrp->IoStatus.Status = status;
	pIrp->IoStatus.Information = 0;
	// 设置 Irp 请求已经处理完成，不要再继续传递
	IoCompleteRequest(pIrp, IO_NO_INCREMENT);

	return status;
}

BOOLEAN GetRegistryObjectCompleteName(
	PUNICODE_STRING pRegistryPath, 
	PUNICODE_STRING pPartialRegistryPath, 
	PVOID pRegistryObject
)
{
	NTSTATUS		status;
	BOOLEAN			bFoundCompleteName = FALSE;
	ULONG			ulReturLength;
	PUNICODE_STRING	pObjectName = NULL;

	if ((!MmIsAddressValid(pRegistryObject)) || (pRegistryObject == NULL))
	{
		return FALSE;
	}

	/* 如果 pPartialRegistryPath 不为 NULL，且符合绝对路径规则，则直接返回给外部使用*/
	if (pPartialRegistryPath != NULL && pPartialRegistryPath->Length >= 4)
	{
		if ((((pPartialRegistryPath->Buffer[0] == '\\') || (pPartialRegistryPath->Buffer[0] == '%')) ||
			((pPartialRegistryPath->Buffer[0] == 'T') && (pPartialRegistryPath->Buffer[1] == 'R') &&
			(pPartialRegistryPath->Buffer[2] == 'Y') && (pPartialRegistryPath->Buffer[3] == '\\'))))
		{
			RtlCopyUnicodeString(pRegistryPath, pPartialRegistryPath);
			bFoundCompleteName = TRUE;
		}
	}

	/* 如果不符合绝对路径规则，则查询 pRegistryObject 对应的注册表对象和 pPartialRegistryPath 拼接 */
	if (!bFoundCompleteName)
	{
		status = ObQueryNameString(pRegistryObject, NULL, 0, &ulReturLength);
		if (status == STATUS_INFO_LENGTH_MISMATCH)
		{
			pObjectName = ExAllocatePoolWithTag(NonPagedPool, ulReturLength, MEM_TAG);
			status = ObQueryNameString(pRegistryObject, (POBJECT_NAME_INFORMATION)pObjectName, ulReturLength, &ulReturLength);
			if (NT_SUCCESS(status))
			{
				/* 将查询到的注册表对象拷贝到传出参数中 */
				RtlCopyUnicodeString(pRegistryPath, pObjectName);

				/* 如果 pPartialRegistryPath 不为 NULL，则是新建的项名 */
				/* 如果为 NULL，则是删除项名，不用拷贝到路径后面。*/
				if (NULL != pPartialRegistryPath)
				{
					status = RtlUnicodeStringCatString(pRegistryPath, L"\\");
					if (!NT_SUCCESS(status))
					{
						KdPrint(("Failed to call RtlUnicodeStringCatString, error code = 0x%08X\r\n", status));
					}
					RtlUnicodeStringCat(pRegistryPath, pPartialRegistryPath);
					if (!NT_SUCCESS(status))
					{
						KdPrint(("Failed to call RtlUnicodeStringCat, error code = 0x%08X\r\n", status));
					}
				}
				bFoundCompleteName = TRUE;
			}
			ExFreePoolWithTag(pObjectName, MEM_TAG);
		}
	}

	return bFoundCompleteName;
}

NTSTATUS RegistryCallback(
	_In_      PVOID CallbackContext,
	_In_opt_  PVOID Argument1,
	_In_opt_  PVOID Argument2
)
{
	NTSTATUS			status = STATUS_SUCCESS;
	ULONG				ulCallbackCtx;
	REG_NOTIFY_CLASS	ulType = (REG_NOTIFY_CLASS)(ULONG_PTR)Argument1;
	UNICODE_STRING		usRegistryPath = { 0 };
	BOOLEAN				bSuccess = FALSE;
	LARGE_INTEGER		unCurrentSystemTime;
	LARGE_INTEGER		unCurrentLocalTime;
	PVOID				pData = NULL;
	ULONG				ulDataSize = 0;
	ULONG				ulKeyValueType = REG_NONE;
	HANDLE				hProcessId = NULL;
	WCHAR				wzProcessPath[MAX_STRING_LENGTH] = { 0 };

	// 时间
	KeQuerySystemTime(&unCurrentSystemTime);
	ExSystemTimeToLocalTime(&unCurrentSystemTime, &unCurrentLocalTime);

	// 进程路径
	hProcessId = PsGetCurrentProcessId();
	GetProcessPathBySectionObject(hProcessId, wzProcessPath);

	ulCallbackCtx = (ULONG)(ULONG_PTR)CallbackContext;

	usRegistryPath.Length = 0;
	usRegistryPath.MaximumLength = 2048 * sizeof(WCHAR);
	usRegistryPath.Buffer = ExAllocatePoolWithTag(NonPagedPool, usRegistryPath.MaximumLength, MEM_TAG);
	if (NULL == usRegistryPath.Buffer)
	{
		KdPrint(("[RegistryCallback] Failed to call ExAllocPollWithTag.\r\n"));
		return status;
	}

	switch (ulType)
	{
	case RegNtPreCreateKeyEx:
		{
			PREG_CREATE_KEY_INFORMATION_V1	pCreateInfo = (PREG_CREATE_KEY_INFORMATION_V1)Argument2;
			UNICODE_STRING					usFilter = { 0 };
			BOOLEAN							bEqual = FALSE;
			WCHAR							*wzFilters[] =
			{
				L"\\REGISTRY\\MACHINE\\SYSTEM\\ControlSet001\\Control\\DeviceClasses",
				L"\\Registry\\Machine\\System\\CurrentControlSet\\Control\\DeviceClasses",
				L"\\REGISTRY\\MACHINE\\SYSTEM\\ControlSet001\\Services\\Tcpip\\Parameters",
				L"\\REGISTRY\\MACHINE\\System\\CurrentControlSet\\Services\\Tcpip\\Parameters"
			};

			bSuccess = GetRegistryObjectCompleteName(
				&usRegistryPath,
				pCreateInfo->CompleteName,
				pCreateInfo->RootObject);
			
			if (bSuccess)
			{
				for (size_t nCount = 0; nCount < sizeof(wzFilters) / sizeof(ULONG_PTR); nCount++)
				{
					RtlInitUnicodeString(&usFilter, wzFilters[nCount]);
					if (RtlEqualUnicodeString(&usRegistryPath, &usFilter, TRUE))
					{
						bEqual = TRUE;
					}
				}
				if (!bEqual)
				{
					//WCHAR wzProcessPath[MAX_STRING_LENGTH] = { 0 };
					//GetProcessPathBySectionObject(PsGetCurrentProcessId(), wzProcessPath);
					//KdPrint(("[RegNtPreCreateKeyEx] [%ws] %wZ\r\n", wzProcessPath, &usRegistryPath));
				}
				else
				{
					usRegistryPath.Length = 0;
				}
			}
		}
		break;
	case RegNtPreDeleteKey:
		{
			PREG_DELETE_KEY_INFORMATION pDeleteKey = (PREG_DELETE_KEY_INFORMATION)Argument2;
			bSuccess = GetRegistryObjectCompleteName(
				&usRegistryPath, 
				NULL, 
				pDeleteKey->Object);
			if (bSuccess)
			{
				// KdPrint(("[RegNtPreDeleteKey]: %wZ\r\n", &usRegistryPath));
			}
		}
		break;
	case RegNtPreSetValueKey:
		{
			PREG_SET_VALUE_KEY_INFORMATION	pSetKeyValue = (PREG_SET_VALUE_KEY_INFORMATION)Argument2;
			bSuccess = GetRegistryObjectCompleteName(
				&usRegistryPath,
				NULL,
				pSetKeyValue->Object);
			if (bSuccess && pSetKeyValue->ValueName->Length > 0 && 
				(REG_SZ == pSetKeyValue->Type || REG_DWORD == pSetKeyValue->Type))
			{
				RtlUnicodeStringCatString(&usRegistryPath, L"\\");
				RtlUnicodeStringCat(&usRegistryPath, pSetKeyValue->ValueName);

				ulKeyValueType = pSetKeyValue->Type;
				ulDataSize = pSetKeyValue->DataSize;
				pData = pSetKeyValue->Data;

				// Data 是以 \0 为结尾的字符串，- sizoef(WCHAR) 是为了不复制 \0
				/*UNICODE_STRING	usData = { 0 };
				usData.Buffer = ExAllocatePoolWithTag(NonPagedPool, pSetKeyValue->DataSize - sizeof(WCHAR), MEM_TAG);
				usData.Length = (USHORT)pSetKeyValue->DataSize - sizeof(WCHAR);
				usData.MaximumLength = MAX_STRING_LENGTH;

				RtlCopyMemory(usData.Buffer, pSetKeyValue->Data, pSetKeyValue->DataSize - sizeof(WCHAR));

				KdPrint(("[RegNtPreSetValueKey]: %wZ, Data = %wZ\r\n", &usRegistryPath, &usData));
				ExFreePoolWithTag(usData.Buffer, MEM_TAG);*/
			}
			else
			{
				usRegistryPath.Length = 0;
			}
		}
		break;
	case RegNtPreDeleteValueKey:
		{
			PREG_DELETE_VALUE_KEY_INFORMATION pDeleteValueKey = (PREG_DELETE_VALUE_KEY_INFORMATION)Argument2;
			bSuccess = GetRegistryObjectCompleteName(&usRegistryPath, NULL, pDeleteValueKey->Object);
			if (bSuccess && (pDeleteValueKey->ValueName->Length > 0))
			{
				RtlUnicodeStringCatString(&usRegistryPath, L"\\");
				RtlUnicodeStringCat(&usRegistryPath, pDeleteValueKey->ValueName);
			}
		}
		break;
	default:
		break;
	}

	// 创建数据链表
	if (usRegistryPath.Length != 0)
	{
		PEVENT_DATA_NODE	pNode = InitListNode();
		ULONG				ulProcessPathLength = (ULONG)(wcslen(wzProcessPath) * sizeof(WCHAR) + sizeof(WCHAR));							// 进程的长度
		ULONG				ulRegistryPathLength = usRegistryPath.Length + sizeof(WCHAR);											// 注册表路径的长度
		SIZE_T				ulNumberOfBytes = sizeof(REGISTRY_EVENT) + ulProcessPathLength + ulRegistryPathLength + ulDataSize;		// 总长度=进程+注册表+数据

		pNode->pstRegistryEvent = ExAllocatePoolWithTag(NonPagedPool, ulNumberOfBytes, MEM_TAG);

		// 给各节点数据赋值
		pNode->pstRegistryEvent->hProcessId					= hProcessId;
		pNode->pstRegistryEvent->enRegistryNotifyClass		= ulType;
		pNode->pstRegistryEvent->ulDataLength				= ulDataSize;
		pNode->pstRegistryEvent->ulProcessPathLength		= ulProcessPathLength;
		pNode->pstRegistryEvent->ulRegistryPathLength		= ulRegistryPathLength;
		pNode->pstRegistryEvent->ulKeyValueType				= ulKeyValueType;
		
		RtlCopyBytes(pNode->pstRegistryEvent->uData,						wzProcessPath, ulProcessPathLength);			// 拷贝进程信息
		RtlCopyBytes(pNode->pstRegistryEvent->uData + ulProcessPathLength,	usRegistryPath.Buffer, usRegistryPath.Length);	// 追加注册表路径信息
		pNode->pstRegistryEvent->uData[ulProcessPathLength + usRegistryPath.Length + 0] = '\0';								// 给注册表路径后面添加 \0 结束符
		pNode->pstRegistryEvent->uData[ulProcessPathLength + usRegistryPath.Length + 1] = '\0';
		RtlCopyBytes(pNode->pstRegistryEvent->uData + ulProcessPathLength + ulRegistryPathLength, pData, ulDataSize);		// 追加修改的数据信息（如果不是修改值，这里可能为空）

		ExInterlockedInsertTailList(&g_ListHead, (PLIST_ENTRY)pNode, &g_Lock);
		KeSetEvent(&g_Event, 0, FALSE);
	}

	if (NULL != usRegistryPath.Buffer)
	{
		ExFreePoolWithTag(usRegistryPath.Buffer, MEM_TAG);
	}

	return status;
}

NTSTATUS DeviceControlCompleteRoutine(PDEVICE_OBJECT pDeviceObject, PIRP pIrp)
{
	UNREFERENCED_PARAMETER(pDeviceObject);

	NTSTATUS			status = STATUS_SUCCESS;
	PIO_STACK_LOCATION	pIrpsp = IoGetCurrentIrpStackLocation(pIrp);
	ULONG				uLength = 0;

	PVOID				pBuffer = pIrp->AssociatedIrp.SystemBuffer;
	ULONG				ulInputlength = pIrpsp->Parameters.DeviceIoControl.InputBufferLength;
	ULONG				ulOutputlength = pIrpsp->Parameters.DeviceIoControl.OutputBufferLength;

	do
	{
		switch (pIrpsp->Parameters.DeviceIoControl.IoControlCode)
		{
		case CWK_DVC_SEND_STR:
			{
				ASSERT(pBuffer != NULL);
				ASSERT(ulInputlength > 0);
				ASSERT(ulOutputlength == 0);
			}
			break;
		case CWK_DVC_RECV_STR:
			{
				ASSERT(ulInputlength == 0);

				while (TRUE)
				{
					PEVENT_DATA_NODE pNode = (PEVENT_DATA_NODE)ExInterlockedRemoveHeadList(&g_ListHead, &g_Lock);
					if (NULL != pNode)
					{
						PREGISTRY_EVENT pOutputBuffer = (PREGISTRY_EVENT)pBuffer;
						SIZE_T			ulNumberOfBytes = sizeof(REGISTRY_EVENT) +						// 结构体大小
							pNode->pstRegistryEvent->ulProcessPathLength +								// 进程路径长度
							pNode->pstRegistryEvent->ulRegistryPathLength +								// 路径长度
							pNode->pstRegistryEvent->ulDataLength;										// 数据长度

						if (NULL != pNode->pstRegistryEvent)
						{
							if (ulOutputlength >= ulNumberOfBytes)
							{
								RtlCopyBytes(pOutputBuffer, pNode->pstRegistryEvent, ulNumberOfBytes);
								ExFreePoolWithTag(pNode->pstRegistryEvent, MEM_TAG);
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

VOID DriverUnload(IN PDRIVER_OBJECT pDriverObject)
{
	UNICODE_STRING usSymbolicName;
	RtlInitUnicodeString(&usSymbolicName, L"\\??\\_RegistryMonitor");

	// 删除符号链接和设备对象
	if (NULL != pDriverObject->DeviceObject)
	{
		IoDeleteSymbolicLink(&usSymbolicName);
		IoDeleteDevice(pDriverObject->DeviceObject);
		KdPrint(("Unload driver success.."));
	}

	// 释放链表所有内存
	while (TRUE)
	{
		// 从链表中取出一个节点
		PEVENT_DATA_NODE pNode = (PEVENT_DATA_NODE)ExInterlockedRemoveHeadList(&g_ListHead, &g_Lock);
		if (NULL != pNode)
		{
			if (NULL != pNode->pstRegistryEvent)
			{
				ExFreePoolWithTag(pNode->pstRegistryEvent, MEM_TAG);
			}
			ExFreePoolWithTag(pNode, MEM_TAG);
		}
		else
		{
			break;
		}
	};

	CmUnRegisterCallback(g_Cookie);
	KdPrint(("%wZ Unload.\r\n", pDriverObject->DriverName));
}

NTSTATUS DriverEntry(IN PDRIVER_OBJECT pDriverObject, IN PUNICODE_STRING pRegistryPath)
{
	KdPrint(("%wZ DriverEntry.\r\n", pRegistryPath));

	// 初始化事件、锁、链表头
	KeInitializeEvent(&g_Event, SynchronizationEvent, TRUE);
	KeInitializeSpinLock(&g_Lock);
	InitializeListHead(&g_ListHead);

	// 开始注册表监控
	CmRegisterCallback(RegistryCallback, 0, &g_Cookie);

	// 创建设备和符号链接
	CreateDevice(pDriverObject);
	// IoControl
	pDriverObject->MajorFunction[IRP_MJ_CREATE] = CreateCompleteRoutine;
	pDriverObject->MajorFunction[IRP_MJ_CLOSE] = CloseCompleteRoutine;
	pDriverObject->MajorFunction[IRP_MJ_READ] = ReadCompleteRoutine;
	pDriverObject->MajorFunction[IRP_MJ_WRITE] = WriteCompleteRoutine;
	pDriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] = DeviceControlCompleteRoutine;

	pDriverObject->DriverUnload = DriverUnload;

	return STATUS_SUCCESS;
}