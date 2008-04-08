//
// <ex.h> built by header file parser at 20:46:52  08 Apr 2008
// This is a part of gr8os include files for GR8OS Driver & Extender Development Kit (DEDK)
//

#pragma once

KESYSAPI
PVOID
KEAPI
ExAllocateHeap(
	BOOLEAN Paged,
	ULONG Size
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

