// SPDX-License-Identifier: GPL-3.0-only

#include "stdafx.h"
#include "Hal.h"

NTSTATUS KrtpQueryHalPlatformTimerInformation(
    _Out_ PHAL_PLATFORM_TIMER_INFORMATION_V1 HalInformation
)
{
    NTSTATUS status;
    ULONG rl;

    RtlZeroMemory(HalInformation, sizeof(*HalInformation));

    status = HalQuerySystemInformation(HalPlatformTimerInformation, sizeof(*HalInformation), HalInformation, &rl);
    if (!NT_SUCCESS(status)) {
        return status;
    }

    if (rl < sizeof(*HalInformation) || HalInformation->Version != 1) {
        return STATUS_UNSUCCESSFUL;
    }

    return STATUS_SUCCESS;
}
