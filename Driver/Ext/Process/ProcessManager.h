#pragma once
#include <ntdef.h>
#include <ntifs.h>

typedef struct _PROCESS_CONTEXT {
    HANDLE ProcessId;
    PEPROCESS Process;
    CR3 ProcessCr3;
    LIST_ENTRY Entry;
} PROCESS_CONTEXT, *PPROCESS_CONTEXT;

typedef struct _PROCESS_MANAGER {
    LIST_ENTRY ProcessList;
    FAST_MUTEX Lock;
    ULONG64 TotalProcesses;
} PROCESS_MANAGER, *PPROCESS_MANAGER;

NTSTATUS InitializeProcessManager();
VOID DestroyProcessManager();
NTSTATUS AttachToProcess(HANDLE ProcessId);
NTSTATUS DetachFromProcess(HANDLE ProcessId);
NTSTATUS GetProcessCr3(HANDLE ProcessId, CR3* Cr3);
NTSTATUS SwitchProcessContext(HANDLE TargetProcessId);
