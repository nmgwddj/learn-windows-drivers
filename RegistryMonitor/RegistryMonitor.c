/*
*	PROJECT: Capture
*	FILE: CaptureProcessMonitor.c
*	AUTHORS: Ramon Steenson (rsteenson@gmail.com) & Christian Seifert (christian.seifert@gmail.com)
*
*	Developed by Victoria University of Wellington and the New Zealand Honeynet Alliance
*
*	This file is part of Capture.
*
*	Capture is free software; you can redistribute it and/or modify
*	it under the terms of the GNU General Public License as published by
*	the Free Software Foundation; either version 2 of the License, or
*  (at your option) any later version.
*
*  Capture is distributed in the hope that it will be useful,
*  but WITHOUT ANY WARRANTY; without even the implied warranty of
*  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*  GNU General Public License for more details.
*
*  You should have received a copy of the GNU General Public License
*  along with Capture; if not, write to the Free Software
*  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

/*
Capture Process Monitor - Kernel Driver
Based on code from James M. Finnegan - http://www.microsoft.com/msj/0799/nerd/nerd0799.aspx

Driver for monitoring process on Windows XP (should work on all NT systems)

By Ramon Steenson (rsteenson@gmail.com)
*/
#define NTDDI_WINXPSP2                      0x05010200
#define OSVERSION_MASK      0xFFFF0000
#define SPVERSION_MASK      0x0000FF00
#define SUBVERSION_MASK     0x000000FF


//
// macros to extract various version fields from the NTDDI version
//
#define OSVER(Version)  ((Version) & OSVERSION_MASK)
#define SPVER(Version)  (((Version) & SPVERSION_MASK) >> 8)
#define SUBVER(Version) (((Version) & SUBVERSION_MASK) )
//#define NTDDI_VERSION   NTDDI_WINXPSP2

#include <ntifs.h>
#include <wdm.h>
#include <ntstrsafe.h>

#define FILE_DEVICE_UNKNOWN	0x00000022
#define IOCTL_UNKNOWN_BASE	FILE_DEVICE_UNKNOWN
#define IOCTL_CAPTURE_GET_REGEVENTS	CTL_CODE(FILE_DEVICE_UNKNOWN, 0x803, METHOD_NEITHER,FILE_READ_DATA | FILE_WRITE_DATA) 
#define USERSPACE_CONNECTION_TIMEOUT 10
#define REGISTRY_POOL_TAG 'pRE'

typedef unsigned int UINT;
typedef char * PCHAR;
typedef PVOID POBJECT;

/* Registry event */
typedef struct  _REGISTRY_EVENT {
	REG_NOTIFY_CLASS eventType;
	TIME_FIELDS time;
	HANDLE processId;
	ULONG dataType;
	ULONG dataLengthB;
	ULONG registryPathLengthB;
	/* Contains path and optionally data */
	UCHAR registryData[];
} REGISTRY_EVENT, *PREGISTRY_EVENT;

/* Storage for registry event to be put into a linked list */
typedef struct  _REGISTRY_EVENT_PACKET {
	LIST_ENTRY     Link;
	PREGISTRY_EVENT pRegistryEvent;
} REGISTRY_EVENT_PACKET, *PREGISTRY_EVENT_PACKET;

/* Context stuff */
typedef struct _CAPTURE_REGISTRY_MANAGER
{
	PDEVICE_OBJECT deviceObject;
	BOOLEAN bReady;
	LARGE_INTEGER  registryCallbackCookie;
	LIST_ENTRY lQueuedRegistryEvents;
	KTIMER connectionCheckerTimer;
	KDPC connectionCheckerFunction;
	KSPIN_LOCK lQueuedRegistryEventsSpinLock;
	ULONG lastContactTime;
} CAPTURE_REGISTRY_MANAGER, *PCAPTURE_REGISTRY_MANAGER;

/* Methods */
NTSTATUS KDispatchIoctl(IN PDEVICE_OBJECT DeviceObject, IN PIRP Irp);
NTSTATUS KDispatchCreateClose(IN PDEVICE_OBJECT DeviceObject, IN PIRP Irp);
VOID UnloadDriver(PDRIVER_OBJECT DriverObject);
NTSTATUS RegistryCallback(IN PVOID CallbackContext, IN PVOID  Argument1, IN PVOID  Argument2);
//BOOLEAN GetRegistryObjectCompleteName(PREGISTRY_EVENT pRegistryEvent, PUNICODE_STRING pPartialObjectName, PVOID pRegistryObject);
//VOID QueueRegistryEvent(PREGISTRY_EVENT pRegistryEvent);
VOID UpdateLastContactTime();
ULONG GetCurrentTime();
NTSTATUS HandleIoctlGetRegEvents(IN PDEVICE_OBJECT DeviceObject, PIRP Irp,
	PIO_STACK_LOCATION pIoStackIrp, UINT *pdwDataWritten);
VOID ConnectionChecker(
	IN struct _KDPC  *Dpc,
	IN PVOID  DeferredContext,
	IN PVOID  SystemArgument1,
	IN PVOID  SystemArgument2
);

/* Global values */
PDEVICE_OBJECT gpDeviceObject;


/*	Main entry point into the driver, is called when the driver is loaded */
NTSTATUS DriverEntry(
	IN PDRIVER_OBJECT DriverObject,
	IN PUNICODE_STRING RegistryPath
)
{
	NTSTATUS status;
	UNICODE_STRING uszDriverString;
	UNICODE_STRING uszDeviceString;
	LARGE_INTEGER registryEventsTimeout;
	PDEVICE_OBJECT pDeviceObject;
	PCAPTURE_REGISTRY_MANAGER pRegistryManager;

	// Point uszDriverString at the driver name
	RtlInitUnicodeString(&uszDriverString, L"\\Device\\CaptureRegistryMonitor");

	// Create and initialize device object
	status = IoCreateDevice(
		DriverObject,
		sizeof(CAPTURE_REGISTRY_MANAGER),
		&uszDriverString,
		FILE_DEVICE_UNKNOWN,
		0,
		FALSE,
		&pDeviceObject
	);
	if (!NT_SUCCESS(status))
	{
		DbgPrint("CaptureRegistryMonitor: ERROR IoCreateDevice ->  \\Device\\CaptureRegistryMonitor - %08x\n", status);
		return status;
	}

	/* Set global device object to newly created object */
	gpDeviceObject = pDeviceObject;

	/* Get the registr manager from the extension of the device */
	pRegistryManager = gpDeviceObject->DeviceExtension;

	pRegistryManager->bReady = FALSE;

	/* Point uszDeviceString at the device name */
	RtlInitUnicodeString(&uszDeviceString, L"\\DosDevices\\CaptureRegistryMonitor");
	/* Create symbolic link to the user-visible name */
	status = IoCreateSymbolicLink(&uszDeviceString, &uszDriverString);

	if (!NT_SUCCESS(status))
	{
		DbgPrint("CaptureRegistryMonitor: ERROR IoCreateSymbolicLink ->  \\DosDevices\\CaptureRegistryMonitor - %08x\n", status);
		IoDeleteDevice(pDeviceObject);
		return status;
	}

	KeInitializeSpinLock(&pRegistryManager->lQueuedRegistryEventsSpinLock);
	InitializeListHead(&pRegistryManager->lQueuedRegistryEvents);

	// Load structure to point to IRP handlers
	DriverObject->DriverUnload = UnloadDriver;
	DriverObject->MajorFunction[IRP_MJ_CREATE] = KDispatchCreateClose;
	DriverObject->MajorFunction[IRP_MJ_CLOSE] = KDispatchCreateClose;
	DriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] = KDispatchIoctl;

	status = CmRegisterCallback(RegistryCallback, pRegistryManager, &pRegistryManager->registryCallbackCookie);

	if (!NT_SUCCESS(status))
	{
		DbgPrint("CaptureRegistryMonitor: ERROR CmRegisterCallback - %08x\n", status);
		return status;
	}

	UpdateLastContactTime();

	/* Create a DPC routine so that it can be called periodically */
	KeInitializeDpc(&pRegistryManager->connectionCheckerFunction,
		(PKDEFERRED_ROUTINE)ConnectionChecker, pRegistryManager);
	KeInitializeTimer(&pRegistryManager->connectionCheckerTimer);
	registryEventsTimeout.QuadPart = 0;

	/* Set the ConnectionChecker routine to be called every so often */
	KeSetTimerEx(&pRegistryManager->connectionCheckerTimer,
		registryEventsTimeout,
		(USERSPACE_CONNECTION_TIMEOUT + (USERSPACE_CONNECTION_TIMEOUT / 2)) * 1000,
		&pRegistryManager->connectionCheckerFunction);

	pRegistryManager->bReady = TRUE;

	DbgPrint("CaptureRegistryMonitor: Successfully Loaded\n");

	/* Return success */
	return STATUS_SUCCESS;
}

/* Checks to see if the registry monitor has received an IOCTL from a userspace
program in a while. If it hasn't then all old queued registry events are
cleared. This is called periodically when the driver is loaded */
VOID ConnectionChecker(
	IN struct _KDPC  *Dpc,
	IN PVOID  DeferredContext,
	IN PVOID  SystemArgument1,
	IN PVOID  SystemArgument2
)
{
	PCAPTURE_REGISTRY_MANAGER pRegistryManager = (PCAPTURE_REGISTRY_MANAGER)DeferredContext;

	if ((GetCurrentTime() - pRegistryManager->lastContactTime) > (USERSPACE_CONNECTION_TIMEOUT + (USERSPACE_CONNECTION_TIMEOUT / 2)))
	{
		DbgPrint("CaptureRegistryMonitor: WARNING Userspace IOCTL timeout, clearing old queued registry events\n");
		while (!IsListEmpty(&pRegistryManager->lQueuedRegistryEvents))
		{
			PLIST_ENTRY head = ExInterlockedRemoveHeadList(&pRegistryManager->lQueuedRegistryEvents, &pRegistryManager->lQueuedRegistryEventsSpinLock);
			PREGISTRY_EVENT_PACKET pRegistryEventPacket = CONTAINING_RECORD(head, REGISTRY_EVENT_PACKET, Link);
			ExFreePoolWithTag(pRegistryEventPacket->pRegistryEvent, REGISTRY_POOL_TAG);
			ExFreePoolWithTag(pRegistryEventPacket, REGISTRY_POOL_TAG);
		}
	}
}


NTSTATUS KDispatchCreateClose(IN PDEVICE_OBJECT DeviceObject, IN PIRP Irp)
{
	Irp->IoStatus.Status = STATUS_SUCCESS;
	Irp->IoStatus.Information = 0;

	IoCompleteRequest(Irp, IO_NO_INCREMENT);
	return STATUS_SUCCESS;
}

NTSTATUS HandleIoctlGetRegEvents(IN PDEVICE_OBJECT DeviceObject, PIRP Irp,
	PIO_STACK_LOCATION pIoStackIrp, UINT *pdwDataWritten)
{
	NTSTATUS status = STATUS_UNSUCCESSFUL;
	PCHAR pOutputBuffer = Irp->UserBuffer;
	UINT dwOutputBufferSize = pIoStackIrp->Parameters.DeviceIoControl.OutputBufferLength;
	PCAPTURE_REGISTRY_MANAGER pRegistryManager;

	/* Get the registry manager from the device extension */
	pRegistryManager = gpDeviceObject->DeviceExtension;
	*pdwDataWritten = 0;

	/* Check we are allowed to write into the buffer passed from the user space program */
	if (pOutputBuffer)
	{
		try {
			ProbeForWrite(pOutputBuffer,
				pIoStackIrp->Parameters.DeviceIoControl.OutputBufferLength,
				__alignof (REGISTRY_EVENT));
			if (pIoStackIrp->Parameters.DeviceIoControl.OutputBufferLength >= sizeof(REGISTRY_EVENT))
			{
				PLIST_ENTRY pRegistryListHead;
				PREGISTRY_EVENT_PACKET pRegistryPacket;
				UINT bufferSpace = dwOutputBufferSize;
				UINT bufferSpaceUsed = 0;

				/* Fill the buffer passed from userspace with registry events */
				/* 从链表中取出节点填充到用户空间提供的 buffer 中 */
				while (!IsListEmpty(&pRegistryManager->lQueuedRegistryEvents) &&		// 如果链表不为空
					(bufferSpaceUsed < bufferSpace) &&									// 用户空间够大
					((bufferSpace - bufferSpaceUsed) >= sizeof(REGISTRY_EVENT)))		// 用户各处的空间大于等于一个链表的节点大小
				{
					UINT registryEventSize = 0;
					pRegistryListHead = ExInterlockedRemoveHeadList(&pRegistryManager->lQueuedRegistryEvents, &pRegistryManager->lQueuedRegistryEventsSpinLock);
					pRegistryPacket = CONTAINING_RECORD(pRegistryListHead, REGISTRY_EVENT_PACKET, Link);

					registryEventSize = sizeof(REGISTRY_EVENT) + pRegistryPacket->pRegistryEvent->registryPathLengthB + pRegistryPacket->pRegistryEvent->dataLengthB;
					if ((bufferSpace - bufferSpaceUsed) >= registryEventSize)
					{
						//DbgPrint("Sending-%i: %ls\n",bufferSpaceUsed, pRegistryPacket->pRegistryEvent->registryPath);
						RtlCopyMemory(pOutputBuffer + bufferSpaceUsed, pRegistryPacket->pRegistryEvent, registryEventSize);
						bufferSpaceUsed += registryEventSize;

						ExFreePoolWithTag(pRegistryPacket->pRegistryEvent, REGISTRY_POOL_TAG);
						ExFreePoolWithTag(pRegistryPacket, REGISTRY_POOL_TAG);
					}
					else {
						ExInterlockedInsertHeadList(&pRegistryManager->lQueuedRegistryEvents, &pRegistryPacket->Link, &pRegistryManager->lQueuedRegistryEventsSpinLock);
						break;
					}
				}

				/* Return the amount of space that is occupied with registry events */
				*pdwDataWritten = bufferSpaceUsed;
				status = STATUS_SUCCESS;
			}
			else {
				*pdwDataWritten = sizeof(REGISTRY_EVENT);
				status = STATUS_BUFFER_TOO_SMALL;
			}
		} except(EXCEPTION_EXECUTE_HANDLER) {
			status = GetExceptionCode();
			DbgPrint("CaptureRegistryMonitor: EXCEPTION IOCTL_CAPTURE_GET_REGEVENTS - %i\n", status);
		}
	}
	return status;
}

NTSTATUS KDispatchIoctl(IN PDEVICE_OBJECT DeviceObject, IN PIRP Irp)
{
	NTSTATUS status = STATUS_UNSUCCESSFUL;
	PIO_STACK_LOCATION irpStack = IoGetCurrentIrpStackLocation(Irp);
	UINT dwDataWritten = 0;

	switch (irpStack->Parameters.DeviceIoControl.IoControlCode)
	{
	case IOCTL_CAPTURE_GET_REGEVENTS:
		UpdateLastContactTime();
		status = HandleIoctlGetRegEvents(DeviceObject, Irp, irpStack, &dwDataWritten);
		break;
	default:
		break;
	}

	Irp->IoStatus.Status = status;

	// Set # of bytes to copy back to user-mode...
	if (NT_SUCCESS(status))
		Irp->IoStatus.Information = dwDataWritten;
	else
		Irp->IoStatus.Information = 0;

	IoCompleteRequest(Irp, IO_NO_INCREMENT);

	return status;
}

VOID UpdateLastContactTime()
{
	PCAPTURE_REGISTRY_MANAGER pRegistryManager;

	/* Get the process manager from the device extension */
	pRegistryManager = gpDeviceObject->DeviceExtension;

	pRegistryManager->lastContactTime = GetCurrentTime();
}

ULONG GetCurrentTime()
{
	LARGE_INTEGER currentSystemTime;
	LARGE_INTEGER currentLocalTime;
	ULONG time;

	KeQuerySystemTime(&currentSystemTime);
	ExSystemTimeToLocalTime(&currentSystemTime, &currentLocalTime);
	RtlTimeToSecondsSince1970(&currentLocalTime, &time);
	return time;
}

BOOLEAN GetRegistryObjectCompleteName(PUNICODE_STRING pRegistryPath, PUNICODE_STRING pPartialRegistryPath, PVOID pRegistryObject)
{
	PCAPTURE_REGISTRY_MANAGER pRegistryManager;
	BOOLEAN foundCompleteName = FALSE;
	BOOLEAN partial = FALSE;

	/* Get the process manager from the device extension */
	pRegistryManager = gpDeviceObject->DeviceExtension;

	/* Check to see if everything is valid */
	/* We sometimes see a partial registry object name which is actually complete
	however if fails one of these checks for some reason. Not sure whether to report
	this registry event */
	if ((!MmIsAddressValid(pRegistryObject)) ||
		(pRegistryObject == NULL))
	{
		return FALSE;
	}

	/* Check to see if the partial name is really the complete name */
	if (pPartialRegistryPath != NULL)
	{
		if ((((pPartialRegistryPath->Buffer[0] == '\\') || (pPartialRegistryPath->Buffer[0] == '%')) ||
			((pPartialRegistryPath->Buffer[0] == 'T') && (pPartialRegistryPath->Buffer[1] == 'R') && (pPartialRegistryPath->Buffer[2] == 'Y') && (pPartialRegistryPath->Buffer[3] == '\\'))))
		{
			RtlUnicodeStringCopy(pRegistryPath, pPartialRegistryPath);
			partial = TRUE;
			foundCompleteName = TRUE;
		}
	}

	if (!foundCompleteName)
	{
		/* Query the object manager in the kernel for the complete name */
		NTSTATUS status;
		ULONG returnedLength;
		PUNICODE_STRING	pObjectName = NULL;

		status = ObQueryNameString(pRegistryObject, (POBJECT_NAME_INFORMATION)pObjectName, 0, &returnedLength);
		if (status == STATUS_INFO_LENGTH_MISMATCH)
		{
			pObjectName = ExAllocatePoolWithTag(NonPagedPool, returnedLength, REGISTRY_POOL_TAG);
			status = ObQueryNameString(pRegistryObject, (POBJECT_NAME_INFORMATION)pObjectName, returnedLength, &returnedLength);
			if (NT_SUCCESS(status))
			{
				RtlUnicodeStringCopy(pRegistryPath, pObjectName);
				foundCompleteName = TRUE;
			}
			ExFreePoolWithTag(pObjectName, REGISTRY_POOL_TAG);
		}
	}
	//ASSERT(foundCompleteName == TRUE);
	return foundCompleteName;
}

BOOLEAN QueueRegistryEvent(PREGISTRY_EVENT pRegistryEvent)
{
	PCAPTURE_REGISTRY_MANAGER pRegistryManager;

	/* Get the registry manager from the device extension */
	pRegistryManager = gpDeviceObject->DeviceExtension;

	/* Check the last contact time of the user space program before queuing */
	if ((GetCurrentTime() - pRegistryManager->lastContactTime) <= USERSPACE_CONNECTION_TIMEOUT)
	{
		PREGISTRY_EVENT_PACKET pRegistryEventPacket = ExAllocatePoolWithTag(NonPagedPool, sizeof(REGISTRY_EVENT_PACKET), REGISTRY_POOL_TAG);
		pRegistryEventPacket->pRegistryEvent = pRegistryEvent;

		/* Queue registry event */
		ExInterlockedInsertTailList(&pRegistryManager->lQueuedRegistryEvents, &pRegistryEventPacket->Link, &pRegistryManager->lQueuedRegistryEventsSpinLock);
		return TRUE;
	}
	return FALSE;
}

NTSTATUS RegistryCallback(IN PVOID CallbackContext,
	IN PVOID  Argument1,
	IN PVOID  Argument2)
{
	//REGISTRY_EVENT registryEvent;
	BOOLEAN registryEventIsValid = FALSE;
	BOOLEAN exception = FALSE;
	LARGE_INTEGER CurrentSystemTime;
	LARGE_INTEGER CurrentLocalTime;
	TIME_FIELDS TimeFields;
	int type;
	UNICODE_STRING registryPath;
	UCHAR* registryData = NULL;
	ULONG registryDataLength = 0;
	ULONG registryDataType = 0;

	/* Allocate a large 64kb string ... maximum path name allowed in windows */
	registryPath.Length = 0;
	registryPath.MaximumLength = NTSTRSAFE_UNICODE_STRING_MAX_CCH * sizeof(WCHAR);
	registryPath.Buffer = ExAllocatePoolWithTag(NonPagedPool, registryPath.MaximumLength, REGISTRY_POOL_TAG);

	if (registryPath.Buffer == NULL)
	{
		return STATUS_SUCCESS;
	}

	/* Put the time this event occured into the registry event */
	KeQuerySystemTime(&CurrentSystemTime);
	ExSystemTimeToLocalTime(&CurrentSystemTime, &CurrentLocalTime);


	//registryEvent.processId = PsGetCurrentProcessId();
	//registryEvent.eventType = (REG_NOTIFY_CLASS)Argument1;
	type = (REG_NOTIFY_CLASS)(ULONG_PTR)Argument1;
	try
	{
		/* Large switch statement for all registry events ... fairly easy to understand */
		switch (type)
		{
		case RegNtPostCreateKey:
		{
			PREG_POST_CREATE_KEY_INFORMATION createKey = (PREG_POST_CREATE_KEY_INFORMATION)Argument2;
			if (NT_SUCCESS(createKey->Status))
			{
				PVOID* registryObject = createKey->Object;
				registryEventIsValid = GetRegistryObjectCompleteName(&registryPath, createKey->CompleteName, *registryObject);
			}
			break;
		}
		case RegNtPostOpenKey:
		{
			PREG_POST_OPEN_KEY_INFORMATION openKey = (PREG_POST_OPEN_KEY_INFORMATION)Argument2;
			if (NT_SUCCESS(openKey->Status))
			{
				PVOID* registryObject = openKey->Object;
				registryEventIsValid = GetRegistryObjectCompleteName(&registryPath, openKey->CompleteName, *registryObject);
			}
			break;
		}
		case RegNtPreDeleteKey:
		{
			PREG_DELETE_KEY_INFORMATION deleteKey = (PREG_DELETE_KEY_INFORMATION)Argument2;
			registryEventIsValid = GetRegistryObjectCompleteName(&registryPath, NULL, deleteKey->Object);
			break;
		}
		case RegNtDeleteValueKey:
		{
			PREG_DELETE_VALUE_KEY_INFORMATION deleteValueKey = (PREG_DELETE_VALUE_KEY_INFORMATION)Argument2;
			registryEventIsValid = GetRegistryObjectCompleteName(&registryPath, NULL, deleteValueKey->Object);
			if ((registryEventIsValid) && (deleteValueKey->ValueName->Length > 0))
			{
				RtlUnicodeStringCatString(&registryPath, L"\\");
				RtlUnicodeStringCat(&registryPath, deleteValueKey->ValueName);
			}
			break;
		}
		case RegNtPreSetValueKey:
		{
			PREG_SET_VALUE_KEY_INFORMATION setValueKey = (PREG_SET_VALUE_KEY_INFORMATION)Argument2;
			registryEventIsValid = GetRegistryObjectCompleteName(&registryPath, NULL, setValueKey->Object);
			if ((registryEventIsValid) && (setValueKey->ValueName->Length > 0))
			{
				registryDataType = setValueKey->Type;
				registryDataLength = setValueKey->DataSize;
				registryData = ExAllocatePoolWithTag(NonPagedPool, registryDataLength, REGISTRY_POOL_TAG);
				if (registryData != NULL)
				{
					RtlCopyBytes(registryData, setValueKey->Data, setValueKey->DataSize);
				}
				else {
					DbgPrint("CaptureRegistryMonitor: ERROR can't allocate memory for setvalue data\n");
				}
				RtlUnicodeStringCatString(&registryPath, L"\\");
				RtlUnicodeStringCat(&registryPath, setValueKey->ValueName);
			}
			break;
		}
		case RegNtEnumerateKey:
		{
			PREG_ENUMERATE_KEY_INFORMATION enumerateKey = (PREG_ENUMERATE_KEY_INFORMATION)Argument2;
			registryDataType = enumerateKey->KeyInformationClass;
			registryEventIsValid = GetRegistryObjectCompleteName(&registryPath, NULL, enumerateKey->Object);
			break;
		}
		case RegNtEnumerateValueKey:
		{
			PREG_ENUMERATE_VALUE_KEY_INFORMATION enumerateValueKey = (PREG_ENUMERATE_VALUE_KEY_INFORMATION)Argument2;
			registryDataType = enumerateValueKey->KeyValueInformationClass;
			registryEventIsValid = GetRegistryObjectCompleteName(&registryPath, NULL, enumerateValueKey->Object);
			break;
		}
		case RegNtQueryKey:
		{
			PREG_QUERY_KEY_INFORMATION queryKey = (PREG_QUERY_KEY_INFORMATION)Argument2;
			registryDataType = queryKey->KeyInformationClass;
			registryEventIsValid = GetRegistryObjectCompleteName(&registryPath, NULL, queryKey->Object);
			break;
		}
		case RegNtQueryValueKey:
		{
			PREG_QUERY_VALUE_KEY_INFORMATION queryValueKey = (PREG_QUERY_VALUE_KEY_INFORMATION)Argument2;
			registryEventIsValid = GetRegistryObjectCompleteName(&registryPath, NULL, queryValueKey->Object);
			if ((registryEventIsValid) && (queryValueKey->ValueName->Length > 0))
			{
				registryDataType = queryValueKey->KeyValueInformationClass;
				RtlUnicodeStringCatString(&registryPath, L"\\");
				RtlUnicodeStringCat(&registryPath, queryValueKey->ValueName);
			}
			break;
		}
		case RegNtKeyHandleClose:
		{
			PREG_KEY_HANDLE_CLOSE_INFORMATION closeKey = (PREG_KEY_HANDLE_CLOSE_INFORMATION)Argument2;
			registryEventIsValid = GetRegistryObjectCompleteName(&registryPath, NULL, closeKey->Object);
			break;
		}
		default:
			break;
		}
	} except(EXCEPTION_EXECUTE_HANDLER) {
		/* Do nothing if an exception occured ... event won't be queued */
		registryEventIsValid = FALSE;
		exception = TRUE;
	}

	if (registryEventIsValid)
	{
		PREGISTRY_EVENT pRegistryEvent;
		UINT eventSize = sizeof(REGISTRY_EVENT) + registryPath.Length + (sizeof(WCHAR)) + registryDataLength;
		pRegistryEvent = ExAllocatePoolWithTag(NonPagedPool, eventSize, REGISTRY_POOL_TAG);

		if (pRegistryEvent != NULL)
		{
			pRegistryEvent->registryPathLengthB = registryPath.Length + sizeof(WCHAR);
			// 查询或枚举注册表时给出的获取类型，也标记了修改注册表键时的数据类型
			pRegistryEvent->dataType = registryDataType;
			// 修改注册表时，修改数据的长度
			pRegistryEvent->dataLengthB = registryDataLength;
			//RtlStringCbCopyUnicodeString(pRegistryEvent->registryPath, pRegistryEvent->registryPathLength, &registryPath);
			// 拷贝新建、修改、删除、查询等注册表路径，并设置末尾以 \0 结尾，WCHAR 要有两个 \0
			RtlCopyBytes(pRegistryEvent->registryData, registryPath.Buffer, registryPath.Length);
			pRegistryEvent->registryData[registryPath.Length] = '\0';
			pRegistryEvent->registryData[registryPath.Length + 1] = '\0';
			// 在 registryData 加上上面路径长度的位置后面追加拷贝修改键值时的数据内容
			RtlCopyBytes(pRegistryEvent->registryData + pRegistryEvent->registryPathLengthB, registryData, registryDataLength);

			// 释放修改的数据的临时储存区
			if (registryData != NULL)
			{
				ExFreePoolWithTag(registryData, REGISTRY_POOL_TAG);
			}

			// 记录当前操作的进程 ID
			pRegistryEvent->processId = PsGetCurrentProcessId();
			// 记录时间
			RtlTimeToTimeFields(&CurrentLocalTime, &pRegistryEvent->time);
			// 记录注册表操作的类型
			pRegistryEvent->eventType = (REG_NOTIFY_CLASS)(ULONG_PTR)Argument1;
			if (!QueueRegistryEvent(pRegistryEvent))
			{
				ExFreePoolWithTag(pRegistryEvent, REGISTRY_POOL_TAG);
			}
		}
	}
	if (registryPath.Buffer != NULL)
	{
		ExFreePoolWithTag(registryPath.Buffer, REGISTRY_POOL_TAG);
	}
	/* Always return a success ... we aren't doing any filtering, just monitoring */
	return STATUS_SUCCESS;
}

VOID UnloadDriver(IN PDRIVER_OBJECT DriverObject)
{
	UNICODE_STRING  uszDeviceString;
	NTSTATUS        ntStatus;
	PCAPTURE_REGISTRY_MANAGER pRegistryManager;

	/* Get the registry manager from the device extension */
	pRegistryManager = gpDeviceObject->DeviceExtension;



	if (pRegistryManager->bReady == TRUE)
	{
		KeCancelTimer(&pRegistryManager->connectionCheckerTimer);
		CmUnRegisterCallback(pRegistryManager->registryCallbackCookie);
		pRegistryManager->bReady = FALSE;
	}

	while (!IsListEmpty(&pRegistryManager->lQueuedRegistryEvents))
	{
		PLIST_ENTRY head = ExInterlockedRemoveHeadList(&pRegistryManager->lQueuedRegistryEvents, &pRegistryManager->lQueuedRegistryEventsSpinLock);
		PREGISTRY_EVENT_PACKET pRegistryEventPacket = CONTAINING_RECORD(head, REGISTRY_EVENT_PACKET, Link);
		ExFreePoolWithTag(pRegistryEventPacket->pRegistryEvent, REGISTRY_POOL_TAG);
		ExFreePoolWithTag(pRegistryEventPacket, REGISTRY_POOL_TAG);
	}

	RtlInitUnicodeString(&uszDeviceString, L"\\DosDevices\\CaptureRegistryMonitor");
	IoDeleteSymbolicLink(&uszDeviceString);
	if (DriverObject->DeviceObject != NULL)
	{
		IoDeleteDevice(DriverObject->DeviceObject);
	}
}
