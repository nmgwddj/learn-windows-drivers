#include <ntifs.h>

VOID UnLoadDriver(PDRIVER_OBJECT pDriverObject)
{
	KdPrint(("UnLoadDriver"));
}

NTSTATUS Regedit()
{
	NTSTATUS			status;
	HANDLE				hKey;
	OBJECT_ATTRIBUTES	ObjectAttributes;
	UNICODE_STRING		usKeyPath;

	RtlInitUnicodeString(&usKeyPath, L"\\Registry\\Machine\\SOFTWARE");

	InitializeObjectAttributes(&ObjectAttributes, &usKeyPath, OBJ_CASE_INSENSITIVE, NULL, NULL);

	status = ZwOpenKey(&hKey, KEY_QUERY_VALUE, &ObjectAttributes);
	if (!NT_SUCCESS(status))
	{
		KdPrint(("Fail to call ZwOpenKey, error code = 0x%08X", status));
		return status;
	}

	PKEY_FULL_INFORMATION	pKeyInformation;
	ULONG					ulResultLength = 0;

	status = ZwQueryKey(hKey, KeyFullInformation, NULL, 0, &ulResultLength);

	pKeyInformation = (PKEY_FULL_INFORMATION)ExAllocatePool(PagedPool, ulResultLength);
	status = ZwQueryKey(hKey, KeyFullInformation, pKeyInformation, ulResultLength, &ulResultLength);
	if (!NT_SUCCESS(status))
	{
		KdPrint(("Fail to call ZwOpenKey.."));
		return status;
	}

	KdPrint(("Subkeys = %ld", pKeyInformation->SubKeys));

	for (ULONG i = 0; i < pKeyInformation->SubKeys; i++)
	{
		UNICODE_STRING			usKeyName;
		PKEY_BASIC_INFORMATION	pKeyBasicInformation;
		ULONG					ulResultLength = 0;

		status = ZwEnumerateKey(hKey, i, KeyBasicInformation, NULL, 0, &ulResultLength);
		KdPrint(("ZwEnumerateKey result ulResultlength = %d, i = %d", ulResultLength, i));

		pKeyBasicInformation = (PKEY_BASIC_INFORMATION)ExAllocatePool(PagedPool, ulResultLength);
		status = ZwEnumerateKey(hKey, i, KeyBasicInformation, pKeyBasicInformation, ulResultLength, &ulResultLength);
		if (!NT_SUCCESS(status))
		{
			KdPrint(("Fail to call ZwEnumerateKey, error code = 0x%08X", status));
			return status;
		}

		usKeyName.Length = (USHORT)pKeyBasicInformation->NameLength;
		usKeyName.Buffer = pKeyBasicInformation->Name;
		
		KdPrint(("key name = %wZ", &usKeyName));
		ExFreePool(pKeyBasicInformation);
	}

	ExFreePool(pKeyInformation);
	ZwClose(hKey);

	return STATUS_SUCCESS;
}

NTSTATUS DriverEntry(PDRIVER_OBJECT pDriverObject, PUNICODE_STRING pRegistryPath)
{
	Regedit();
	pDriverObject->DriverUnload = UnLoadDriver;
	return STATUS_SUCCESS;
}