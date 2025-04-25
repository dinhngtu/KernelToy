// SPDX-License-Identifier: GPL-3.0-only

#include "stdafx.h"
#include "Driver.hpp"

struct DriverServiceConfig {
    std::wstring driverPath;
    std::wstring serviceName;

    // Default constructor
    DriverServiceConfig() = default;

    // Constructor used by generation function
    DriverServiceConfig(std::wstring path, std::wstring svcName, std::wstring dispName)
        : driverPath(std::move(path)),
        serviceName(std::move(svcName)) {
    }
};

static constexpr const DWORD ServiceStopTimeout = 30000;
static constexpr const wchar_t* DriverName = L"KernelToy.sys";

static std::wstring GenerateRandomAlphanumeric(size_t length) {
    const std::wstring charset = L"abcdefghijklmnopqrstuvwxyz234567";
    std::random_device rd;
    // Use Mersenne Twister seeded with random_device
    std::mt19937 generator(rd());
    // Uniform distribution over the character set indices
    std::uniform_int_distribution<> distribution(0, static_cast<int>(charset.length() - 1));

    std::wstring result;
    result.reserve(length);
    for (size_t i = 0; i < length; ++i) {
        result += charset[distribution(generator)];
    }
    return result;
}

// --- Helper: Generation Logic ---
static DriverServiceConfig GenerateDriverServiceConfigInternal() {
    // 1. Determine executable path dynamically
    std::vector<wchar_t> buffer(MAX_PATH);
    DWORD pathLen = 0;
    do {
        pathLen = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
        if (pathLen == 0) {
            THROW_LAST_ERROR_MSG("GetModuleFileNameW failed to get executable path.");
        }
        else if (pathLen == buffer.size()) {
            // Buffer was too small, resize and try again
             // Prevent infinite loops and excessive allocation
            if (buffer.size() > 32767) { // Check against MAX_UNICODE_PATH limit
                THROW_HR_MSG(E_OUTOFMEMORY, "Executable path exceeds reasonable limits or GetModuleFileNameW loop failed.");
            }
            buffer.resize(buffer.size() * 2);
        }
    } while (pathLen == buffer.size());


    // 2. Construct full driver path using <filesystem>
    try {
        std::filesystem::path exePath(buffer.data(), buffer.data() + pathLen);
        std::filesystem::path driverDir = exePath.parent_path();
        std::filesystem::path fullDriverPath = driverDir / DriverName;

        // 3. Generate random service name
        std::wstring randomPart = GenerateRandomAlphanumeric(8);
        std::wstring serviceName = L"KernelToy_" + randomPart;
        std::wstring displayName = L"Kernel Toy Driver Service (" + randomPart + L")";

        return DriverServiceConfig(fullDriverPath.wstring(), serviceName, displayName);

    }
    catch (const std::filesystem::filesystem_error& fs_err) {
        std::string narrow_what = fs_err.what(); // Convert potentially narrow char* message
        std::wstring wide_what(narrow_what.begin(), narrow_what.end());
        THROW_HR_MSG(HRESULT_FROM_WIN32(ERROR_PATH_NOT_FOUND), "Filesystem error processing path: %ls", wide_what.c_str());
    }
    catch (...) {
        THROW_HR_MSG(E_UNEXPECTED, "Unexpected error during path construction.");
    }

}

// --- Lazy Singleton Accessor ---
// Using C++11 magic static for thread-safe lazy initialization
static const DriverServiceConfig& GetDriverServiceConfig() {
    static DriverServiceConfig config = GenerateDriverServiceConfigInternal();
    return config;
}

void ensure_driver_loaded() {
    wprintf(L"starting %s\n", GetDriverServiceConfig().serviceName.c_str());

    wil::unique_schandle hSCManager(THROW_LAST_ERROR_IF_NULL(
        OpenSCManagerW(
            nullptr,                         // Local machine
            nullptr,                         // Default database (ServicesActive)
            SC_MANAGER_CONNECT | SC_MANAGER_CREATE_SERVICE // Permissions needed
        )
    ));

    wil::unique_schandle hService(THROW_LAST_ERROR_IF_NULL(CreateServiceW(
        hSCManager.get(),         // SCM database handle
        GetDriverServiceConfig().serviceName.c_str(),              // Name of service (internal)
        DriverName,             // Name to display
        SERVICE_START | SERVICE_QUERY_STATUS, // Required access rights
        SERVICE_KERNEL_DRIVER,    // Service type for drivers
        SERVICE_DEMAND_START,     // Start type (manual/programmatic start)
        SERVICE_ERROR_NORMAL,     // Error control (log errors)
        GetDriverServiceConfig().driverPath.c_str(),               // Path to the driver binary (.sys)
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
            &bytesNeeded
        )
    );

    if (ssp.dwCurrentState == SERVICE_RUNNING) {
        // Service is already running, our goal is met.
    }
    else if (ssp.dwCurrentState == SERVICE_STOPPED || ssp.dwCurrentState == SERVICE_STOP_PENDING || ssp.dwCurrentState == SERVICE_START_PENDING) {
        // If stop/start pending, wait briefly for it to finish before starting
        if (ssp.dwCurrentState != SERVICE_STOPPED) {
            wprintf(L"Waiting briefly for service stop to complete...\n");
            std::this_thread::sleep_for(std::chrono::milliseconds(ssp.dwWaitHint > 0 ? ssp.dwWaitHint : 1000));
            // Re-query might be prudent in a complex scenario, but StartService handles some states
        }

        if (ssp.dwCurrentState != SERVICE_START_PENDING) {
            // Attempt to start the service
            try {
                THROW_IF_WIN32_BOOL_FALSE(StartServiceW(hService.get(), 0, nullptr));
                // Check Event Log for actual driver load status
            }
            catch (const wil::ResultException& e) {
                // StartService can fail with ERROR_SERVICE_ALREADY_RUNNING if there's a race or state change
                if (e.GetErrorCode() == ERROR_SERVICE_ALREADY_RUNNING) {
                    wprintf(L"Service '%s' started between status check and StartService call, or was already running.\n", GetDriverServiceConfig().serviceName.c_str());
                    // This is considered success in the context of "ensure running"
                }
                else {
                    throw; // Re-throw other errors
                }
            }
        }
    }
    else {
        throw std::runtime_error("unexpected service status");
    }
}

void ensure_driver_unloaded() {
    wprintf(L"deleting %s\n", GetDriverServiceConfig().serviceName.c_str());

    wil::unique_schandle hSCManager(THROW_LAST_ERROR_IF_NULL(
        OpenSCManagerW(nullptr, nullptr, SC_MANAGER_CONNECT)
    ));

    wil::unique_schandle hService;
    try {
        hService.reset(THROW_LAST_ERROR_IF_NULL(
            OpenServiceW(
                hSCManager.get(),
                GetDriverServiceConfig().serviceName.c_str(),
                SERVICE_QUERY_STATUS | SERVICE_STOP | DELETE)
        ));
    }
    catch (const wil::ResultException& e) {
        // Make service not found non-fatal for deletion purposes if needed, but keep as error for now
        if (e.GetErrorCode() == HRESULT_FROM_WIN32(ERROR_SERVICE_DOES_NOT_EXIST)) {
            return; // Successfully did nothing :)
        }
        throw; // Re-throw other open errors
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

    if (ssp.dwCurrentState == SERVICE_RUNNING || ssp.dwCurrentState == SERVICE_PAUSED || ssp.dwCurrentState == SERVICE_START_PENDING) {
        SERVICE_STATUS statusStop = {}; // To receive status after control message
        THROW_IF_WIN32_BOOL_FALSE(
            ControlService(hService.get(), SERVICE_CONTROL_STOP, &statusStop)
        );

        // Wait for the service to stop
        auto startTime = std::chrono::steady_clock::now();
        DWORD lastCheckPoint = statusStop.dwCheckPoint; // Use checkpoint from control result

        while (statusStop.dwCurrentState != SERVICE_STOPPED) {
            // Calculate time elapsed
            auto now = std::chrono::steady_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - startTime).count();

            // Check for timeout
            if (static_cast<DWORD>(elapsed) > ServiceStopTimeout) {
                THROW_WIN32(ERROR_TIMEOUT);
            }

            // Determine wait time before next query
            DWORD waitHint = statusStop.dwWaitHint / 10; // Poll more frequently than hint suggests
            if (waitHint < 100) waitHint = 100;         // Minimum wait
            if (waitHint > 3000) waitHint = 3000;       // Maximum wait between polls
            std::this_thread::sleep_for(std::chrono::milliseconds(waitHint));

            // Query status again
            THROW_IF_WIN32_BOOL_FALSE(
                QueryServiceStatusEx(
                    hService.get(),
                    SC_STATUS_PROCESS_INFO,
                    reinterpret_cast<LPBYTE>(&statusStop), // Use same struct
                    sizeof(statusStop),
                    &bytesNeeded)
            );

            // Optional: Check if checkpoint is progressing if stop is taking long
            if (statusStop.dwCurrentState == SERVICE_STOP_PENDING) {
                if (statusStop.dwCheckPoint > lastCheckPoint) {
                    // Progress is being made
                    //wprintf(L"  CheckPoint: %lu\n", statusStop.dwCheckPoint); // Uncomment for debug
                    lastCheckPoint = statusStop.dwCheckPoint;
                    startTime = std::chrono::steady_clock::now(); // Reset timeout start if progress occurs? Or keep overall timeout? Let's keep overall.
                }
                // else: Checkpoint hasn't increased, could be stuck, timeout will handle it.
            }
        } // end while waiting for stop
    }
    else if (ssp.dwCurrentState == SERVICE_STOPPED) {
        // already stopped
    }
    else {
        wprintf(L"Service '%s' is in state %lu, not attempting stop.\n", GetDriverServiceConfig().serviceName.c_str(), ssp.dwCurrentState);
        // Proceed directly to delete if appropriate for these states? Or error out?
        // Let's proceed to delete for simplicity here. It might fail if the state is bad.
    }

    THROW_IF_WIN32_BOOL_FALSE(DeleteService(hService.get()));
}
