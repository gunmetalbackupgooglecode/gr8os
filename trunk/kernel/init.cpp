//
// FILE:		init.cpp
// CREATED:		22-Mar-2008  by Great
// PART:        KE
// ABSTRACT:
//			Initializes the kernel
//
//
//	All characters and events in this show, even those
//   based on real people, are entirely fictional.
//	All celebrity voices are impresonated (poorly). The
//	 following OS contains coarse language and due to 
//	its content it should not be viewed by anyone.
//


#include "common.h"

//==================================
//           Start up
//==================================

//
// Demo code with three counter threads..
//
// BUGBUG: Delete in final version
//

THREAD Thread1, Thread2, Thread3;

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
KiStallExecutionMilliseconds(
	ULONG Ms
	);

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

		//
		// Restore digits if someone wiped them
		//

		if (KiReadChar(Pos)<'0' || KiReadChar(Pos)>'9' ||
			KiReadChar(Pos-1)<'0' || KiReadChar(Pos-1)>'9' ||
			KiReadChar(Pos-2)<'0' || KiReadChar(Pos-2)>'9')
		{
			KiWriteChar( Pos, '0' );
			KiWriteChar( Pos-1, '0' );
			KiWriteChar( Pos-2, '0' );		
		}


		KiIncChar( Pos );

		// This is stupid determining of the current thread.
		// We can use PsGetCurrentThread()==&Thread1 instead.

		if ( Argument == (PVOID)( 80*3 + 40 ) && KiReadChar(Pos) == '3' )
		{
			KeClearEvent (&ev);
			KeWaitForSingleObject (&ev, 0, 0);
		}

		if ( Argument == (PVOID)( 80*4 + 45 ) && KiReadChar(Pos) == '7' )
		{
			KeSetEvent (&ev, 0);
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

void _f()
{
	__try
	{
		ULONG r = 0;
		ULONG b = 9/r;
	}
	__finally
	{
		KiDebugPrint("_f internal finally block\n");
	}
}

VOID
KEAPI
KiDemoThread(
	PVOID Argument
	)
{
	KdPrint(("In KiDemoThread\n"));

	//KeInitializeEvent (&ev, SynchronizationEvent, FALSE);


	/*PVOID HeapBuffer = ExAllocateHeap (TRUE, 10);
	strncpy ((char*)HeapBuffer, "1234", 5);

	PMMPTE Pte = MiGetPteAddress (HeapBuffer);

	KdPrint(("\n\n"));
	KdPrint (("Allocated buffer %08x : %s [phys page %08x]\n", HeapBuffer, HeapBuffer, Pte->u1.e1.PageFrameNumber));

	PMMD Mmd = MmAllocateMmd (HeapBuffer, 10000);*/

	PMMD Mmd;
	STATUS Status;

	Status = MmAllocatePhysicalPages (3, &Mmd);
	if (!SUCCESS(Status))
	{
		KeBugCheck (KE_INITIALIZATION_FAILED, Status, __LINE__, 0, 0);
	}

	PVOID Mapped = MmMapLockedPages(Mmd, KernelMode);
	strncpy ((char*)Mapped, "1234", 5);

	KdPrint(("Allocated MMD at %08x [fl=%08x]\n", Mmd, Mmd->Flags));
	
//	MmBuildMmdForNonpagedSpace (Mmd);
//	KdPrint(("Built mmd for nonpaged space\n"));

///	PVOID Mapped = MmMapLockedPages (Mmd, KernelMode);
	KdPrint(("Mapped at %08x\n", Mapped));

	PMMPTE Pte = MiGetPteAddress (Mapped);
	KdPrint(("mapped: %s  [phys page %08x]\n", Mapped, Pte->u1.e1.PageFrameNumber));

	MmUnlockPages (Mmd);

	KdPrint(("Pages unlocked from working set\n"));

	MmUnmapLockedPages (Mmd);
	MmFreePhysicalPages (Mmd);
	MmFreeMmd (Mmd);

	MiDumpPageLists();

#if 1
	//
	// Test file reading
	//

	PFILE File;
	//STATUS Status;
	UNICODE_STRING FdName;
	IO_STATUS_BLOCK IoStatus;

	//RtlInitUnicodeString (&FdName, L"\\SystemRoot\\message.txt");
	RtlInitUnicodeString (&FdName, L"\\Global\\A:\\message.txt");

	Status = IoCreateFile (&File, FILE_READ_DATA, &FdName, &IoStatus, FILE_OPEN_EXISTING, 0);
	if (!SUCCESS(Status)) {
		KeBugCheck (KE_INITIALIZATION_FAILED,
					__LINE__,
					Status,
					0,
					0
					);
	}

	PVOID Buffer = ExAllocateHeap (TRUE, 512);

	PFSFATFCB fcb = (PFSFATFCB) File->FsContext;
	KdPrint(("\nFile opened, fcb=%08x, filename=%s\n", fcb, fcb->DirEnt->Filename));

	KdPrint(("\nRead 1\n\n"));

	Status = IoReadFile (File, Buffer, 512, NULL, &IoStatus);
	if (!SUCCESS(Status)) {
		KeBugCheck (KE_INITIALIZATION_FAILED,
					__LINE__,
					Status,
					0,
					0
					);
	}

	((UCHAR*)Buffer)[IoStatus.Information] = 0;

	KdPrint(("INIT: Read=%d, Buffer: \n%s\n", IoStatus.Information, Buffer));

	KdPrint(("\nClose\n\n"));

	ExFreeHeap (Buffer);
	IoCloseFile (File);
#endif

	ULONG *Dummy = (ULONG*) ExAllocateHeap (0, sizeof(LONG));

	PMMPTE PointerPte = MiGetPteAddress (Dummy);
	PointerPte->u1.e1.Accessed = 0;
	PointerPte->u1.e1.Dirty = 0;

	KdPrint(("Dummy = %08x\n", *Dummy));
	KdPrint(("Pte: A=%d, D=%d\n", PointerPte->u1.e1.Accessed, PointerPte->u1.e1.Dirty));
	*Dummy = 4;
	KdPrint(("Dummy = %08x\n", *Dummy));
	KdPrint(("Pte: A=%d, D=%d\n", PointerPte->u1.e1.Accessed, PointerPte->u1.e1.Dirty));

	ExFreeHeap (Dummy);

	__try
	{
		__try
		{
			__try
			{
				__try
				{
					__try
					{
						_f();
					}
					__finally
					{
						KiDebugPrint(("__finally block 1\n"));
					}
				}
				__finally
				{
					KiDebugPrint(("__finally block 2\n"));
				}
			}
			__except (EXCEPTION_EXECUTE_HANDLER)
			{
				KiDebugPrint("In exception handler [code %08x]\n", GetExceptionCode());
			}
		}
		__except (EXCEPTION_EXECUTE_HANDLER)
		{
			KiDebugPrint("Exception handler that is neved called\n");
		}
	}
	__finally
	{
		KiDebugPrint("Last finally block\n");
	}

	// Fall through counter thread code.
	PsCounterThread( (PVOID)( 80*5 + 35 ) );
}


KEVAR  TSS32 SpecialDFTss;
KEVAR  TSS32 SpecialSFTss;

CPU_FEATURES KeProcessorInfo;


VOID
KEAPI
KiStallExecutionMilliseconds(
	ULONG Ms
	)
/*++
	Suspends exeuction for the specified number of milliseconds.
--*/
{
	ULONG Ticks = (Ms * HalBusClockFrequency)/2;

	HalpWriteApicConfig(APIC_INITCNT, -1);
	
	ULONG Initial, Current;
	Initial = Current = HalpReadApicConfig (APIC_CURRCNT);
	do
	{
		Current = HalpReadApicConfig (APIC_CURRCNT);
	}
	while ( (Initial-Current) < Ticks );
}

VOID
KEAPI
KiStallExecutionHalfSecond(
	)
{
	KiStallExecutionMilliseconds (1);
}

//#define KiDebugPrintRaw
//#define KiDebugPrint

LOADER_ARGUMENTS KiLoaderBlock;
ULONG KiInitializationPhase = 0;


KENORETURN
VOID
KEAPI
KiInitSystem(
	PLOADER_ARGUMENTS LdrArgs
	)
{
	KiLoaderBlock = *LdrArgs;

	KiDebugPrint( "KERNEL: Got execution. Starting with %d megabytes of RAM on board\n", LdrArgs->PhysicalMemoryPages / 256);
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

	// Initialize HAL
	HalInitSystem( );
	KiDebugPrint("HAL: Passed initialization\n");

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

	KiInitializationPhase = 1;

	KiDebugPrintRaw("INIT: Starting initialization phase 1\n");

//	KiDebugPrint("Stalling execution...\n");
//	KiStallExecutionMilliseconds (3);
//	KiDebugPrint("OK\n");


	// Initialize memory management
	MmInitSystem( );
	KiDebugPrintRaw( "MM: Passed initialization\n" );

	// Initialize objective subsystem
	ObInitSystem( );
	KiDebugPrintRaw( "OB: Passed initialization\n" );

	// Initialize I/O subsystem
	IoInitSystem( );
	KiDebugPrintRaw( "IO: Passed initialization\n" );

	KiDebugPrintRaw( "INIT: Initialization phase 1 completed\n"  );

	ObpDumpDirectory (ObRootObjectDirectory,0);


	// Create two additional threads
	KeInitializeEvent (&ev, SynchronizationEvent, 0);
	PspCreateThread( &Thread1, &InitialSystemProcess, PsCounterThread, (PVOID)( 80*3 + 40 ) );
	PspCreateThread( &Thread2, &InitialSystemProcess, PsCounterThread, (PVOID)( 80*4 + 45 ) );
	PspCreateThread( &Thread3, &InitialSystemProcess, KiDemoThread, NULL );

	KiDebugPrint ("PS: SystemThread=%08x, Thread1=%08x, Thread2=%08x, Thread3=%08x\n", &SystemThread, &Thread1, &Thread2, &Thread3);

	KiDebugPrintRaw( "INIT: Initialization completed.\n\n" );

	MiZeroPageThread( );
}