#pragma once
#include <ntdef.h>

#define PAGE_SHIFT 12
#define PAGE_SIZE (1ULL << PAGE_SHIFT)
#define PML4_INDEX(va) (((ULONG_PTR)(va) >> 39) & 0x1FF)
#define PDPT_INDEX(va) (((ULONG_PTR)(va) >> 30) & 0x1FF)
#define PD_INDEX(va)   (((ULONG_PTR)(va) >> 21) & 0x1FF)
#define PT_INDEX(va)   (((ULONG_PTR)(va) >> 12) & 0x1FF)
#define OFFSET(va)     ((ULONG_PTR)(va) & 0xFFF)

typedef union _CR3 {
    struct {
        ULONG64 Pcid : 12;
        ULONG64 PageFrameNumber : 36;
        ULONG64 Reserved : 16;
    };
    ULONG64 Flags;
} CR3, *PCR3;

typedef union _PML4E {
    struct {
        ULONG64 Present : 1;
        ULONG64 Write : 1;
        ULONG64 UserAccess : 1;
        ULONG64 WriteThrough : 1;
        ULONG64 CacheDisable : 1;
        ULONG64 Accessed : 1;
        ULONG64 Reserved0 : 1;
        ULONG64 PageSize : 1;
        ULONG64 Reserved1 : 4;
        ULONG64 PageFrameNumber : 36;
        ULONG64 Reserved2 : 16;
    };
    ULONG64 Flags;
} PML4E, *PPML4E;

typedef union _PDPTE {
    struct {
        ULONG64 Present : 1;
        ULONG64 Write : 1;
        ULONG64 UserAccess : 1;
        ULONG64 WriteThrough : 1;
        ULONG64 CacheDisable : 1;
        ULONG64 Accessed : 1;
        ULONG64 Dirty : 1;
        ULONG64 PageSize : 1;
        ULONG64 Reserved0 : 4;
        ULONG64 PageFrameNumber : 36;
        ULONG64 Reserved1 : 16;
    };
    ULONG64 Flags;
} PDPTE, *PPDPTE;

typedef union _PDE {
    struct {
        ULONG64 Present : 1;
        ULONG64 Write : 1;
        ULONG64 UserAccess : 1;
        ULONG64 WriteThrough : 1;
        ULONG64 CacheDisable : 1;
        ULONG64 Accessed : 1;
        ULONG64 Dirty : 1;
        ULONG64 PageSize : 1;
        ULONG64 Global : 1;
        ULONG64 Reserved0 : 3;
        ULONG64 PageFrameNumber : 36;
        ULONG64 Reserved1 : 16;
    };
    ULONG64 Flags;
} PDE, *PPDE;

typedef union _PTE {
    struct {
        ULONG64 Present : 1;
        ULONG64 Write : 1;
        ULONG64 UserAccess : 1;
        ULONG64 WriteThrough : 1;
        ULONG64 CacheDisable : 1;
        ULONG64 Accessed : 1;
        ULONG64 Dirty : 1;
        ULONG64 Pat : 1;
        ULONG64 Global : 1;
        ULONG64 Reserved0 : 3;
        ULONG64 PageFrameNumber : 36;
        ULONG64 Reserved1 : 15;
        ULONG64 NoExecute : 1;
    };
    ULONG64 Flags;
} PTE, *PPTE;
