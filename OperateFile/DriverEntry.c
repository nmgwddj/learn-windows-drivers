#include <ntifs.h>

VOID UnLoadDriver(PDRIVER_OBJECT pDriverObject)
{
	KdPrint(("UnLoadDriver"));
}

NTSTATUS MyCreateFile()
{
	NTSTATUS			status;

	HANDLE				hFile;

	OBJECT_ATTRIBUTES	ObjectAttributes;
	IO_STATUS_BLOCK		IoStatusBlock;
	UNICODE_STRING		usObjectName;

	RtlInitUnicodeString(&usObjectName, L"\\??\\C:\\CreateFile.dat");
	InitializeObjectAttributes(&ObjectAttributes, &usObjectName, OBJ_CASE_INSENSITIVE, NULL, NULL);

	/* Create File
	status = ZwCreateFile(&hFile, GENERIC_ALL,
	&ObjectAttributes,
	&IoStatusBlock,
	NULL,
	FILE_ATTRIBUTE_NORMAL,
	FILE_SHARE_READ,
	FILE_CREATE,
	FILE_NON_DIRECTORY_FILE,
	NULL,
	0);
	if (!NT_SUCCESS(status))
	{
	KdPrint(("Fail to create file ."));
	return status;
	}
	KdPrint(("Create file success \\??\\C:\\CreateFile.dat."));
	*/

	status = ZwOpenFile(&hFile, GENERIC_ALL, &ObjectAttributes, &IoStatusBlock, FILE_SHARE_READ, FILE_NON_DIRECTORY_FILE);
	if (!NT_SUCCESS(status))
	{
		KdPrint(("Fail to call ZwOpenFile, errorCode = 0x%08X", status));
		return status;
	}
	KdPrint(("OpenFile success  \\??\\C:\\CreateFile.dat."));

	ZwClose(hFile);

	return STATUS_SUCCESS;
}

NTSTATUS DriverEntry(PDRIVER_OBJECT pDriverObject, PUNICODE_STRING pRegistryPath)
{
	MyCreateFile();
	pDriverObject->DriverUnload = UnLoadDriver;
	return STATUS_SUCCESS;
}