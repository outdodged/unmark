#include "MemoryManager.h"
#include "../../Includes/UnmarkDefinitions.h"

MEMORY_MANAGER g_MemoryManager;

NTSTATUS InitializeMemoryManager() {
    InitializeListHead(&g_MemoryManager.PhysicalAddressList);
    InitializeListHead(&g_MemoryManager.VirtualRegionList);
    ExInitializeFastMutex(&g_MemoryManager.Lock);
    g_MemoryManager.TotalManagedRegions = 0;
    return STATUS_SUCCESS;
}

VOID DestroyMemoryManager() {
    PLIST_ENTRY entry;
    ExAcquireFastMutex(&g_MemoryManager.Lock);
    
    while (!IsListEmpty(&g_MemoryManager.PhysicalAddressList)) {
        entry = RemoveHeadList(&g_MemoryManager.PhysicalAddressList);
        ExFreePool(CONTAINING_RECORD(entry, PHYSICAL_ADDRESS_ENTRY, Entry));
    }
    
    while (!IsListEmpty(&g_MemoryManager.VirtualRegionList)) {
        entry = RemoveHeadList(&g_MemoryManager.VirtualRegionList);
        ExFreePool(CONTAINING_RECORD(entry, VIRTUAL_MEMORY_REGION, Entry));
    }
    
    ExReleaseFastMutex(&g_MemoryManager.Lock);
}

NTSTATUS MapPhysicalMemory(PHYSICAL_ADDRESS PhysicalAddress, SIZE_T Size, PVOID* VirtualAddress) {
    PVOID mappedAddress;
    PPHYSICAL_ADDRESS_ENTRY entry;
    
    mappedAddress = MmMapIoSpace(PhysicalAddress, Size, MmNonCached);
    if (!mappedAddress) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }
    
    entry = ExAllocatePool(NonPagedPool, sizeof(PHYSICAL_ADDRESS_ENTRY));
    if (!entry) {
        MmUnmapIoSpace(mappedAddress, Size);
        return STATUS_INSUFFICIENT_RESOURCES;
    }
    
    entry->PhysicalAddress = PhysicalAddress;
    entry->Size = Size;
    
    ExAcquireFastMutex(&g_MemoryManager.Lock);
    InsertTailList(&g_MemoryManager.PhysicalAddressList, &entry->Entry);
    g_MemoryManager.TotalManagedRegions++;
    ExReleaseFastMutex(&g_MemoryManager.Lock);
    
    *VirtualAddress = mappedAddress;
    return STATUS_SUCCESS;
}

NTSTATUS UnmapPhysicalMemory(PVOID VirtualAddress) {
    PLIST_ENTRY entry;
    PPHYSICAL_ADDRESS_ENTRY physEntry = NULL;
    
    ExAcquireFastMutex(&g_MemoryManager.Lock);
    
    for (entry = g_MemoryManager.PhysicalAddressList.Flink;
         entry != &g_MemoryManager.PhysicalAddressList;
         entry = entry->Flink) {
        physEntry = CONTAINING_RECORD(entry, PHYSICAL_ADDRESS_ENTRY, Entry);
        if (VirtualAddress == MmMapIoSpace(physEntry->PhysicalAddress, physEntry->Size, MmNonCached)) {
            RemoveEntryList(&physEntry->Entry);
            g_MemoryManager.TotalManagedRegions--;
            break;
        }
    }
    
    ExReleaseFastMutex(&g_MemoryManager.Lock);
    
    if (!physEntry) {
        return STATUS_NOT_FOUND;
    }
    
    MmUnmapIoSpace(VirtualAddress, physEntry->Size);
    ExFreePool(physEntry);
    return STATUS_SUCCESS;
}

NTSTATUS GetPhysicalAddress(PVOID VirtualAddress, PHYSICAL_ADDRESS* PhysicalAddress) {
    PHYSICAL_ADDRESS physAddr;
    
    if (!MmIsAddressValid(VirtualAddress)) {
        return STATUS_INVALID_ADDRESS;
    }
    
    physAddr = MmGetPhysicalAddress(VirtualAddress);
    if (physAddr.QuadPart == 0) {
        return STATUS_UNSUCCESSFUL;
    }
    
    *PhysicalAddress = physAddr;
    return STATUS_SUCCESS;
}

NTSTATUS ModifyPageTableEntry(PVOID VirtualAddress, ULONG Protection, BOOLEAN IgnoreProtection) {
    CR3 cr3;
    PHYSICAL_ADDRESS pml4PhysAddr;
    PVOID pml4VirtAddr;
    PML4E* pml4e;
    PDPTE* pdpte;
    PDE* pde;
    PTE* pte;
    
    __try {
        cr3.Flags = __readcr3();
        pml4PhysAddr.QuadPart = cr3.AddressOfPageDirectory << PAGE_SHIFT;
        
        pml4VirtAddr = MmMapIoSpace(pml4PhysAddr, PAGE_SIZE, MmNonCached);
        if (!pml4VirtAddr) {
            return STATUS_INSUFFICIENT_RESOURCES;
        }
        
        pml4e = (PML4E*)pml4VirtAddr + PML4_INDEX(VirtualAddress);
        if (!pml4e->Present) {
            MmUnmapIoSpace(pml4VirtAddr, PAGE_SIZE);
            return STATUS_PTE_NOT_PRESENT;
        }
        
        PHYSICAL_ADDRESS pdptPhysAddr;
        pdptPhysAddr.QuadPart = pml4e->PageFrameNumber << PAGE_SHIFT;
        PVOID pdptVirtAddr = MmMapIoSpace(pdptPhysAddr, PAGE_SIZE, MmNonCached);
        
        if (!pdptVirtAddr) {
            MmUnmapIoSpace(pml4VirtAddr, PAGE_SIZE);
            return STATUS_INSUFFICIENT_RESOURCES;
        }
        
        pdpte = (PDPTE*)pdptVirtAddr + PDPT_INDEX(VirtualAddress);
        if (!pdpte->Present) {
            MmUnmapIoSpace(pdptVirtAddr, PAGE_SIZE);
            MmUnmapIoSpace(pml4VirtAddr, PAGE_SIZE);
            return STATUS_PTE_NOT_PRESENT;
        }
        
        PHYSICAL_ADDRESS pdPhysAddr;
        pdPhysAddr.QuadPart = pdpte->PageFrameNumber << PAGE_SHIFT;
        PVOID pdVirtAddr = MmMapIoSpace(pdPhysAddr, PAGE_SIZE, MmNonCached);
        
        if (!pdVirtAddr) {
            MmUnmapIoSpace(pdptVirtAddr, PAGE_SIZE);
            MmUnmapIoSpace(pml4VirtAddr, PAGE_SIZE);
            return STATUS_INSUFFICIENT_RESOURCES;
        }
        
        pde = (PDE*)pdVirtAddr + PD_INDEX(VirtualAddress);
        if (!pde->Present) {
            MmUnmapIoSpace(pdVirtAddr, PAGE_SIZE);
            MmUnmapIoSpace(pdptVirtAddr, PAGE_SIZE);
            MmUnmapIoSpace(pml4VirtAddr, PAGE_SIZE);
            return STATUS_PTE_NOT_PRESENT;
        }
        
        PHYSICAL_ADDRESS ptPhysAddr;
        ptPhysAddr.QuadPart = pde->PageFrameNumber << PAGE_SHIFT;
        PVOID ptVirtAddr = MmMapIoSpace(ptPhysAddr, PAGE_SIZE, MmNonCached);
        
        if (!ptVirtAddr) {
            MmUnmapIoSpace(pdVirtAddr, PAGE_SIZE);
            MmUnmapIoSpace(pdptVirtAddr, PAGE_SIZE);
            MmUnmapIoSpace(pml4VirtAddr, PAGE_SIZE);
            return STATUS_INSUFFICIENT_RESOURCES;
        }
        
        pte = (PTE*)ptVirtAddr + PT_INDEX(VirtualAddress);
        
        if (IgnoreProtection) {
            pte->Write = 1;
            pte->NoExecute = 0;
        } else {
            pte->Write = (Protection & PAGE_READWRITE) != 0;
            pte->NoExecute = (Protection & PAGE_EXECUTE) == 0;
        }
        
        MmUnmapIoSpace(ptVirtAddr, PAGE_SIZE);
        MmUnmapIoSpace(pdVirtAddr, PAGE_SIZE);
        MmUnmapIoSpace(pdptVirtAddr, PAGE_SIZE);
        MmUnmapIoSpace(pml4VirtAddr, PAGE_SIZE);
        
        __invlpg(VirtualAddress);
        
        return STATUS_SUCCESS;
    }
    __except(EXCEPTION_EXECUTE_HANDLER) {
        return GetExceptionCode();
    }
}
