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
	ULONG PhysicalAddress,
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

PVOID
KEAPI
MmMapPhysicalPages(
	ULONG PhysicalAddress,
	ULONG PageCount
	)
/*++
	Map physical pages to an arbitrary virtual address.
	Returns the virtual address of the mapped pages
--*/
{
	ExAcquireMutex (&MmPageDatabaseLock);

	ULONG StartVirtual = MM_CRITICAL_AREA;
	PMMPTE PointerPte = MiGetPteAddress (StartVirtual);

	for (ULONG i=0; i<MM_CRITICAL_AREA_PAGES; i++)
	{
		if ( *(ULONG*)PointerPte == 0 )
		{
			BOOLEAN Free = 1;

			for ( ULONG j=i+1; j<i+PageCount; j++ )
			{
				Free &= ( *(ULONG*)&PointerPte[j] == 0 );
			}

			if (Free)
			{
				ULONG VirtualAddress = StartVirtual;

				for( ULONG j=i; j<i+PageCount; j++ )
				{
					MiWriteValidKernelPte (PointerPte);
					PointerPte->u1.e1.PageFrameNumber = (PhysicalAddress >> PAGE_SHIFT) + (j-i);

					MmInvalidateTlb ((PVOID)VirtualAddress);

					PointerPte = MiNextPte (PointerPte);
					VirtualAddress += PAGE_SIZE;
				}

				ExReleaseMutex (&MmPageDatabaseLock);
				return (PVOID)StartVirtual;
			}

			i += PageCount - 1;
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
	VirtualAddress = (PVOID) ALIGN_DOWN ((ULONG)VirtualAddress, PAGE_SIZE);
	PMMPTE PointerPte = MiGetPteAddress (VirtualAddress);

	//KeZeroMemory( PointerPte, PageCount*sizeof(MMPTE) );
	for (ULONG i=0; i<PageCount; i++)
	{
//		KdPrint(("MM: Unmapping %08x [->%08x] pte %08x\n", (ULONG)VirtualAddress + (i<<PAGE_SHIFT), (PointerPte->u1.e1.PageFrameNumber)<<PAGE_SHIFT, PointerPte));
		
		PointerPte->RawValue = 0;

		MmInvalidateTlb ((PVOID)((ULONG)VirtualAddress + (i<<PAGE_SHIFT)));

		PointerPte = MiNextPte (PointerPte);
	}
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
				0,
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


VOID
KEAPI
MiZeroPageThread(
	)
{
	for(;;)
	{
	}
}

LIST_ENTRY PsLoadedModuleList;
MUTEX PsLoadedModuleListLock;

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

VOID
KEAPI
MmInitSystem(
	)
{
	ExInitializeMutex (&MmPageDatabaseLock);
	ExInitializeMutex (&MmPpdLock);

	MiMapPhysicalPages (MM_PPD_START, MM_PPD_START_PHYS, (KiLoaderBlock.PhysicalMemoryPages * sizeof(MMPPD)) / PAGE_SIZE);

	//
	// Initialize our module
	//

	InitializeListHead (&PsLoadedModuleList);
	ExInitializeMutex (&PsLoadedModuleListLock);

	MiKernelModule.Base = (PVOID) MM_KERNEL_START;
	RtlInitUnicodeString (&MiKernelModule.ModuleName, L"kernel.exe");

	InterlockedOp (&PsLoadedModuleListLock, InsertTailList (&PsLoadedModuleList, &MiKernelModule.ListEntry));

	PIMAGE_DOS_HEADER KrnlDosHdr = (PIMAGE_DOS_HEADER) MM_KERNEL_START;
	PIMAGE_NT_HEADERS KrnlNtHdrs = (PIMAGE_NT_HEADERS) (MM_KERNEL_START + KrnlDosHdr->e_lfanew);

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

	MiSystemWorkingSet = (PMMWORKING_SET) ExAllocateHeap (FALSE, sizeof(MMWORKING_SET) + (InitialPageCount - 1)*sizeof(MMWORKING_SET_ENTRY));

	ExInitializeMutex (&MiSystemWorkingSet->Lock);
	MiSystemWorkingSet->TotalPageCount = InitialPageCount;
	MiSystemWorkingSet->LockedPageCount = InitialPageCount;
	MiSystemWorkingSet->OwnerMode = KernelMode;
	MiSystemWorkingSet->Owner = &InitialSystemProcess;

	InitialSystemProcess.WorkingSet = MiSystemWorkingSet;
	
	ULONG i;

	for (i=0; i<InitialPageCount; i++)
	{
		Ppd[i].PageLocation = ActiveAndValid;
		Ppd[i].BelongsToSystemWorkingSet = TRUE;
		Ppd[i].ProcessorMode = KernelMode;

		MiSystemWorkingSet->WsPages[i].LockedInWs = TRUE;
		MiSystemWorkingSet->WsPages[i].PageDescriptor = &Ppd[i];

		Ppd[i].u1.WsIndex = i;
		Ppd[i].ShareCount = 1;
	}

	//
	// All other pages describe as Zeroed pages.
	//

	MmZeroedPageListHead = &Ppd[i];

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

		MiChangePageLocation (Ppd, ActiveAndValid);
		
#if MM_TRACE_MMDS
		KdPrint(("MM: Allocated page %08x\n", AllocatedPages[alloc]));
#endif

		//
		// MMPPD in unlinked from double-linked list, but it contains
		//  valid pointers to previous and next entries
		//

		Ppd = Ppd->u1.NextFlink;	// This link is valid despite the PPD is already unlinked.
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

			MiChangePageLocation (Ppd, ActiveAndValid);
			
#if MM_TRACE_MMDS
			KdPrint(("MM: Allocated page %08x\n", AllocatedPages[alloc]));
#endif
			//
			// MMPPD in unlinked from double-linked list, but it contains
			//  valid pointers to previous and next entries
			//

			Ppd = Ppd->u1.NextFlink;	// This link is valid despite the PPD is already unlinked.
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

		WorkingSet->WsPages[WsIndex].LockedInWs = FALSE;
	}

	ExReleaseMutex (&WorkingSet->Lock);

	Mmd->Flags &= ~MDL_LOCKED;
}


KESYSAPI
PVOID
KEAPI
MmMapLockedPages(
	IN PMMD Mmd,
	IN PROCESSOR_MODE TargetMode
	)
/*++
	Map locked pages to target address space
--*/
{
	//
	// Don't support other modes now..
	//

	ASSERT (TargetMode == KernelMode);
	

	ExAcquireMutex (&MmPageDatabaseLock);

	ULONG StartVirtual = MM_CRITICAL_AREA;
	PMMPTE PointerPte = MiGetPteAddress (StartVirtual);

#if MM_TRACE_MMDS
	KdPrint(("MM: Maping locked pages.\n"));
#endif

	for (ULONG i=0; i<MM_CRITICAL_AREA_PAGES; i++)
	{
		if ( *(ULONG*)PointerPte == 0 )
		{
			BOOLEAN Free = 1;

			for ( ULONG j=i+1; j<i+Mmd->PageCount; j++ )
			{
				Free &= ( *(ULONG*)&PointerPte[j] == 0 );
			}

			if (Free)
			{
				ULONG VirtualAddress = StartVirtual;

				for( ULONG j=i; j<i+Mmd->PageCount; j++ )
				{
					MiWriteValidKernelPte (PointerPte);
					PointerPte->u1.e1.PageFrameNumber = Mmd->PfnList[(j-i)];

#if MM_TRACE_MMDS
					KdPrint(("MM: Mapping PFN %08x to VA %08x\n", Mmd->PfnList[j-i], VirtualAddress));
#endif
					MmPfnPpd(Mmd->PfnList[j-i])->ShareCount ++;

					MmInvalidateTlb ((PVOID)VirtualAddress);

					PointerPte = MiNextPte (PointerPte);
					VirtualAddress += PAGE_SIZE;
				}

				ExReleaseMutex (&MmPageDatabaseLock);

				Mmd->Flags |= MDL_MAPPED;
				Mmd->MappedVirtual = (PVOID) StartVirtual;

				return (PVOID)( StartVirtual + Mmd->Offset );
			}

			i += Mmd->PageCount - 1;
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
	MI_LOCK_PPD();

	MiUnmapPhysicalPages (Mmd->MappedVirtual, Mmd->PageCount);

	for (ULONG i=0; i<Mmd->PageCount; i++)
	{
		PMMPPD Ppd = MmPfnPpd (Mmd->PfnList[i]);

		Ppd->ShareCount --;
	}

	MI_UNLOCK_PPD();

	Mmd->MappedVirtual = NULL;
	Mmd->Flags &= ~MDL_MAPPED;
}
