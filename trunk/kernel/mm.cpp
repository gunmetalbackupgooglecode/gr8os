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
MiMapPhysicalPages(
	PVOID VirtualAddress,
	PHYSICAL_ADDRESS PhysicalAddress,
	ULONG PageCount
	)
/*++
	Map physical pages to kernel virtual address space
--*/
{
	VirtualAddress = (PVOID) ALIGN_DOWN ((ULONG)VirtualAddress, PAGE_SIZE);
	PMMPTE PointerPte = MiGetPteAddress (VirtualAddress);

	ExAcquireMutex (&MmPageDatabaseLock);

	for( ULONG i=0; i<PageCount; i++ )
	{
		MiWriteValidKernelPte (PointerPte);
		PointerPte->u1.e1.PageFrameNumber = (PhysicalAddress >> PAGE_SHIFT) + i;

//		KdPrint(("MM: Mapped %08x [<-%08x] pte %08x\n", (ULONG)VirtualAddress + (i<<PAGE_SHIFT), PointerPte->u1.e1.PageFrameNumber<<PAGE_SHIFT, PointerPte));

		MmInvalidateTlb (VirtualAddress);

		PointerPte = MiNextPte (PointerPte);
		*(ULONG*)&VirtualAddress += PAGE_SIZE;
	}

	ExReleaseMutex (&MmPageDatabaseLock);
}

#define MM_PTE_IS_FREE(PTE) \
	( (PointerPte)->u1.e1.Valid == 0 && (PointerPte)->u1.e1.PteType == PTE_TYPE_NORMAL_OR_NOTMAPPED )

KESYSAPI
PVOID
KEAPI
MmMapPhysicalPagesKernel(
	PHYSICAL_ADDRESS PhysicalAddress,
	ULONG PageCount
	)
/*++
	Map physical pages to an arbitrary virtual address.
	Returns the virtual address of the mapped pages
--*/
{
	return MmMapPhysicalPagesInRange (
		(PVOID)MM_CRITICAL_AREA,
		(PVOID)(MM_CRITICAL_AREA_END),
		PhysicalAddress,
		PageCount,
		FALSE
		);
}


KESYSAPI
PVOID
KEAPI
MmMapPhysicalPagesInRange(
	PVOID VirtualAddressStart,
	PVOID VirtualAddressEnd,
	PHYSICAL_ADDRESS PhysicalAddress,
	ULONG PageCount,
	BOOLEAN AddToWorkingSet
	)
/*++
	Try to map physical pages within the specified virtual address range
--*/
{
	ExAcquireMutex (&MmPageDatabaseLock);

	ULONG StartVirtual = (ULONG) ALIGN_DOWN((ULONG)VirtualAddressStart,PAGE_SIZE);
	ULONG EndVirtual = (ULONG)ALIGN_UP((ULONG)VirtualAddressEnd,PAGE_SIZE);

	ULONG Virtual;

	PMMPTE PointerPte = MiGetPteAddress (StartVirtual);

	for (ULONG i=0; i<((EndVirtual-StartVirtual)>>PAGE_SHIFT); i++)
	{
		Virtual = StartVirtual + (i<<PAGE_SHIFT);
		PointerPte = MiGetPteAddress (Virtual);

//		KdPrint(("Checking pte %08x, va %08x, valid=%d, type=%d, PFN=%08x\n", PointerPte, Virtual, PointerPte->u1.e1.Valid,
//			PointerPte->u1.e1.PteType, PointerPte->u1.e1.PageFrameNumber));

		if ( MM_PTE_IS_FREE(PointerPte) )
		{
//			KdPrint(("Found free pte %08x, va %08x\n", PointerPte, Virtual));

			BOOLEAN Free = 1;

			for ( ULONG j=1; j<PageCount; j++ )
			{
				Free &= ( MM_PTE_IS_FREE (&PointerPte[j]) );
			}

			if (Free)
			{
				ULONG VirtualAddress = Virtual;

				for( ULONG j=0; j<PageCount; j++ )
				{
					MiWriteValidKernelPte (PointerPte);
					PointerPte->u1.e1.PageFrameNumber = (PhysicalAddress >> PAGE_SHIFT) + (j);

#if MM_TRACE_MMDS
					KdPrint(("MM: Mapping PFN %08x to VA %08x\n", PointerPte->u1.e1.PageFrameNumber, VirtualAddress));
#endif

					MI_LOCK_PPD();

					PMMPPD Ppd = MmPfnPpd(PointerPte->u1.e1.PageFrameNumber);
					Ppd->ShareCount ++;
					
					if (Ppd->ShareCount == 1)
					{
						//
						// If this is the first mapping, fill out some fiedls
						//

						Ppd->u2.PointerPte = PointerPte;
						Ppd->u1.WsIndex = -1; 
						
						if (AddToWorkingSet)
						{
							Ppd->u1.WsIndex = MiAddPageToWorkingSet (Ppd);

							if (Ppd->u1.WsIndex == -1)
							{
								KeBugCheck (MEMORY_MANAGEMENT,
											__LINE__,
											(ULONG)Ppd,
											0,
											0
											);
							}
						}
					}
					
					MI_UNLOCK_PPD();

					MmInvalidateTlb ((PVOID)VirtualAddress);

					PointerPte = MiNextPte (PointerPte);
					VirtualAddress += PAGE_SIZE;
				}

				ExReleaseMutex (&MmPageDatabaseLock);
				return (PVOID)Virtual;
			}

			//i += PageCount - 1;
		}
	}

	ExReleaseMutex (&MmPageDatabaseLock);
	return NULL;
}


VOID
KEAPI
MiUnmapPhysicalPages(
	PVOID VirtualAddress,
	ULONG PageCount
	)
/*++
	Unmap physical pages from the kernel virtual address space
--*/
{
	MI_LOCK_PPD();
	ExAcquireMutex (&MmPageDatabaseLock);


	VirtualAddress = (PVOID) ALIGN_DOWN ((ULONG)VirtualAddress, PAGE_SIZE);
	PMMPTE PointerPte = MiGetPteAddress (VirtualAddress);

	//KeZeroMemory( PointerPte, PageCount*sizeof(MMPTE) );
	for (ULONG i=0; i<PageCount; i++)
	{
//		KdPrint(("MM: Unmapping %08x [->%08x] pte %08x\n", (ULONG)VirtualAddress + (i<<PAGE_SHIFT), (PointerPte->u1.e1.PageFrameNumber)<<PAGE_SHIFT, PointerPte));
		
		PMMPPD Ppd = MmPfnPpd (PointerPte->u1.e1.PageFrameNumber);

		PointerPte->RawValue = 0;

		Ppd->ShareCount --;
		if (Ppd->ShareCount == 0)
		{
			//
			// Page is not used anymore. Remove from working set
			//

			MiRemovePageFromWorkingSet (Ppd);
			Ppd->u1.WsIndex = -1;
		}

		MmInvalidateTlb ((PVOID)((ULONG)VirtualAddress + (i<<PAGE_SHIFT)));

		PointerPte = MiNextPte (PointerPte);
	}

	ExReleaseMutex (&MmPageDatabaseLock);
	MI_UNLOCK_PPD();
}

VOID
KEAPI
MiMapPageToHyperSpace(
	ULONG Pfn
	)
/*++
	Maps page to hyperspace
--*/
{
	PMMPTE PointerPte = MiGetPteAddress (MM_HYPERSPACE_START);

	MiWriteValidKernelPte (PointerPte);
	PointerPte->u1.e1.PageFrameNumber = Pfn;
}


char MmDebugBuffer[1024];

VOID
KEAPI
MmAccessFault(
	PVOID VirtualAddress,
	PVOID FaultingAddress
	)
/*++
	Resolve access fault
--*/
{
	sprintf( MmDebugBuffer, "MM: Page fault at %08x, referring code: %08x\n", VirtualAddress, FaultingAddress );
	KiDebugPrintRaw( MmDebugBuffer );

	PMMPTE PointerPte = MiGetPteAddress (VirtualAddress);

	switch (PointerPte->u1.e1.PteType)
	{
	case PTE_TYPE_NORMAL_OR_NOTMAPPED:
		{
			sprintf( MmDebugBuffer, "MM: Access violation in %08x [invalid page]\n", VirtualAddress );
			KiDebugPrintRaw( MmDebugBuffer );

			__asm lea esi, [MmDebugBuffer]
			__asm xor ax, ax
			__asm int 0x30

			break;
		}

	case PTE_TYPE_PAGEDOUT:
		{
			sprintf( MmDebugBuffer, "MM: Access violation in %08x [paging not supported]\n", VirtualAddress );
			KiDebugPrintRaw( MmDebugBuffer );

			__asm lea esi, [MmDebugBuffer]
			__asm xor ax, ax
			__asm int 0x30

			break;
		}

	case PTE_TYPE_TRIMMED:
		{
			sprintf( MmDebugBuffer, "MM: Access violation in %08x [trimming not supported]\n", VirtualAddress );
			KiDebugPrintRaw( MmDebugBuffer );

			__asm lea esi, [MmDebugBuffer]
			__asm xor ax, ax
			__asm int 0x30

			break;
		}

	case PTE_TYPE_VIEW:
		{
			sprintf( MmDebugBuffer, "MM: Access violation in %08x [views not supported]\n", VirtualAddress );
			KiDebugPrintRaw( MmDebugBuffer );

			__asm lea esi, [MmDebugBuffer]
			__asm xor ax, ax
			__asm int 0x30

			break;
		}
	}

	KeBugCheck (KERNEL_MODE_EXCEPTION_NOT_HANDLED,
				STATUS_ACCESS_VIOLATION,
				(ULONG)VirtualAddress,
				(ULONG)FaultingAddress,
				0
				);
	
	KiStopExecution( );
}


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


KESYSAPI
BOOLEAN
KEAPI
MmIsAddressValid(
	IN PVOID VirtualAddress
	)
/*++
	This function checks that specified virtual address is valid
--*/
{
	PMMPTE PointerPte = MiGetPteAddress (VirtualAddress);
	return PointerPte->u1.e1.Valid;
}

KESYSAPI
ULONG
KEAPI
MmIsAddressValidEx(
	IN PVOID VirtualAddress
	)
/*++
	This function checks that specified virtual address is valid and determines the page type
--*/
{
	PMMPTE PointerPte = MiGetPteAddress (VirtualAddress);

	if (PointerPte->u1.e1.Valid)
	{
		return PageStatusNormal;
	}

	switch (PointerPte->u1.e1.PteType)
	{
	case PTE_TYPE_NORMAL_OR_NOTMAPPED:
		return PageStatusFree;

	case PTE_TYPE_TRIMMED:
		return PageStatusTrimmed;
	
	case PTE_TYPE_PAGEDOUT:
		return PageStatusPagedOut;

	case PTE_TYPE_VIEW:
		return PageStatusView;
	}

	KeBugCheck (MEMORY_MANAGEMENT,
				__LINE__,
				(ULONG)PointerPte,
				PointerPte->u1.e1.PteType,
				0);
}


LOCKED_LIST PsLoadedModuleList;

LDR_MODULE MiKernelModule;

PMMWORKING_SET MiSystemWorkingSet;

PMMPPD MmZeroedPageListHead;
PMMPPD MmFreePageListHead;
PMMPPD MmModifiedPageListHead;
PMMPPD MmStandbyPageListHead;

PMMPPD *MmPageLists[ActiveAndValid] = {
	&MmZeroedPageListHead,
	&MmFreePageListHead,
	&MmModifiedPageListHead,
	&MmStandbyPageListHead
};

PMMPPD MmPpdDatabase;

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

	for ( int i=0; i<2; i++ )
	{
		KdPrint(("MI: Initializing range 0x%08x - 0x%08x\n", VirtualAddresses[i][0], VirtualAddresses[i][1]));

		for (ULONG VirtualAddress = VirtualAddresses[i][0]; VirtualAddress < VirtualAddresses[i][1]; VirtualAddress += PAGE_SIZE)
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

	ExInitializeMutex (&MmExtenderList.Lock);
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
MiUnlinkPpd(
	PMMPPD Ppd
	)
{
	ASSERT (Ppd->PageLocation < ActiveAndValid);

	PMMPPD Prev = Ppd->u2.PrevBlink;
	PMMPPD Next = Ppd->u1.NextFlink;

	if (Prev == NULL)
	{
		//
		// This page is the first.
		// So, set the list head to point to the next page
		//

		*MmPageLists[Ppd->PageLocation] = Next;

		if (Next)	// this condition is required because page can be the first and the last both.
		{
			Next->u2.PrevBlink = NULL;
		}
	}
	
	if (Next == NULL)
	{
		//
		// This page is the last.
		// Set previous page to be the last.
		//

		if (Prev)	// this condition is required because page can be the first and the last both.
		{
			Prev->u1.NextFlink = NULL;
		}
	}

	if (Prev && Next)
	{
		Prev->u1.NextFlink = Next;
		Next->u2.PrevBlink = Prev;
	}
}

VOID
KEAPI
MiLinkPpd(
	PMMPPD *List,
	PMMPPD Ppd
	)
{
	Ppd->u1.NextFlink = *List;
	Ppd->u2.PrevBlink = NULL;
	if (*List)
	{
		(*List)->u2.PrevBlink = Ppd;
	}
	*List = Ppd;
}


VOID
KEAPI
MiChangePageLocation(
	PMMPPD Ppd,
	UCHAR NewLocation
	)
/*++
	This function changes page location and optionally
	 moves it to the appripriate page list
 --*/
{
	ASSERT (Ppd->PageLocation != NewLocation);

	if (Ppd->PageLocation < ActiveAndValid)
	{
		MiUnlinkPpd (Ppd);
	}

	if (NewLocation < ActiveAndValid)
	{
		MiLinkPpd ( MmPageLists[NewLocation], Ppd );
	}
	else
	{
		Ppd->u1.WsIndex = 0;
		Ppd->u2.PointerPte = NULL;
	}

	Ppd->PageLocation = NewLocation;
}


PCHAR
MiPageLocations[] = {
	"ZeroedPageList",
	"FreePageList",
	"ModifiedPageList",
	"StandbyPageList",
	"ActiveAndValid",
	"TransitionPage",
	"ReservedOrBadPage"
};

VOID
KEAPI
MiDumpPageLists(
	)
{
	MI_LOCK_PPD();

	for (ULONG PageList = ZeroedPageList; PageList < ActiveAndValid; PageList ++)
	{
		KdPrint (("MM: Dumping page list %d [%s]\n", PageList, MiPageLocations[PageList]));

		ULONG i = 0;

		for (PMMPPD Ppd = *MmPageLists[PageList]; Ppd && i<15; Ppd = Ppd->u1.NextFlink, i++)
		{
			KdPrint(("MM: Ppd=%08x, PFN=%08x, Next=%08x, Prev=%08x\n",
				Ppd,
				MmGetPpdPfn(Ppd),
				Ppd->u1.NextFlink,
				Ppd->u2.PrevBlink
				));
		}
	}

	MI_UNLOCK_PPD();
}

MUTEX MmPpdLock;


KESYSAPI
VOID
KEAPI
MmReservePhysicalAddressRange(
	PHYSICAL_ADDRESS PhysStart,
	PHYSICAL_ADDRESS PhysEnd
	)
/*++
	Reserve the specified address range
--*/
{
	MI_LOCK_PPD();

	PhysStart = ALIGN_DOWN (PhysStart, PAGE_SIZE);
	PhysEnd = ALIGN_UP (PhysEnd, PAGE_SIZE);

	ULONG PageStart = PhysStart >> PAGE_SHIFT;
	ULONG PageEnd = PhysEnd >> PAGE_SHIFT;

	KdPrint(("MM: Reserving pages %05x-%05x\n", PageStart, PageEnd));

	for (ULONG i=PageStart; i<PageEnd; i++)
	{
		PMMPPD Ppd = MmPfnPpd (i);

		KdPrint(("."));

		MiChangePageLocation (Ppd, ReservedOrBadPage);
	}
	KdPrint(("\n"));

	MI_UNLOCK_PPD();
}



KESYSAPI
STATUS
KEAPI
MmAllocatePhysicalPages(
	IN ULONG PageCount,
	OUT PMMD *pMmd
	)
/*++
	Allocate physical pages from zeroed or free page lists
--*/
{
	STATUS Status;

	if (PageCount > KiLoaderBlock.PhysicalMemoryPages)
	{
		//
		// Can't allocate memory greater than physical memory size
		//

		return STATUS_INSUFFICIENT_RESOURCES;
	}

	MI_LOCK_PPD();

	//
	// Try to allocate from free page list
	//

	PMMPPD Ppd = MmFreePageListHead;		// MMPPD iterator

	// Array of allocated PFNs
	ULONG *AllocatedPages = (ULONG*) ExAllocateHeap (TRUE, sizeof(ULONG)*PageCount);
	if (!AllocatedPages)
	{
		MI_UNLOCK_PPD();
		return STATUS_INSUFFICIENT_RESOURCES;
	}

	bzero (AllocatedPages, sizeof(ULONG)*PageCount);

	// Number of allocated pages
	ULONG alloc = 0;

#if MM_TRACE_MMDS
	KdPrint(("MM: Allocating pages from free page list\n"));
#endif

	while (Ppd)
	{
		AllocatedPages[alloc] = MmGetPpdPfn (Ppd);

		PMMPPD Next = Ppd->u1.NextFlink;

		MiChangePageLocation (Ppd, ActiveAndValid);
		
#if MM_TRACE_MMDS
		KdPrint(("MM: Allocated page %08x\n", AllocatedPages[alloc]));
#endif

		Ppd = Next;	
		alloc ++;					// Increment number of allocated pages

		if (alloc == PageCount)
		{
			//
			// We have allocated adequate number of pages.
			//

			goto _finish;
		}
	}

	if (alloc < PageCount)
	{
		//
		// If free page list is too small to satisfy the allocation, try zeroed page list
		//

#if MM_TRACE_MMDS
		KdPrint(("MM: Allocating pages from zeroed page list\n"));
#endif

		Ppd = MmZeroedPageListHead;

		while (Ppd)
		{
			AllocatedPages[alloc] = MmGetPpdPfn (Ppd);
	
			PMMPPD Next = Ppd->u1.NextFlink;

			MiChangePageLocation (Ppd, ActiveAndValid);
			
#if MM_TRACE_MMDS
			KdPrint(("MM: Allocated page %08x\n", AllocatedPages[alloc]));
#endif
			//
			// MMPPD in unlinked from double-linked list, but it contains
			//  valid pointers to previous and next entries
			//

			Ppd = Next;
			alloc ++;					// Increment number of allocated pages

			if (alloc == PageCount)
			{
				//
				// We have allocated adequate number of pages.
				//

				goto _finish;
			}
		}
	}

_finish:

#if MM_TRACE_MMDS
	KdPrint(("MM: Allocated %d pages\n", alloc));
#endif

	MI_UNLOCK_PPD();

	if (alloc < PageCount)
	{
		//
		// We failed to allocate adequate number of pages.
		// Return all pages we have allocated and indicate that only
		// the part of the request has been completed.
		//

		Status = STATUS_PARTIAL_COMPLETION;
	}
	else
	{
		//
		// We have successfully allocated all pages.
		//

		Status = STATUS_SUCCESS;
	}

	//
	// Allocate memory descriptor to describe all allocated pages
	//

	PMMD Mmd = MmAllocateMmd (NULL, alloc*PAGE_SIZE);

	for (ULONG i=0; i<alloc; i++)
	{
		//
		// Copy all PFNs
		//

		Mmd->PfnList[i] = AllocatedPages[i];
	}

	//
	// Free temporary array
	//

	ExFreeHeap (AllocatedPages);

	Mmd->Flags |= MDL_ALLOCATED;

	// Return pointer to MMD
	*pMmd = Mmd;

	// Return status code
	return Status;
}


KESYSAPI
VOID
KEAPI
MmFreePhysicalPages(
	PMMD Mmd
	)
/*++
	Free physical pages allocated by MmAllocatePhysicalPages
--*/
{
	MI_LOCK_PPD();

#if MM_TRACE_MMDS
	KdPrint(("MM: Freeing physical pages to free page list\n"));
#endif

	for (ULONG i=0; i<Mmd->PageCount; i++)
	{
		PMMPPD Ppd = MmPfnPpd(Mmd->PfnList[i]);

#if MM_TRACE_MMDS
		KdPrint(("MM: Freeing page %08x\n", Mmd->PfnList[i]));
#endif
		MiChangePageLocation (Ppd, FreePageList);
	}

	MI_UNLOCK_PPD();

	Mmd->Flags &= ~MDL_ALLOCATED;
}


KESYSAPI
PMMD
KEAPI
MmAllocateMmd(
	IN PVOID VirtualAddress,
	IN ULONG Size
	)
/*++
	Allocate a memory descriptor list and fill some fields
--*/
{
	ULONG PageCount = ALIGN_UP(Size,PAGE_SIZE) / PAGE_SIZE;
	PMMD Mmd = (PMMD) ExAllocateHeap (TRUE, sizeof(MMD) + (PageCount-1)*sizeof(ULONG));

	bzero (Mmd, sizeof(MMD) + (PageCount-1)*sizeof(ULONG));

	Mmd->BaseVirtual = (PVOID) ALIGN_DOWN((ULONG)VirtualAddress, PAGE_SIZE);
	Mmd->Offset = (ULONG)VirtualAddress % PAGE_SIZE;
	Mmd->PageCount = PageCount;

#if MM_TRACE_MMDS
	KdPrint(("MMALLOCMMD: Passed VA=%08x, Mmd->BaseVirtual=%08x, Mmd->Offset=%08x, Mmd->PageCount=%08x\n",
		VirtualAddress,
		Mmd->BaseVirtual,
		Mmd->Offset,
		Mmd->PageCount
		));
#endif

	return Mmd;
}

KESYSAPI
VOID
KEAPI
MmBuildMmdForNonpagedSpace(
	PMMD Mmd
	)
/*++
	Fill memory descriptor with page frame numbers
--*/
{
	PMMPTE PointerPte = MiGetPteAddress (Mmd->BaseVirtual);

#if MM_TRACE_MMDS
	KdPrint(("MM: Building MMD for nonpaged space..\n"));
#endif

	for (ULONG i=0; i<Mmd->PageCount; i++)
	{
		Mmd->PfnList[i] = PointerPte->u1.e1.PageFrameNumber;

#if MM_TRACE_MMDS
		KdPrint(("MM: For page %d, PFN=%08x\n", i, Mmd->PfnList[i]));
#endif

		PointerPte = MiNextPte (PointerPte);
	}
}


KESYSAPI
VOID
KEAPI
MmFreeMmd(
	PMMD Mmd
	)
/*++
	Frees memory descriptor.
	Auxillary, unmap, unlock and free pages if need.
--*/
{
	//
	// Unmap pages if they were mapped
	//

	if (Mmd->Flags & MDL_MAPPED)
	{
		MmUnmapLockedPages (Mmd);
	}

	//
	// Unlock pages if they were locked
	//

	if (Mmd->Flags & MDL_LOCKED)
	{
		MmUnlockPages (Mmd);
	}

	//
	// Free pages if they were allocated.
	//

	if (Mmd->Flags & MDL_ALLOCATED)
	{
		MmFreePhysicalPages (Mmd);
	}

	// Free MMD
	ExFreeHeap (Mmd);
}


KESYSAPI
VOID
KEAPI
MmLockPages(
	IN PMMD Mmd
	)
/*++
	Lock pages in the current process' working set and fill MMD->PfnList appropriately
--*/
{
	PMMWORKING_SET WorkingSet = MmGetCurrentWorkingSet();

	ULONG VirtualAddress = (ULONG) Mmd->BaseVirtual;

	ExAcquireMutex (&WorkingSet->Lock);
	ExAcquireMutex (&MmPageDatabaseLock);

	for (ULONG i=0; i<Mmd->PageCount; i++)
	{
		PMMPTE PointerPte = MiGetPteAddress (VirtualAddress);

		Mmd->PfnList[i] = PointerPte->u1.e1.PageFrameNumber;

		ULONG WsIndex = MmPfnPpd (Mmd->PfnList[i])->u1.WsIndex;

#if MM_TRACE_MMDS
		KdPrint(("MM: Locking PFN %08x in WsIndex=%08x\n", Mmd->PfnList[i], WsIndex));
#endif

		WorkingSet->WsPages[WsIndex].LockedInWs = TRUE;
	}

	ExReleaseMutex (&MmPageDatabaseLock);
	ExReleaseMutex (&WorkingSet->Lock);

	Mmd->Flags |= MDL_LOCKED;
}


KESYSAPI
VOID
KEAPI
MmUnlockPages(
	IN PMMD Mmd
	)
/*++
	Unlock pages in the current process' working set
--*/
{
	PMMWORKING_SET WorkingSet = MmGetCurrentWorkingSet();

	ExAcquireMutex (&WorkingSet->Lock);

	for (ULONG i=0; i<Mmd->PageCount; i++)
	{
		ULONG WsIndex = MmPfnPpd (Mmd->PfnList[i])->u1.WsIndex;

#if MM_TRACE_MMDS
		KdPrint(("MM: Unlocking PFN %08x in WsIndex=%08x\n", Mmd->PfnList[i], WsIndex));
#endif

		KdPrint(("&WorkingSet->WsPages[WsIndex] = %08x\n", &WorkingSet->WsPages[WsIndex]));

		WorkingSet->WsPages[WsIndex].LockedInWs = FALSE;
	}

	ExReleaseMutex (&WorkingSet->Lock);

	Mmd->Flags &= ~MDL_LOCKED;
}


ULONG
KEAPI
MiAddPageToWorkingSet(
	PMMPPD Ppd
	)
/*++
	Add page to current process' working set.
	Return value: working set index of the new page
	Environment: working set mutex held
--*/
{
	PMMWORKING_SET WorkingSet = MmGetCurrentWorkingSet();

	do
	{
		for (ULONG i=0; i<WorkingSet->TotalPageCount; i++)
		{
			if (WorkingSet->WsPages[i].Present == 0)
			{
				WorkingSet->WsPages[i].Present = TRUE;
				WorkingSet->WsPages[i].PageDescriptor = Ppd;
				WorkingSet->WsPages[i].LockedInWs = TRUE;
				return i;
			}
		}

		WorkingSet->TotalPageCount += MM_WORKING_SET_INCREMENT;
		WorkingSet->WsPages= (PMMWORKING_SET_ENTRY) ExReallocHeap (
			WorkingSet->WsPages, 
			sizeof(MMWORKING_SET_ENTRY)*(WorkingSet->TotalPageCount)
			);

		if (WorkingSet->WsPages == NULL)
		{
			return -1;
		}

		bzero (&WorkingSet->WsPages[WorkingSet->TotalPageCount-MM_WORKING_SET_INCREMENT], sizeof(MMWORKING_SET_ENTRY)*MM_WORKING_SET_INCREMENT);

		// Continue the loop, so for() loop will be executed again. So, because we've allocated additional space for WS,
		//  there will be Present==0 entries.
	}
	while (TRUE);
}

VOID
KEAPI
MiRemovePageFromWorkingSet(
	PMMPPD Ppd
	)
/*++
	Add page to current process' working set.
	Return value: working set index of the new page
	Environment: working set mutex held
--*/
{
	PMMWORKING_SET WorkingSet = MmGetCurrentWorkingSet();

	ASSERT (Ppd->PageLocation == ActiveAndValid);

	WorkingSet->WsPages[Ppd->u1.WsIndex].Present = FALSE;
}

KESYSAPI
PVOID
KEAPI
MmMapLockedPages(
	IN PMMD Mmd,
	IN PROCESSOR_MODE TargetMode,
	IN BOOLEAN IsImage,
	IN BOOLEAN AddToWorkingSet
	)
/*++
	Map locked pages to target address space
--*/
{
	//
	// Don't support other modes now..
	//

	ULONG StartVirtual;
	ULONG EndVirtual;
	ULONG Virtual;

	if (TargetMode == KernelMode)
	{
		if (IsImage)
		{
			StartVirtual = (ULONG) MM_CRITICAL_DRIVER_AREA;
			EndVirtual = (ULONG) MM_CRITICAL_DRIVER_AREA_END;
		}
		else
		{
			StartVirtual = (ULONG) MM_CRITICAL_AREA;
			EndVirtual = (ULONG) MM_CRITICAL_AREA_END;
		}
	}
	else if (TargetMode == DriverMode)
	{
		StartVirtual = (ULONG) MM_DRIVER_AREA;
		EndVirtual = (ULONG) MM_DRIVER_AREA_END;
	}
	else if (TargetMode == UserMode)
	{
		StartVirtual = (ULONG) MM_USERMODE_AREA;
		EndVirtual = (ULONG) MM_USERMODE_AREA_END;
	}
	else
	{
		KeRaiseStatus (STATUS_INVALID_PARAMETER);
	}
	

#if MM_TRACE_MMDS
	KdPrint(("MM: Mapping locked pages.\n"));
#endif

	ExAcquireMutex (&MmPageDatabaseLock);

	PMMPTE PointerPte;

	for (ULONG i=0; i<((EndVirtual-StartVirtual)>>PAGE_SHIFT); i++)
	{
		Virtual = StartVirtual + (i<<PAGE_SHIFT);
		PointerPte = MiGetPteAddress (Virtual);

		if ( MM_PTE_IS_FREE(PointerPte) )
		{
			BOOLEAN Free = 1;

			for ( ULONG j=1; j<Mmd->PageCount; j++ )
			{
				Free &= ( MM_PTE_IS_FREE (&PointerPte[j]) );
			}

			if (Free)
			{
				ULONG VirtualAddress = Virtual;

				for( ULONG j=0; j<Mmd->PageCount; j++ )
				{
					MiWriteValidKernelPte (PointerPte);
					PointerPte->u1.e1.PageFrameNumber = Mmd->PfnList[j];

#if MM_TRACE_MMDS
					KdPrint(("MM: Mapping PFN %08x to VA %08x\n", Mmd->PfnList[j], VirtualAddress));
#endif

					MI_LOCK_PPD();

					PMMPPD Ppd = MmPfnPpd(Mmd->PfnList[j]);
					Ppd->ShareCount ++;

					
					if (Ppd->ShareCount == 1)
					{
						//
						// If this is the first mapping, fill out some fiedls
						//

						Ppd->u2.PointerPte = PointerPte;
						Ppd->u1.WsIndex = -1;
						
						if (AddToWorkingSet)
						{
							Ppd->u1.WsIndex = MiAddPageToWorkingSet (Ppd);

							if (Ppd->u1.WsIndex == -1)
							{
								KeBugCheck (MEMORY_MANAGEMENT,
											__LINE__,
											(ULONG)Ppd,
											0,
											0
											);
							}
						}
					}
					

					MI_UNLOCK_PPD();

					MmInvalidateTlb ((PVOID)VirtualAddress);

					PointerPte = MiNextPte (PointerPte);
					VirtualAddress += PAGE_SIZE;
				}

				ExReleaseMutex (&MmPageDatabaseLock);

				Mmd->Flags |= MDL_MAPPED;
				Mmd->MappedVirtual = (PVOID) Virtual;

				return (PVOID)( Virtual + Mmd->Offset );
			}
		}
	}

	ExReleaseMutex (&MmPageDatabaseLock);
	return NULL;
}



KESYSAPI
VOID
KEAPI
MmUnmapLockedPages(
	IN PMMD Mmd
	)
/*++
	Unmap pages from the address space
--*/
{
	MiUnmapPhysicalPages (Mmd->MappedVirtual, Mmd->PageCount);

	Mmd->MappedVirtual = NULL;
	Mmd->Flags &= ~MDL_MAPPED;
}

#if 0
VOID
KEAPI
MiSwapPageFromPhysicalMemory(
	PMMPPD Ppd,
	PMMPTE PointerPte
	)
/*++
	Swap nonmodified page from the physical memory to pagefile
--*/
{
	//
	//BUGBUG: Handle file mapping correctly
	//

	MiWritePagedoutPte (PointerPte);

	for (ULONG pf=0; pf<MAX_PAGE_FILES; pf++)
	{
		if (MmPagingFiles[pf].Size)
		{
		}
	}



	//PointerPte->u1.e1.PageFrameNumber = 
}

ULONG
KEAPI
MiTrimWorkingSet (
	PMMWORKING_SET WorkingSet,
	BOOLEAN ForceGetFreePages
	)
/*++
	This function trims the working set, moving pages to standby or modified lists.
	Also, some pages can be swapped out if ForceGetFreePages==1
	Environment: working set lock held.
	Return Value:
		if (ForceGetFreePages==1) - number of retrieved free pages
		if (ForceGetFreePages==0) - number of trimmed pages
--*/
{
	ULONG TrimmedPages = 0;
	ULONG FreedPages = 0;

	for (ULONG i=0; i<WorkingSet->TotalPageCount; i++)
	{
		if (WorkingSet->WsPages[i].LockedInWs == 0 &&
			WorkingSet->WsPages[i].Present == 1)
		{
			PMMPPD Ppd = WorkingSet->WsPages[i].PageDescriptor;
			PMMPTE PointerPte = Ppd->u2.PointerPte;

			if (PointerPte->u1.e1.Dirty || Ppd->Modified)
			{
				Ppd->Modified = 1;
				MiChangePageLocation (Ppd, ModifiedPageList);

				MiWriteTrimmedPte (PointerPte);
			}
			else
			{
				MiChangePageLocation (Ppd, StandbyPageList);

				if (ForceGetFreePages)
				{
					MiSwapPageFromPhysicalMemory (Ppd, PointerPte);
				}
				else
				{
					MiWriteTrimmedPte (PointerPte);
				}
			}

			TrimmedPages ++;
		}
	}

	return ForceGetFreePages ? FreedPages : TrimmedPages;
}

#endif


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

inline 
VOID
_MiGetHeaders(
	PCHAR ibase, 
	PIMAGE_FILE_HEADER *ppfh, 
	PIMAGE_OPTIONAL_HEADER *ppoh, 
	PIMAGE_SECTION_HEADER *ppsh
	)
/*++
	Get pointers to PE headers
--*/
{
	PIMAGE_DOS_HEADER mzhead = (PIMAGE_DOS_HEADER)ibase;
	PIMAGE_FILE_HEADER pfh;
	PIMAGE_OPTIONAL_HEADER poh;
	PIMAGE_SECTION_HEADER psh;

	pfh = (PIMAGE_FILE_HEADER)&ibase[mzhead->e_lfanew];
	pfh = (PIMAGE_FILE_HEADER)((PBYTE)pfh + sizeof(IMAGE_NT_SIGNATURE));
	poh = (PIMAGE_OPTIONAL_HEADER)((PBYTE)pfh + sizeof(IMAGE_FILE_HEADER));
	psh = (PIMAGE_SECTION_HEADER)((PBYTE)poh + sizeof(IMAGE_OPTIONAL_HEADER));

	if (ppfh) *ppfh = pfh;
	if (ppoh) *ppoh = poh;
	if (ppsh) *ppsh = psh;
}

#define MiGetHeaders(x,y,z,q) _MiGetHeaders((PCHAR)(x),(y),(z),(q))

#if LDR_TRACE_MODULE_LOADING
#define LdrTrace(x) KiDebugPrint x
#define LdrPrint(x) KiDebugPrint x
#else
#define LdrTrace(x)
#define LdrPrint(x)
#endif

VOID
KEAPI
LdrAddModuleToLoadedList(
	IN PUNICODE_STRING ModuleName,
	IN PVOID ImageBase,
	IN ULONG ImageSize
	)
/*++	
	Add module to PsLoadedModuleList
--*/
{
	PLDR_MODULE Module = (PLDR_MODULE) ExAllocateHeap (FALSE, sizeof(LDR_MODULE));

	Module->Base = ImageBase;
	Module->Size = ImageSize;
	RtlDuplicateUnicodeString( ModuleName, &Module->ModuleName );

	InterlockedInsertTailList (&PsLoadedModuleList, &Module->ListEntry);
}

STATUS
KEAPI
LdrLookupModule(
	IN PUNICODE_STRING ModuleName,
	OUT PLDR_MODULE *pModule
	)
/*++
	Search module in PsLoadedModuleList.
	For each module its name is compared with the specified name. 
	On success, pointer to LDR_MODULE is returned.
--*/
{
	ExAcquireMutex (&PsLoadedModuleList.Lock);

	PLDR_MODULE Ret = NULL;
	PLDR_MODULE Module = CONTAINING_RECORD (PsLoadedModuleList.ListEntry.Flink, LDR_MODULE, ListEntry);
	STATUS Status = STATUS_NOT_FOUND;

	while (Module != CONTAINING_RECORD (&PsLoadedModuleList, LDR_MODULE, ListEntry))
	{
		if (!wcsicmp(ModuleName->Buffer, Module->ModuleName.Buffer))
		{
			Status = STATUS_SUCCESS;
			Ret = Module;
			break;
		}

		Module = CONTAINING_RECORD (Module->ListEntry.Flink, LDR_MODULE, ListEntry);
	}

	ExReleaseMutex (&PsLoadedModuleList.Lock);

	*pModule = Ret;
	return Status;
}


PVOID
KEAPI
LdrGetProcedureAddressByOrdinal(
	IN PVOID Base,
	IN USHORT Ordinal
	)
/*++
	Retrieves export symbol by ordinal
--*/
{
	PIMAGE_OPTIONAL_HEADER poh;
	PIMAGE_EXPORT_DIRECTORY pexd;
	PULONG AddressOfFunctions;
	
	// Get headers
	MiGetHeaders (Base, NULL, &poh, NULL);

	// Get export
	*(PBYTE*)&pexd = (PBYTE)Base + poh->DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].VirtualAddress;
	if( (PVOID)pexd == Base )
		return NULL;

	*(PBYTE*)&AddressOfFunctions = (PBYTE)Base + pexd->AddressOfFunctions;

	if( !pexd->NumberOfNames )
	{
		LdrPrint(("LDR: Assertion failed for pexd->NumberOfNames != 0\n"));
	}

	if (Ordinal - pexd->Base < pexd->NumberOfFunctions)
	{
		return (PVOID)((ULONG)Base + (ULONG)AddressOfFunctions[Ordinal - pexd->Base]);
	}

	return NULL;
}


PVOID
KEAPI
LdrGetProcedureAddressByName(
	IN PVOID Base,
	IN PCHAR FunctionName
	)
/*++
	Walks the export directory of the image and look for the specified function name.
	Entry point of this function will be returned on success.
	If FunctionName==NULL, return module entry point.
--*/
{
	PIMAGE_OPTIONAL_HEADER poh;
	PIMAGE_EXPORT_DIRECTORY pexd;
	PULONG AddressOfFunctions;
	PULONG AddressOfNames;
	PUSHORT AddressOfNameOrdinals;
	ULONG i;
	ULONG SizeOfExport;
	
	// Get headers
	MiGetHeaders (Base, NULL, &poh, NULL);

	if (FunctionName == NULL)
	{
		return (PVOID)((ULONG)Base + poh->AddressOfEntryPoint);
	}

	// Get export
	*(PBYTE*)&pexd = (PBYTE)Base + poh->DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].VirtualAddress;
	SizeOfExport = poh->DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].Size;
	if( (PVOID)pexd == Base )
		return NULL;

	*(PBYTE*)&AddressOfFunctions = (PBYTE)Base + pexd->AddressOfFunctions;
	*(PBYTE*)&AddressOfNames = (PBYTE)Base + pexd->AddressOfNames;
	*(PBYTE*)&AddressOfNameOrdinals = (PBYTE)Base + pexd->AddressOfNameOrdinals;

	if( !pexd->NumberOfNames )
	{
		LdrPrint(("LDR: Assertion failed for pexd->NumberOfNames != 0\n"));
	}

	// Find function
	for( i=0; i<pexd->NumberOfNames; i++ ) 
	{
		PCHAR name = ((char*)Base + AddressOfNames[i]);
		PVOID addr = (PVOID*)((DWORD)Base + AddressOfFunctions[AddressOfNameOrdinals[i]]);

		if( !strcmp( name, FunctionName ) )
		{
			//
			// Check for export forwarding.
			//

			if( ((ULONG)addr >= (ULONG)pexd) && 
				((ULONG)addr < (ULONG)pexd + SizeOfExport) )
			{
				LdrPrint(("LDR: GETPROC: Export forwarding found [%s to %s]\n", FunctionName, addr));

				char* tname = (char*)ExAllocateHeap(TRUE, strlen((char*)addr)+5);
				if (!tname)
				{
					LdrPrint(("LDR: GETPROC: Not enough resources\n"));
					return NULL;
				}
				memcpy( tname, (void*)addr, strlen((char*)addr)+1 );

				char* dot = strchr(tname, '.');
				if( !dot ) {
					LdrPrint(("LDR: GETPROC: Bad export forwarding for %s\n", addr));
					ExFreeHeap(tname);
					return NULL;
				}

				*dot = 0;
				dot++;      // dot    ->    func name
				            // tname  ->    mod  name

				char ModName[100];
				strcpy(ModName, tname);
				if( stricmp(tname, "kernel") )
					strcat(ModName, ".sys");

				PLDR_MODULE Module;
				UNICODE_STRING ModuleName;
				wchar_t wmod[200];
				STATUS Status;

				mbstowcs (wmod, ModName, -1);
				RtlInitUnicodeString( &ModuleName, wmod );

				Status = LdrLookupModule (&ModuleName, &Module);

				if( !SUCCESS(Status) ) 
				{
					LdrPrint(("LDR: GETPROC: Bad module in export forwarding: %s\n", tname));
					ExFreeHeap(tname);
					return NULL;
				}

				void* func = LdrGetProcedureAddressByName(Module->Base, dot);
				if( !func ) {
					LdrPrint(("LDR: GETPROC: Bad symbol in export forwarding: %s\n", dot));
					ExFreeHeap(tname);
					return NULL;
				}

				ExFreeHeap(tname);

				LdrPrint(("LDR: GETPROC: Export forwarding %s resolved to %08x\n", addr, func));
				return func;
			}
			else
			{
				return addr;
			}
		}
	}
	
	return NULL;
}

STATUS
KEAPI
LdrWalkImportDescriptor(
	IN PVOID ImageBase,
	IN PIMAGE_OPTIONAL_HEADER OptHeader
	)
/*++
	Walk image's import descriptor.
	For each 
--*/
{
	PIMAGE_IMPORT_DESCRIPTOR ImportDescriptor = (PIMAGE_IMPORT_DESCRIPTOR) RVATOVA(OptHeader->DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress, ImageBase);

	for ( ; ImportDescriptor->Name; ImportDescriptor++ )
	{
		PLDR_MODULE Module;
		STATUS Status;
		UNICODE_STRING Unicode;
		WCHAR wbuff[200];

		char* ModName = (char*) RVATOVA(ImportDescriptor->Name, ImageBase);

		mbstowcs (wbuff, ModName, -1);
		RtlInitUnicodeString( &Unicode, wbuff );

		LdrPrint(("LdrWalkImportDescriptor: searching module %S\n", wbuff));

		//
		// Find module
		//

		Status = LdrLookupModule (
			&Unicode,
			&Module
			);

		if (!SUCCESS(Status))
		{
			LdrPrint(("LdrWalkImportDescriptor: %S not found (St=%08x)\n", wbuff, Status));
			return Status;
		}

		// Bound import?
		if (ImportDescriptor->TimeDateStamp == -1 )
		{
			LdrPrint(("LdrWalkImportDescriptor: %S: bound import not supported\n", wbuff));
			return STATUS_NOT_SUPPORTED;
		}

		//
		// Process imports
		//
		for (
			PIMAGE_THUNK_DATA Thunk = (PIMAGE_THUNK_DATA) RVATOVA(ImportDescriptor->FirstThunk,ImageBase);
			Thunk->u1.Ordinal; 
			Thunk ++ )
		{
			if (IMAGE_SNAP_BY_ORDINAL(Thunk->u1.Ordinal))
			{
				Thunk->u1.Function = (ULONG) LdrGetProcedureAddressByOrdinal (Module->Base, IMAGE_ORDINAL(Thunk->u1.Ordinal));

				if (!Thunk->u1.Function)
				{
					LdrPrint(("Can't resolve inmport by ordinal %d from %S: not found\n", IMAGE_ORDINAL(Thunk->u1.Ordinal), wbuff));
					return STATUS_INVALID_FILE_FOR_IMAGE;
				}

				LdrTrace (("LDR: [Loading %S]: Resolved import by ordinal %d\n", wbuff, IMAGE_ORDINAL(Thunk->u1.Ordinal)));
			}
			else
			{
				PIMAGE_IMPORT_BY_NAME Name = (PIMAGE_IMPORT_BY_NAME) RVATOVA(Thunk->u1.AddressOfData, ImageBase);

				Thunk->u1.Function = (ULONG) LdrGetProcedureAddressByName (Module->Base, (char*) Name->Name);

				if (!Thunk->u1.Function)
				{
					LdrPrint(("Can't resolve inmport by name %s from %S: not found\n", Name->Name, wbuff));
					return STATUS_INVALID_FILE_FOR_IMAGE;
				}

				LdrTrace (("LDR: [Loading %S]: Resolved import by name '%s' -> %08x\n", wbuff, Name->Name, Thunk->u1.Function));
			}
		}
	}

	return STATUS_SUCCESS;
}


STATUS
KEAPI
LdrRelocateImage(
	IN PVOID ImageBase,
	IN PIMAGE_OPTIONAL_HEADER OptHeader
	)
/*++
	Apply relocations to the specified image(ImageBase) from the address OptHeader->ImageBase to ImageBase.
--*/
{
	ULONG Delta = (ULONG)ImageBase - OptHeader->ImageBase;
	PIMAGE_BASE_RELOCATION Relocation = IMAGE_FIRST_RELOCATION(ImageBase);

	LdrPrint(("LDR: Fixing image %08x, delta %08x\n", ImageBase, Delta));

	if( (PVOID)Relocation == ImageBase )
	{
		LdrPrint(("LDR: No relocation table present\n"));
		return FALSE;
	}

	BOOLEAN bFirstChunk = TRUE;

	while ( ((ULONG)Relocation - (ULONG)IMAGE_FIRST_RELOCATION(ImageBase)) < OptHeader->DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC].Size )
	{
		bFirstChunk = FALSE;

		PIMAGE_FIXUP_ENTRY pfe = (PIMAGE_FIXUP_ENTRY)((DWORD)Relocation + sizeof(IMAGE_BASE_RELOCATION));

		LdrPrint(("LDR: Relocs size %08x, diff=%08x\n", 
			OptHeader->DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC].Size,
			((ULONG)Relocation - (ULONG)IMAGE_FIRST_RELOCATION(ImageBase))
			));

		LdrPrint(("LDR: Processing relocation block %08x [va=%08x, size %08x]\n", ((ULONG)Relocation-(ULONG)ImageBase),
			Relocation->VirtualAddress, Relocation->SizeOfBlock));

		if (Relocation->SizeOfBlock > 0x10000)
			INT3

		for ( ULONG i = 0; i < (Relocation->SizeOfBlock - sizeof(IMAGE_BASE_RELOCATION))/2; i++, pfe++) 
		{
			if (pfe->Type == IMAGE_REL_BASED_HIGHLOW) 
			{
				ULONG dwPointerRva = Relocation->VirtualAddress + pfe->Offset;
				ULONG* Patch = (ULONG*)RVATOVA(dwPointerRva,ImageBase);

				if (*Patch >= OptHeader->ImageBase &&
					*Patch <= OptHeader->ImageBase + OptHeader->SizeOfImage)
				{
					*Patch += Delta;
				}
				else
				{
					LdrPrint(("LDR: Warn: Invalid relocation at offset %08x: %08x -> %08x\n", 
						pfe->Offset,
						*Patch,
						*Patch + Delta
						));
				}
			}
		}
		(*((ULONG*)&Relocation)) += Relocation->SizeOfBlock;
	}

	return TRUE;
}


MMSYSTEM_MODE MmSystemMode = NormalMode;

LOCKED_LIST MmExtenderList;

STATUS
KEAPI
MiCreateExtenderObject(
	IN PVOID ExtenderStart,
	IN PVOID ExtenderEnd,
	IN PVOID ExtenderEntry,
	IN PUNICODE_STRING ExtenderName,
	OUT PEXTENDER *ExtenderObject
	)
/*++
	Create EXTENDER object representing the extender being loaded into the system.
--*/
{
	STATUS Status;
	PEXTENDER Extender;

	Status = ObCreateObject (
		(PVOID*) &Extender,
		sizeof(EXTENDER),
		MmExtenderObjectType,
		ExtenderName,
		OB_OBJECT_OWNER_MM
		);

	if (SUCCESS(Status))
	{
		Extender->ExtenderStart = ExtenderStart;
		Extender->ExtenderEnd = ExtenderEnd;
		Extender->ExtenderEntry = (PEXTENDER_ENTRY) ExtenderEntry;

		Status = (Extender->ExtenderEntry) (Extender);

		if (!SUCCESS(Status))
		{
			ObpDeleteObject (Extender);
		}
		else
		{
			//
			// Insert EXTENDER object to the list MmExtenderListHead
			// Insert each callback to the appropriate global list.
			//

			InterlockedInsertTailList( &MmExtenderList, &Extender->ExtenderListEntry );

			if (Extender->CreateThread)
			{
				InterlockedInsertHeadList (&PsCreateThreadCallbackList, &Extender->CreateThread->InternalListEntry);
			}

			if (Extender->TerminateThread)
			{
				InterlockedInsertHeadList (&PsTerminateThreadCallbackList, &Extender->TerminateThread->InternalListEntry);
			}

			if (Extender->CreateProcess)
			{
				InterlockedInsertHeadList (&PsCreateProcessCallbackList, &Extender->CreateProcess->InternalListEntry);
			}

			if (Extender->TerminateProcess)
			{
				InterlockedInsertHeadList (&PsTerminateProcessCallbackList, &Extender->TerminateProcess->InternalListEntry);
			}

			if (Extender->BugcheckDispatcher)
			{
				InterlockedInsertHeadList (&KeBugcheckDispatcherCallbackList, &Extender->BugcheckDispatcher->InternalListEntry);
			}
		}
	}

	return Status;
}


KESYSAPI
STATUS
KEAPI
MmLoadSystemImage(
	IN PUNICODE_STRING ImagePath,
	IN PUNICODE_STRING ModuleName,
	IN PROCESSOR_MODE TargetMode,
	IN BOOLEAN Extender,
	OUT PVOID *ImageBase,
	OUT PVOID *ModuleObject
	)
/*++
	Attempt to load system image into the memory.

	if TargetMode == DriverMode, we're loading a non-critical driver
	if TargetMode == KernelMode && Extender==0 we're loading critical driver
	if TargetMode == KernelMode && Extender==1 we're loading an extender
--*/
{
	PFILE FileObject = 0;
	STATUS Status = STATUS_UNSUCCESSFUL;
	PIMAGE_FILE_HEADER FileHeader = 0;
	PIMAGE_OPTIONAL_HEADER OptHeader = 0;
	PIMAGE_SECTION_HEADER SectHeader = 0;
	PMMD Mmd = 0;
	IO_STATUS_BLOCK IoStatus = {0};
	PVOID Hdr = 0;
	ULONG AlignedImageSize = 0;
	PVOID Image = 0;

	ASSERT (TargetMode != UserMode);

#define RETURN(st)  { Status = (st); __leave; }

	if (Extender && MmSystemMode != UpdateMode)
	{
		return STATUS_PRIVILEGE_NOT_HELD;
	}

	__try
	{
		//
		// Open the file
		//

		LdrPrint (("LDR: Opening file\n"));

		Status = IoCreateFile(
			&FileObject,
			FILE_READ_DATA,
			ImagePath,
			&IoStatus,
			FILE_OPEN_EXISTING, 
			0
			);
		if (!SUCCESS(Status))
			__leave;

		//
		// Allocate heap space for the header page
		//

		LdrPrint (("LDR: Allocating page\n"));

		Hdr = ExAllocateHeap (TRUE, PAGE_SIZE);
		if (!Hdr)
			RETURN (STATUS_INSUFFICIENT_RESOURCES);

		//
		// Read header page
		//

		LdrPrint (("LDR: Reading page\n"));

		Status = IoReadFile (
			FileObject,
			Hdr,
			PAGE_SIZE,
			NULL,
			&IoStatus
			);
		if (!SUCCESS(Status))
		{
			LdrPrint(("LDR: FAILED %d\n", __LINE__));
			__leave;
		}

		MiGetHeaders (Hdr, &FileHeader, &OptHeader, &SectHeader);
		AlignedImageSize = ALIGN_UP (OptHeader->SizeOfImage, PAGE_SIZE);

		//
		// Allocate physical pages enough to hold the whole image
		//

		LdrPrint (("LDR: Allocating phys pages\n"));

		Status = MmAllocatePhysicalPages (AlignedImageSize>>PAGE_SHIFT, &Mmd);
		if (!SUCCESS(Status))
		{
			LdrPrint(("LDR: FAILED %d\n", __LINE__));
			__leave;
		}

		//
		// Map physical pages
		//

		LdrPrint (("LDR: Mapping pages\n"));

		// BUGBUG: Last parameter should be TRUE (add to working set)
		Image = MmMapLockedPages (Mmd, TargetMode, TRUE, FALSE);
		if (Image == NULL)
			RETURN ( STATUS_INSUFFICIENT_RESOURCES );

		// Copy the first page
		memcpy (Image, Hdr, PAGE_SIZE);

		ExFreeHeap (Hdr);
		Hdr = NULL;
		MiGetHeaders (Image, &FileHeader, &OptHeader, &SectHeader);

		//
		// Go read sections from the file
		//

		LdrPrint (("LDR: Reading sections\n"));

		for (ULONG Section = 0; Section < FileHeader->NumberOfSections; Section++)
		{
			ULONG VaStart = SectHeader[Section].VirtualAddress;
			ULONG Size;

			if (Section == FileHeader->NumberOfSections-1)
			{
				Size = OptHeader->SizeOfImage - SectHeader[Section].VirtualAddress;
			}
			else
			{
				Size = SectHeader[Section+1].VirtualAddress - SectHeader[Section].VirtualAddress;
			}

			if (SectHeader[Section].Misc.VirtualSize > Size) {
				Size = SectHeader[Section].Misc.VirtualSize;
			}

#if LDR_TRACE_MODULE_LOADING
			char sname[9];
			strncpy (sname, (char*)SectHeader[Section].Name, 8);
			sname[8] = 0;

			LdrPrint (("LDR: Reading section %s: %08x, size %08x\n", sname, VaStart, Size));
#endif

			LARGE_INTEGER Offset = {0};
			Offset.LowPart = SectHeader[Section].PointerToRawData;
	
			PVOID Buffer = (PVOID) ((ULONG)Image + VaStart);

			Status = IoReadFile (
				FileObject,
				Buffer,
				Size,
				&Offset,
				&IoStatus
				);

			if (!SUCCESS(Status))
			{
				LdrPrint(("LDR: FAILED %d\n", __LINE__));
				__leave;
			}
		}

		//
		// Fixups
		//

		LdrPrint (("LDR: Relocating image\n"));

		Status = LdrRelocateImage (Image, OptHeader);
		if (!SUCCESS(Status))
		{
			LdrPrint(("LDR: FAILED %d\n", __LINE__));
			__leave;
		}

		//
		// Process imports
		//

		LdrPrint (("LDR: Resolving imports\n"));

		Status = LdrWalkImportDescriptor (Image, OptHeader);
		if (!SUCCESS(Status))
		{
			LdrPrint(("LDR: FAILED %d\n", __LINE__));
			__leave;
		}

		if (Extender) {
			LdrPrint (("LDR: Creating extender object\n"));

			KeBugCheck (MEMORY_MANAGEMENT, __LINE__, STATUS_NOT_IMPLEMENTED, 0, 0);
		}
		else
		{
			PVOID DriverEntry = LdrGetProcedureAddressByName (Image, NULL);

			LdrPrint (("LDR: Creating driver object [DRVENTRY=%08x]\n", DriverEntry));

			Status = IopCreateDriverObject (
				Image,
				(PVOID)((ULONG)Image + OptHeader->SizeOfImage - 1),
				(TargetMode == KernelMode ? DRV_FLAGS_CRITICAL : 0),
				(PDRIVER_ENTRY) DriverEntry,
				ModuleName,
				(PDRIVER*)ModuleObject
				);
		}

		LdrPrint (("LDR: Success\n"));
	}
	__finally
	{
		if (!SUCCESS(Status))
		{
			if (Mmd)
				MmFreeMmd (Mmd);
		}
		else
		{
			*ImageBase = Image;
		}

		if (Hdr)
			ExFreeHeap (Hdr);

		if (FileObject)
			IoCloseFile (FileObject);
	}

	return Status;
}