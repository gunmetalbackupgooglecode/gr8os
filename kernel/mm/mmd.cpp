//
// FILE:		mmd.cpp
// CREATED:		17-Feb-2008  by Great
// PART:        MM
// ABSTRACT:
//			Memory Management routines. MMD support
//

#include "common.h"


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


PVOID
KEAPI
MmAllocateMemory(
	ULONG MemorySize
	)
/*++
	Internal routine for allocating some memory space for kernel purposes
--*/
{
	PMMD Mmd;
	STATUS Status;
	PVOID Ptr;

	MemorySize = ALIGN_UP (MemorySize, PAGE_SIZE);

	Status = MmAllocatePhysicalPages (MemorySize >> PAGE_SHIFT, &Mmd);
	if (!SUCCESS(Status))
		return 0;

	Ptr = MmMapLockedPages (Mmd, KernelMode, FALSE, FALSE);

	ASSERT (Ptr != NULL);

	ExFreeHeap (Mmd);
	return Ptr;
}


VOID
KEAPI
MmFreeMemory(
	PVOID Ptr,
	ULONG MemorySize
	)
/*++
	Internal routine used to free memory allocated by MmAllocateMemory
--*/
{
	PMMD Mmd = MmAllocateMmd (Ptr, MemorySize);

	// BUGBUG: NOT TESTED

	ASSERT (Mmd != NULL);

	MmBuildMmdForNonpagedSpace (Mmd);
	Mmd->Flags = MDL_ALLOCATED | MDL_MAPPED;

  /*
  // this fill be performed by MmFreeMmd()
	MmUnmapLockedPages (Mmd);
	MmFreePhysicalPages (Mmd);
  */
	MmFreeMmd (Mmd);
}


PVOID
KEAPI
MmAllocatePage(
	)
/*++
	Internal routine used to allocate one page
--*/
{
	/*PMMD Mmd;
	STATUS Status;
	PVOID Ptr;

	//BUGBUG: Not tested

	Status = MmAllocatePhysicalPages (1, &Mmd);
	if (!SUCCESS(Status))
		return 0;

	Ptr = MmMapLockedPages (Mmd, KernelMode, FALSE, FALSE);

	ExFreeHeap (Mmd);
	return Ptr;*/

	return MmAllocateMemory (PAGE_SIZE);
}

VOID
KEAPI
MmFreePage(
	PVOID Page
	)
/*++
	Internal routine used to free one page
--*/
{
	/*
	MMD Mmd;
	PMMPTE Pte;

	//BUGBUG: Not tested

	Pte = MiGetPteAddress (Page);

	ASSERT (MmIsAddressValidEx (Page) == PTE_TYPE_NORMAL_OR_NOTMAPPED);

	Mmd.MappedVirtual = Mmd.BaseVirtual = Page;
	Mmd.PageCount = 1;
	Mmd.PfnList[0] = Pte->u1.e1.PageFrameNumber;
	Mmd.Flags = MDL_ALLOCATED|MDL_MAPPED;
	Mmd.Offset = 0;
	
	MmUnmapLockedPages (&Mmd);
	MmFreePhysicalPages (&Mmd);
	*/

	MmFreeMemory (Page, PAGE_SIZE);
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
						Ppd->ProcessorMode = TargetMode;
						
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

							if (TargetMode == KernelMode)
								Ppd->BelongsToSystemWorkingSet = 1;
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


