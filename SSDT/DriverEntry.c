#include <ntifs.h>

typedef struct _SYSTEM_SERVICE_TABLE {
	PVOID ServiceTableBase;
	PVOID ServiceCounterTableBase;
	ULONGLONG NumberOfServices;
	PVOID ParamTableBase;
} SYSTEM_SERVICE_TABLE, *PSYSTEM_SERVICE_TABLE;

typedef NTSTATUS (__fastcall *pfnNTCREATEFILE)(
	_Out_    PHANDLE            FileHandle,
	_In_     ACCESS_MASK        DesiredAccess,
	_In_     POBJECT_ATTRIBUTES ObjectAttributes,
	_Out_    PIO_STATUS_BLOCK   IoStatusBlock,
	_In_opt_ PLARGE_INTEGER     AllocationSize,
	_In_     ULONG              FileAttributes,
	_In_     ULONG              ShareAccess,
	_In_     ULONG              CreateDisposition,
	_In_     ULONG              CreateOptions,
	_In_opt_ PVOID              EaBuffer,
	_In_     ULONG              EaLength
);

ULONG OldTpVal;
pfnNTCREATEFILE pfnNtCreateFile = NULL;

NTSTATUS NtCreateFileHOOK(
	_Out_    PHANDLE            FileHandle,
	_In_     ACCESS_MASK        DesiredAccess,
	_In_     POBJECT_ATTRIBUTES ObjectAttributes,
	_Out_    PIO_STATUS_BLOCK   IoStatusBlock,
	_In_opt_ PLARGE_INTEGER     AllocationSize,
	_In_     ULONG              FileAttributes,
	_In_     ULONG              ShareAccess,
	_In_     ULONG              CreateDisposition,
	_In_     ULONG              CreateOptions,
	_In_opt_ PVOID              EaBuffer,
	_In_     ULONG              EaLength
)
{
	KdPrint(("NtCreateFileHOOK is called, %wZ", ObjectAttributes->ObjectName));
	pfnNtCreateFile(FileHandle, DesiredAccess, ObjectAttributes,
		IoStatusBlock, AllocationSize, FileAttributes, ShareAccess,
		CreateDisposition, CreateOptions, EaBuffer, EaLength);
	return STATUS_SUCCESS;
}

KIRQL PageProtectOff()
{
	KIRQL irql = KeRaiseIrqlToDpcLevel();
	UINT64 cr0 = __readcr0();
	cr0 &= 0xfffffffffffeffff;
	__writecr0(cr0);
	_disable();
	return irql;
}

VOID PageProtectOn(KIRQL irql)
{
	UINT64 cr0 = __readcr0();
	cr0 |= 0x10000;
	_enable();
	__writecr0(cr0);
	KeLowerIrql(irql);
}

ULONGLONG GetKeServiceDescriptorTable64()
{
	// 起始地址
	PUCHAR pStartSearchAddress = (PUCHAR)__readmsr(0xC0000082);
	// 结束地址 = 起始地址后移 0x500
	PUCHAR pEndSearchAddress = pStartSearchAddress + 0x500;

	PUCHAR		pCurrentAddress = NULL;
	UCHAR		uAddr1 = 0, uAddr2 = 0, uAddr3 = 0;
	ULONG		uTempAddress = 0;
	ULONGLONG	ullAddress = 0;

	// 一个字节一个字节的遍历
	for (pCurrentAddress = pStartSearchAddress;
		pCurrentAddress < pEndSearchAddress; pCurrentAddress++)
	{
		if (MmIsAddressValid(pCurrentAddress) &&
			MmIsAddressValid(pCurrentAddress + 1) &&
			MmIsAddressValid(pCurrentAddress + 2))
		{
			uAddr1 = *pCurrentAddress;
			uAddr2 = *(pCurrentAddress + 1);
			uAddr3 = *(pCurrentAddress + 2);
			if (uAddr1 == 0x4c &&
				uAddr2 == 0x8d &&
				uAddr3 == 0x15)
			{
				// 特征位置后面的 4 个字节
				memcpy(&uTempAddress, pCurrentAddress + 3, 4);
				// 暂时没看懂为什么要 +7
				ullAddress = (ULONGLONG)uTempAddress + (ULONGLONG)pCurrentAddress + 7;
				return ullAddress;
			}
		}
	}
	
	return 0;
}

ULONG GetOffsetAddress64(ULONGLONG llFunctionAddress)
{
	LONG					lTemp = 0;
	PSYSTEM_SERVICE_TABLE	pstSSDT = NULL;
	PULONG					pServiceTableBase = NULL;

	pstSSDT = (PSYSTEM_SERVICE_TABLE)GetKeServiceDescriptorTable64();
	pServiceTableBase = (PULONG)pstSSDT->ServiceTableBase;
	lTemp = (LONG)(llFunctionAddress - (ULONGLONG)pServiceTableBase);
	
	return lTemp << 4;
}

ULONGLONG GetSSDTFunctionAddress64(ULONGLONG ullIndex)
{
	LONG					lTemp = 0;
	ULONGLONG				ullTemp = 0, stb = 0, ret = 0;
	PSYSTEM_SERVICE_TABLE	pstSSDT = NULL;

	pstSSDT = (PSYSTEM_SERVICE_TABLE)GetKeServiceDescriptorTable64();

	// 拿到基址
	stb = (ULONGLONG)(pstSSDT->ServiceTableBase);
	// 基址 + 函数编号 * 4字节的偏移位置
	ullTemp = stb + (4 * ullIndex);
	lTemp = *(PLONG)ullTemp;
	lTemp = lTemp >> 4;
	ret = stb + (LONG64)lTemp;

	/*for (ULONGLONG ull = 0; ull < pstSSDT->NumberOfServices; ull++)
	{
		ULONGLONG ullFunction = (stb + 4 * ull);
		KdPrint(("Function address = 0x%llx", ullFunction));
	}*/

	return ret;
}

VOID FuckKeBugCheckEx()
{
	KIRQL irql;
	ULONGLONG myfun;
	UCHAR jmp_code[] = "\x48\xB8\xFF\xFF\xFF\xFF\xFF\xFF\xFF\x00\xFF\xE0";
	myfun = (ULONGLONG)NtCreateFileHOOK;
	memcpy(jmp_code + 2, &myfun, 8);
	irql = PageProtectOff();
	memset(KeBugCheckEx, 0x90, 15);
	memcpy(KeBugCheckEx, jmp_code, 12);
	PageProtectOn(irql);
}

VOID HookSSDT()
{
	KIRQL irql;
	ULONGLONG dwtmp = 0;
	PULONG ServiceTableBase = NULL;
	PSYSTEM_SERVICE_TABLE	pstSSDT = NULL;

	pstSSDT = (PSYSTEM_SERVICE_TABLE)GetKeServiceDescriptorTable64();

	//get old address
	pfnNtCreateFile = (pfnNTCREATEFILE)GetSSDTFunctionAddress64(0x52);
	KdPrint(("Old_NtTerminateProcess: %llx", (ULONGLONG)pfnNtCreateFile));
	//set kebugcheckex
	FuckKeBugCheckEx();

	//show new address
	ServiceTableBase = (PULONG)pstSSDT->ServiceTableBase;
	OldTpVal = ServiceTableBase[0x52]; //record old offset value
	irql = PageProtectOff();
	ServiceTableBase[0x52] = GetOffsetAddress64((ULONGLONG)KeBugCheckEx);
	PageProtectOn(irql);
	KdPrint(("KeBugCheckEx: %llx", (ULONGLONG)KeBugCheckEx));
	KdPrint(("New_NtTerminateProcess: %llx", GetSSDTFunctionAddress64(0x52)));

}

VOID UnhookSSDT()
{
	KIRQL					irql;
	PULONG					ServiceTableBase = NULL;
	PSYSTEM_SERVICE_TABLE	pstSSDT = NULL;

	pstSSDT = (PSYSTEM_SERVICE_TABLE)GetKeServiceDescriptorTable64();

	ServiceTableBase = (PULONG)pstSSDT->ServiceTableBase;
	irql = PageProtectOff();
	ServiceTableBase[0x52] = GetOffsetAddress64((ULONGLONG)NtCreateFile);
	PageProtectOn(irql);

	KdPrint(("NtTerminateProcess: %llx", GetSSDTFunctionAddress64(0x52)));
}

VOID DriverUnLoad(PDRIVER_OBJECT pDriverObject)
{
	UnhookSSDT();
	KdPrint(("DriverUnLoad..."));
}

NTSTATUS DriverEntry(PDRIVER_OBJECT pDriverObject, PUNICODE_STRING pRegistryPath)
{
	KdPrint(("[SSDT_Table] NtCreateFile: 0x%llx", GetSSDTFunctionAddress64(0x52)));

	HookSSDT();

	pDriverObject->DriverUnload = DriverUnLoad;
	return STATUS_SUCCESS;
}