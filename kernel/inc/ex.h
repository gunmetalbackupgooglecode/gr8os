#pragma once

#define HEAP_BLOCK_FREE	1
#define HEAP_BLOCK_ALLOCATED 2
#define HEAP_BLOCK_LOCKED 3

#pragma pack(1)

typedef struct EHEAP_BLOCK
{
	//
	//  Magic1 used to check for a buffer overflow of the previous block
	//  Magic2 used to check for a buffer underflow of the current block
	//
	//   Valid block should contain:
	//     1. Cookie == ExpHeapCookie (ExpHeapCookie should be different for each heap.
	//     2. Magic1 == Magic2 == (ListEntry + Size + Cookie).
	//    It is used to avoid buffer overflow exploitation.
	//

	ULONG Magic1;		// magic
	LIST_ENTRY List;	// linked list..
	ULONG Size;			// in 16byte units.
	//16
	UCHAR BlockType;	// see HEAP_BLOCK_*
	UCHAR PaddingSize;	// in bytes
	USHORT Cookie;		// cookie

	LIST_ENTRY MemoryOrderList;

	ULONG Magic2;

#pragma warning (disable:4200)  // zero-sized array in struct/union
	UCHAR Data[0];
#pragma warning (default:4200)

} *PEHEAP_BLOCK;

#pragma pack()

#define EX_HEAP_ALIGN 16
#define SIZEOF_HEAP_BLOCK (sizeof(EHEAP_BLOCK)/EX_HEAP_ALIGN)

extern USHORT ExpHeapCookie;

extern PVOID ExpHeapArea;
extern ULONG ExpHeapSize;

extern LIST_ENTRY ExpFreeBlockList;
extern LIST_ENTRY ExpAllocatedBlockList;
extern LIST_ENTRY ExpMemoryOrderBlockList;

ULONG 
KEAPI 
ExpChecksumBlock (
	PEHEAP_BLOCK Block
	);

VOID
KEAPI
ExpRecalculateChecksumBlock(
	PEHEAP_BLOCK Block
	);

#define ALLOCATED_PADDING	0xFE		// This byte is stored into unused space of allocated block.
#define FREED_PADDING		0xFD		// This byte is stored into free block.

VOID
KEAPI
ExInitializeHeap(
	);

VOID
KEAPI
ExpAcquireHeapLock(
	PBOOLEAN OldState
	);

VOID
KEAPI
ExpReleaseHeapLock(
	BOOLEAN OldState
	);

VOID
KEAPI
ExpCheckBlockValid(
	PEHEAP_BLOCK Block
	);


VOID
KEAPI
ExpRebuildFreeBlocks(
	PEHEAP_BLOCK Block
	);

PVOID
KEAPI
ExpAllocateHeapInBlock(
	PEHEAP_BLOCK Block,
	ULONG Size
	);

PEHEAP_BLOCK
KEAPI
ExpFindIdealBlock(
	ULONG Size
	);

KESYSAPI
PVOID
KEAPI
ExAllocateHeap(
	BOOLEAN Paged,
	ULONG Size
	);

VOID
KEAPI
ExpFreeHeapBlock(
	PEHEAP_BLOCK Block
	);

KESYSAPI
VOID
KEAPI
ExFreeHeap(
	PVOID Ptr
	);

KESYSAPI
PVOID
KEAPI
ExReallocHeap(
	PVOID Ptr,
	ULONG NewSize
	);


KESYSAPI
VOID
KEAPI
ExLockHeapBlock(
	PVOID Ptr
	);

KESYSAPI
VOID
KEAPI
ExUnlockHeapBlock(
	PVOID Ptr
	);

VOID
KEAPI
ExpDumpHeap(
	);

typedef struct EHEAP_GUARD_TABLE
{
	PEHEAP_BLOCK* Table;
	ULONG TableSize;
} *PEHEAP_GUARD_TABLE;

// In the real kernel these fields are within THREAD Structure
//extern BOOLEAN InExGuardedRegion;
//extern PEHEAP_GUARD_TABLE ExGuardTable;

//#define ExIsCurrentThreadGuarded() (InExGuardedRegion)
#define ExIsCurrentThreadGuarded() (PsGetCurrentThread()->InExGuardedRegion)


//#define ExCurrentThreadGuardTable() (ExGuardTable)
#define ExCurrentThreadGuardTable() (*(PEHEAP_GUARD_TABLE*)&(PsGetCurrentThread()->ExGuardTable))    // l-value


#define EX_GUARD_TABLE_INITIAL_SIZE		128

KESYSAPI
VOID
KEAPI
ExEnterHeapGuardedRegion(
	);
	
KESYSAPI
VOID
KEAPI
ExLeaveHeapGuardedRegion(
	);

VOID
KEAPI
ExpLogAllocation(
	PEHEAP_BLOCK BlockBeingAllocated
	);

VOID
KEAPI
ExpLogRellocation(
	PEHEAP_BLOCK BlockBeingFreed,
	PEHEAP_BLOCK BlockBeingAllocated
	);

VOID
KEAPI
ExpLogFreeing(
	PEHEAP_BLOCK BlockBeingFreed
	);


VOID
KEAPI
ExInitSystem(
	);

typedef struct MUTEX
{
	SCHEDULER_HEADER Header;
} *PMUTEX;

KESYSAPI
VOID
KEAPI
ExInitializeMutex(
	  PMUTEX Mutex
	  );

KESYSAPI
VOID
KEAPI
ExAcquireMutex(
	PMUTEX Mutex
	);

KESYSAPI
VOID
KEAPI
ExReleaseMutex(
	PMUTEX Mutex
	);
