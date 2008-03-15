#pragma once

/*
	Physical memory map
		
		00000000 - 000FFFFF    Loader, kernel, special system structures.
		00180000 - 00FFFFFF    PTEs, PDEs
		01000000 - 017FFFFF    PPDs
		02000000 - 020FFFFF    Kernel heap

	Kernel virtual memory map

		80000000 - 800FFFFF    Loader. This will be unmapped during boot up
		80100000 - 801FFFFF    Kernel. Resident part of kernel address space
		80200000 - 80FFFFFF    Kernel heap.
		90000000 - A7FFFFFF    Free for further allocations
		A8000000 - BFFFFFFF    Critical drivers
		C0000000 - C7FFFFFF    PTEs, PDEs
		C8000000 - CFFFFFFF    PPDs
		D0000000 - DFFFFFFF    \
		E0000000 - EFFFFFFF    / Heap for drivers
		F0000000 - FFEFFFFF    Drivers
		FFF00000 - FFFFFFFF    Kernel critical area
*/

#define MM_LOADER_START		0x80000000
#define MM_KERNEL_START		0x80100000
#define MM_KERNEL_HEAP		((PVOID)0x80200000)
#define MM_KERNEL_HEAP_PHYS 0x02000000
#define MM_PDE_START		0xC0000000
#define MM_PPD_START		0xC8000000

#define MM_PDE_START_PHYS	0x00180000
#define MM_PPD_START_PHYS	0x01000000


#define MM_DPC_LIST			((PVOID)0x80000000)
#define MM_DPCLIST_PHYS		0x00000000

typedef struct MMPTE {
    ULONG Valid : 1;
    ULONG Write : 1;
    ULONG Owner : 1;
    ULONG WriteThrough : 1;
    ULONG CacheDisable : 1;
    ULONG Accessed : 1;
    ULONG Dirty : 1;
    ULONG LargePage : 1;
    ULONG Global : 1;

	ULONG PteType : 2;
    ULONG reserved : 1;

    ULONG PageFrameNumber : 20;
} *PMMPTE;

#define PTE_TYPE_NORMAL_OR_NOTMAPPED	0
#define PTE_TYPE_PAGEDOUT				1
#define PTE_TYPE_TRIMMED				2
#define PTE_TYPE_VIEW					3

#define PAGE_SIZE 0x1000
#define PAGE_SHIFT 12

//
// PMMPTE
// MiGetPteAddress(
//     PVOID VirtualAddress
//     );
//
#define MiGetPteAddress(va) ((PMMPTE)(MM_PDE_START + PAGE_SIZE + ((((ULONG)va)>>22)<<12) + ((((ULONG)va)>>10)&0xFFC)))


// Owner, Write, Valid
#define MiWriteValidKernelPte(PTE) (*(ULONG*)(PTE)) = 7

#define MiNextPte(PTE) (PMMPTE)((ULONG_PTR)PTE+4)

VOID
KEAPI
MiMapPhysicalPages(
	PVOID VirtualAddress,
	ULONG PhysicalAddress,
	ULONG PageCount
	);

VOID
KEAPI
MiUnmapPhysicalPages(
	PVOID VirtualAddress,
	ULONG PageCount
	);

VOID
KEAPI
MmInitSystem(
	);

VOID
KEAPI
MiZeroPageThread(
	);

VOID
KEAPI
MmAccessFault(
	PVOID VirtualAddress,
	PVOID FaultingAddress
	);

KESYSAPI
VOID
KEAPI
MmInvalidateTlb(
	PVOID VirtualAddress
	);

KESYSAPI
BOOLEAN
KEAPI
MmIsAddressValid(
	IN PVOID VirtualAddress
	);

typedef struct PROCESS *PPROCESS;

VOID
KEAPI
MmCreateAddressSpace(
	IN PPROCESS Process
	);

extern char MmDebugBuffer[];