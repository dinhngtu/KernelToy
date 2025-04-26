// SPDX-License-Identifier: GPL-3.0-only

#include "stdafx.h"
#include "Driver.hpp"

struct DriverServiceConfig {
    std::wstring driverPath;
    std::wstring serviceName;

    DriverServiceConfig() = default;

    DriverServiceConfig(const std::wstring& path, const std::wstring& svcName)
        : driverPath(path), serviceName(svcName) {
    }
};

static constexpr const DWORD ServiceStopTimeout = 30000;
static constexpr const wchar_t* DriverName = L"KernelToy.sys";

static std::wstring get_random_base32(size_t length) {
    const std::wstring charset = L"abcdefghijklmnopqrstuvwxyz234567";
    std::random_device rd;
    std::mt19937 generator(rd());
    std::uniform_int_distribution<> distribution(0, static_cast<int>(charset.length() - 1));

    std::wstring result;
    result.reserve(length);
    for (size_t i = 0; i < length; ++i) {
        result += charset[distribution(generator)];
    }
    return result;
}

static DriverServiceConfig create_driver_config() {
    auto exePathString = wil::GetModuleFileNameW<wil::unique_cotaskmem_string>();
    std::filesystem::path exePath(exePathString.get());
    auto driverPath = exePath.parent_path() / DriverName;
    auto serviceName = L"KernelToy_" + get_random_base32(8);
    return DriverServiceConfig(driverPath.wstring(), serviceName);
}

static const DriverServiceConfig& get_driver_config() {
    static DriverServiceConfig config = create_driver_config();
    return config;
}

static bool driverLoaded;
static std::mutex driverLoadedLock;

void ensure_driver_loaded() {
    auto lock = std::lock_guard(driverLoadedLock);

    wprintf(L"starting %s\n", get_driver_config().serviceName.c_str());

    wil::unique_schandle hSCManager(THROW_LAST_ERROR_IF_NULL(
        OpenSCManagerW(
            nullptr,                         // Local machine
            nullptr,                         // Default database (ServicesActive)
            SC_MANAGER_CONNECT | SC_MANAGER_CREATE_SERVICE // Permissions needed
        )
    ));

    wil::unique_schandle hService(THROW_LAST_ERROR_IF_NULL(CreateServiceW(
        hSCManager.get(),         // SCM database handle
        get_driver_config().serviceName.c_str(),              // Name of service (internal)
        DriverName,             // Name to display
        SERVICE_START | SERVICE_QUERY_STATUS, // Required access rights
        SERVICE_KERNEL_DRIVER,    // Service type for drivers
        SERVICE_DEMAND_START,     // Start type (manual/programmatic start)
        SERVICE_ERROR_NORMAL,     // Error control (log errors)
        get_driver_config().driverPath.c_str(),               // Path to the driver binary (.sys)
        nullptr,                  // No load order group
        nullptr,                  // No tag identifier
        nullptr,                  // No dependencies
        nullptr,                  // Service start name (NULL for kernel drivers)
        nullptr                   // Password (NULL for kernel drivers)
    )));

    SERVICE_STATUS_PROCESS ssp = {}; // Use PROCESS struct for more info if needed later
    DWORD bytesNeeded = 0;

    THROW_IF_WIN32_BOOL_FALSE(
        QueryServiceStatusEx(
            hService.get(),
            SC_STATUS_PROCESS_INFO, // Info level
            reinterpret_cast<LPBYTE>(&ssp),
            sizeof(ssp),
            &bytesNeeded)
    );

    switch (ssp.dwCurrentState) {
    case SERVICE_RUNNING:
        // nothing to do
        break;
    case SERVICE_STOPPED:
    case SERVICE_STOP_PENDING:
    case SERVICE_START_PENDING:
        if (ssp.dwCurrentState != SERVICE_STOPPED) {
            wprintf(L"waiting for service to stop/start\n");
            std::this_thread::sleep_for(std::chrono::milliseconds(ssp.dwWaitHint > 0 ? ssp.dwWaitHint : 1000));
        }
        if (ssp.dwCurrentState != SERVICE_START_PENDING) {
            try {
                THROW_IF_WIN32_BOOL_FALSE(StartServiceW(hService.get(), 0, nullptr));
            }
            catch (const wil::ResultException& e) {
                if (e.GetErrorCode() != ERROR_SERVICE_ALREADY_RUNNING) {
                    throw;
                }
            }
        }
        break;
    default:
        throw std::runtime_error("unexpected service status");
    }

    driverLoaded = true;
}

void ensure_driver_unloaded() {
    auto lock = std::lock_guard(driverLoadedLock);

    if (!driverLoaded) {
        return;
    }

    wprintf(L"deleting %s\n", get_driver_config().serviceName.c_str());

    wil::unique_schandle hSCManager(THROW_LAST_ERROR_IF_NULL(
        OpenSCManagerW(nullptr, nullptr, SC_MANAGER_CONNECT)
    ));

    wil::unique_schandle hService;
    try {
        hService.reset(THROW_LAST_ERROR_IF_NULL(OpenServiceW(
            hSCManager.get(),
            get_driver_config().serviceName.c_str(),
            SERVICE_QUERY_STATUS | SERVICE_STOP | DELETE)));
    }
    catch (const wil::ResultException& e) {
        if (e.GetErrorCode() != HRESULT_FROM_WIN32(ERROR_SERVICE_DOES_NOT_EXIST)) {
            throw;
        }
    }

    SERVICE_STATUS_PROCESS ssp = {};
    DWORD bytesNeeded = 0;
    THROW_IF_WIN32_BOOL_FALSE(
        QueryServiceStatusEx(
            hService.get(),
            SC_STATUS_PROCESS_INFO,
            reinterpret_cast<LPBYTE>(&ssp),
            sizeof(ssp),
            &bytesNeeded
        )
    );

    switch (ssp.dwCurrentState) {
    case SERVICE_STOPPED:
        break;
    case SERVICE_RUNNING:
    case SERVICE_PAUSED:
    case SERVICE_START_PENDING: {
        SERVICE_STATUS statusStop = {};
        THROW_IF_WIN32_BOOL_FALSE(ControlService(hService.get(), SERVICE_CONTROL_STOP, &statusStop));

        auto startTime = std::chrono::steady_clock::now();
        DWORD lastCheckPoint = statusStop.dwCheckPoint;

        while (statusStop.dwCurrentState != SERVICE_STOPPED) {
            auto now = std::chrono::steady_clock::now();
            auto elapsed = static_cast<DWORD>(std::chrono::duration_cast<std::chrono::milliseconds>(now - startTime).count());

            if (elapsed > ServiceStopTimeout) {
                THROW_WIN32(ERROR_TIMEOUT);
            }

            DWORD waitHint = std::clamp(statusStop.dwWaitHint / 10, 100ul, 3000ul);
            std::this_thread::sleep_for(std::chrono::milliseconds(waitHint));

            THROW_IF_WIN32_BOOL_FALSE(QueryServiceStatusEx(
                hService.get(),
                SC_STATUS_PROCESS_INFO,
                reinterpret_cast<LPBYTE>(&statusStop), // Use same struct
                sizeof(statusStop),
                &bytesNeeded));
        }

        break;
    }
    default:
        wprintf(L"unexpected state %lu, not stopping\n", ssp.dwCurrentState);
    }

    THROW_IF_WIN32_BOOL_FALSE(DeleteService(hService.get()));

    driverLoaded = false;
}
