#pragma once

/*
	Physical memory map
		
		00000000 - 000FFFFF    Loader, kernel, special system structures.
		00180000 - 00FFFFFF    PTEs, PDEs
		01000000 - 017FFFFF    PPDs
		02000000 - 02FFFFFF    Kernel heap

		//
		// first 0x3000 pages are reserved for kernel (48 megabytes)
		// 

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
#define MM_KERNEL_START_PHYS		0x0000B000
#define MM_KERNEL_HEAP		((PVOID)0x80200000)
#define MM_KERNEL_HEAP_PHYS 0x02000000
#define MM_PDE_START		0xC0000000
#define MM_PPD_START		((PVOID)0xC8000000)

#define MM_PDE_START_PHYS	0x00180000
#define MM_PPD_START_PHYS	0x01000000

#define MM_CRITICAL_AREA	0xFFF00000
#define MM_CRITICAL_AREA_PAGES 256


#define MM_DPC_LIST			((PVOID)0x80000000)
#define MM_DPCLIST_PHYS		0x00000000

//
// This structure describes both a valid PTE and not valid PTE
//
// The following combinations are accepted:
//
//  Valid=1, PteType=PTE_TYPE_NORMAL_OR_NOTMAPPED
//    This is a regular PTE, page is in physical memory now.
//
//  Valid=0, PteType=PTE_TYPE_NORMAL_OR_NOTMAPPED
//    This is a free address range, the whole pte may be zeroed 
//     because Valid=0 and PteType is 0
//
//  Valid=0, PteType=PTE_TYPE_PAGEDOUT
//    This page has been paged out to the disk. PageFrameNumber contains the page number in the paging file.
//
//  Valid=0, PteType=PTE_TYPE_TRIMMED
//    This page has been trimmed from the working set, but not excluded from
//     the phsyical memory. Physical page can be in modified or standby page list.
//    The following access to this page will make it valid.
//
//  Valid=0, PteType=PTE_TYPE_VIEW
//    This page is a part of a view of some file. The following access to such a page
//    will initiate read operation for this file and page will become valid.
//

#pragma pack(1)

typedef union MMPTE {
	struct
	{
		union
		{
			struct
			{
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
			} e1;

			struct
			{
				ULONG PagingFileNumber : 4;
				ULONG Reserved : 5;
				ULONG PteType : 2;
				ULONG reserved : 1;

				ULONG PageFrameNumber : 20;
			} e2;
		} u1;
	};

	ULONG RawValue;
} *PMMPTE;

STATIC_ASSERT (sizeof(MMPTE) == 4);

#pragma pack()

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

PVOID
KEAPI
MmMapPhysicalPages(
	ULONG PhysicalAddress,
	ULONG PageCount
	);

VOID
KEAPI
MiUnmapPhysicalPages(
	PVOID VirtualAddress,
	ULONG PageCount
	);

#define MmUnmapPhysicalPages MiUnmapPhysicalPages

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

enum PAGE_STATUS
{
	PageStatusFree = 0,
	PageStatusNormal = 1,
	PageStatusPagedOut = 2,
	PageStatusTrimmed = 3,
	PageStatusView = 4

	//
	// If this value is > 0, than you can access this page safely.
	//
};



//
// Returns PAGE_STATUS. If this value is > 0, than you can access this page safely.
//

KESYSAPI
ULONG /*PAGE_STATUS*/
KEAPI
MmIsAddressValidEx(
	IN PVOID VirtualAddress
	);

typedef struct PROCESS *PPROCESS;

VOID
KEAPI
MmCreateAddressSpace(
	IN PPROCESS Process
	);

extern char MmDebugBuffer[];

//
// The following struct describes paging file
//

#define MAX_PAGE_FILES  16

typedef struct FILE *PFILE;

typedef struct MMPAGING_FILE
{
	ULONG Size;
	ULONG FreeSpace;
	ULONG CurrentUsage;
	PVOID Bitmap;
	PFILE FileObject;
	ULONG PageFileNumber;
	BOOLEAN BootPartition;
} *PMMPAGING_FILE;

extern PMMPAGING_FILE MmPagingFile[MAX_PAGE_FILES];


//
// The following struct describes process' working set
//

#pragma pack(1)

typedef struct MMPPD *PMMPPD;

typedef struct MMWORKING_SET_ENTRY
{
	PMMPPD PageDescriptor;
	BOOLEAN LockedInWs;
	UCHAR Reserved[3];
} *PMMWORKING_SET_ENTRY;

// Variable-length structure
typedef struct MMWORKING_SET
{
	ULONG TotalPageCount;
	ULONG LockedPageCount;
	PROCESSOR_MODE OwnerMode;
	PPROCESS Owner;

	MMWORKING_SET_ENTRY WsPages[1];
} *PMMWORKING_SET;

extern PMMWORKING_SET MiSystemWorkingSet;

//
// The following MMPPD structure is the 
//  Physical Page Descriptor (PPD)
//
//  Each physical page in the system has such descriptor and
//   such structure describes the state of a page
//
//  Note that pages in the low megabyte of the physical memory (256 pages)
//  are not described in this database, so they can't be used by the kernel or drivers.
//  Such pages are used by HAL to allocate intermediate buffers for DMA requests.
//

enum PAGE_LOCATION
{
	ZeroedPageList,
	FreePageList,
	ModifiedPageList,
	StandbyPageList,
	// not lists:
	ActiveAndValid,
	TransitionPage,
	ReservedOrBadPage
};

#pragma pack(1)

typedef struct MMPPD
{
	//
	// Back pointer to the MMPTE
	//
	PMMPTE PointerPte;

	union
	{
		PMMPPD NextFlink;		// If PageLocation < ActiveAndValid
		ULONG WsIndex;			// If PageLocation == ActiveAndValid
		PEVENT Event;			// If PageLocation == TransitionPage
		STATUS ErrorStatus;		// If InPageError==1
	} u1;

	UCHAR PageLocation : 3;

	// Only if PageLocation >= ActiveAndValid
	UCHAR BelongsToSystemWorkingSet : 1;
	UCHAR InPageError : 1;
	UCHAR ReadInProgress : 1;
	UCHAR WriteInProgress : 1;
	UCHAR Modified : 1;
	USHORT ReferenceCount;
	UCHAR KernelStack : 1;
	UCHAR ProcessorMode : 2;
	UCHAR Reserved : 5;

	//
	// The whole size:  12 bytes
	//
} *PMMPPD;

extern PMMPPD MmPpdDatabase;

// Retrieves PPD for the specified physical address
#define MmPpdEntry(PhysicalAddress) (&MmPpdDatabase[((ULONG)PhysicalAddress) >> PAGE_SHIFT])

// Retrieves PPD for the specified virtual address
#define MmPpdForVirtual(VirtualAddress) MmPpdEntry( MiGetPteAddress(VirtualAddress)->PageFrameNumber )

#pragma pack()


typedef LARGE_INTEGER PHYSICAL_ADDRESS;

KESYSAPI
PHYSICAL_ADDRESS
KEAPI
MmAllocatePhysicalPages(
	ULONG PageCount
	);

KESYSAPI
VOID
KEAPI
MmFreePhysicalPages(
	PHYSICAL_ADDRESS PhysicalAddress,
	ULONG PageCount
	);
