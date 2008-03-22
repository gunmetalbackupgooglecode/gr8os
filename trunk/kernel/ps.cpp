//
// FILE:		ps.cpp
// CREATED:		18-Feb-2008  by Great
// PART:        PS
// ABSTRACT:
//			Processes and Threads support.
//

#include "common.h"

LOCK SchedulerLock;

THREAD SystemThread;

ULONG UniqueThreadIdSeed;

KESYSAPI
BOOLEAN
KEAPI
PspLockSchedulerDatabase(
	)
/*++
	This function locks scheduler database
--*/
{
	return KeAcquireLock( &SchedulerLock );
}


KESYSAPI
VOID
KEAPI
PspUnlockSchedulerDatabase(
	)
/*++
	This function unlocks scheduler database
--*/
{
	KeReleaseLock( &SchedulerLock );
}


KENORETURN
KESYSAPI
VOID
KEAPI
PsExitThread(
	ULONG Code
	)
/*++
	Terminates current thread
--*/
{
	PTHREAD Thread;
	BOOLEAN OldIrqState;

	Thread = PsGetCurrentThread();
	OldIrqState = PspLockSchedulerDatabase ();

	RemoveEntryList (&Thread->SchedulerListEntry);
	Thread->ExitCode = Code;
	Thread->State = THREAD_STATE_TERMINATED;

	PspUnlockSchedulerDatabase ();
	KeReleaseIrqState (OldIrqState);

	PsSwapThread();
}


KESYSAPI
VOID
KEAPI
PsTerminateThread(
	IN PTHREAD Thread,
	IN ULONG Code
	)
/*++
	This function terminates any thread
--*/
{
	RemoveEntryList (&Thread->SchedulerListEntry);
	Thread->ExitCode = Code;
	Thread->State = THREAD_STATE_TERMINATED;
}

#if PS_TRACE_CONTEXT_SWITCH
#define PspTraceContextSwitch(x) KiDebugPrint x
#else
#define PspTraceContextSwitch(x)
#endif

#if DBG
char *ThreadStates[] = {
	"Ready",
	"Running",
	"Wait",
	"Terminated"
};
char *WaitTypes[] = {
	"NotWaiting",
	"Suspended",
	"ExecutionDelayed",
	"ObjectsAll",
	"ObjectsAny"
};
#endif

PTHREAD
KEAPI
FindReadyThread(
	)
/*++
	This function searches for ready thread in the system.

--*/
{
	PTHREAD Curr, Thread;

	Curr = PsGetCurrentThread();
	Thread = (PTHREAD) PsReadyListHead.Flink;

	PspTraceContextSwitch (("In FindReadyThread [CURRENT=%08x]\n", Curr));
	
	do
	{

#if DBG
		if(Thread->State != THREAD_STATE_READY)
		{
			ASSERT (Thread->State == THREAD_STATE_READY);
			KiDebugPrint ("PS: Thread->State = %d (%s)\n", Thread->State, /*ThreadStates[Thread->State]*/0);
		}
#endif

		PspTraceContextSwitch (("FRT: Thread=%08x [F=%08x, B=%08x] Stack=%08x CtxFrame=%08x\n", 
			Thread, 
			Thread->SchedulerListEntry.Flink,
			Thread->SchedulerListEntry.Blink,
			Thread->KernelStack,
			Thread->ContextFrame
			));

		if( Thread != Curr )
		{
			PspTraceContextSwitch (("\n"));
			return Thread;
		}

		Thread = (PTHREAD) Thread->SchedulerListEntry.Flink;
	}
	while( (PLIST_ENTRY)Thread != &PsReadyListHead );

	PspTraceContextSwitch (("FRT: No ready thread.\n\n"));
	
	return Curr;
}


VOID
KEAPI
PspReadyThread(
	IN PTHREAD Thread
	)
/*++
	Move thread to the end of ready list.
--*/
{
	ASSERT (Thread->State != THREAD_STATE_TERMINATED);

	RemoveEntryList (&Thread->SchedulerListEntry);
	InsertTailList (&PsReadyListHead, &Thread->SchedulerListEntry);
}

KESYSAPI
VOID
KEAPI
PspDumpReadyQueue(
	)
{
	PTHREAD Thread = (PTHREAD) PsReadyListHead.Flink;

	KiDebugPrint ("Ready list [%08x]:\n", &PsReadyListHead);

	do
	{
		KiDebugPrint ("Thread %08x  [F=%08x, B=%08x]  State=%d[%s]  ExcList=%08x  CurrExc=%08x\n", 
			Thread,
			Thread->SchedulerListEntry,
			Thread->State,
			ThreadStates[Thread->State],
			Thread->ExceptionList,
			Thread->CurrentException
			);

		Thread = (PTHREAD) Thread->SchedulerListEntry.Flink;
	}
	while ((PLIST_ENTRY)Thread != &PsReadyListHead);
}


KESYSAPI
VOID
KEAPI
PspDumpSystemThreads(
	)
{
	PTHREAD Thread = CONTAINING_RECORD (InitialSystemProcess.ThreadListHead.Flink, THREAD, ProcessThreadListEntry);
	BOOLEAN OldIrqState = PspLockSchedulerDatabase ();

	KiDebugPrint ("~ Threads in system process:\n");

	for( ;; )
	{
		KiDebugPrint ("Th=%08x, State=%d[%s], EXC=%08x CE=%08x", 
			Thread, 
			Thread->State, 
			ThreadStates[Thread->State],
			Thread->ExceptionList,
			Thread->CurrentException
			);

		if( Thread->State == THREAD_STATE_TERMINATED )
		{
			KiDebugPrint (" ExitCode=%08x", Thread->ExitCode);
		}

		if( Thread->State == THREAD_STATE_WAIT )
		{
			KiDebugPrint (" WaitType=%d[%s] NumberObjects=%d", 
				Thread->WaitType,
				WaitTypes[Thread->WaitType],
				Thread->NumberOfObjectsWaiting
				);
		}

		KiDebugPrintRaw ("\n");

		if (Thread->ProcessThreadListEntry.Flink == &InitialSystemProcess.ThreadListHead)
			break;

		Thread = CONTAINING_RECORD (Thread->ProcessThreadListEntry.Flink, THREAD, ProcessThreadListEntry);
	}

	PspUnlockSchedulerDatabase ();
	KeReleaseIrqState (OldIrqState);
	
}


#if PS_TRACE_WAIT_LIST_PROCESSING
#define PspTraceWaitList(x) KiDebugPrint x
#else
#define PspTraceWaitList(x)
#endif

extern "C" BOOLEAN  PspQuantumEndBreakPoint = FALSE;


VOID
KEAPI
PspUnwaitThread(
	IN PTHREAD Thread,
	IN USHORT  QuantumIncrement
	)
/*++
	Unwaits the specified thread. It will be executed first at the next context switch
--*/
{
	ASSERT (Thread->State != THREAD_STATE_TERMINATED);

	Thread->State = THREAD_STATE_READY;
	RemoveEntryList (&Thread->SchedulerListEntry);
	InsertHeadList (&PsReadyListHead, &Thread->SchedulerListEntry);
	Thread->QuantumDynamic = QuantumIncrement;
}


VOID
KEAPI
PspProcessWaitList(
	)
/*++
	Process PsWaitListHead thread list and exclude threads from it if necessary.
--*/
{
	PTHREAD Thread = (PTHREAD) PsWaitListHead.Flink;

	PspTraceWaitList (("PSP: Processing PsWaitListHead (%08x)\n", &PsWaitListHead));

	while ((PLIST_ENTRY)Thread != &PsWaitListHead)
	{
		
		PspTraceWaitList (("Thread %08x  [F=%08x, B=%08x]  State=%d[%s] WaitType=%d[%s] TickCount=%d\n", 
			Thread,
			Thread->SchedulerListEntry,
			Thread->State,
			ThreadStates[Thread->State],
			Thread->WaitType,
			WaitTypes[Thread->WaitType],
			Thread->DelayedTickCount
			));
		

		if( Thread->WaitType == THREAD_WAIT_EXECUTIONDELAYED )
		{
			-- Thread->DelayedTickCount;
			if( Thread->DelayedTickCount == 0 )
			{
				PTHREAD NextThread = (PTHREAD) Thread->SchedulerListEntry.Flink;

				PspTraceWaitList (("PSP: Resumed thread %08x\n", Thread));

				// Unwait thread.
				PspUnwaitThread (Thread, 0);

				// Skip current thread and go to next.
				Thread = NextThread;
				continue;
			}
		}

		Thread = (PTHREAD) Thread->SchedulerListEntry.Flink;
	}

	PspTraceWaitList (("PSP: End of list\n\n"));
}

VOID
KEAPI
PspMakeInitialThread(
	OUT PTHREAD InitialThread
	)
/*++
	Make initial system thread
--*/
{
	KeZeroMemory (InitialThread, sizeof(THREAD));
	InitializeListHead ((LIST_ENTRY*)InitialThread);

	InitialThread->UniqueId = ++UniqueThreadIdSeed;
	InitialThread->State = THREAD_STATE_RUNNING;
	InitialThread->Quantum = THREAD_NORMAL_QUANTUM;
	
	__asm
	{
		mov  eax, InitialThread
		mov  dword ptr fs:[PcCurrentThread], eax
		mov  word ptr fs:[PcQuantumLeft], THREAD_NORMAL_QUANTUM
	}
}


VOID
KEAPI
PspCreateThread(
	OUT PTHREAD Thread,
	IN  PPROCESS OwningProcess,
	IN  PVOID  StartRoutine,
	IN  PVOID  StartContext
	)
/*++
	Create new thread using existing space for THREAD
--*/
{
	PspLockSchedulerDatabase( );

	KeZeroMemory( Thread, sizeof(THREAD) );
	
	// Set thread ID, state, quantum.
	Thread->UniqueId = (++UniqueThreadIdSeed);
	Thread->State = THREAD_STATE_READY;
	Thread->Quantum = THREAD_NORMAL_QUANTUM;

	Thread->OwningProcess = OwningProcess;

	// Add thread to ready thread list
	InsertHeadList (&PsReadyListHead, &Thread->SchedulerListEntry);

	// Add thread to process' thread list
	InsertTailList (&OwningProcess->ThreadListHead, &Thread->ProcessThreadListEntry);

	// Create kernel stack
	PVOID Stack = ExAllocateHeap( FALSE, THREAD_INITIAL_STACK_SIZE );
	*(ULONG*)&Stack += THREAD_INITIAL_STACK_SIZE - 12;

	*(PVOID*)(Stack) = PspBaseThreadStartup;
	*(PVOID*)((ULONG_PTR)Stack + 4)  = StartRoutine;
	*(PVOID*)((ULONG_PTR)Stack + 8)  = StartContext;

	Thread->KernelStack = Stack;
	Thread->JustInitialized = TRUE;

	PspUnlockSchedulerDatabase( );
}


KESYSAPI
PTHREAD
KEAPI
PsCreateThread(
	IN  PPROCESS OwningProcess,
	IN  PVOID  StartRoutine,
	IN  PVOID  StartContext
	)
/*++
	Create new thread allocating new space for THREAD
--*/
{
	PTHREAD NewThread = (PTHREAD) ExAllocateHeap( FALSE, sizeof(THREAD) );

	PspCreateThread( NewThread, OwningProcess, StartRoutine, StartContext );
	return NewThread;
}


LIST_ENTRY PsActiveProcessHead;
LIST_ENTRY PsReadyListHead;
LIST_ENTRY PsWaitListHead;


VOID
KEAPI
PspCreateProcess(
	OUT PPROCESS Process
	)
/*++
	Create process using existing space for PROCESS
--*/
{
	// Empty threads list head
	InitializeListHead (&Process->ThreadListHead);

	//
	// Lock scheduler database, insert process into process list and unlock DB
	//

	PspLockSchedulerDatabase( );

	InsertTailList (&PsActiveProcessHead, &Process->ActiveProcessLinks);

	PspUnlockSchedulerDatabase( );

	// Set number of threads
	Process->NumberOfThreads = 0;
}

KESYSAPI
PPROCESS
KEAPI
PsCreateProcess(
	)
/*++
	Create process allocating new space for PROCESS
--*/
{
	PPROCESS Process = (PPROCESS) ExAllocateHeap( FALSE, sizeof(PROCESS) );
	PspCreateProcess( Process );
	MmCreateAddressSpace( Process );
	return Process;
}


PROCESS InitialSystemProcess;

VOID
KEAPI
PspInitSystem(
	)
/*++
	Initialize process subsystem. Threads already initialized by PsInitSystem
--*/
{
	InitializeListHead (&PsActiveProcessHead);
	PspCreateProcess( &InitialSystemProcess );
	InitialSystemProcess.DirectoryTableBase = MM_PDE_START;
	SystemThread.OwningProcess = &InitialSystemProcess;

	InitializeListHead (&PsReadyListHead);
	InitializeListHead (&PsWaitListHead);

	InsertTailList (&InitialSystemProcess.ThreadListHead, &SystemThread.ProcessThreadListEntry);
	InsertTailList (&PsReadyListHead, &SystemThread.SchedulerListEntry);
}


KESYSAPI
PPROCESS
KEAPI
PsGetCurrentProcess(
	)
/*++
	Returns current process
--*/
{
	return PsGetCurrentThread()->OwningProcess;
}

KESYSAPI
VOID
KEAPI
PsDelayThreadExecution(
	IN ULONG ClockTickCount
	)
/*++
	Delay thread execution for the specified clock tick count
--*/
{
	PTHREAD Thread = PsGetCurrentThread ();
	BOOLEAN OldIrqState;
	CONTEXT_FRAME *ContextFrame;

	OldIrqState = PspLockSchedulerDatabase ();

	Thread->State = THREAD_STATE_WAIT;
	Thread->WaitType = THREAD_WAIT_EXECUTIONDELAYED;
	Thread->DelayedTickCount = ClockTickCount;

	RemoveEntryList (&Thread->SchedulerListEntry);
	InsertTailList (&PsWaitListHead, &Thread->SchedulerListEntry);

	//
	// Create fictive trap frame
	//

	__asm
	{
		pushfd
		push cs
		call $+5
		push fs
		push gs
		push es
		push ds
		push edi
		push esi
		push ebp
		push esp
		push ebx
		push 0    ; edx
		push 0    ; ecx
		push 0    ; eax
		push 0    ; irq0_handler->PspSchedulerTimerInterruptDispatcher
		push 0    ; PspScheduler...->PsQuantumEnd
		push 0    ; PsQuantumEnd->PsSwapThread
				  ; PsSwapThread->PspSwapThread

	    mov [ContextFrame], esp
	}
	Thread->ContextFrame = ContextFrame;

	PspSwapThread();

	//
	// Restore trap frame
	//

	__asm
	{
		add  esp, 24   ; ret addresses; eax, ecx, edx
		pop  ebx
		add  esp, 4    ; esp
		pop  ebp
		pop  esi
		pop  edi
		add  esp, 16  ; segment regs
		add  esp, 12  ; cs,eip,efl
	}

	PspUnlockSchedulerDatabase ();
	KeReleaseIrqState (OldIrqState);
}