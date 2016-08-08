#include <ntifs.h>

VOID MemoryOperation()
{
	// 申请内存锁定页和不锁定页，对应 C 语言 malloc
	PVOID ptr1 = ExAllocatePool(PagedPool, 0x100);
	PVOID ptr2 = ExAllocatePool(NonPagedPool, 0x200);

	// 内存的初始化和复制，对应 C 语言 memset 和 memcpy
	RtlFillMemory(ptr2, 0x200, 0x90);
	RtlMoveMemory(ptr1, ptr2, 0x50);

	// 释放内存，对应 C 语言 free
	ExFreePool(ptr1);
	ExFreePool(ptr2);
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

VOID DriverUnload(IN PDRIVER_OBJECT pDriverObject)
{
	KdPrint(("[MemoryOperation] Unload..."));
	return;
}

NTSTATUS DriverEntry(IN PDRIVER_OBJECT pDriverObject, IN PUNICODE_STRING pRegistryPath)
{
	pDriverObject->DriverUnload = DriverUnload;
	KdPrint(("[MemoryOperation] Load..."));

	KIRQL irql = PageProtectOff();

	KdPrint(("[MemoryOperation] Trun off the page protect..."));

	PageProtectOn(irql);

	return STATUS_SUCCESS;
}