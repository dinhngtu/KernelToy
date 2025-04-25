// SPDX-License-Identifier: GPL-3.0-only

#include "stdafx.h"
#include "KrtDecl.h"
#include "Driver.h"
#include "Device.h"

VOID KrtpEnableLogging()
{
    DbgSetDebugFilterState(DPFLTR_IHVDRIVER_ID, DPFLTR_ERROR_LEVEL, TRUE);
    DbgSetDebugFilterState(DPFLTR_IHVDRIVER_ID, DPFLTR_WARNING_LEVEL, TRUE);
    DbgSetDebugFilterState(DPFLTR_IHVDRIVER_ID, DPFLTR_TRACE_LEVEL, TRUE);
    DbgSetDebugFilterState(DPFLTR_IHVDRIVER_ID, DPFLTR_INFO_LEVEL, TRUE);
}

_Function_class_(DRIVER_INITIALIZE)
_IRQL_requires_same_
_IRQL_requires_(PASSIVE_LEVEL)
NTSTATUS
DriverEntry(
    _In_ PDRIVER_OBJECT DriverObject,
    _In_ PUNICODE_STRING RegistryPath
)
{
    NTSTATUS status;
    WDF_OBJECT_ATTRIBUTES attributes;
    WDF_DRIVER_CONFIG driverConfig;
    WDFDRIVER driver;
    PWDFDEVICE_INIT controlDeviceInit;

    KrtpEnableLogging();

    WDF_OBJECT_ATTRIBUTES_INIT(&attributes);

    WDF_DRIVER_CONFIG_INIT(&driverConfig, NULL);
    driverConfig.EvtDriverUnload = &KrtpDriverUnload;
    driverConfig.DriverInitFlags |= WdfDriverInitNonPnpDriver;

    status = WdfDriverCreate(
        DriverObject,
        RegistryPath,
        &attributes,
        &driverConfig,
        &driver);
    if (!NT_SUCCESS(status)) {
        goto fail;
    }

    controlDeviceInit = WdfControlDeviceInitAllocate(driver, &SDDL_DEVOBJ_SYS_ALL_ADM_ALL);
    if (!controlDeviceInit) {
        status = STATUS_INSUFFICIENT_RESOURCES;
        goto fail;
    }

    status = KrtpDeviceAdd(controlDeviceInit);
    if (!NT_SUCCESS(status)) {
        goto fail;
    }

    return STATUS_SUCCESS;

fail:
    return status;
}

_Function_class_(EVT_WDF_DRIVER_UNLOAD)
_IRQL_requires_same_
_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
KrtpDriverUnload(
    _In_ WDFDRIVER Driver
)
{
    UNREFERENCED_PARAMETER(Driver);
}
