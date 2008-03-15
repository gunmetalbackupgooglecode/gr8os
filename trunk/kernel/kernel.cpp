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

KESYSAPI
VOID
KEAPI
KeConfigureTimer(
	UCHAR Timer,
	ULONG Freq
	)
/*++
	Congure one of three counters of i8254 timer
--*/
{
	TIMER_CONTROL Tmr;
	Tmr.CounterSelector = Timer;
	Tmr.CountMode = 0;
	Tmr.CounterMode = MeandrGenerator;
	Tmr.RequestMode = LSBMSB;

	ULONG Div32 = ( (ULONG)TIMER_FREQ / (ULONG)Freq );
	USHORT Divisor = (USHORT) Div32;

	if( Div32 >= 0x10000 )
		Divisor = 0;

	KiDebugPrint ("KE: Timer configured for: channel=%d, freq=%d, divisor=0x%04x\n", Timer, Freq, Divisor);


	KiOutPort (0x43, Tmr.RawValue);
	KeStallExecution(1);

	KiOutPort (0x40 + Timer, Divisor & 0xFF);
	KeStallExecution(1);

	KiOutPort (0x40 + Timer, Divisor >> 8);
	KeStallExecution(1);
}


KESYSAPI
USHORT
KEAPI
KeQueryTimerCounter(
	UCHAR Timer
	)
/*++
	Query counter channel current value
--*/
{
	UCHAR lsb, msb;

	lsb = KiInPort (Timer + 0x40);
	__asm nop
	__asm nop

	msb = KiInPort (Timer + 0x40);
	__asm nop
	__asm nop

	return (msb << 8) | lsb;
}


KESYSAPI
SYSTEM_PORT
KEAPI
KeReadSystemPort(
	)
/*++
	Reads system port
--*/
{
	volatile UCHAR Val = KiInPort( SYSTEM_PORT_NUMBER );
	return *(SYSTEM_PORT*)&Val;
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


	for( ;; )
	{
		KD_WAKE_UP_DEBUGGER_BP();
	}
}


//==================================
//           Start up
//==================================



THREAD Thread1, Thread2, Thread3;

CHAR
KiReadChar( 
	ULONG Pos
	)
{
	__asm mov eax, [Pos]
	__asm shl eax, 1
	__asm movzx eax, byte ptr gs:[eax]
}

VOID
KiWriteChar(
	ULONG Pos,
	CHAR chr
	)
{
	__asm mov eax, [Pos]
	__asm shl eax, 1
	__asm mov cl, [chr]
	__asm mov byte ptr gs:[eax], cl
}

VOID KiIncChar(
	ULONG Pos
	)
{
	__asm mov eax, [Pos]
	__asm shl eax, 1
	__asm inc byte ptr gs:[eax]
}

EVENT ev;

VOID
KEAPI
PsCounterThread(
	PVOID Argument
	)
{
	ULONG Pos = (ULONG) Argument;

	KiWriteChar( Pos, '0' );
	KiWriteChar( Pos-1, '0' );
	KiWriteChar( Pos-2, '0' );

	for( ;; )
	{
		KeStallExecution( 500000 );
		
		KiIncChar( Pos );

		if ( Argument == (PVOID)( 80*3 + 40 ) && KiReadChar(Pos) == '3' )
		{
			KeClearEvent (&ev);
			KeWaitForSingleObject (&ev, 0, 0);
		}

		if ( Argument == (PVOID)( 80*4 + 45 ) && KiReadChar(Pos) == '7' )
		{
			/*
			if (STATUS_SUCCESS != HalWriteComPort (0, "Hello", 5))
			{
				KiDebugPrintRaw ("HalWriteComPort failed\n");
			}
			else
			{
				KiDebugPrintRaw ("HalWriteComPort OK\n");

				char mes[4] = { 0, 0, 0, 0 };
				ULONG size = 4;

				switch (HalReadComPort (0, mes, &size))
				{
				case STATUS_PARTIAL_READ:
					KiDebugPrint ("HalReadComPort: Partial read performed : [%d bytes] %02x %02x %02x %02x\n",
						size, mes[0], mes[1], mes[2], mes[3]);
					break;
				case STATUS_SUCCESS:
					KiDebugPrint ("HalReadComPort: Full read performed : %02x %02x %02x %02x\n",
						mes[0], mes[1], mes[2], mes[3]);
					break;
				case STATUS_DEVICE_NOT_READY:
					KiDebugPrint ("HalReadComPort: Not ready\n");
					break;
				default:
					KiDebugPrint ("HalReadComPort: FAILURE\n");
				}
			}
			*/

			KeSetEvent (&ev, 0);
		}

		if( Argument == (PVOID)( 80*3 + 40 ) && KiReadChar(Pos) == '9' )
		{
			__asm
			{
				push sehandler
				push dword ptr fs:[0]
				mov  fs:[0], esp

				xor ebx,ebx
				div ebx

sehandler:
			}

			KiDebugPrint ("THRD: In exception handler\n");

			__asm
			{
				mov  eax, EXCEPTION_CONTINUE_SEARCH
				retn 8
			}
		}

		if( KiReadChar (Pos) == ':' )
		{
			KiWriteChar (Pos, '0');
			KiIncChar (Pos-1);

			if( KiReadChar (Pos-1) == ':' )
			{
				KiWriteChar (Pos-1, '0');
				KiIncChar (Pos-2);

				if( KiReadChar (Pos-2) == ':' )
				{
					KiWriteChar (Pos, '0');
					KiWriteChar (Pos-1, '0');
					KiWriteChar (Pos-2, '0');
				}
			}
		}
	}
}

KEVAR  TSS32 SpecialDFTss;
KEVAR  TSS32 SpecialSFTss;

CPU_FEATURES KeProcessorInfo;


VOID
KEAPI
KiStallExecutionMilliseconds(
	UCHAR Ms,
	USHORT WorkingFreq
	)
{
	USHORT Cnt, Cnt2, Diff;
	Diff = (Ms * WorkingFreq) / 1000;

	Cnt = Cnt2 = KeQueryTimerCounter (2);

	while( (Cnt-Cnt2) < Diff )
	{
		Cnt2 = KeQueryTimerCounter (2);
	}
}



KENORETURN
VOID
KEAPI
KiInitSystem(
	PLOADER_ARGUMENTS LdrArgs
	)
{
	//ULONG PhysPages = LdrArgs->PhysicalMemoryPages;

	KiDebugPrintRaw( "INIT: Initializing kernel\n" );

	// Initialize executive
	ExInitSystem( );
	KiDebugPrintRaw( "EX: Passed initialization successfully\n" );

	//
	// Executive initialized - now we can use heaps etc.
	//

	KeGetCpuFeatures( &KeProcessorInfo );

	ULONG FamilyID = KeProcessorInfo.FamilyID;
	ULONG Model = KeProcessorInfo.Model;

	if( FamilyID == 0x0F || FamilyID == 0x06 )
		Model += KeProcessorInfo.ExModelID;

	if( FamilyID == 0x0F )
		FamilyID += KeProcessorInfo.ExFamilyID;

	KiDebugPrint ("INIT: CPUID = %s, MaxEax = %08x, Type=[%b]b, Family=%d(%x), Model=%d(%x)\n", 
		KeProcessorInfo.ProcessorId,
		KeProcessorInfo.MaximumEax,
		KeProcessorInfo.Type,
		FamilyID, FamilyID,
		Model, Model
		);

	KiDebugPrint ("INID: CPU BrandString: %s\n", KeProcessorInfo.BrandString);

	//
	// Query timer freq
	//

	APIC_TIMER_CONFIG Timer = {0};
	KeQueryApicTimerConf (&Timer);

	KiDebugPrint ("INIT: APIC Timer:\n  Initial = %08x, Current = %08x, Divisor = %08x, LVT = %08x\n", 
		Timer.InitialCounter,
		Timer.CurrentCounter,
		Timer.Divisor,
		Timer.LvtTimer.RawValue
		);

	Timer.Divisor = 1;
	Timer.Flags = TIMER_MODIFY_DIVISOR | TIMER_MODIFY_LVT_ENTRY;
	Timer.LvtTimer.Masked = FALSE;
	Timer.LvtTimer.Vector = EXC_MACHINE_CHECK;
	Timer.LvtTimer.TimerMode = Periodic;

	KeSetApicTimerConf (&Timer);
	
	KiDebugPrint ("INIT: APIC Timer:\n  Initial = %08x, Current = %08x, Divisor = %08x, LVT = %08x\n", 
		Timer.InitialCounter,
		Timer.CurrentCounter,
		Timer.Divisor,
		Timer.LvtTimer.RawValue
		);

	ULONG Tpr;
	
	Tpr = KiReadApicConfig (APIC_TPR);
	KiDebugPrint ("INIT: APIC[TPR] = %08x\n", Tpr);

	/*KiWriteApicConfig (APIC_TPR, 0xFFFFFFFF);

	Tpr = KiReadApicConfig (APIC_TPR);
	KiDebugPrint ("INIT: APIC[TPR] = %08x\n", Tpr);
	*.

	SYSTEM_PORT SysPort = KeReadSystemPort();
	KiDebugPrint ("INIT: SystemPort [%02x]: Gate2=%d, Timer2Out=%d\n", SysPort.RawValue, SysPort.Gate2, SysPort.Timer2Out);

	USHORT Cnt, Cnt2, Cnt3;

	KeConfigureTimer( 2, 300 );
	//KeConfigureTimer( 0, 50 );		// channel 0 - irq0 - context switches per second.

	KeStallExecution( 500000 );

	for( int i=0; i<10; i++ )
		KiDebugPrint ("INIT: Counter = %04x\n", KeQueryTimerCounter(2));

	Cnt  = KeQueryTimerCounter(2);
	KeStallExecution (3);
	Cnt2 = KeQueryTimerCounter(2);
	Cnt3 = KeQueryTimerCounter(2);

	KiDebugPrint ("\nINIT: Timer0 = 0x%04x, Timer1 = 0x%04x, Timer2 = 0x%04x\n", Cnt, Cnt2, Cnt3);
	KiDebugPrint ("INIT: DIFFS: %04x , %04x\n\n", Cnt2-Cnt3, Cnt-Cnt2);

	Cnt  = KeQueryTimerCounter(2);
	KeStallExecution (3);
	Cnt2 = KeQueryTimerCounter(2);
	Cnt3 = KeQueryTimerCounter(2);

	KiDebugPrint ("\nINIT: Timer0 = 0x%04x, Timer1 = 0x%04x, Timer2 = 0x%04x\n", Cnt, Cnt2, Cnt3);
	KiDebugPrint ("INIT: DIFFS: %04x , %04x\n\n", Cnt2-Cnt3, Cnt-Cnt2);

	//KiDebugPrint ("INIT: 10 seconds waiting..\n");

	/*Cnt = Cnt2 = KeQueryTimerCounter(2);
	USHORT NeedDiff = 2 * 300;  // 2 seconds * 3000 Hz
	while( (Cnt-Cnt2) < NeedDiff )
	{
		Cnt2 = KeQueryTimerCounter(2);
	}*/

	/*
	const USHORT WorkingFreq = 20;
	KeConfigureTimer (2, WorkingFreq);

	KiDebugPrint ("Timer counter = %04x\n", KeQueryTimerCounter(2));

	for( int i=0; i<100; i++ )
	{
		KiStallExecutionMilliseconds (200, WorkingFreq);

		KiDebugPrint ("Timer counter = %04x\n", KeQueryTimerCounter(2));
	}
	
	KiDebugPrint ("INIT: OK\n");
	*/

	KeConfigureTimer( 2, 20 );

	// Initialize interrupts
	KiInitializeIdt( );
	KiDebugPrintRaw( "IDT: Passed initialization\n" );

	//
	// Allocate new GDT entry for Double-Fault Special TSS and fill it
	//

	USHORT DfTssSelector = KeAllocateGdtDescriptor();
	PSEG_DESCRIPTOR SegDfTss = KeGdtEntry( DfTssSelector );

	KeZeroMemory( SegDfTss, sizeof(SEG_DESCRIPTOR) );
	KiFillDataSegment( SegDfTss, (ULONG)&SpecialDFTss, sizeof(TSS32)-1, FALSE, SEGMENT_TSS32_FREE );

	//
	// Replace Double-Fault IDT entry with task-gate
	//

	PGATE_ENTRY DF_Entry  = (PGATE_ENTRY) KeIdtEntry (EXC_DOUBLE_FAULT);

	KeZeroMemory( DF_Entry, sizeof(GATE_ENTRY) );

	DF_Entry->ThirdWord.Present = 1;
	DF_Entry->ThirdWord.Type = SEGMENT_TASK_GATE;
	DF_Entry->Selector = DfTssSelector;

	KiDebugPrint( "INIT: Set new task-gate for double-fault exception:\nINIT:  GDT #6: %08x`%08x,  IDT #8: %08x`%08x\n", 
		*(ULONG*)((ULONG)SegDfTss+4), 
		*(ULONG*)SegDfTss, 
		*(ULONG*)((ULONG)DF_Entry+4),
		*(ULONG*)DF_Entry );


	//
	// Allocate new GDT entry for Stack-Fault Special TSS and fill it
	//

	USHORT SfTssSelector = KeAllocateGdtDescriptor();
	PSEG_DESCRIPTOR SegSfTss = KeGdtEntry( SfTssSelector );

	KeZeroMemory( SegSfTss, sizeof(SEG_DESCRIPTOR) );
	KiFillDataSegment( SegSfTss, (ULONG)&SpecialSFTss, sizeof(TSS32)-1, FALSE, SEGMENT_TSS32_FREE );

	//
	// Replace Stack-Fault IDT entry with task-gate
	//

	PGATE_ENTRY SF_Entry  = (PGATE_ENTRY) KeIdtEntry (EXC_STACK_FAULT);

	KeZeroMemory( SF_Entry, sizeof(GATE_ENTRY) );

	SF_Entry->ThirdWord.Present = 1;
	SF_Entry->ThirdWord.Type = SEGMENT_TASK_GATE;
	SF_Entry->Selector = SfTssSelector;

	KiDebugPrint( "INIT: Set new task-gate for stack-fault exception:\nINIT:  GDT #7: %08x`%08x,  IDT #12: %08x`%08x\n", 
		*(ULONG*)((ULONG)SegSfTss+4), 
		*(ULONG*)SegSfTss, 
		*(ULONG*)((ULONG)SF_Entry+4),
		*(ULONG*)SF_Entry );

	KiDebugPrintRaw( "KE: Passed initialization\n" );

	// Initialize threads
	PsInitSystem( );
	KiDebugPrintRaw( "PS: Passed initialization\n" );

	// Initialize kernel debugger
	KdInitSystem ();
	KiDebugPrintRaw ("KD: Passed initialization\n");

	__asm sti;

	// Initialize memory management
	MmInitSystem( );
	KiDebugPrintRaw( "MM: Passed initialization\n" );

	KiDebugPrintRaw( "INIT: Initialization phase 0 completed, starting initialization phase 1\n"  );

	ULONG KernelStack;

	__asm mov [KernelStack], esp

	KernelStack &= 0xFFFFF000;
	KernelStack -= PAGE_SIZE;

	PMMPTE Pte = MiGetPteAddress (KernelStack);
	Pte->PteType = PTE_TYPE_TRIMMED;

/*
	__asm call $

	__asm
	{
		xor esp, esp
		push eax
	}
*/
	KeInitializeEvent (&ev, SynchronizationEvent, FALSE);

	//KeBugCheck (0, 0, 0, 0, 0);

	PspCreateThread( &Thread1, &InitialSystemProcess, PsCounterThread, (PVOID)( 80*3 + 40 ) );
	PspCreateThread( &Thread2, &InitialSystemProcess, PsCounterThread, (PVOID)( 80*4 + 45 ) );

	KiDebugPrint ("PS: SystemThread=%08x, Thread1=%08x, Thread2=%08x\n", &SystemThread, &Thread1, &Thread2);

	KiDebugPrintRaw( "INIT: Initialization completed.\n\n" );

	PsDelayThreadExecution( 30 );

	PsCounterThread( (PVOID)( 80*5 + 35 ) );

	//MiZeroPageThread( );
}