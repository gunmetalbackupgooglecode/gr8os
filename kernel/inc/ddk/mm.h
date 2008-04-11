//
// <mm.h> built by header file parser at 19:50:50  11 Apr 2008
// This is a part of gr8os include files for GR8OS Driver & Extender Development Kit (DEDK)
//

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


#define PAGE_SIZE 0x1000
#define PAGE_SHIFT 12

typedef ULONG PHYSICAL_ADDRESS;


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


typedef struct THREAD *PTHREAD;
typedef struct PROCESS *PPROCESS;

/*
typedef
VOID
(KEAPI
 *PEXT_SWAP_THREAD_CALLBACK)(
	IN PTHREAD PrevThread,
	IN PTHREAD NextThread
	);
*/

typedef
VOID
(KEAPI
 *PEXT_CREATE_THREAD_CALLBACK)(
	IN PTHREAD ThreadBeingCreated,
	IN PVOID StartRoutine,
	IN PVOID StartContext
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
	IN ULONG StopCode,
	IN ULONG Parameter1,
	IN ULONG Parameter2,
	IN ULONG Parameter3,
	IN ULONG Parameter4
	);

/*
typedef
VOID
(KEAPI
 *PEXT_EXCEPTION_CALLBACK)(
	IN PEXCEPTION_ARGUMENTS ExceptionArguments,
	IN PEXCEPTION_FRAME EstablisherFrame,
	IN PCONTEXT_FRAME CallerContext,
	IN PVOID Reserved
	);
*/

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

	PEXCALLBACK CreateThread;
	PEXCALLBACK TerminateThread;
	PEXCALLBACK CreateProcess;
	PEXCALLBACK TerminateProcess;
	PEXCALLBACK BugcheckDispatcher;

} *PEXTENDER;

extern POBJECT_TYPE MmExtenderObjectType;

extern LOCKED_LIST MmExtenderList;

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

