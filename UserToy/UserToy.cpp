// SPDX-License-Identifier: GPL-3.0-only

#include "stdafx.h"
#include <intrin.h>
#include "KernelToyIoctl.h"
#include "Driver.hpp"

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

static void do_read_halpti() {
    ensure_driver_loaded();

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
    if (sz < sizeof(halResponse)) {
        throw std::runtime_error("halResponse too small");
    }

    if (halResponse.ClockInterruptSource < HalPlatformTimerSources.size())
        wprintf(L"ClockInterruptSource = %s\n", HalPlatformTimerSources.at(halResponse.ClockInterruptSource));
    else
        wprintf(L"ClockInterruptSource = %lu\n", halResponse.ClockInterruptSource);

    if (halResponse.PerformanceCounterSource < HalPlatformTimerSources.size())
        wprintf(L"PerformanceCounterSource = %s\n", HalPlatformTimerSources.at(halResponse.PerformanceCounterSource));
    else
        wprintf(L"PerformanceCounterSource = %lu\n", halResponse.PerformanceCounterSource);
}

static void do_read_vmgen() {
    auto handle = wil::open_file(L"\\\\." VM_GENCOUNTER_SYMBOLIC_LINK_NAME, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_DELETE);

    VM_GENCOUNTER vmgen;
    DWORD sz;

    THROW_IF_WIN32_BOOL_FALSE(DeviceIoControl(
        handle.get(),
        IOCTL_VMGENCOUNTER_READ,
        NULL,
        0,
        &vmgen,
        sizeof(vmgen),
        &sz,
        NULL));
    if (sz < sizeof(vmgen)) {
        throw std::runtime_error("vmgen too small");
    }

    wprintf(L"GenerationCount Low = 0x%llx High = 0x%llx\n", vmgen.GenerationCount, vmgen.GenerationCountHigh);
}

static void dump_cpuid(unsigned int initflag) {
    int regs[4];
    __cpuid(regs, initflag);
    printf("%08lx = %08lx %08lx %08lx %08lx\n", initflag, regs[0], regs[1], regs[2], regs[3]);
    unsigned int max = regs[0];
    for (unsigned int leaf = initflag + 1; leaf <= max; leaf++) {
        __cpuid(regs, leaf);
        printf("%08lx = %08lx %08lx %08lx %08lx\n", leaf, regs[0], regs[1], regs[2], regs[3]);
    }
}

static void do_cpuid() {
    dump_cpuid(0);
    dump_cpuid(0x40000000);
    dump_cpuid(0x80000000);
}

int wmain(int argc, wchar_t** argv) {
    int ret = 0;

    try {
        if (argc != 2) {
            throw std::exception("unknown command");
        }

        if (!_wcsicmp(argv[1], L"read_halpti")) {
            do_read_halpti();
        }
        else if (!_wcsicmp(argv[1], L"read_vmgen")) {
            do_read_vmgen();
        }
        else if (!_wcsicmp(argv[1], L"cpuid")) {
            do_cpuid();
        }
        else {
            throw std::exception("unknown command");
        }

        ret = 0;
    }
    catch (const std::exception& ex) {
        wprintf(L"%S\n", ex.what());

        ret = 1;
    }

    try {
        ensure_driver_unloaded();
    }
    catch (const std::exception& ex2) {
        wprintf(L"Driver unload error: %S\n", ex2.what());
        ret = 2;
    }

    return ret;
}
