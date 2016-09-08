#ifndef __IODEVICECONTROL_H__
#define __IODEVICECONTROL_H__

#include <ntifs.h>
#include "NtStructDef.h"
#include "ListEntry.h"

NTSTATUS CreateDevice(PDRIVER_OBJECT pDriverObject);

NTSTATUS RemoveDevice(PDRIVER_OBJECT pDriverObject);

NTSTATUS CreateCompleteRoutine(PDEVICE_OBJECT pDeviceObject, PIRP pIrp);

NTSTATUS CloseCompleteRoutine(PDEVICE_OBJECT pDeviceObject, PIRP pIrp);

NTSTATUS ReadCompleteRoutine(PDEVICE_OBJECT pDeviceObject, PIRP pIrp);

NTSTATUS WriteCompleteRoutine(PDEVICE_OBJECT pDeviceObject, PIRP pIrp);

NTSTATUS DeviceControlCompleteRoutine(PDEVICE_OBJECT pDeviceObject, PIRP pIrp);

#endif
