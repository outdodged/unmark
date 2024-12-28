#include "../Includes/UnmarkDefinitions.h"
#include "../Ext/Memory/MemoryManager.h"
#include "../Ext/Process/ProcessManager.h"
#include "../Ext/PageTable/PageTableStructures.h"

UNMARK_GLOBAL_DATA GlobalData = { 0 };
PDEVICE_OBJECT DeviceObject = NULL;
UNICODE_STRING DeviceName = RTL_CONSTANT_STRING(L"\\Device\\Unmark");
UNICODE_STRING SymbolicLink = RTL_CONSTANT_STRING(L"\\??\\Unmark");

NTSTATUS UnmarkCreateClose(PDEVICE_OBJECT DeviceObject, PIRP Irp) {
    Irp->IoStatus.Status = STATUS_SUCCESS;
    Irp->IoStatus.Information = 0;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);
    return STATUS_SUCCESS;
}

NTSTATUS UnmarkDeviceControl(PDEVICE_OBJECT DeviceObject, PIRP Irp) {
    PIO_STACK_LOCATION stack = IoGetCurrentIrpStackLocation(Irp);
    NTSTATUS status = STATUS_SUCCESS;
    ULONG code = stack->Parameters.DeviceIoControl.IoControlCode;
    
    switch (code) {
        case REQUEST___PROTECT_REGION: {
            if (stack->Parameters.DeviceIoControl.InputBufferLength < sizeof(PROTECTION_REQUEST)) {
                status = STATUS_BUFFER_TOO_SMALL;
                break;
            }
            
            PPROTECTION_REQUEST req = (PPROTECTION_REQUEST)Irp->AssociatedIrp.SystemBuffer;
            CR3 cr3; cr3.Flags = __readcr3();
            
            status = ModifyPageTableEntry(req->TargetAddress, 
                req->ProtectionMode == UnmarkProtectHiddenRWX ? PAGE_EXECUTE_READWRITE : req->ProtectionMode,
                req->ProtectionMode == UnmarkProtectHiddenRWX);
            break;
        }
        
        case REQUEST___UNPROTECT_REGION: {
            if (stack->Parameters.DeviceIoControl.InputBufferLength < sizeof(PVOID)) {
                status = STATUS_BUFFER_TOO_SMALL;
                break;
            }
            
            PVOID* addr = (PVOID*)Irp->AssociatedIrp.SystemBuffer;
            status = ModifyPageTableEntry(*addr, PAGE_EXECUTE_READWRITE, FALSE);
            break;
        }
        
        case REQUEST___ATTACH_PROCESS: {
            HANDLE* pid = (HANDLE*)Irp->AssociatedIrp.SystemBuffer;
            status = AttachToProcess(*pid);
            break;
        }
        
        case REQUEST___DETACH_PROCESS: {
            HANDLE* pid = (HANDLE*)Irp->AssociatedIrp.SystemBuffer;
            status = DetachFromProcess(*pid);
            break;
        }
        
        case REQUEST___GET_PROCESS_CR3: {
            HANDLE* pid = (HANDLE*)Irp->AssociatedIrp.SystemBuffer;
            CR3 cr3;
            status = GetProcessCr3(*pid, &cr3);
            break;
        }
        
        case REQUEST___SWITCH_PROCESS_CONTEXT: {
            HANDLE* pid = (HANDLE*)Irp->AssociatedIrp.SystemBuffer;
            status = SwitchProcessContext(*pid);
            break;
        }
        
        default:
            status = STATUS_INVALID_DEVICE_REQUEST;
            break;
    }
    
    Irp->IoStatus.Status = status;
    Irp->IoStatus.Information = 0;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);
    return status;
}

VOID UnmarkUnload(PDRIVER_OBJECT DriverObject) {
    DestroyMemoryManager();
    DestroyProcessManager();
    IoDeleteSymbolicLink(&SymbolicLink);
    IoDeleteDevice(DriverObject->DeviceObject);
}

NTSTATUS DriverEntry(PDRIVER_OBJECT DriverObject, PUNICODE_STRING RegistryPath) {
    NTSTATUS status;
    
    InitializeListHead(&GlobalData.ProtectedRegions);
    ExInitializeFastMutex(&GlobalData.RegionLock);
    
    status = InitializeMemoryManager();
    if (!NT_SUCCESS(status)) return status;
    
    status = InitializeProcessManager();
    if (!NT_SUCCESS(status)) {
        DestroyMemoryManager();
        return status;
    }
    
    DriverObject->MajorFunction[IRP_MJ_CREATE] = UnmarkCreateClose;
    DriverObject->MajorFunction[IRP_MJ_CLOSE] = UnmarkCreateClose;
    DriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] = UnmarkDeviceControl;
    DriverObject->DriverUnload = UnmarkUnload;
    
    status = IoCreateDevice(DriverObject, 0, &DeviceName, FILE_DEVICE_UNKNOWN,
                          FILE_DEVICE_SECURE_OPEN, FALSE, &DeviceObject);
                          
    if (!NT_SUCCESS(status)) {
        DestroyMemoryManager();
        DestroyProcessManager();
        return status;
    }
    
    status = IoCreateSymbolicLink(&SymbolicLink, &DeviceName);
    if (!NT_SUCCESS(status)) {
        DestroyMemoryManager();
        DestroyProcessManager();
        IoDeleteDevice(DeviceObject);
        return status;
    }
    
    GlobalData.DriverInitialized = TRUE;
    return STATUS_SUCCESS;
}

NTSTATUS ManageMemoryProtection(PVOID BaseAddress, SIZE_T RegionSize, ULONG NewProtection, ULONG DisplayProtection) {
    NTSTATUS status;
    PMDL mdl;
    PVOID mappedAddress;

    mdl = IoAllocateMdl(BaseAddress, (ULONG)RegionSize, FALSE, FALSE, NULL);
    if (!mdl) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    __try {
        MmProbeAndLockPages(mdl, KernelMode, IoReadAccess);
        mappedAddress = MmMapLockedPagesSpecifyCache(mdl, KernelMode, MmNonCached, NULL, FALSE, NormalPagePriority);
        
        if (mappedAddress) {
            ULONG oldProtect;
            status = MmProtectMdlSystemAddress(mdl, NewProtection);
            
            if (NT_SUCCESS(status)) {
                PPROTECTED_REGION region = ExAllocatePool(NonPagedPool, sizeof(PROTECTED_REGION));
                if (region) {
                    region->BaseAddress = BaseAddress;
                    region->RegionSize = RegionSize;
                    region->OriginalProtection = NewProtection;
                    region->DisplayedProtection = DisplayProtection;
                    
                    ExAcquireFastMutex(&GlobalData.RegionLock);
                    InsertTailList(&GlobalData.ProtectedRegions, &region->RegionList);
                    ExReleaseFastMutex(&GlobalData.RegionLock);
                }
            }
            
            MmUnmapLockedPages(mappedAddress, mdl);
        } else {
            status = STATUS_INSUFFICIENT_RESOURCES;
        }
        
        MmUnlockPages(mdl);
    }
    __except(EXCEPTION_EXECUTE_HANDLER) {
        status = GetExceptionCode();
    }
    
    IoFreeMdl(mdl);
    return status;
}

NTSTATUS ProtectMemoryRegion(PPROTECTION_REQUEST Request) {
    ULONG realProtection = PAGE_EXECUTE_READWRITE; 
    ULONG displayProtection;
    
    switch (Request->ProtectionMode) {
        case UnmarkProtectRW:
            displayProtection = PAGE_READWRITE;
            break;
        case UnmarkProtectRX:
            displayProtection = PAGE_EXECUTE_READ;
            break;
        case UnmarkProtectRO:
            displayProtection = PAGE_READONLY;
            break;
        case UnmarkProtectHiddenRWX:
            displayProtection = PAGE_EXECUTE_READWRITE;
            break;
        default:
            return STATUS_INVALID_PARAMETER;
    }
    
    return ManageMemoryProtection(Request->TargetAddress,
                                Request->RegionSize,
                                realProtection,
                                displayProtection);
}

NTSTATUS UnprotectMemoryRegion(PVOID BaseAddress) {
    PLIST_ENTRY entry;
    PPROTECTED_REGION region = NULL;
    
    ExAcquireFastMutex(&GlobalData.RegionLock);
    for (entry = GlobalData.ProtectedRegions.Flink;
         entry != &GlobalData.ProtectedRegions;
         entry = entry->Flink) {
        PPROTECTED_REGION currentRegion = CONTAINING_RECORD(entry, PROTECTED_REGION, RegionList);
        if (currentRegion->BaseAddress == BaseAddress) {
            region = currentRegion;
            RemoveEntryList(&region->RegionList);
            break;
        }
    }
    ExReleaseFastMutex(&GlobalData.RegionLock);
    
    if (!region) {
        return STATUS_NOT_FOUND;
    }
    
    //Restore original protection
    NTSTATUS status = ManageMemoryProtection(region->BaseAddress,
                                           region->RegionSize,
                                           PAGE_EXECUTE_READWRITE,
                                           PAGE_EXECUTE_READWRITE);
    
    ExFreePool(region);
    return status;
}
