#include <ntifs.h>
#include <Ntstrsafe.h>
#include "NtStructDef.h"

LARGE_INTEGER g_Cookie;

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
	if (pPartialRegistryPath != NULL)
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
		/* Query the object manager in the kernel for the complete name */
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
					RtlUnicodeStringCatString(pRegistryPath, L"\\");
					RtlUnicodeStringCat(pRegistryPath, pPartialRegistryPath);
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
	UNICODE_STRING		usRegisterPath = { 0 };
	BOOLEAN				bSuccess = FALSE;

	ulCallbackCtx = (ULONG)(ULONG_PTR)CallbackContext;

	usRegisterPath.Length = 0;
	usRegisterPath.MaximumLength = 2048 * sizeof(WCHAR);
	usRegisterPath.Buffer = ExAllocatePoolWithTag(NonPagedPool, usRegisterPath.MaximumLength, MEM_TAG);
	if (NULL == usRegisterPath.Buffer)
	{
		KdPrint(("[RegNtPreCreateKey] Failed to call ExAllocPollWithTag.\r\n"));
		return STATUS_SUCCESS;
	}

	switch (ulType)
	{
	case RegNtPreCreateKeyEx:
		{
			bSuccess = GetRegistryObjectCompleteName(
				&usRegisterPath, 
				((PREG_CREATE_KEY_INFORMATION)Argument2)->CompleteName, 
				((PREG_CREATE_KEY_INFORMATION)Argument2)->RootObject);
			if (bSuccess)
			{
				KdPrint(("[RegNtPreCreateKeyEx] %wZ\r\n", &usRegisterPath));
			}
		}
		break;
	case RegNtPreDeleteKey:
		{
			bSuccess = GetRegistryObjectCompleteName(
				&usRegisterPath, 
				NULL, 
				((PREG_DELETE_KEY_INFORMATION)Argument2)->Object);
			if (bSuccess)
			{
				KdPrint(("[RegNtPreDeleteKey]: %wZ\r\n", &usRegisterPath));
			}
		}
		break;
	}

	if (NULL != usRegisterPath.Buffer)
	{
		ExFreePoolWithTag(usRegisterPath.Buffer, MEM_TAG);
	}

	return status;
}

VOID DriverUnload(IN PDRIVER_OBJECT pDriverObject)
{
	KdPrint(("%wZ Unload.\r\n", pDriverObject->DriverName));
	CmUnRegisterCallback(g_Cookie);
}

NTSTATUS DriverEntry(IN PDRIVER_OBJECT pDriverObject, IN PUNICODE_STRING pRegistryPath)
{
	KdPrint(("%wZ DriverEntry.\r\n", pRegistryPath));

	pDriverObject->DriverUnload = DriverUnload;

	CmRegisterCallback(RegistryCallback, 0, &g_Cookie);

	return STATUS_SUCCESS;
}