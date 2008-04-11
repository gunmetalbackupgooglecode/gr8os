// begin_ddk
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

		00000000 - 0000FFFF    Reserved for null pointers.
		00010000 - 7FFFF000    UserMode address space
		7FFFF000 - 7FFFFFFF    Reserved

		// ring0
		80000000 - 800FFFFF    Loader. This will be unmapped during boot up
		80100000 - 801FFFFF    Kernel. Resident part of kernel address space
		80200000 - 802FFFFF    Kernel heap.
		80300000 - BFFFFFFF    Critical Drivers & Extenders
		C0000000 - C7FFFFFF    PTEs, PDEs
		C8000000 - CFFFFFFF    PPDs

		// ring1 context-swappable area
		D0000000 - D1FFFFFF    \ Paged heap for drivers
		D2000000 - D3FFFFFF    / NonPaged heap for drivers
		D4000000 - FEFFFFFF    Non-Critical drivers

		// ring0
		FF000000 - FFFFDFFF    Kernel critical area
		FFFFE000 - FFFFEFFF    Hyperspace
		FFFFF000 - FFFFFFFF	   Reserved
*/

#define MM_DPCLIST_PHYS				0x00000000
#define MM_KERNEL_START_PHYS		0x0000B000
#define MM_PDE_START_PHYS			0x00180000
#define MM_PPD_START_PHYS			0x01000000
#define MM_KERNEL_HEAP_PHYS			0x02000000


//
// Ring 3
//

#define MM_USERMODE_AREA					0x00010000
#define MM_USERMODE_AREA_END				0x7FFFF000

//
// Ring 0  (Write = 0, does not change when context switches)
//

#define MM_DPC_LIST					((PVOID)0x80000000)
#define MM_LOADER_START				((PVOID)0x80000000)
#define MM_KERNEL_START				((PVOID)0x80100000)
#define MM_KERNEL_HEAP				((PVOID)0x80200000)
#define MM_CRITICAL_DRIVER_AREA		((PVOID)0x80300000)
#define MM_CRITICAL_DRIVER_AREA_END	((PVOID)0xBFFFFFFF)
#define MM_PDE_START						0xC0000000
#define MM_PPD_START				((PVOID)0xC8000000)

//
// Ring1  (Write = 1, involved into context switching)
//
#define MM_PAGED_HEAP_START			((PVOID)0xD0000000)
#define MM_PAGED_HEAP_END			((PVOID)0xD1FFFFFF)
#define MM_NONPAGED_HEAP_START		((PVOID)0xD2000000)
#define MM_NONPAGED_HEAP_END		((PVOID)0xD3FFFFFF)
#define MM_DRIVER_AREA				((PVOID)0xD4000000)
#define MM_DRIVER_AREA_END			((PVOID)0xFEFFFFFF)

//
// Ring 0  (Write = 0, does not change when context switches)
//
#define MM_CRITICAL_AREA					0xFF000000
#define MM_CRITICAL_AREA_END				0xFFFFDFFF
#define MM_CRITICAL_AREA_PAGES	((0xFFFFE000 - 0xFF000000)>>PAGE_SHIFT)
#define MM_HYPERSPACE_START			((PVOID)0xFFFFE000)
#define MM_HYPERSPACE_PAGES					1

// end_ddk

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

// begin_ddk

#define PAGE_SIZE 0x1000
#define PAGE_SHIFT 12

// end_ddk

//
// PMMPTE
// MiGetPteAddress(
//     PVOID VirtualAddress
//     );
//
#define MiGetPteAddress(va) ((PMMPTE)(MM_PDE_START + PAGE_SIZE + ((((ULONG)va)>>22)<<12) + ((((ULONG)va)>>10)&0xFFC)))


// Owner, Write, Valid
#define MiWriteValidKernelPte(PTE) (*(ULONG*)(PTE)) = 5

#define MiWriteTrimmedPte(PTE) { \
	(PTE)->u1.e1.Valid = 0; \
	(PTE)->u1.e1.PteType = PTE_TYPE_TRIMMED; \
}

#define MiWritePagedoutPte(PTE) { \
	(PTE)->u1.e1.Valid = 0; \
	(PTE)->u1.e1.PteType = PTE_TYPE_PAGEDOUT; \
}

#define MiNextPte(PTE) (PMMPTE)((ULONG_PTR)PTE+4)

typedef ULONG PHYSICAL_ADDRESS;

VOID
KEAPI
MiMapPhysicalPages(
	PVOID VirtualAddress,
	PHYSICAL_ADDRESS PhysicalAddress,
	ULONG PageCount
	);

KESYSAPI
VOID
KEAPI
MmReservePhysicalAddressRange(
	PHYSICAL_ADDRESS PhysStart,
	PHYSICAL_ADDRESS PhysEnd
	);

// begin_ddk

KESYSAPI
PVOID
KEAPI
MmMapPhysicalPagesKernel(
	PHYSICAL_ADDRESS PhysicalAddress,
	ULONG PageCount
	);

KESYSAPI
PVOID
KEAPI
MmMapPhysicalPagesInRange(
	PVOID VirtualAddressStart,
	PVOID VirtualAddressEnd,
	PHYSICAL_ADDRESS PhysicalAddress,
	ULONG PageCount,
	BOOLEAN AddToWorkingSet
	);

VOID
KEAPI
MiUnmapPhysicalPages(
	PVOID VirtualAddress,
	ULONG PageCount
	);

VOID
KEAPI
MiMapPageToHyperSpace(
	ULONG Pfn
	);

#define MmUnmapPhysicalPages MiUnmapPhysicalPages

// end_ddk

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

// begin_ddk

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

// end_ddk

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

extern MMPAGING_FILE MmPagingFile[MAX_PAGE_FILES];


//
// The following struct describes process' working set
//

#pragma pack(1)

typedef struct MMPPD *PMMPPD;

typedef struct MMWORKING_SET_ENTRY
{
	PMMPPD PageDescriptor;

	ULONG Present : 1;			// = 0 if entry is not used
	ULONG LockedInWs : 1;		// = 1 if page is locked
	ULONG Reserved : 30;
} *PMMWORKING_SET_ENTRY;

struct MUTEX;

// Variable-length structure
typedef struct MMWORKING_SET
{
	MUTEX Lock;
	ULONG TotalPageCount;
	ULONG LockedPageCount;
	PROCESSOR_MODE OwnerMode;
	PPROCESS Owner;

	PMMWORKING_SET_ENTRY WsPages;
} *PMMWORKING_SET;

extern PMMWORKING_SET MiSystemWorkingSet;

#define MM_WORKING_SET_INCREMENT	100

ULONG
KEAPI
MiAddPageToWorkingSet(
	PMMPPD Ppd
	);

VOID
KEAPI
MiRemovePageFromWorkingSet(
	PMMPPD Ppd
	);

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
	union
	{
		//
		// Next PPD in list
		//
		PMMPPD NextFlink;		// If PageLocation < ActiveAndValid

		//
		// Index in the process' working set
		//
		ULONG WsIndex;			// If PageLocation == ActiveAndValid

		//
		// Event object for the transition page
		//
		PEVENT Event;			// If PageLocation == TransitionPage

		//
		// If some error occurred.
		//
		STATUS ErrorStatus;		// If InPageError==1
	} u1;

	union
	{
		//
		// Back pointer to the MMPTE
		//
		PMMPTE PointerPte;		// If PageLocation >= ActiveAndValid

		//
		// Back link to previous PPD in list
		//
		PMMPPD PrevBlink;		// If PageLocation < ActiveAndValid
	} u2;


	//
	// Also we can treat first two DWORDs as the LIST_ENTRY when PageLocation < ActiveAndValid :
	//
	//  ((PLIST_ENTRY)PointerPpd)
	//
	//  Notice, that this list is NOT circular, last Flink is NULL and first Blink is NULL
	//


	UCHAR PageLocation : 3;

	// Only if PageLocation >= ActiveAndValid
	UCHAR BelongsToSystemWorkingSet : 1;		// Page belongs to system working set
	UCHAR InPageError : 1;						// Error occurred during I/O operation
	UCHAR ReadInProgress : 1;					// Read operation in progress
	UCHAR WriteInProgress : 1;					// Write operation in progress
	UCHAR Modified : 1;							// Page was modified
	USHORT Reserved;
	UCHAR KernelStack : 1;						// Page belongs to kernel stack
	UCHAR ProcessorMode : 2;					// Owner's processor mode, who owns this page.
	UCHAR ShareCount : 5;						// Share count for the page. Increments with each mapping.

	//
	// Full struct size:  12 bytes
	//

} *PMMPPD;

STATIC_ASSERT (sizeof(MMPPD) == 12);

extern PMMPPD MmPpdDatabase;

extern MUTEX MmPpdLock;

#define MI_LOCK_PPD() ExAcquireMutex (&MmPpdLock);
#define MI_UNLOCK_PPD() ExReleaseMutex (&MmPpdLock);

// Retrieves PPD for the specified physical address
#define MmPpdEntry(PhysicalAddress) (&MmPpdDatabase[((PHYSICAL_ADDRESS)PhysicalAddress) >> PAGE_SHIFT])

// Retrieves PPD for the specified page frame number
#define MmPfnPpd(PFN) (&MmPpdDatabase[PFN])

// Retrieves PPD for the specified virtual address
#define MmPpdForVirtual(VirtualAddress) MmPpdEntry( MiGetPteAddress(VirtualAddress)->PageFrameNumber )

// Calculates page frame number of PPD
#define MmGetPpdPfn(Ppd) (((ULONG)(Ppd) - (ULONG)MmPpdDatabase)/sizeof(MMPPD))


VOID
KEAPI
MiDumpPageLists(
	);

#pragma pack()

// begin_ddk

//
// Memory Descriptor
//

#define MDL_ALLOCATED	0x00000001
#define MDL_LOCKED		0x00000002
#define MDL_MAPPED		0x00000004

typedef struct MMD
{
	ULONG Flags;
	ULONG PageCount;

	PVOID BaseVirtual;
	PVOID MappedVirtual;
	ULONG Offset;
	
	ULONG PfnList[1];
} *PMMD;

KESYSAPI
PMMD
KEAPI
MmAllocateMmd(
	IN PVOID VirtualAddress,
	IN ULONG Size
	);

KESYSAPI
VOID
KEAPI
MmFreeMmd(
	PMMD Mmd
	);

KESYSAPI
VOID
KEAPI
MmBuildMmdForNonpagedSpace(
	PMMD Mmd
	);


KESYSAPI
STATUS
KEAPI
MmAllocatePhysicalPages(
	IN ULONG PageCount,
	OUT PMMD *Mmd
	);

KESYSAPI
VOID
KEAPI
MmFreePhysicalPages(
	IN PMMD Mmd
	);

KESYSAPI
VOID
KEAPI
MmLockPages(
	IN PMMD Mmd
	);

KESYSAPI
VOID
KEAPI
MmUnlockPages(
	IN PMMD Mmd
	);

KESYSAPI
PVOID
KEAPI
MmMapLockedPages(
	IN PMMD Mmd,
	IN PROCESSOR_MODE TargetMode,
	IN BOOLEAN IsImage,
	IN BOOLEAN AddToWorkingSet
	);

KESYSAPI
VOID
KEAPI
MmUnmapLockedPages(
	IN PMMD Mmd
	);

KESYSAPI
VOID
KEAPI
MmReservePhysicalAddressRange(
	PHYSICAL_ADDRESS PhysStart,
	PHYSICAL_ADDRESS PhysEnd
	);

extern PVOID MmAcpiInfo;

// end_ddk

#define MmGetCurrentWorkingSet() (PsGetCurrentProcess()->WorkingSet)


// begin_ddk

typedef struct THREAD *PTHREAD;
typedef struct PROCESS *PPROCESS;

typedef
VOID
(KEAPI
 *PEXT_SWAP_THREAD_CALLBACK)(
	IN PTHREAD PrevThread,
	IN PTHREAD NextThread
	);

typedef
VOID
(KEAPI
 *PEXT_CREATE_THREAD_CALLBACK)(
	IN PTHREAD ThreadBeingCreated
	);

typedef
VOID
(KEAPI
 *PEXT_TERMINATE_THREAD_CALLBACK)(
	IN PTHREAD ThreadBeingTerminated,
	IN ULONG ExitCode
	);

typedef
VOID
(KEAPI
 *PEXT_CREATE_PROCESS_CALLBACK)(
	IN PPROCESS ProcessBeingCreated
	);

typedef
VOID
(KEAPI
 *PEXT_TERMINATE_PROCESS_CALLBACK)(
	IN PPROCESS ProcessBeingTerminated
	);

typedef
VOID
(KEAPI
 *PEXT_BUGCHECK_CALLBACK)(
	IN ULONG BugCheckCode,
	IN ULONG Parameter1,
	IN ULONG Parameter2,
	IN ULONG Parameter3,
	IN ULONG Parameter4
	);

typedef
VOID
(KEAPI
 *PEXT_EXCEPTION_CALLBACK)(
	IN PEXCEPTION_ARGUMENTS ExceptionArguments,
	IN PEXCEPTION_FRAME EstablisherFrame,
	IN PCONTEXT_FRAME CallerContext,
	IN PVOID Reserved
	);

typedef struct DRIVER *PDRIVER;
typedef struct OBJECT_TYPE *POBJECT_TYPE;

typedef struct EXTENDER *PEXTENDER;

typedef
STATUS
(KEAPI
 *PEXTENDER_ENTRY)(
	PEXTENDER ExtenderObject
	);

typedef struct EXTENDER
{
	PVOID ExtenderStart;
	PVOID ExtenderEnd;
	PEXTENDER_ENTRY ExtenderEntry;
	PDRIVER CorrespondingDriverObject;

	LIST_ENTRY ExtenderListEntry;

	PEXCALLBACK SwapThread;
	PEXCALLBACK CreateThread;
	PEXCALLBACK TerminateThread;
	PEXCALLBACK CreateProcess;
	PEXCALLBACK TerminateProcess;
	PEXCALLBACK BugcheckDispatcher;
	PEXCALLBACK ExceptionDispatcher;

} *PEXTENDER;


extern POBJECT_TYPE MmExtenderObjectType;
extern LIST_ENTRY MmExtenderListHead;
extern LOCK MmExtenderListLock;

STATUS
KEAPI
MiCreateExtenderObject(
	IN PVOID ExtenderStart,
	IN PVOID ExtenderEnd,
	IN PVOID ExtenderEntry,
	IN PUNICODE_STRING ExtenderName,
	OUT PEXTENDER *ExtenderObject
	);


KESYSAPI
STATUS
KEAPI
MmLoadSystemImage(
	IN PUNICODE_STRING ImagePath,
	IN PUNICODE_STRING ModuleName,
	IN PROCESSOR_MODE TargetMode,
	IN BOOLEAN Extender,
	OUT PVOID *ImageBase,
	OUT PVOID *ModuleObject
	);

// end_ddk

enum MMSYSTEM_MODE {
	NormalMode,
	UpdateMode
};

extern MMSYSTEM_MODE MmSystemMode;
