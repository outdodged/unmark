#include <Windows.h>
#include <iostream>
#include <vector>
#include "../../Includes/UnmarkDefinitions.h"

class UnmarkDriver {
private:
    HANDLE hDriver;

public:
    UnmarkDriver() : hDriver(INVALID_HANDLE_VALUE) {}

    bool Initialize() {
        hDriver = CreateFile(L"\\\\.\\Unmark", GENERIC_READ | GENERIC_WRITE, 
            FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
        return hDriver != INVALID_HANDLE_VALUE;
    }

    ~UnmarkDriver() {
        if (hDriver != INVALID_HANDLE_VALUE) CloseHandle(hDriver);
    }

    bool ProtectRegion(PVOID address, SIZE_T size, UNMARK_PROTECTION_MODE mode, HANDLE targetPid = NULL) {
        PROTECTION_REQUEST req = {};
        req.TargetAddress = address;
        req.RegionSize = size;
        req.ProtectionMode = mode;
        req.PreservePhysicalMapping = TRUE;
        req.TargetProcessId = targetPid;

        DWORD bytes;
        return DeviceIoControl(hDriver, REQUEST___PROTECT_REGION, &req, sizeof(req), 
            NULL, 0, &bytes, NULL);
    }

    bool UnprotectRegion(PVOID address) {
        DWORD bytes;
        return DeviceIoControl(hDriver, REQUEST___UNPROTECT_REGION, &address, 
            sizeof(PVOID), NULL, 0, &bytes, NULL);
    }

    bool AttachToProcess(HANDLE pid) {
        DWORD bytes;
        return DeviceIoControl(hDriver, REQUEST___ATTACH_PROCESS, &pid, 
            sizeof(HANDLE), NULL, 0, &bytes, NULL);
    }

    bool DetachFromProcess(HANDLE pid) {
        DWORD bytes;
        return DeviceIoControl(hDriver, REQUEST___DETACH_PROCESS, &pid, 
            sizeof(HANDLE), NULL, 0, &bytes, NULL);
    }
};

void TestMemoryProtection() {
    UnmarkDriver drv;
    if (!drv.Initialize()) return;

    PVOID mem = VirtualAlloc(NULL, PAGE_SIZE * 2, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (!mem) return;

    memset(mem, 0xCC, PAGE_SIZE * 2);

    if (drv.ProtectRegion(mem, PAGE_SIZE, UnmarkProtectRO)) {
        __try {
            *(PBYTE)mem = 0xAA;
        } __except(EXCEPTION_EXECUTE_HANDLER) {}
    }

    if (drv.ProtectRegion((PBYTE)mem + PAGE_SIZE, PAGE_SIZE, UnmarkProtectRX)) {
        __try {
            *((PBYTE)mem + PAGE_SIZE) = 0xBB;
        } __except(EXCEPTION_EXECUTE_HANDLER) {}
    }

    DWORD pid = GetCurrentProcessId();
    if (drv.AttachToProcess((HANDLE)pid)) {
        drv.ProtectRegion(mem, PAGE_SIZE * 2, UnmarkProtectHiddenRWX);
        drv.DetachFromProcess((HANDLE)pid);
    }

    drv.UnprotectRegion(mem);
    drv.UnprotectRegion((PBYTE)mem + PAGE_SIZE);
    VirtualFree(mem, 0, MEM_RELEASE);
}

int main() {
    TestMemoryProtection();
    return 0;
}
