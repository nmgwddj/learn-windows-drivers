#ifndef __PROCESSMGR_H__
#define __PROCESSMGR_H__
#include <ntifs.h>
#include "NtStructDef.h"

typedef struct _CONTROL_AREA64
{
	PVOID64 Segment;
	PVOID64 p1;
	PVOID64 p2;
	ULONG64 NumberOfSectionReferences;
	ULONG64 NumberOfPfnReferences;
	ULONG64 NumberOfMappedViews;
	ULONG64 NumberOfUserReferences;
	union
	{
		ULONG LongFlags;
		ULONG Flags;
	} u;
	PVOID64 FilePointer;
} CONTROL_AREA64, *PCONTROL_AREA64;

typedef struct _CONTROL_AREA
{
	PVOID Segment;
	LIST_ENTRY DereferenceList;
	ULONG NumberOfSectionReferences;
	ULONG NumberOfPfnReferences;
	ULONG NumberOfMappedViews;
	ULONG NumberOfSystemCacheViews;
	ULONG NumberOfUserReferences;
	union
	{
		ULONG LongFlags;
		ULONG Flags;
	} u;
	PFILE_OBJECT FilePointer;
} CONTROL_AREA, *PCONTROL_AREA;

typedef struct _SEGMENT64
{
	PVOID64 ControlArea;
	ULONG TotalNumberOfPtes;
	ULONG NonExtendedPtes;
	ULONG Spare0;
}SEGMENT64, *PSEGMENT64;

typedef struct _SEGMENT
{
	struct _CONTROL_AREA *ControlArea;
	ULONG TotalNumberOfPtes;
	ULONG NonExtendedPtes;
	ULONG Spare0;
} SEGMENT, *PSEGMENT;

typedef struct _SECTION_OBJECT
{
	PVOID StartingVa;
	PVOID EndingVa;
	PVOID Parent;
	PVOID LeftChild;
	PVOID RightChild;
	PSEGMENT Segment;
} SECTION_OBJECT, *PSECTION_OBJECT;


typedef struct _SECTION_OBJECT64
{
	PVOID64 StartingVa;
	PVOID64 EndingVa;
	PVOID64 Parent;
	PVOID64 LeftChild;
	PVOID64 RightChild;
	PVOID64 Segment;
} SECTION_OBJECT64, *PSECTION_OBJECT64;

NTSTATUS PsReferenceProcessFilePointer(
	IN PEPROCESS Process,
	OUT PVOID *pFilePointer
);

NTKERNELAPI PCHAR PsGetProcessImageFileName(
	PEPROCESS Process
);

NTKERNELAPI NTSTATUS PsLookupProcessByProcessId(
	HANDLE ProcessId,
	PEPROCESS *Process
);

NTKERNELAPI PPEB PsGetProcessPeb(
	__in PEPROCESS Process
);

typedef NTSTATUS(*ZWQUERYINFORMATIONPROCESS) (
	__in HANDLE ProcessHandle,
	__in PROCESSINFOCLASS ProcessInformationClass,
	__out_bcount(ProcessInformationLength) PVOID ProcessInformation,
	__in ULONG ProcessInformationLength,
	__out_opt PULONG ReturnLength
	);

extern ZWQUERYINFORMATIONPROCESS ZwQueryInformationProcess;
ZWQUERYINFORMATIONPROCESS ZwQueryInformationProcess;

PCHAR GetProcessNameByProcessId(
	HANDLE hProcessId
)
{
	NTSTATUS		status = STATUS_UNSUCCESSFUL;
	PEPROCESS		ProcessObj = NULL;
	PCHAR			pProcessName = NULL;

	status = PsLookupProcessByProcessId(hProcessId, &ProcessObj);
	if (NT_SUCCESS(status))
	{
		pProcessName = PsGetProcessImageFileName(ProcessObj);
		ObfDereferenceObject(ProcessObj);
	}

	return pProcessName;
}

BOOLEAN GetPathByFileObject(PFILE_OBJECT FileObject, WCHAR* wzPath)
{
	BOOLEAN bGetPath = FALSE;

	POBJECT_NAME_INFORMATION ObjectNameInformation = NULL;
	__try
	{
		if (FileObject && MmIsAddressValid(FileObject) && wzPath)
		{
			KdPrint(("MmIsAddressValid success."));
			if (NT_SUCCESS(IoQueryFileDosDeviceName(FileObject, &ObjectNameInformation)))   //注意该函数调用后要释放内存
			{
				KdPrint(("IoQueryFileDosDeviceName success."));
				wcsncpy(wzPath, ObjectNameInformation->Name.Buffer, ObjectNameInformation->Name.Length);

				bGetPath = TRUE;

				ExFreePool(ObjectNameInformation);
			}


			if (!bGetPath)
			{
				if (IoVolumeDeviceToDosName || RtlVolumeDeviceToDosName)
				{
					NTSTATUS	Status = STATUS_UNSUCCESSFUL;
					ULONG		ulRet = 0;
					PVOID		Buffer = ExAllocatePool(PagedPool, 0x1000);

					if (Buffer)
					{
						// ObQueryNameString : \Device\HarddiskVolume1\Program Files\VMware\VMware Tools\VMwareTray.exe
						memset(Buffer, 0, 0x1000);
						Status = ObQueryNameString(FileObject, (POBJECT_NAME_INFORMATION)Buffer, 0x1000, &ulRet);
						if (NT_SUCCESS(Status))
						{
							POBJECT_NAME_INFORMATION Temp = (POBJECT_NAME_INFORMATION)Buffer;
							KdPrint(("ObQueryNameString success.%wZ\r\n", Temp));

							WCHAR szHarddiskVolume[100] = L"\\Device\\HarddiskVolume";

							if (Temp->Name.Buffer != NULL)
							{
								if (Temp->Name.Length / sizeof(WCHAR) > wcslen(szHarddiskVolume) &&
									!_wcsnicmp(Temp->Name.Buffer, szHarddiskVolume, wcslen(szHarddiskVolume)))
								{
									// 如果是以 "\\Device\\HarddiskVolume" 这样的形式存在的，那么再查询其卷名。
									UNICODE_STRING uniDosName;

									if (NT_SUCCESS(IoVolumeDeviceToDosName(FileObject->DeviceObject, &uniDosName)))
									{
										if (uniDosName.Buffer != NULL)
										{

											wcsncpy(wzPath, uniDosName.Buffer, uniDosName.Length);
											wcsncat(wzPath, Temp->Name.Buffer + wcslen(szHarddiskVolume) + 1, Temp->Name.Length - (wcslen(szHarddiskVolume) + 1));
											bGetPath = TRUE;
										}

										ExFreePool(uniDosName.Buffer);
									}

									else if (NT_SUCCESS(RtlVolumeDeviceToDosName(FileObject->DeviceObject, &uniDosName)))
									{
										if (uniDosName.Buffer != NULL)
										{

											wcsncpy(wzPath, uniDosName.Buffer, uniDosName.Length);
											wcsncat(wzPath, Temp->Name.Buffer + wcslen(szHarddiskVolume) + 1, Temp->Name.Length - (wcslen(szHarddiskVolume) + 1));
											bGetPath = TRUE;
										}

										ExFreePool(uniDosName.Buffer);
									}

								}
								else
								{
									// 如果不是以 "\\Device\\HarddiskVolume" 这样的形式开头的，那么直接复制名称。
									wcsncpy(wzPath, Temp->Name.Buffer, Temp->Name.Length);
									bGetPath = TRUE;
								}
							}
						}

						ExFreePool(Buffer);
					}
				}
			}
		}
	}
	__except (1)
	{
		DbgPrint("GetPathByFileObject Catch __Except\r\n");
		bGetPath = FALSE;
	}

	return bGetPath;
}

BOOLEAN GetProcessPathBySectionObject(HANDLE ulProcessID, WCHAR* wzProcessPath)
{
	PEPROCESS			EProcess = NULL;
	PFILE_OBJECT		FileObject = NULL;
	BOOLEAN				bGetPath = FALSE;

	if (NT_SUCCESS(PsLookupProcessByProcessId(ulProcessID, &EProcess)))
	{
		PsReferenceProcessFilePointer(EProcess, (PVOID)&FileObject);
		if (FileObject && MmIsAddressValid(FileObject))
		{
			FileObject = (PFILE_OBJECT)((ULONG_PTR)FileObject & 0xFFFFFFFFFFFFFFF0);
			bGetPath = GetPathByFileObject(FileObject, wzProcessPath);
			if (!bGetPath)
			{
				KdPrint(("Failed to get process full path by object, FileObject = 0x%08X", FileObject));
			}
		}
	}
	else
	{
		KdPrint(("Failed to call PsLookupProcessByProcessId.\r\n"));
	}

	if (bGetPath == FALSE)
	{
		wcscpy(wzProcessPath, L"Unknow");
	}

	return bGetPath;

}

#endif
