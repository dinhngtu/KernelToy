// SPDX-License-Identifier: GPL-3.0-only

#include "stdafx.h"
#include "Device.h"
#include "KernelToyIoctl.h"
#include "KrtDecl.h"
#include "Hal.h"

_Function_class_(EVT_WDF_IO_QUEUE_IO_DEVICE_CONTROL)
_IRQL_requires_same_
_IRQL_requires_max_(DISPATCH_LEVEL)
VOID
KrtpFileIoctl(
    _In_ WDFQUEUE Queue,
    _In_ WDFREQUEST Request,
    _In_ size_t OutputBufferLength,
    _In_ size_t InputBufferLength,
    _In_ ULONG IoControlCode
)
{
    NTSTATUS status;

    UNREFERENCED_PARAMETER(Queue);
    UNREFERENCED_PARAMETER(OutputBufferLength);
    UNREFERENCED_PARAMETER(InputBufferLength);

    DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_INFO_LEVEL, "IoControlCode %lx\n", IoControlCode);

    switch (IoControlCode) {
    case IOCTL_KERNELTOY_QUERY_HAL_INFORMATION: {
        PKERNELTOY_HAL_INFORMATION_REQUEST halRequest;
        size_t inLength;

        status = WdfRequestRetrieveInputBuffer(Request, sizeof(KERNELTOY_HAL_INFORMATION_REQUEST), &halRequest, &inLength);
        if (!NT_SUCCESS(status)) {
            goto end;
        }

        DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_INFO_LEVEL, "HalInformationClass %d\n", halRequest->HalInformationClass);

        switch (halRequest->HalInformationClass) {
        case KrtHalPlatformTimerInformationV1: {
            HAL_PLATFORM_TIMER_INFORMATION_V1 halPti;
            PKERNELTOY_HAL_PLATFORM_TIMER_INFORMATION_V1 halOut;
            size_t outLength;

            status = KrtpQueryHalPlatformTimerInformation(&halPti);
            if (!NT_SUCCESS(status)) {
                goto end;
            }

            status = WdfRequestRetrieveOutputBuffer(Request, sizeof(KERNELTOY_HAL_PLATFORM_TIMER_INFORMATION_V1), &halOut, &outLength);
            if (!NT_SUCCESS(status)) {
                goto end;
            }

            halOut->ClockInterruptSource = halPti.ClockInterruptSource;
            halOut->PerformanceCounterSource = halPti.PerformanceCounterSource;

            WdfRequestSetInformation(Request, sizeof(*halOut));

            status = STATUS_SUCCESS;
            break;
        }
        default:
            status = STATUS_INVALID_PARAMETER;
            break;
        }

        break;
    }
    default:
        status = STATUS_INVALID_DEVICE_REQUEST;
        break;
    }

end:
    WdfRequestComplete(Request, status);
}

NTSTATUS
KrtpDeviceAdd(
    _In_ PWDFDEVICE_INIT ControlDeviceInit
)
{
    NTSTATUS status;
    WDF_OBJECT_ATTRIBUTES attributes;
    WDFDEVICE device;
    WDF_IO_QUEUE_CONFIG ioQueueConfig;
    WDFQUEUE queue;

    status = WdfDeviceInitAssignName(ControlDeviceInit, &g_KrtDeviceName);
    if (!NT_SUCCESS(status)) {
        goto fail;
    }

    WdfDeviceInitSetIoType(ControlDeviceInit, WdfDeviceIoBuffered);
    WdfDeviceInitSetDeviceClass(ControlDeviceInit, &KRT_CONTROL_CLASS_GUID);

    WDF_OBJECT_ATTRIBUTES_INIT(&attributes);
    status = WdfDeviceCreate(&ControlDeviceInit, &attributes, &device);
    if (!NT_SUCCESS(status)) {
        goto fail;
    }

    WDF_IO_QUEUE_CONFIG_INIT_DEFAULT_QUEUE(&ioQueueConfig, WdfIoQueueDispatchSequential);
    ioQueueConfig.EvtIoDeviceControl = KrtpFileIoctl;

    WDF_OBJECT_ATTRIBUTES_INIT(&attributes);
    status = WdfIoQueueCreate(
        device,
        &ioQueueConfig,
        &attributes,
        &queue);
    if (!NT_SUCCESS(status)) {
        goto fail;
    }

    WdfControlFinishInitializing(device);

    return STATUS_SUCCESS;

fail:
    if (ControlDeviceInit) {
        WdfDeviceInitFree(ControlDeviceInit);
    }

    return status;
}
