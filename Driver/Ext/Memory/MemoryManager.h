#pragma once
#include <ntdef.h>
#include <ntifs.h>

typedef struct _PHYSICAL_ADDRESS_ENTRY {
    PHYSICAL_ADDRESS PhysicalAddress;
    SIZE_T Size;
    LIST_ENTRY Entry;
} PHYSICAL_ADDRESS_ENTRY, *PPHYSICAL_ADDRESS_ENTRY;

typedef struct _VIRTUAL_MEMORY_REGION {
    PVOID VirtualAddress;
    PHYSICAL_ADDRESS PhysicalAddress;
    SIZE_T Size;
    CR3 ProcessCr3;
    LIST_ENTRY Entry;
} VIRTUAL_MEMORY_REGION, *PVIRTUAL_MEMORY_REGION;

typedef struct _MEMORY_MANAGER {
    LIST_ENTRY PhysicalAddressList;
    LIST_ENTRY VirtualRegionList;
    FAST_MUTEX Lock;
    ULONG64 TotalManagedRegions;
} MEMORY_MANAGER, *PMEMORY_MANAGER;

NTSTATUS InitializeMemoryManager();
VOID DestroyMemoryManager();
NTSTATUS MapPhysicalMemory(PHYSICAL_ADDRESS PhysicalAddress, SIZE_T Size, PVOID* VirtualAddress);
NTSTATUS UnmapPhysicalMemory(PVOID VirtualAddress);
NTSTATUS GetPhysicalAddress(PVOID VirtualAddress, PHYSICAL_ADDRESS* PhysicalAddress);
NTSTATUS ModifyPageTableEntry(PVOID VirtualAddress, ULONG Protection, BOOLEAN IgnoreProtection);
