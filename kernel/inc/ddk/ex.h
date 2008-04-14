//
// <ex.h> built by header file parser at 08:46:13  14 Apr 2008
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

typedef struct LOCKED_LIST
{
	LIST_ENTRY ListEntry;
	MUTEX Lock;
} *PLOCKED_LIST;

#define InitializeLockedList(list) { InitializeListHead(&(list)->ListEntry); ExInitializeMutex(&(list)->Lock); }
#define InterlockedInsertHeadList(list,entry) InterlockedOp(&(list)->Lock, InsertHeadList(&(list)->ListEntry, (entry)))
#define InterlockedInsertTailList(list,entry) InterlockedOp(&(list)->Lock, InsertTailList(&(list)->ListEntry, (entry)))
#define InterlockedRemoveEntryList(list,entry) InterlockedOp(&(list)->Lock, RemoveEntryList(entry))

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

//
// Callbacks
//

typedef struct OBJECT_TYPE *POBJECT_TYPE;
typedef struct THREAD *PTHREAD;
typedef struct OBJECT_HEADER *POBJECT_HEADER;

extern POBJECT_TYPE ExCallbackObjectType;

typedef struct EXCALLBACK
{
	PVOID CallbackRoutine;
	PVOID Context;
	PTHREAD Owner;
	
	LIST_ENTRY InternalListEntry;
	BOOLEAN Inserted;

} *PEXCALLBACK;

KESYSAPI
STATUS
KEAPI
ExCreateCallback(
	IN PVOID Routine,
	IN PVOID Context OPTIONAL UNIMPLEMENTED,
	OUT PEXCALLBACK *CallbackObject
	);

KESYSAPI
STATUS
KEAPI
ExDeleteCallback(
	IN PEXCALLBACK CallbackObject
	);

VOID
KEAPI
ExpDeleteCallback(
	IN POBJECT_HEADER Object
	);

