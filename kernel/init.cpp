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
// Demo code with two counter threads..
//
// BUGBUG: Delete in final version
//

PTHREAD Thread1, Thread2;

// Loader thread
PTHREAD LoaderThread;

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
InitIncrementProgressBar(
	)
{
	static int bar = 5;

	ASSERT (bar <= 10);

	KiMoveLoadingProgressBar (bar);

	bar ++;
}

WCHAR DriverDir[] = L"\\Driver\\";

VOID 
KEAPI
KiLoadDeviceDrivers(
	)
{
	PFILE File;
	STATUS Status;
	UNICODE_STRING FdName;
	IO_STATUS_BLOCK IoStatus;

	KeSetOnScreenStatus ("Loading device drivers");
	InitIncrementProgressBar ();

	RtlInitUnicodeString (&FdName, L"\\SystemRoot\\bootdrv.init");
	
	Status = IoCreateFile (&File, FILE_READ_DATA, &FdName, &IoStatus, FILE_OPEN_EXISTING, 0);
	if (!SUCCESS(Status)) 
	{
		KeBugCheck (KE_INITIALIZATION_FAILED,
					__LINE__,
					Status,
					0,
					0
					);
	}

	//PCHAR Buffer = (PCHAR) ExAllocateHeap (TRUE, PAGE_SIZE);
	PCHAR Buffer = (PCHAR) MmAllocatePage ();

	Status = IoReadFile (File, Buffer, PAGE_SIZE, NULL, 0, &IoStatus);
	if (!SUCCESS(Status)) 
	{
		KeBugCheck (KE_INITIALIZATION_FAILED,
					__LINE__,
					Status,
					0,
					0
					);
	}

	IoCloseFile (File);

	KdPrint(("INIT: Read bootdrv.ini: \n%s\n\n", Buffer));

	UNICODE_STRING DriverName, ImagePath;
	PVOID ImageBase = 0;
	PDRIVER DriverObject = 0;
	PCHAR pLine = Buffer;
	PCHAR pNextLine, pp;

	// Enumerate lines
	for ( ; ((pNextLine = (pp = strchr (pLine, '\n'))) != NULL); pLine = pNextLine + 1)
	{
		// Remove \r and \n from the end of the line
		do
		{
			*pp = '\0';
			pp --;
		}
		while (*pp == '\r');

		// pp points to the last character of the line.

		//
		// Find & kill all spaces
		//

		// At the beginning
		while (*pLine == ' ')
			pLine ++;

		// Does line contain spaces only?
		if (*pLine == '\0')
			continue;

		// At the end
		while (*pp == ' ')
			*(pp--) = '\0';

		// Move to \0
		pp++;

		//
		// Calculate line length
		//

		int cchLineLength = (pp-pLine);

		// Skip empty lines
		if (cchLineLength == 0)
			continue;

		// Skip comment lines
		if (pLine[0] == ';')
			continue;

		KdPrint(("INIT: Processing entry '%s'\n", pLine));

		// Make from \DIRDIR\FILEFILE.sys -> \Driver\FILEFILE
		WCHAR *pImagePath = (WCHAR*) ExAllocateHeap (TRUE, cchLineLength * 2 + 2);

		// Make unicode string from ansi string
		mbstowcs (pImagePath, pLine, cchLineLength);

		KdPrint(("INIT: pImagePath = '%S' (cchLineLength=%d)\n", pImagePath, cchLineLength));

		// Found position of FILEFILE
		PCHAR paDriver = strrchr (pLine, '\\');
		PCHAR paDot = strrchr (pLine, '.');
		
		if (!paDriver)
		{
      ExFreeHeap (pImagePath);
			KdPrint(("INIT: Invalid entry in bootdrv.ini: '%s'\n", pLine));
			INT3
			continue;
		}

		paDriver ++;

		if (!paDot)
			paDot = pp + 1;

		//
		//  \Some\Big\Path\To\Driver.sys
		//   paDriver---------^     ^-----paDot

		int cchDriverLength = (paDot - paDriver) + STATIC_WCSLEN(DriverDir);
		WCHAR *pDriverName = (WCHAR*) ExAllocateHeap (TRUE, cchDriverLength * 2 + 2);

		wcscpy  (pDriverName,								DriverDir);
		wcsncpy (pDriverName + STATIC_WCSLEN(DriverDir),	pImagePath + ((int)(paDriver - pLine)),  (paDot - paDriver));
		pDriverName[cchDriverLength] = L'\0';

		KdPrint(("INIT: pDriverName = '%S'\n", pDriverName));

		RtlInitUnicodeString( &ImagePath,  pImagePath  );
		RtlInitUnicodeString( &DriverName, pDriverName );

		Status = MmLoadSystemImage (
			&ImagePath,
			&DriverName,
			DriverMode,
			FALSE,
			&ImageBase,
			(PVOID*) &DriverObject
			);

		if (!SUCCESS(Status))
		{
			KdPrint(("MmLoadSystemImage: %S failed with status %08x\n", pImagePath, Status));
			KdPrint(("Failed loading boot driver %S\n", pImagePath));
			ASSERT (FALSE);
		}

		KdPrint(("MmLoadSystemImage: %S Mapped at %08x, DrvObj %08x, Status %08x\n", pImagePath, ImageBase, DriverObject, Status));	

		ExFreeHeap (pImagePath);
		ExFreeHeap (pDriverName);
	}

	MmFreePage (Buffer);

	KdPrint(("INIT: Boot drivers loaded successfully\n"));

//	INT3
}

VOID
KEAPI
KiLoaderThread(
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

	KdPrint(("Allocated MMD at %08x [fl=%08x]\n", Mmd, Mmd->Flags));

	PVOID Mapped = MmMapLockedPages(Mmd, KernelMode, FALSE, FALSE);
	strncpy ((char*)Mapped, "1234", 5);
	
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

	Status = IoReadFile (File, Buffer, 512, NULL, 0, &IoStatus);
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
	
	MmInvalidateTlb (Dummy);

	KdPrint(("Dummy = %08x\n", *Dummy));

	MmInvalidateTlb (Dummy);

	KdPrint(("Pte: A=%d, D=%d\n", PointerPte->u1.e1.Accessed, PointerPte->u1.e1.Dirty));
	*Dummy = 4;
	MmInvalidateTlb (Dummy);
	KdPrint(("Dummy = %08x\n", *Dummy));
	MmInvalidateTlb (Dummy);
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

	KiLoadDeviceDrivers ();

	ObpDumpDirectory (IoDeviceDirectory, 0);
	//ObpDumpDirectory (IoDriverDirectory, 0);

  for(;;)
  { }

	INT3

	KeSetOnScreenStatus ("Reading boot.ini");

	UNICODE_STRING FileName;
	RtlInitUnicodeString( &FileName, L"\\Global\\B:\\boot.ini" );

	Status = IoCreateFile (
		&File,
		FILE_READ_DATA | FILE_WRITE_DATA,
		&FileName,
		&IoStatus,
		FILE_OPEN_EXISTING,
		0
		);
	if (!SUCCESS(Status)) {
		KdPrint(("IoCreateFile failed with status %08x\n", Status));
		INT3
	}

	UCHAR buff[513];
	for (int i=0; i<1; i++)
	{
		Status = IoReadFile (File, buff, SECTOR_SIZE, NULL, 0, &IoStatus);
		if (!SUCCESS(Status)) {
			KdPrint(("IoReadFile failed with status %08x\n", Status));
			INT3
		}

		buff[IoStatus.Information] = 0;

		KdPrint(("Read: \n%s\n", buff));
		//KdPrint(("Read: OK\n"));
	}


	KdPrint(("\nExMutexAcquirementsGlobalCounter = %08x\nExMutexSatisfactionsGlobalCounter = %08x\n",
		ExMutexAcquirementsGlobalCounter,
		ExMutexSatisfactionsGlobalCounter
		));

	//IoCloseFile (File);




	/*
	PEXCALLBACK Callback;
	PEXCALLBACK Callback2;

	VOID KEAPI InitTerminateThreadCallback(PTHREAD,ULONG);
	VOID KEAPI InitCreateThreadCallback(PTHREAD,PVOID,PVOID);
	VOID KEAPI InitDemoThreadStartRoutine(PVOID);

	Status = ExCreateCallback(
		InitTerminateThreadCallback,
		NULL,
		&Callback);

	if (!SUCCESS(Status)) {
		KeBugCheck (KE_INITIALIZATION_FAILED,
					__LINE__,
					Status,
					0,
					0
					);
	}

	Status = ExCreateCallback(
		InitCreateThreadCallback,
		NULL,
		&Callback2);

	if (!SUCCESS(Status)) {
		KeBugCheck (KE_INITIALIZATION_FAILED,
					__LINE__,
					Status,
					0,
					0
					);
	}

	InterlockedInsertHeadList (&PsTerminateThreadCallbackList, &Callback->InternalListEntry);
	InterlockedInsertHeadList (&PsCreateThreadCallbackList, &Callback2->InternalListEntry);

	PTHREAD Thread = PsCreateThread (PsGetCurrentProcess(), InitDemoThreadStartRoutine, NULL);
	*/

	// Fall through counter thread code.
	//PsCounterThread( (PVOID)( 80*5 + 35 ) );

	INT3

	KeSetOnScreenStatus ("Testing file mapping");
	InitIncrementProgressBar ();

	KdPrint(("all ok\n"));
	KdPrint(("Testing file mapping\n"));

	HANDLE hMapping;
	
	Status = MmCreateFileMapping (
		File,
		KernelMode,
		//MM_READONLY,
		MM_READWRITE,
		&hMapping
		);

	KdPrint(("MmCreateFileMapping for [boot.ini] : %08x\n", Status));

	if(!SUCCESS(Status)) INT3;

	PVOID VirtualAddress = NULL;

	Status = MmMapViewOfFile (
		hMapping,
		0,
		0,
		PAGE_SIZE,
		//MM_READONLY,
		MM_READWRITE,
		&VirtualAddress
		);

	KdPrint(("MmMapViewOfFile for [hMapping=%04x] : %08x [VA=%08x]\n", hMapping, Status, VirtualAddress));

	if(!SUCCESS(Status)) INT3;

//	MiDisplayMappings ();

	char buf[100];
	Pte = MiGetPteAddress (VirtualAddress);

	memcpy (buf, VirtualAddress, 99);
	buf[99] = 0;

	KdPrint(("-->buf : %s\n", buf));
	KdPrint(("ACCESSED %X DIRTY %X\n", Pte->u1.e1.Accessed, Pte->u1.e1.Dirty));

	*(ULONG*)VirtualAddress = 'GNOL';

	memcpy (buf, VirtualAddress, 99);
	buf[99] = 0;

	KdPrint(("-->buf : %s\n", buf));
	KdPrint(("ACCESSED %X DIRTY %X\n", Pte->u1.e1.Accessed, Pte->u1.e1.Dirty));

	//INT3

	CcPurgeCacheFile (File);
	KdPrint(("CC: Cache purged\n"));

	INT3

	MiDisplayMappings ();

	extern POBJECT_DIRECTORY ObGlobalObjectDirectory;
	ObpDumpDirectory (ObGlobalObjectDirectory, 0);
	ObpDumpDirectory (IoDeviceDirectory, 0);

	//INT3

	IoCloseFile (File);

	KeSetOnScreenStatus ("Loaded");
	INT3
}

VOID KEAPI InitCreateThreadCallback(PTHREAD Thread, PVOID StartRoutine, PVOID StartContext)
{
	KdPrint(("InitCreateThreadCallback: Thread=%08x, Routine=%08x, Context=%08x\n", Thread, StartRoutine, StartContext));
}

VOID KEAPI InitTerminateThreadCallback(PTHREAD Thread,ULONG Code)
{
	KdPrint(("InitTerminateThreadCallback: Thread=%08x, Code=%08x\n", Thread, Code));
}

VOID KEAPI InitDemoThreadStartRoutine(PVOID)
{
	KdPrint(("In new thread - InitDemoThreadStartRoutine\n"));
	PsExitThread (0x99773311);
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

POBJECT_TYPE KeEventObjectType;
POBJECT_DIRECTORY KeBaseNamedObjectsDirectory;

extern "C" void _cdecl set_debug_traps (void);
extern "C" void _cdecl breakpoint (void);

VOID
KEAPI
Kd2Initialize (
	VOID
	);

BOOLEAN
KEAPI
Kd2PrintString (
	PSTR String
	);

KENORETURN
VOID
KEAPI
KiInitSystem(
	PLOADER_ARGUMENTS LdrArgs
	)
{
	STATUS Status;

	KiInitializeOnScreenStatusLine();
	KeSetOnScreenStatus ("Loading");

	KiLoaderBlock = *LdrArgs;

	KiDebugPrint( "KERNEL: Got execution. Starting with %d megabytes of RAM on board\n", LdrArgs->PhysicalMemoryPages / 256);

	if (LdrArgs->PhysicalMemoryPages/256 < 52)
	{
		panic("KERNEL: Not enough memory to run, need at least 52 megs. System halted\n");
	}

	KiDebugPrintRaw( "INIT: Initializing kernel\n" );
	KeSetOnScreenStatus ("Initializing kernel (phase0)");

	//
	// Phase0: Low-level initialization
	//

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

	KiMoveLoadingProgressBar (1);

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


	//
	// Phase 1: Initializing high-level subsystems
	//

	KeSetOnScreenStatus ("Initializing kernel (phase1)");
	KiMoveLoadingProgressBar (2);

	InitializeLockedList (&KeBugcheckDispatcherCallbackList);

	// Initialize memory management
	MmInitSystem( );
	KiDebugPrintRaw( "MM: Passed initialization\n" );

	// Initialize objective subsystem
	ObInitSystem( );
	KiDebugPrintRaw( "OB: Passed initialization\n" );

	// Initialize I/O subsystem
	IoInitSystem( );
	KiDebugPrintRaw( "IO: Passed initialization\n" );

	// Initiialize kernel
	UNICODE_STRING Name;

	RtlInitUnicodeString (&Name, L"Event");

	Status = ObCreateObjectType (
		&KeEventObjectType,
		&Name,
		NULL,
		NULL,
		NULL,
		NULL,
		NULL,
		OB_OBJECT_OWNER_KE
		);

	if (!SUCCESS(Status))
	{
		KeBugCheck (KE_INITIALIZATION_FAILED, __LINE__, Status, 0, 0);
	}

	RtlInitUnicodeString (&Name, L"BaseNamedObjects");

	Status = ObCreateDirectory (
		&KeBaseNamedObjectsDirectory, 
		&Name,
		OB_OBJECT_OWNER_IO,
		ObRootObjectDirectory
		);

	if (!SUCCESS(Status))
	{
		KeBugCheck (KE_INITIALIZATION_FAILED, __LINE__, Status, 0, 0);
	}

	KiDebugPrintRaw( "INIT: Initialization phase 1 completed, Starting initialization phase 2\n"  );

	//
	// Phase 2: Finalizing high-level initialization
	//

	KeSetOnScreenStatus ("Initializing kernel (phase2)");
	KiInitializationPhase = 2;

	KiMoveLoadingProgressBar (3);

	MmInitSystem( );
	KiDebugPrintRaw( "MM: Finalized initialization\n" );

	ExInitSystem( );
	KiDebugPrintRaw( "EX: Finalized initialization\n" );

	KiDebugPrintRaw( "INIT: Initialization phase 2 completed. Initialization completed.\n"  );

#if COM_DEBUG

	do
	{
		KdWakeUpDebugger (0x80000003, 0, 0, 0, 0, 0, (ULONG) &KiInitSystem, TRUE);
//		KdWakeUpDebugger (STATUS_BREAKPOINT, 0, 0, 0, 0, 0, &KiInitSystem, FALSE);

//		KiDebugPrint("What the fuck?!");
		panic("What the fuck?!");
	}
	while (TRUE);

	/*
	if (!HalpInitializeComPort (0, 9600))
	{
		panic ("HAL: com port not connected (required by debug build)\n");		
	}

	KiDebugPrintRaw ("HAL: com port initialized\n");
	set_debug_traps ();
	for(;;)
		breakpoint ();
	*/
#elif KD2_DBG

	Kd2Initialize ();
	
	do {
		Kd2PrintString ("Hello to WinDbg!\n");
		Kd2PrintString ("Running GR8OS...\n");
	}
	while (TRUE);
#endif

	//
	// Start demo threads
	//

	KiMoveLoadingProgressBar (4);

	// Create two additional threads
	KeInitializeEvent (&ev, SynchronizationEvent, 0);

	KiDebugPrint ("PS: ZeroPageThread=%08x\n", &SystemThread);

//	PspCreateThread( &Thread1, &InitialSystemProcess, PsCounterThread, (PVOID)( 80*3 + 40 ) );
//	PspCreateThread( &Thread2, &InitialSystemProcess, PsCounterThread, (PVOID)( 80*4 + 45 ) );
	LoaderThread = PsCreateThread( &InitialSystemProcess, KiLoaderThread, NULL );

	ASSERT (LoaderThread != NULL);

	KiDebugPrintRaw( "INIT: Initialization completed.\n\n" );

	MiZeroPageThread( );
}