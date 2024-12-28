#include "ProcessManager.h"

PROCESS_MANAGER g_ProcessManager;

NTSTATUS InitializeProcessManager() {
    InitializeListHead(&g_ProcessManager.ProcessList);
    ExInitializeFastMutex(&g_ProcessManager.Lock);
    g_ProcessManager.TotalProcesses = 0;
    return STATUS_SUCCESS;
}

VOID DestroyProcessManager() {
    PLIST_ENTRY entry;
    ExAcquireFastMutex(&g_ProcessManager.Lock);
    
    while (!IsListEmpty(&g_ProcessManager.ProcessList)) {
        entry = RemoveHeadList(&g_ProcessManager.ProcessList);
        PPROCESS_CONTEXT ctx = CONTAINING_RECORD(entry, PROCESS_CONTEXT, Entry);
        ObDereferenceObject(ctx->Process);
        ExFreePool(ctx);
    }
    
    ExReleaseFastMutex(&g_ProcessManager.Lock);
}

NTSTATUS AttachToProcess(HANDLE ProcessId) {
    PEPROCESS targetProcess;
    PPROCESS_CONTEXT processContext;
    NTSTATUS status;
    
    status = PsLookupProcessByProcessId(ProcessId, &targetProcess);
    if (!NT_SUCCESS(status)) {
        return status;
    }
    
    processContext = ExAllocatePool(NonPagedPool, sizeof(PROCESS_CONTEXT));
    if (!processContext) {
        ObDereferenceObject(targetProcess);
        return STATUS_INSUFFICIENT_RESOURCES;
    }
    
    processContext->ProcessId = ProcessId;
    processContext->Process = targetProcess;
    processContext->ProcessCr3.Flags = __readcr3();
    
    ExAcquireFastMutex(&g_ProcessManager.Lock);
    InsertTailList(&g_ProcessManager.ProcessList, &processContext->Entry);
    g_ProcessManager.TotalProcesses++;
    ExReleaseFastMutex(&g_ProcessManager.Lock);
    
    return STATUS_SUCCESS;
}

NTSTATUS DetachFromProcess(HANDLE ProcessId) {
    PLIST_ENTRY entry;
    PPROCESS_CONTEXT processContext = NULL;
    
    ExAcquireFastMutex(&g_ProcessManager.Lock);
    
    for (entry = g_ProcessManager.ProcessList.Flink;
         entry != &g_ProcessManager.ProcessList;
         entry = entry->Flink) {
        processContext = CONTAINING_RECORD(entry, PROCESS_CONTEXT, Entry);
        if (processContext->ProcessId == ProcessId) {
            RemoveEntryList(&processContext->Entry);
            g_ProcessManager.TotalProcesses--;
            break;
        }
    }
    
    ExReleaseFastMutex(&g_ProcessManager.Lock);
    
    if (!processContext) {
        return STATUS_NOT_FOUND;
    }
    
    ObDereferenceObject(processContext->Process);
    ExFreePool(processContext);
    return STATUS_SUCCESS;
}

NTSTATUS GetProcessCr3(HANDLE ProcessId, CR3* Cr3) {
    PLIST_ENTRY entry;
    PPROCESS_CONTEXT processContext = NULL;
    NTSTATUS status = STATUS_NOT_FOUND;
    
    ExAcquireFastMutex(&g_ProcessManager.Lock);
    
    for (entry = g_ProcessManager.ProcessList.Flink;
         entry != &g_ProcessManager.ProcessList;
         entry = entry->Flink) {
        processContext = CONTAINING_RECORD(entry, PROCESS_CONTEXT, Entry);
        if (processContext->ProcessId == ProcessId) {
            *Cr3 = processContext->ProcessCr3;
            status = STATUS_SUCCESS;
            break;
        }
    }
    
    ExReleaseFastMutex(&g_ProcessManager.Lock);
    return status;
}

NTSTATUS SwitchProcessContext(HANDLE TargetProcessId) {
    PPROCESS_CONTEXT processContext = NULL;
    PLIST_ENTRY entry;
    NTSTATUS status = STATUS_NOT_FOUND;
    
    ExAcquireFastMutex(&g_ProcessManager.Lock);
    
    for (entry = g_ProcessManager.ProcessList.Flink;
         entry != &g_ProcessManager.ProcessList;
         entry = entry->Flink) {
        processContext = CONTAINING_RECORD(entry, PROCESS_CONTEXT, Entry);
        if (processContext->ProcessId == TargetProcessId) {
            __writecr3(processContext->ProcessCr3.Flags);
            status = STATUS_SUCCESS;
            break;
        }
    }
    
    ExReleaseFastMutex(&g_ProcessManager.Lock);
    return status;
}
