#include "ProcessMgr.h"

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

BOOLEAN GetProcessPathBySectionObject(HANDLE ulProcessID, WCHAR* wzProcessPath)
{
	PEPROCESS			EProcess = NULL;
	PSECTION_OBJECT		SectionObject = NULL;
	PSECTION_OBJECT64	SectionObject64 = NULL;
	PSEGMENT			Segment = NULL;
	PSEGMENT64			Segment64 = NULL;
	PCONTROL_AREA		ControlArea = NULL;
	PCONTROL_AREA64		ControlArea64 = NULL;
	PFILE_OBJECT		FileObject = NULL;
	BOOLEAN				bGetPath = FALSE;
	ULONG				SectionObjectOfEProcess = 0x268;

	if (NT_SUCCESS(PsLookupProcessByProcessId(ulProcessID, &EProcess)))
	{
		if (SectionObjectOfEProcess != 0 && MmIsAddressValid((PVOID)((ULONG_PTR)EProcess + SectionObjectOfEProcess)))
		{
			SectionObject64 = *(PSECTION_OBJECT64*)((ULONG_PTR)EProcess + SectionObjectOfEProcess);
			if (SectionObject64 && MmIsAddressValid(SectionObject64))
			{
				Segment64 = (PSEGMENT64)(SectionObject64->Segment);
				if (Segment64 && MmIsAddressValid(Segment64))
				{
					ControlArea64 = (PCONTROL_AREA64)Segment64->ControlArea;
					if (ControlArea64 && MmIsAddressValid(ControlArea64))
					{
						FileObject = (PFILE_OBJECT)ControlArea64->FilePointer;
						if (FileObject&&MmIsAddressValid(FileObject))
						{
							FileObject = (PFILE_OBJECT)((ULONG_PTR)FileObject & 0xFFFFFFFFFFFFFFF0);
							bGetPath = GetPathByFileObject(FileObject, wzProcessPath);
							if (!bGetPath)
							{
								DbgPrint("SectionObject: 0x%08X, FileObject: 0x%08X\n", SectionObject, FileObject);
							}
						}
					}
				}
			}
		}
	}
	if (bGetPath == FALSE)
	{
		wcscpy(wzProcessPath, L"Unknow");
	}

	return bGetPath;

}

BOOLEAN GetPathByFileObject(PFILE_OBJECT FileObject, WCHAR* wzPath)
{
	BOOLEAN bGetPath = FALSE;
	CHAR szIoQueryFileDosDeviceName[] = "IoQueryFileDosDeviceName";
	CHAR szIoVolumeDeviceToDosName[] = "IoVolumeDeviceToDosName";
	CHAR szRtlVolumeDeviceToDosName[] = "RtlVolumeDeviceToDosName";

	POBJECT_NAME_INFORMATION ObjectNameInformation = NULL;
	__try
	{
		if (FileObject && MmIsAddressValid(FileObject) && wzPath)
		{
			if (NT_SUCCESS(IoQueryFileDosDeviceName(FileObject, &ObjectNameInformation)))   //注意该函数调用后要释放内存
			{
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
