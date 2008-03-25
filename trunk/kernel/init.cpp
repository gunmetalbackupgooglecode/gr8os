//
// FILE:		init.cpp
// CREATED:		22-Mar-2008  by Great
// PART:        KE
// ABSTRACT:
//			Initializes the kernel
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

	KiDebugPrintRaw( "INIT: Initialization phase 0 completed, starting initialization phase 1\n"  );

	//
	// Correct kernel stack PTEs
	//

	ULONG KernelStack;

	__asm mov [KernelStack], esp

	KernelStack &= 0xFFFFF000;
	KernelStack -= PAGE_SIZE;

	PMMPTE Pte = MiGetPteAddress (KernelStack);
	Pte->u1.e1.PteType = PTE_TYPE_TRIMMED;

	KeInitializeEvent (&ev, SynchronizationEvent, FALSE);

	
	PFILE File;
	STATUS Status;
	UNICODE_STRING FdName;
	IO_STATUS_BLOCK IoStatus;

	RtlInitUnicodeString (&FdName, L"\\Device\\fdd0");

	Status = IoCreateFile (&File, 0, &FdName, &IoStatus, 0, 0);
	if (!SUCCESS(Status)) {
		KeBugCheck (KE_INITIALIZATION_FAILED,
					__LINE__,
					Status,
					0,
					0
					);
	}

	PVOID Buffer = ExAllocateHeap (TRUE, 512);

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

	KdPrint(("INIT: Buffer: %02x %02x %02x %02x\n",
		((UCHAR*)Buffer)[0],
		((UCHAR*)Buffer)[1],
		((UCHAR*)Buffer)[2],
		((UCHAR*)Buffer)[3]));

	KdPrint(("\nRead 2\n\n"));

	Status = IoReadFile (File, Buffer, 512, NULL, &IoStatus);
	if (!SUCCESS(Status)) {
		KeBugCheck (KE_INITIALIZATION_FAILED,
					__LINE__,
					Status,
					0,
					0
					);
	}

	KdPrint(("INIT: Buffer: %02x %02x %02x %02x\n",
		((UCHAR*)Buffer)[0],
		((UCHAR*)Buffer)[1],
		((UCHAR*)Buffer)[2],
		((UCHAR*)Buffer)[3]));


	KdPrint(("\nRead 3\n\n"));

	LARGE_INTEGER offs = { 0, 0 };

	Status = IoReadFile (File, Buffer, 512, &offs, &IoStatus);
	if (!SUCCESS(Status)) {
		KeBugCheck (KE_INITIALIZATION_FAILED,
					__LINE__,
					Status,
					0,
					0
					);
	}

	KdPrint(("INIT: Buffer: %02x %02x %02x %02x\n",
		((UCHAR*)Buffer)[0],
		((UCHAR*)Buffer)[1],
		((UCHAR*)Buffer)[2],
		((UCHAR*)Buffer)[3]));


	KdPrint(("\nClose\n\n"));

	ExFreeHeap (Buffer);
	IoCloseFile (File);


	// Create two additional threads
	PspCreateThread( &Thread1, &InitialSystemProcess, PsCounterThread, (PVOID)( 80*3 + 40 ) );
	PspCreateThread( &Thread2, &InitialSystemProcess, PsCounterThread, (PVOID)( 80*4 + 45 ) );

	KiDebugPrint ("PS: SystemThread=%08x, Thread1=%08x, Thread2=%08x\n", &SystemThread, &Thread1, &Thread2);

	KiDebugPrintRaw( "INIT: Initialization completed.\n\n" );

	// Demo code for thread execution's delaying.
	PsDelayThreadExecution( 3 );

	// Fall through counter thread code.
	PsCounterThread( (PVOID)( 80*5 + 35 ) );

	//MiZeroPageThread( );
}