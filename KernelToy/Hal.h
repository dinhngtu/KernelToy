#pragma once

#include "stdafx.h"

enum _HAL_PLATFORM_TIMER_SOURCE {
    HalPlatformTimerNotSpecified,
    HalPlatformTimer8254,
    HalPlatformTimerRtc,
    HalPlatformTimerAcpi,
    HalPlatformTimerAcpiBroken,
    HalPlatformTimerHpet,
    HalPlatformTimerProcessorCounter,
    HalPlatformTimerHvReferenceTime,
    HalPlatformTimerSfi,
    HalPlatformTimerApic,
    HalPlatformTimerHvSynthetic,
    HalPlatformTimerCustom,
    HalPlatformTimerCycleCounter,
    HalPlatformTimerGit,
};

typedef enum _HAL_PLATFORM_TIMER_SOURCE HAL_PLATFORM_TIMER_SOURCE;

typedef struct _HAL_PLATFORM_TIMER_INFORMATION_V1 {
    ULONG Version;
    ULONG ClockInterruptSource;
    ULONG PerformanceCounterSource;
} HAL_PLATFORM_TIMER_INFORMATION_V1, * PHAL_PLATFORM_TIMER_INFORMATION_V1;

NTSTATUS KrtpQueryHalPlatformTimerInformation(
    _Out_ PHAL_PLATFORM_TIMER_INFORMATION_V1 HalInformation);
