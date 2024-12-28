#pragma once
#include <ntdef.h>
#include <ntifs.h>
#include <windef.h>
#include "../Ext/PageTable/PageTableStructures.h"

typedef enum _UNMARK_PROTECTION_MODE {
    UnmarkProtectNone = 0,
    UnmarkProtectRW = PAGE_READWRITE,
    UnmarkProtectRX = PAGE_EXECUTE_READ,
    UnmarkProtectRO = PAGE_READONLY,
    UnmarkProtectHiddenRWX = PAGE_EXECUTE_READWRITE,
    UnmarkProtectCustom = 0x1000
} UNMARK_PROTECTION_MODE;

typedef struct _PROTECTION_REQUEST {
    PVOID TargetAddress;
    SIZE_T RegionSize;
    UNMARK_PROTECTION_MODE ProtectionMode;
    BOOLEAN PreservePhysicalMapping;
    HANDLE TargetProcessId;
} PROTECTION_REQUEST, *PPROTECTION_REQUEST;

#define UNMARK_DEVICE_TYPE 0x8000

//Request codes:
#define REQUEST___PROTECT_REGION CTL_CODE(UNMARK_DEVICE_TYPE, 0x800, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define REQUEST___UNPROTECT_REGION CTL_CODE(UNMARK_DEVICE_TYPE, 0x801, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define REQUEST___ATTACH_PROCESS CTL_CODE(UNMARK_DEVICE_TYPE, 0x802, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define REQUEST___DETACH_PROCESS CTL_CODE(UNMARK_DEVICE_TYPE, 0x803, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define REQUEST___GET_PROCESS_CR3 CTL_CODE(UNMARK_DEVICE_TYPE, 0x804, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define REQUEST___SWITCH_PROCESS_CONTEXT CTL_CODE(UNMARK_DEVICE_TYPE, 0x805, METHOD_BUFFERED, FILE_ANY_ACCESS)