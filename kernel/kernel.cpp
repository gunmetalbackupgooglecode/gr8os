//
// FILE:		kernel.cpp
// CREATED:		17-Feb-2008  by Great
// PART:        KE
// ABSTRACT:
//			General kernel routines
//

#include "common.h"

//PCB Pcb0;

KESYSAPI
PDPC_QUEUE
KEAPI
KeInsertQueueDpc(
	PDPC_ROUTINE Function,
	PVOID Context
	)
/*++
	This function inserts new DPC queue entry
--*/
{
	PDPC_QUEUE Dpc = KiDpcListHead;

	for( int i=0; i<KiNumberDpcs; i++)
	{
		if( Dpc->Function == NULL )
		{
			KeAcquireLock( &KiDpcQueueLock );
			Dpc->Function = Function;
			Dpc->Context = Context;
			KeReleaseLock( &KiDpcQueueLock );
			return Dpc;
		}

		*(ULONG_PTR*)&Dpc += sizeof(DPC_QUEUE);
	}
	return NULL;
}


VOID
KEAPI
KiCallDpc(
	PDPC_QUEUE DpcEntry
	)
/*++
	This function calls DPC routine
--*/
{
	__asm pushad;

	DpcEntry->Function (DpcEntry->Context);

	__asm popad;
}


KESYSAPI
VOID
KEAPI
KeRemoveQueueDpc(
	PDPC_QUEUE DpcEntry,
	BOOLEAN LockQueue
	)
/*++
	This function removes DPC from the queue
--*/
{
	if( LockQueue )
	{
		KeAcquireLock( &KiDpcQueueLock );
	}

	DpcEntry->Function = NULL;
	DpcEntry->Context = NULL;

	if( LockQueue )
	{
		KeReleaseLock( &KiDpcQueueLock );
	}
}

VOID
KEAPI
KiProcessDpcQueue(
	)
/*++
	Process DPC queue and call each routine
--*/
{
	PDPC_QUEUE Dpc = KiDpcListHead;

	for( int i=0; i<KiNumberDpcs; i++)
	{
		if( Dpc->Function )
		{
			KeAcquireLock( &KiDpcQueueLock );
			KiCallDpc( Dpc );
			KeRemoveQueueDpc( Dpc, FALSE );
			KeReleaseLock( &KiDpcQueueLock );
			return;
		}
	}
}


VOID
KEAPI
KiClearDpcQueue(
	)
/*++
	Initialize DPC queue
--*/
{
	MiMapPhysicalPages( MM_DPC_LIST, MM_DPCLIST_PHYS, 1 );
	KeZeroMemory( KiDpcListHead, sizeof(DPC_QUEUE)*KiNumberDpcs );
}


KESYSAPI
VOID
KEFASTAPI
KiOutPort(
	USHORT PortNumber,
	UCHAR  Value
	)
/*++
	Out to port
--*/
{
	__asm mov  al, dl
	__asm mov  dx, cx
	__asm out  dx, al
}


KESYSAPI
UCHAR
KEFASTAPI
KiInPort(
	USHORT PortNumber
	)
/*++
	Read from port
--*/
{
	__asm mov  dx, cx
	__asm in   al, dx
}

KESYSAPI
VOID
KEAPI
KeStallExecution(
	ULONG TickCount
	)
/*++
	Suspends execution for the specified tick count
--*/
{
	while( TickCount )
		-- TickCount;
}

KENORETURN
VOID
KEAPI
KiStopExecution(
	)
/*++
	Stops execution of the current processor
--*/
{
	__asm cli
	__asm hlt
}

KESYSAPI
USHORT
KEAPI
KeAllocateGdtDescriptor(
	)
/*++
	Allocate free GDT descriptor
--*/
{
	GDTR Gdtr;
	__asm sgdt fword ptr [Gdtr];

	PSEG_DESCRIPTOR Seg = Gdtr.Table + 1;
	USHORT Selector = 8;

	do
	{
		if( Seg->HighWord.Bits.Pres == 0 )
		{
			return Selector;
		}

		Selector += 8;
		++ Seg;
	}
	while( (ULONG)Seg <= (ULONG)Gdtr.Table+Gdtr.Limit );

	return 0;
}


KESYSAPI
PSEG_DESCRIPTOR
KEAPI
KeGdtEntry(
	USHORT Selector
	)
/*++
	Retrieves GDT entry by the selector
--*/
{
	GDTR Gdtr;
	__asm sgdt fword ptr [Gdtr];

	return &Gdtr.Table[Selector >> 3];
}


KESYSAPI
PIDT_ENTRY
KEAPI
KeIdtEntry(
	USHORT Index
	)
/*++
	Retrieves IDT entry by the int number
--*/
{
	IDTR Idtr;
	__asm sidt fword ptr [Idtr];

	return &Idtr.Table[Index];
}


KESYSAPI
VOID
KEAPI
KiFillDataSegment(
	PSEG_DESCRIPTOR Segment,
	ULONG Address,
	ULONG Limit,
	BOOLEAN Granularity,
	UCHAR Type
	)
/*++
	Fills GDT/LDT entry
--*/
{
	Segment->BaseLow  = Address & 0xFFFF;
	Segment->LimitLow = Limit & 0xFFFF;
	Segment->HighWord.Bytes.BaseMid = (Address >> 16) & 0xFF;
	Segment->HighWord.Bytes.BaseHi  =  Address >> 24;
	Segment->HighWord.Bits.Pres = TRUE;
	Segment->HighWord.Bits.Granularity = Granularity;
	Segment->HighWord.Bits.Type = Type;
	Segment->HighWord.Bits.LimitHi = (Limit >> 16) & 0xF;
}

KESYSAPI
VOID
KEAPI
KeAssertionFailed(
	PCHAR Message,
	PCHAR FileName,
	ULONG Line
	)
/*++
	Display message about the failed assertion
--*/
{
	sprintf( MmDebugBuffer, "KE: Assertion failed for <%s> in file %s at line %d\n", Message, FileName, Line );
	KiDebugPrintRaw (MmDebugBuffer);
}

//==================================
//            Events
//==================================

KESYSAPI
VOID
KEAPI
KeInitializeEvent(
	PEVENT Event,
	UCHAR Type,
	BOOLEAN InitialState
	)
/*++
	This function initializes event object.
--*/
{
	KeZeroMemory( Event, sizeof(EVENT) );

	ASSERT (Type == NotificationEvent || Type == SynchronizationEvent);

	Event->Header.ObjectType = Type;
	Event->Header.SignaledState = InitialState;
	InitializeListHead (&Event->Header.WaitListHead);
}


KESYSAPI
VOID
KEAPI
KeClearEvent(
	IN PEVENT Event
	)
/*++
	Set event to nonsignaled state
--*/
{
	Event->Header.SignaledState = FALSE;
}


BOOLEAN
KEAPI
KiSetEvent(
	IN PEVENT Event,
	IN USHORT QuantumIncrement
	)
/*++
	Internal set event routine w/o locking scheduler DB
--*/
{
	BOOLEAN OldSignaledState;

	OldSignaledState = Event->Header.SignaledState;
	Event->Header.SignaledState = TRUE;
	
	PWAIT_BLOCK WaitBlock = CONTAINING_RECORD (Event->Header.WaitListHead.Flink, WAIT_BLOCK, WaitListEntry);
	
	if( !IsListEmpty (&Event->Header.WaitListHead) )
	{
		do
		{
			PTHREAD Thread = WaitBlock->BackLink;

			//
			// Remove terminated threads from wait list
			//

			if( Thread->State == THREAD_STATE_TERMINATED )
			{
				PWAIT_BLOCK WaitBlockBeingRemoved = WaitBlock;
				PLIST_ENTRY NextBlockEntry = WaitBlock->WaitListEntry.Flink;

				WaitBlock = CONTAINING_RECORD (WaitBlock->WaitListEntry.Flink, WAIT_BLOCK, WaitListEntry);
				RemoveEntryList (&WaitBlockBeingRemoved->WaitListEntry);

				KiDebugPrint ("KE: Terminated thread %08x removed from wait list\n", Thread);

				if (NextBlockEntry == &Event->Header.WaitListHead)
				{
					break;
				}

				// else continue

				continue;
			}


			//
			// Unwait thread if this is the single object it is waiting for
			//  or thread is waiting for any object of the group.
			//

			if ( (Thread->NumberOfObjectsWaiting > 1 &&
				 Thread->WaitType == THREAD_WAIT_SCHEDULEROBJECTS_ANY) ||
				 Thread->NumberOfObjectsWaiting == 1 )
			{
				PspUnwaitThread (Thread, QuantumIncrement);

				//
				// Don't satisfy other threads if this is the synch event
				//
				if (Event->Header.ObjectType == SynchronizationEvent)
				{
					break;
				}
			}

			if ((PLIST_ENTRY)WaitBlock->WaitListEntry.Flink == &Event->Header.WaitListHead)
				break;

			WaitBlock = CONTAINING_RECORD (WaitBlock->WaitListEntry.Flink, WAIT_BLOCK, WaitListEntry);
		}
		while (TRUE);
	}

	return OldSignaledState;
}

KESYSAPI
BOOLEAN
KEAPI
KeSetEvent(
	IN PEVENT Event,
	IN USHORT QuantumIncrement
	)
/*++
	Set event to signaled state and unwait necessary threads.
	Returns old state of the event object.
--*/
{
	BOOLEAN OldIrqState;
	BOOLEAN OldSignaledState;

	OldIrqState = PspLockSchedulerDatabase ();

	OldSignaledState = KiSetEvent (Event, QuantumIncrement);

	PspUnlockSchedulerDatabase ();
	KeReleaseIrqState (OldIrqState);

	return OldSignaledState;
}


KESYSAPI
BOOLEAN
KEAPI
KeWaitForSingleObject(
	IN PVOID Object,
	IN BOOLEAN Alertable,
	IN PLARGE_INTEGER Timeout
	)
/*++
	Performs a wait on a single object.
--*/
{
	BOOLEAN OldIrqState;
	PTHREAD Thread;
	PWAIT_BLOCK WaitBlock;
	PSCHEDULER_HEADER Header;
	CONTEXT_FRAME *ContextFrame;

	UNREFERENCED_PARAMETER (Alertable);
	UNREFERENCED_PARAMETER (Timeout);

	OldIrqState = PspLockSchedulerDatabase ();
	Thread = PsGetCurrentThread();
	Header = (PSCHEDULER_HEADER) Object;

	if (Header->SignaledState)
	{
		PspUnlockSchedulerDatabase ();
		KeReleaseIrqState(OldIrqState);
		return FALSE;
	}


	WaitBlock = &Thread->WaitBlocks[0];
	WaitBlock->BackLink = Thread;
	InsertTailList (&Header->WaitListHead, &WaitBlock->WaitListEntry);
	
	Thread->WaitBlockUsed = WaitBlock;
	Thread->State = THREAD_STATE_WAIT;
	Thread->WaitType = THREAD_WAIT_SCHEDULEROBJECTS_ANY; // doesn't matter, we're waiting for one object.
	Thread->NumberOfObjectsWaiting = 1;

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
	KeReleaseIrqState(OldIrqState);
	return TRUE;
}




PCHAR KeBugCheckDescriptions[] = {
	"SUCCESS",
	"EX_INITIALIZATION_FAILED",
	"KE_INITIALIZATION_FAILED",
	"PS_INITIALIZATION_FAILED",
	"KD_INITIALIZATION_FAILED",
	"MM_INITIALIZATION_FAILED",
	"KERNEL_MODE_EXCEPTION_NOT_HANDLED",
	"MANUALLY_INITIATED_CRASH",
	"EX_KERNEL_HEAP_FAILURE",
	"OB_INITIALIZATION_FAILED",
	"IO_INITIALIZATION_FAILED",
	"IO_MULTIPLE_COMPLETE_REQUESTS",
	"IO_NO_MORE_IRP_STACK_LOCATIONS",
	"IO_IRP_COMPLETION_WITH_PENDING",
	NULL
};

static char *HeapFirstArguments[] = {
	"SUCCESS",
	"HEAP_BLOCK_VALIDITY_CHECK_FAILED",
	"HEAP_DOUBLE_FREEING",
	"HEAP_LOCKED_BLOCK_FREEING",
	"(not used)",
	"HEAP_DOUBLE_LOCKING",
	"HEAP_FREE_BLOCK_LOCKING",
	"HEAP_NOTLOCKED_BLOCK_UNLOCKING",
	"HEAP_GUARD_FAILURE",
	NULL
};

static char *HeapValidationSecondArguments[] = {
	"SUCCESS",
	"HEAP_VALIDATION_COOKIE_MISMATCH",
	"HEAP_VALIDATION_CHECKSUM_MISMATCH",
	"HEAP_VALIDATION_NO_FREE_PATTERN",
	"HEAP_VALIDATION_OVERFLOW_DETECTED",
	NULL
};

static char *HeapGuardSecondArguments[] = {
	"SUCCESS",
	"HEAP_GUARD_ALREADY_GUARDED",
	"HEAP_GUARD_NOT_GUARDED",
	"HEAP_GUARD_LEAK_DETECTED",
	NULL
};

KENORETURN
KESYSAPI
VOID
KEAPI
KeBugCheck(
	ULONG StopCode,
	ULONG Argument1,
	ULONG Argument2,
	ULONG Argument3,
	ULONG Argument4
	)
/*++
	This function crashes the system
--*/
{
	PCHAR szCode = "no code";

	if (StopCode < MAXIMUM_BUGCHECK)
		szCode = KeBugCheckDescriptions[StopCode];

	KiDebugPrint("\n\n *** STOP : [%08x] %s:  \n  %08x - %s\n",
		StopCode,
		szCode,
		Argument1,
		HeapFirstArguments[Argument1]
		);

	if (Argument1 == HEAP_BLOCK_VALIDITY_CHECK_FAILED)
	{
		KiDebugPrint("  %08x - %s\n",
			Argument2,
			HeapValidationSecondArguments[Argument2]
			);
	}
	else if (Argument1 == HEAP_GUARD_FAILURE)
	{
		KiDebugPrint("  %08x - %s\n",
			Argument2,
			HeapGuardSecondArguments[Argument2]
			);
	}
	else
	{
		KiDebugPrint("  %08x\n", Argument2);
	}

	KiDebugPrint ("  %08x\n  %08x\n\n", Argument3, Argument4);


	// Wake up kernel debugger and pass the control to it.
	for( ;; )
	{
		KD_WAKE_UP_DEBUGGER_BP();
	}
}
