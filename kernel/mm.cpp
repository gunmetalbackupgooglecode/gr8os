//
// FILE:		mm.cpp
// CREATED:		17-Feb-2008  by Great
// PART:        MM
// ABSTRACT:
//			Memory Management routines
//

#include "common.h"

MUTEX MmPageDatabaseLock;

VOID
KEAPI
MmCreateAddressSpace(
	PPROCESS Process
	)
/*++
	Creates process' address space
--*/
{
	KiDebugPrintRaw( "MM: MmCreateAddressSpace unimplemented\n" );

	KeBugCheck (MEMORY_MANAGEMENT,
				__LINE__,
				0,
				0,
				0);
}



LOCKED_LIST PsLoadedModuleList;

LDR_MODULE MiKernelModule;

PMMWORKING_SET MiSystemWorkingSet;

PVOID MmAcpiInfo;


VOID
KEAPI
MmInitSystemPhase1(
	)
{
	ExInitializeMutex (&MmPageDatabaseLock);
	ExInitializeMutex (&MmPpdLock);

	MiMapPhysicalPages (MM_PPD_START, MM_PPD_START_PHYS, (KiLoaderBlock.PhysicalMemoryPages * sizeof(MMPPD)) / PAGE_SIZE);


	//
	// Initialize our module
	//

	InitializeLockedList (&PsLoadedModuleList);

	MiKernelModule.Base = (PVOID) MM_KERNEL_START;
	RtlInitUnicodeString (&MiKernelModule.ModuleName, L"kernel.exe");

	InterlockedInsertTailList (&PsLoadedModuleList, &MiKernelModule.ListEntry);

	PIMAGE_DOS_HEADER KrnlDosHdr = (PIMAGE_DOS_HEADER) MM_KERNEL_START;
	PIMAGE_NT_HEADERS KrnlNtHdrs = (PIMAGE_NT_HEADERS) ((ULONG)MM_KERNEL_START + KrnlDosHdr->e_lfanew);

	MiKernelModule.Size = KrnlNtHdrs->OptionalHeader.ImageBase;

	//
	// Fill MMPPDs appropriately
	//

	PMMPPD Ppd = MmPpdDatabase = (PMMPPD) MM_PPD_START;


	// Describe first meg, PTEs, PPDs and heap.
	ULONG InitialPageCount = 0x3000;
	
	//
	// Set CR0.WP = 1
	//

	KeChangeWpState (TRUE);


	//
	// Create system working set
	//

	MiSystemWorkingSet = (PMMWORKING_SET) ExAllocateHeap (FALSE, sizeof(MMWORKING_SET));
	bzero (MiSystemWorkingSet, sizeof(MMWORKING_SET));

	MiSystemWorkingSet->WsPages = (PMMWORKING_SET_ENTRY) ExAllocateHeap (FALSE, InitialPageCount*sizeof(MMWORKING_SET_ENTRY));
	bzero (MiSystemWorkingSet->WsPages, sizeof(MMWORKING_SET_ENTRY)*InitialPageCount);

	ExInitializeMutex (&MiSystemWorkingSet->Lock);
	MiSystemWorkingSet->TotalPageCount = InitialPageCount;
	MiSystemWorkingSet->LockedPageCount = InitialPageCount;
	MiSystemWorkingSet->OwnerMode = KernelMode;
	MiSystemWorkingSet->Owner = &InitialSystemProcess;

	InitialSystemProcess.WorkingSet = MiSystemWorkingSet;

	KdPrint(("MmInitSystem: initial process' working set %08x\n", MiSystemWorkingSet));
	
	ULONG i;

	for (i=0; i<InitialPageCount; i++)
	{
		Ppd[i].PageLocation = ActiveAndValid;
		Ppd[i].BelongsToSystemWorkingSet = TRUE;
		Ppd[i].ProcessorMode = KernelMode;

		MiSystemWorkingSet->WsPages[i].Present = TRUE;
		MiSystemWorkingSet->WsPages[i].LockedInWs = TRUE;
		MiSystemWorkingSet->WsPages[i].PageDescriptor = &Ppd[i];

		Ppd[i].u1.WsIndex = i;
		Ppd[i].ShareCount = 1;
	}

	ULONG VirtualAddresses[][2] = {
		{ 0x80000000, 0x80FFFFFF },
		{ 0xC0000000, 0xCFFFFFFF }
	};

	for ( int j=0; i<2; i++ )
	{
		KdPrint(("MI: Initializing range 0x%08x - 0x%08x\n", VirtualAddresses[j][0], VirtualAddresses[j][1]));

		for (ULONG VirtualAddress = VirtualAddresses[j][0]; VirtualAddress < VirtualAddresses[j][1]; VirtualAddress += PAGE_SIZE)
		{
			PMMPTE Pte = MiGetPteAddress (VirtualAddress);

			if (Pte->u1.e1.Valid)
			{
				PMMPPD Ppd = MmPfnPpd (Pte->u1.e1.PageFrameNumber);

				ASSERT (Ppd->PageLocation == ActiveAndValid);

				Ppd->u2.PointerPte = Pte;
			}
		}
	}

	//
	// All other pages describe as Zeroed pages.
	//

	MmZeroedPageListHead = &Ppd[i];

	KdPrint(("MI: MM have %d megabytes (0x%08x pages) of free memory\n", 
		(KiLoaderBlock.PhysicalMemoryPages-i)/256, 
		KiLoaderBlock.PhysicalMemoryPages-i
		));

	if (KiLoaderBlock.PhysicalMemoryPages-i == 0)
		MmZeroedPageListHead = NULL;

	for (; i<KiLoaderBlock.PhysicalMemoryPages; i++)
	{
		Ppd[i].PageLocation = ZeroedPageList;
		Ppd[i].ShareCount = 0;

		if (i > InitialPageCount)
		{
			Ppd[i].u2.PrevBlink = &Ppd[i-1];
		}

		if (i == KiLoaderBlock.PhysicalMemoryPages-1)
		{
			Ppd[i].u1.NextFlink = NULL;
		}
		else
		{
			Ppd[i].u1.NextFlink = &Ppd[i+1];
		}
	}

	//
	// Read bios memory map
	//

	KdPrint(("&Runs=%08x, MemoryRunsLoaded: %d, ErrorWas: %d\n", 
		&KiLoaderBlock.MemoryRuns,
		KiLoaderBlock.PhysicalMemoryRunsLoaded,
		KiLoaderBlock.PhysicalMemoryRunsError));

	char *RunTypes[] = {
		"AddressRangeMemory",
		"AddressRangeReserved",
		"AddressRangeACPI",
		"AddressRangeNVS",
		"UnknownType5",
		"UnknownType6",
		"UnknownType7"
	};

#if 0
	KdPrint(("MM: Mapping\n"));
	INT3
	MmAcpiInfo = MmMapPhysicalPages (0x7ff0000, 16);
	KdPrint(("MM: MmAcpiInfo mapped at %08x\n", MmAcpiInfo));
#endif

	for (int i=0; i<KiLoaderBlock.PhysicalMemoryRunsLoaded; i++)
	{
		PPHYSICAL_MEMORY_RUN Run = &KiLoaderBlock.MemoryRuns[i];

		KdPrint(("Run#%d: %08x`%08x - %08x`%08x, type %d [%s]\n", 
			i,
			Run->BaseAddressHigh,
			Run->BaseAddress,
			Run->BaseAddressHigh + Run->LengthHigh,
			Run->BaseAddress + Run->Length,
			Run->Type,
			RunTypes [Run->Type-1]
			));

		switch (Run->Type)
		{
		case AddressRangeReserved:
			if (Run->BaseAddress < 0x00100000)
			{
				HalReservePhysicalLowMegPages (Run->BaseAddress, Run->BaseAddress + Run->Length);
			}
			else if (Run->BaseAddress < KiLoaderBlock.PhysicalMemoryPages<<PAGE_SHIFT)
			{
				MmReservePhysicalAddressRange (Run->BaseAddress, Run->BaseAddress + Run->Length);
			}
			break;

		case AddressRangeACPI:
			KdPrint(("Mapping page count %08x\n", (ALIGN_UP(Run->BaseAddress + Run->Length,PAGE_SIZE) - ALIGN_DOWN(Run->BaseAddress,PAGE_SIZE))/PAGE_SIZE));
			MmAcpiInfo = MmMapPhysicalPagesKernel (Run->BaseAddress, 
				(ALIGN_UP(Run->BaseAddress + Run->Length,PAGE_SIZE) - ALIGN_DOWN(Run->BaseAddress,PAGE_SIZE))/PAGE_SIZE);
			KdPrint(("MM: MmAcpiInfo mapped at %08x\n", MmAcpiInfo));
			break;

		}

	}
}


POBJECT_TYPE MmExtenderObjectType;
POBJECT_TYPE MmFileMappingObjectType;

VOID
KEAPI
MiDeleteMapping(
	IN POBJECT_HEADER Object
	);

VOID
KEAPI
MmInitSystemPhase2(
	)
{
	STATUS Status;
	UNICODE_STRING Name;
	POBJECT_DIRECTORY MmExtenderDirectory;

	RtlInitUnicodeString (&Name, L"Extender");

	Status = ObCreateDirectory( &MmExtenderDirectory, &Name, OB_OBJECT_OWNER_MM, NULL);
	if (!SUCCESS(Status))
	{
		KeBugCheck (MM_INITIALIZATION_FAILED,
					__LINE__,
					Status,
					0,
					0);
	}

	Status = ObCreateObjectType (
		&MmExtenderObjectType, 
		&Name,
		NULL,
		NULL,
		NULL,
		NULL,
		NULL,
		OB_OBJECT_OWNER_MM);

	if (!SUCCESS(Status))
	{
		KeBugCheck (MM_INITIALIZATION_FAILED,
					__LINE__,
					Status,
					0,
					0);
	}


	RtlInitUnicodeString( &Name, L"File Mapping" );

	Status = ObCreateObjectType (
		&MmFileMappingObjectType, 
		&Name,
		NULL,
		NULL,
		NULL,
		MiDeleteMapping,
		NULL,
		OB_OBJECT_OWNER_MM);

	if (!SUCCESS(Status))
	{
		KeBugCheck (MM_INITIALIZATION_FAILED,
					__LINE__,
					Status,
					0,
					0);
	}

	ExInitializeMutex (&MmExtenderList.Lock);
}

static
char*
MiPageStatuses[] = {
	"PageStatusFree",
	"PageStatusNormal",
	"PageStatusPagedOut",
	"PageStatusTrimmed",
	"PageStatusView",
	"PageStatusNormalView"
};

VOID
KEAPI
MiDisplayMappings(
	)
/*++
	Display page mappings of the current process.
--*/
{
	PVOID VirtualAddress = (PVOID) MM_USERMODE_AREA;

	ExAcquireMutex (&MmPageDatabaseLock);

	while (VirtualAddress < MM_HYPERSPACE_START)
	{
		ULONG ps;

		if ( (ps=MmIsAddressValidEx(VirtualAddress)) != PageStatusFree)
		{
			PVOID TempVa = (PVOID)( (ULONG)VirtualAddress + PAGE_SIZE );

			while (TempVa < MM_HYPERSPACE_START && MmIsAddressValidEx (TempVa) == ps)
				*(ULONG*)&TempVa += PAGE_SIZE;

			KdPrint(("%08x - %08x : %s\n", VirtualAddress, (ULONG)TempVa-1, MiPageStatuses[ps]));

			VirtualAddress = TempVa;
		}
		else
		{
			*(ULONG*)&VirtualAddress += PAGE_SIZE;
		}
	}

	ExReleaseMutex (&MmPageDatabaseLock);
}

VOID
KEAPI
MmInitSystem(
	)
/*++
	Initialize memory manager
--*/
{
	if (KiInitializationPhase == 1)
	{
		MmInitSystemPhase1();
	}
	else if (KiInitializationPhase == 2)
	{
		MmInitSystemPhase2();
	}
}




VOID
KEAPI
MiZeroPageThread(
	)
{
	PVOID VirtualAddress = MM_HYPERSPACE_START;

	for(;;)
	{
		PsDelayThreadExecution (100);

#if DBG
		BOOLEAN SomethingWasZeroed = FALSE;
		if (MmFreePageListHead)
		{
			KdPrint(("MiZeroPageThread waked up\n"));
		}
#endif

		MI_LOCK_PPD();

		PMMPPD Ppd = MmFreePageListHead;
		while (Ppd)
		{
			PMMPPD Next = Ppd->u1.NextFlink;

			KdPrint(("Zeroing page %08x [PPD=%08x, NEXT=%08x]\n", MmGetPpdPfn(Ppd), Ppd, Next));

			MiMapPageToHyperSpace (MmGetPpdPfn(Ppd));
			memset (VirtualAddress, 0, PAGE_SIZE);

			MiChangePageLocation (Ppd, ZeroedPageList);
			Ppd = Next;

#if DBG
			SomethingWasZeroed = 1;
#endif
		}

		MI_UNLOCK_PPD();

#if DBG
		if (SomethingWasZeroed)
		{
			KdPrint(("MiZeroPageThread sleeping..\n"));
		}
#endif
	}
}



