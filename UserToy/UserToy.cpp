#include <system_error>
#include <cstdio>
#include <vector>
#include <wil/filesystem.h>
#include "KernelToyIoctl.h"

static const std::vector<PCWSTR> HalPlatformTimerSources = {
    L"HalPlatformTimerNotSpecified",
    L"HalPlatformTimer8254",
    L"HalPlatformTimerRtc",
    L"HalPlatformTimerAcpi",
    L"HalPlatformTimerAcpiBroken",
    L"HalPlatformTimerHpet",
    L"HalPlatformTimerProcessorCounter",
    L"HalPlatformTimerHvReferenceTime",
    L"HalPlatformTimerSfi",
    L"HalPlatformTimerApic",
    L"HalPlatformTimerHvSynthetic",
    L"HalPlatformTimerCustom",
    L"HalPlatformTimerCycleCounter",
    L"HalPlatformTimerGit",
};

int wmain() {
    try {
        auto handle = wil::open_file(L"\\\\.\\GLOBALROOT\\Device\\KernelToy", GENERIC_READ, FILE_SHARE_DELETE);

        KERNELTOY_HAL_INFORMATION_REQUEST halRequest = { KrtHalPlatformTimerInformationV1 };
        KERNELTOY_HAL_PLATFORM_TIMER_INFORMATION_V1 halResponse;
        DWORD sz;

        THROW_IF_WIN32_BOOL_FALSE(DeviceIoControl(
            handle.get(),
            IOCTL_KERNELTOY_QUERY_HAL_INFORMATION,
            &halRequest,
            sizeof(halRequest),
            &halResponse,
            sizeof(halResponse),
            &sz,
            NULL));

        if (halResponse.ClockInterruptSource < HalPlatformTimerSources.size())
            wprintf(L"ClockInterruptSource = %s\n", HalPlatformTimerSources.at(halResponse.ClockInterruptSource));
        else
            wprintf(L"ClockInterruptSource = %lu\n", halResponse.ClockInterruptSource);

        if (halResponse.PerformanceCounterSource < HalPlatformTimerSources.size())
            wprintf(L"PerformanceCounterSource = %s\n", HalPlatformTimerSources.at(halResponse.PerformanceCounterSource));
        else
            wprintf(L"PerformanceCounterSource = %lu\n", halResponse.PerformanceCounterSource);

        return 0;
    }
    catch (const std::exception& ex) {
        wprintf(L"%S\n", ex.what());

        return 1;
    }
}
