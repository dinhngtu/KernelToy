#pragma once

#include "stdafx.h"

// https://www.geoffchappell.com/studies/windows/km/ntoskrnl/inc/ntos/hal/hal_platform_timer_source.htm

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

// deduced from valid output size, along with names from
// https://www.geoffchappell.com/studies/windows/km/ntoskrnl/inc/ntos/hal/hal_platform_timer_source.htm

typedef struct _HAL_PLATFORM_TIMER_INFORMATION_V1 {
    ULONG Version;
    ULONG ClockInterruptSource;
    ULONG PerformanceCounterSource;
} HAL_PLATFORM_TIMER_INFORMATION_V1, * PHAL_PLATFORM_TIMER_INFORMATION_V1;

NTSTATUS KrtpQueryHalPlatformTimerInformation(
    _Out_ PHAL_PLATFORM_TIMER_INFORMATION_V1 HalInformation);
