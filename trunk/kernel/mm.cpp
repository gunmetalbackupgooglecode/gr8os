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
		FALSE,
		KernelMode
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
	BOOLEAN AddToWorkingSet,
	PROCESSOR_MODE TargetMode
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

			if (Ppd->BelongsToSystemWorkingSet)
				Ppd->BelongsToSystemWorkingSet = FALSE;
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

union PF_ERROR_CODE
{
	struct
	{
		ULONG ProtectionFault : 1;
		ULONG WriteFault : 1;
		ULONG User : 1;
		ULONG Reserved : 29;
	} e1;
	ULONG Raw;
};

VOID
KEAPI
MmAccessFault(
	IN PVOID VirtualAddress,
	IN PVOID FaultingAddress,
	IN ULONG ErrorCode
	)
/*++
	Resolve access fault
--*/
{
	PF_ERROR_CODE Err;
	Err.Raw = ErrorCode;

	PMMPTE PointerPte = MiGetPteAddress (VirtualAddress);

	KdPrint(( "MM: Page fault at %08x, referring code: %08x [ErrCode=%x] P=%d,W=%d,O=%d\n", 
		VirtualAddress, FaultingAddress, ErrorCode,
		PointerPte->u1.e1.Valid,
		PointerPte->u1.e1.Write,
		PointerPte->u1.e1.Owner));

	if (Err.e1.WriteFault)
	{
		//
		// #PF occurred because of protection violation
		//

		if (PointerPte->u1.e1.Valid == 1)
		{
			goto bugcheck;
		}
	}

	switch (PointerPte->u1.e1.PteType)
	{
	case PTE_TYPE_NORMAL_OR_NOTMAPPED:
		{
			sprintf( MmDebugBuffer, "MM: Access violation in %08x [invalid page]\n", VirtualAddress );
			KiDebugPrintRaw( MmDebugBuffer );

			break;
		}

	case PTE_TYPE_PAGEDOUT:
		{
			sprintf( MmDebugBuffer, "MM: Access violation in %08x [paging not supported]\n", VirtualAddress );
			KiDebugPrintRaw( MmDebugBuffer );

			break;
		}

	case PTE_TYPE_TRIMMED:
		{
			sprintf( MmDebugBuffer, "MM: Access violation in %08x [trimming detected : PFN=%05x]\n", 
				VirtualAddress, PointerPte->u1.e4.PageFrameNumber );
			KiDebugPrintRaw( MmDebugBuffer );

			PMMPPD Ppd = MmPfnPpd (PointerPte->u1.e1.PageFrameNumber);

			MI_LOCK_PPD();
			ExAcquireMutex (&MmPageDatabaseLock);

			PointerPte->u1.e1.Write = (PointerPte->u1.e4.Protection == MM_READWRITE || PointerPte->u1.e4.Protection == MM_EXECUTE_READWRITE);
			PointerPte->u1.e1.Valid = 1;

			MiChangePageLocation (Ppd, ActiveAndValid);

			ExReleaseMutex (&MmPageDatabaseLock);
			MI_UNLOCK_PPD();

			MmInvalidateTlb (VirtualAddress);

			KdPrint(("MM: #PF: Resolved for trimming\n"));

			return;
		}

	case PTE_TYPE_VIEW:
		{
			sprintf( MmDebugBuffer, "MM: Access violation in %08x [view detected : FileDescriptor=%04x]\n", 
				VirtualAddress, PointerPte->u1.e3.FileDescriptorNumber );

			KiDebugPrintRaw( MmDebugBuffer );

			//
			// Find the appropriate MAPPED_FILE
			//

			PMAPPED_FILE Mapping;
			STATUS Status;

			Status = ObpMapHandleToPointer ( (HANDLE)PointerPte->u1.e3.FileDescriptorNumber, -1, (PVOID*)&Mapping, FALSE);
			if (!SUCCESS(Status))
			{
				KdPrint(("MM: #PF: Invalid view [ObpMapHandleToPointer returned status %08x for mapping handle]\n", Status));
				
				goto bugcheck;
			}

			//
			// Find the appropriate MAPPED_VIEW
			//

			ExAcquireMutex (&Mapping->ViewList.Lock);

			PMAPPED_VIEW View = CONTAINING_RECORD (Mapping->ViewList.ListEntry.Flink, MAPPED_VIEW, ViewListEntry);

			while ( View != CONTAINING_RECORD (&Mapping->ViewList.ListEntry, MAPPED_VIEW, ViewListEntry) )
			{
				if ( VirtualAddress >= View->StartVa &&
					 (ULONG)VirtualAddress < ((ULONG)View->StartVa + View->ViewSize) )
				{
					//
					// This is our view.
					//

					KdPrint(("MM: #PF: Found view for the pagefault: %08x-%08x\n",
						View->StartVa,
						(ULONG)View->StartVa + View->ViewSize - 1));

					PCCFILE_CACHE_MAP CacheMap = Mapping->FileObject->CacheMap;
					ASSERT (CacheMap != NULL);

					ULONG PageNumber = (ALIGN_DOWN((ULONG)VirtualAddress,PAGE_SIZE) - (ULONG)View->StartVa) / PAGE_SIZE;

					ExAcquireMutex (&CacheMap->CacheMapLock);

					for (ULONG i=0; i<CacheMap->MaxCachedPages; i++)
					{
						if (CacheMap->PageCacheMap[i].Cached &&
							CacheMap->PageCacheMap[i].PageNumber == PageNumber)
						{
							KdPrint(("MM: #PF: Found cached page for the view. Mapping it\n"));

							PMMPTE TempPte = MiGetPteAddress (CacheMap->PageCacheMap[i].Buffer);

							ASSERT (TempPte->u1.e1.Valid == 1);

							ExAcquireMutex (&MmPageDatabaseLock);

							PointerPte->u1.e1.Valid = 1;
							PointerPte->u1.e1.Write = (View->Protection == MM_READWRITE || View->Protection == MM_EXECUTE_READWRITE);
							
							// Notice: PointerPte->u1.e1.PteType  is still PTE_TYPE_VIEW !!

							PointerPte->u1.e1.Owner = (Mapping->TargetMode == KernelMode);
							PointerPte->u1.e1.PageFrameNumber = TempPte->u1.e1.PageFrameNumber;

							PointerPte->u1.e1.Dirty = 0;
							PointerPte->u1.e1.Accessed = 0;

							KdPrint(("MM: #PF: Write=%d, Owner=%d, PFN=%x, Protection=%d, Mode=%d\n", PointerPte->u1.e1.Write, PointerPte->u1.e1.Owner,
								PointerPte->u1.e1.PageFrameNumber, View->Protection, Mapping->TargetMode));

							MmInvalidateTlb (VirtualAddress);

							ExReleaseMutex (&MmPageDatabaseLock);

							ExReleaseMutex (&CacheMap->CacheMapLock);

							KdPrint(("MM: #PF: Resolved cached page for view\n"));

							ExReleaseMutex (&Mapping->ViewList.Lock);

							return;
						}
					}
					
					ExReleaseMutex (&CacheMap->CacheMapLock);

					KdPrint(("MM: #PF: Cached page not found, performing actual reading\n"));

					//
					// UNIMPLEMENTED
					//

					KdPrint(("UNIMPLEMENTED YET\n"));
					ASSERT (FALSE);

					ExReleaseMutex (&Mapping->ViewList.Lock);

					return;
				}

				View = CONTAINING_RECORD (View->ViewListEntry.Flink, MAPPED_VIEW, ViewListEntry);
			}

			ExReleaseMutex (&Mapping->ViewList.Lock);

			KdPrint(("MM: #PF: Can't find MAPPED_VIEW for the faulted view\n"));

			INT3
		}
	}

bugcheck:
	KeBugCheck (KERNEL_MODE_EXCEPTION_NOT_HANDLED,
				STATUS_ACCESS_VIOLATION,
				(ULONG)VirtualAddress,
				(ULONG)FaultingAddress,
				Err.e1.WriteFault
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

ULONG
KEAPI
MiIsAddressValidEx(
	IN PMMPTE PointerPte
	)
/*++
	This function checks that specified virtual address is valid and determines the page type
--*/
{
	if (PointerPte->u1.e1.Valid)
	{
		if (PointerPte->u1.e1.PteType == PTE_TYPE_VIEW)
			return PageStatusNormalView;

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

	return MiIsAddressValidEx (PointerPte);
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
	)
/*++
	See remarks to the PDELETE_OBJECT_ROUTINE typedef for the general explanations
	 of the type of such routines.

	This routine is called when the file mapping object is being deleted.
	We should dereference the corresponding file object
--*/
{
	PMAPPED_FILE FileMapping = OBJECT_HEADER_TO_OBJECT (Object, MAPPED_FILE);

	KdPrint(("MiDeleteMapping\n"));

	ExAcquireMutex (&FileMapping->ViewList.Lock);

	if (!IsListEmpty (&FileMapping->ViewList.ListEntry))
	{
		//
		// There are some unmapped views.
		//

		PMAPPED_VIEW View = CONTAINING_RECORD (FileMapping->ViewList.ListEntry.Flink, MAPPED_VIEW, ViewListEntry), NextView;

		while ( View != CONTAINING_RECORD (&FileMapping->ViewList.ListEntry, MAPPED_VIEW, ViewListEntry) )
		{
			KdPrint(("Unmapping not unmapped view: VA=%08x, Size=%08x\n", View->StartVa, View->ViewSize));

			NextView = CONTAINING_RECORD (View->ViewListEntry.Flink, MAPPED_VIEW, ViewListEntry);

			MiUnmapViewOfFile (View);

			View = NextView;
		}
	}

	ExReleaseMutex (&FileMapping->ViewList.Lock);

	if (!ObIsObjectGoingAway(FileMapping->FileObject))
	{
		InterlockedRemoveEntryList (&FileMapping->FileObject->MappingList, &FileMapping->FileMappingsEntry);
	}

	ObDereferenceObject (FileMapping->FileObject);
}

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

	//BUGBUG: Not tested

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

	MmUnmapLockedPages (Mmd);
	MmFreePhysicalPages (Mmd);
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


VOID
KEAPI
MmTrimPage(
	IN PVOID VirtualAddress
	)
/*++
	Write transition PTE
--*/
{
	MI_LOCK_PPD ();
	ExAcquireMutex (&MmPageDatabaseLock);

	PMMPTE Pte = MiGetPteAddress (VirtualAddress);

	ASSERT (Pte->u1.e1.Valid == 1);

	PMMPPD Ppd = MmPfnPpd (Pte->u1.e1.PageFrameNumber);

	ASSERT (Ppd->PageLocation == ActiveAndValid);

	if (Pte->u1.e1.Dirty == 1)
	{
		Ppd->Modified = 1;

		MiChangePageLocation (Ppd, ModifiedPageList);

		KdPrint(("MM: Page at VA %08x moved to modified page list\n", VirtualAddress));
	}
	else
	{
		MiChangePageLocation (Ppd, StandbyPageList);

		KdPrint(("MM: Page at VA %08x moved to standby page list\n", VirtualAddress));
	}

	MiWriteTrimmedPte (Pte);
	Pte->u1.e4.Protection = (Pte->u1.e1.Write ? MM_READWRITE : MM_READONLY);

	ExReleaseMutex (&MmPageDatabaseLock);
	MI_UNLOCK_PPD ();
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
			0,
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
				0,
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



////////------------------------------------//////////
////////            File mapping            //////////
////////------------------------------------//////////

UCHAR
KEAPI
MiFileAccessToPageProtection(
	ULONG GrantedAccess
	)
/*++
	Convert file access to MM page protection
--*/
{
	// If writing is allowed, return read/write protection.
	if (GrantedAccess & FILE_WRITE_DATA)
		return MM_READWRITE;

	// Writing is not allowed, but reading is allowed. Return readonly protection
	if (GrantedAccess & FILE_READ_DATA)
		return MM_READONLY;

	// Both reading and writing are disabled. Return MM_NOACCESS
	return MM_NOACCESS;
}

ULONG
KEAPI
MiPageProtectionToGrantedAccess(
	UCHAR Protection
	)
/*++
	Convert page protection to file access
--*/
{
	ASSERT (Protection >= MM_NOACCESS && Protection <= MM_GUARD);

	Protection &= MM_MAXIMUM_PROTECTION; // select first three bits.

	switch (Protection)
	{
	case MM_NOACCESS:		
	case MM_GUARD:
		return 0;

	case MM_READONLY:
	case MM_EXECUTE_READONLY:	
	case MM_WRITECOPY:
	case MM_EXECUTE_WRITECOPY:
		return FILE_READ_DATA;

	case MM_READWRITE:
	case MM_EXECUTE_READWRITE:	
		return FILE_READ_DATA|FILE_WRITE_DATA;
	}

	return 0; // prevent compiler warning.
}

KESYSAPI
STATUS
KEAPI
MmCreateFileMapping(
	IN PFILE FileObject,
	IN PROCESSOR_MODE MappingMode,
	IN UCHAR StrongestProtection,
	OUT PHANDLE MappingHandle
	)
/*++
	Create file mapping to the specified file object at target processor mode.
	To map particular part of file caller should call MmMapViewOfFile later.
	Strongest acceptable protection should be specified.
	Pointer to the MAPPED_FILE is returned.
--*/
{
	STATUS Status;

	if (FileObject->CacheMap == NULL)
	{
		//
		// File should be cached if the caller wants us to map it
		//

		return STATUS_INVALID_PARAMETER_2;
	}

	if ( MiFileAccessToPageProtection(FileObject->DesiredAccess) < StrongestProtection )
	{
		//
		// StrongestProtection is greater than granted file access.
		// Cannot map
		//

		return STATUS_ACCESS_DENIED;
	}

	//
	// Allocate space for the structure
	//

	PMAPPED_FILE MappedFile;
	
	Status = ObCreateObject (	(PVOID*)&MappedFile, 
								sizeof(MAPPED_FILE), 
								MmFileMappingObjectType,
								NULL,
								OB_OBJECT_OWNER_MM
							);

	if (!SUCCESS(Status))
		return Status;

//	MappedFile = (PMAPPED_FILE) ExAllocateHeap (TRUE, sizeof(MAPPED_FILE));
//	if (!MappedFile)
//	{
//		return STATUS_INSUFFICIENT_RESOURCES;
//	}

	// File cannot go away until all its views are unmapped.
	ObReferenceObject (FileObject);

	MappedFile->FileObject = FileObject;
	MappedFile->StrongestProtection = StrongestProtection;
	MappedFile->TargetMode = MappingMode;
	InitializeLockedList (&MappedFile->ViewList);

	//
	// Insert into locked list of file mappings for this file
	//

	InterlockedInsertTailList (&FileObject->MappingList, &MappedFile->FileMappingsEntry);

	HANDLE hMapping = ObpCreateHandle (MappedFile, MiPageProtectionToGrantedAccess (StrongestProtection));
	if (hMapping == INVALID_HANDLE_VALUE)
	{
		//ExFreeHeap (MappedFile);
		ObpDeleteObject (MappedFile);
		ObDereferenceObject (FileObject);

		return STATUS_INSUFFICIENT_RESOURCES;
	}

	//
	// Return
	//

	*MappingHandle = hMapping;

	return STATUS_SUCCESS;
}

PVOID MiStartAddressesForProcessorModes[MaximumMode] = {
	(PVOID) MM_CRITICAL_AREA,		// ring0
	(PVOID) MM_DRIVER_AREA,			// ring1
	(PVOID) NULL,					// unused
	(PVOID) MM_USERMODE_AREA		// ring3
};

PVOID MiEndAddressesForProcessorModes[MaximumMode] = {
	(PVOID) MM_CRITICAL_AREA_END,		// ring0
	(PVOID) MM_DRIVER_AREA_END,			// ring1
	(PVOID) NULL,						// unused
	(PVOID) MM_USERMODE_AREA_END		// ring3
};

STATUS
KEAPI
MiFindAndReserveVirtualAddressesForFileView(
	IN PVOID StartAddress OPTIONAL,
	IN PROCESSOR_MODE MappingMode,
	IN ULONG ViewPages,
	OUT PVOID *RealAddress
	)
/*++
	(Optionally) Find and reserve virtual address range for file view.
	Real address will be stored in *RealAddress on success.

	Environment: MmPageDatabaseLock held
--*/
{
	ASSERT (MiStartAddressesForProcessorModes[MappingMode] != NULL);
	ASSERT (MiEndAddressesForProcessorModes[MappingMode] != NULL);

	if (StartAddress)
	{
		//
		// Caller has already chosen some virtual address for the view, check if it does really meet the requirements.
		//

		if (StartAddress < MiStartAddressesForProcessorModes[MappingMode] ||
			StartAddress >= MiEndAddressesForProcessorModes[MappingMode])
		{
			return STATUS_INVALID_PARAMETER;
		}

		//
		// Check that this address range is really free
		//

		for ( PVOID va = StartAddress; (ULONG)va < (ULONG)StartAddress + ViewPages*PAGE_SIZE; *(ULONG*)&va += PAGE_SIZE )
		{
			if (MmIsAddressValidEx (va) != PageStatusFree)
				return STATUS_INVALID_PARAMETER;
		}

		*RealAddress = StartAddress;
		return STATUS_SUCCESS;
	}
	else
	{
		//
		// Find the appropriate address range
		//

		for ( PVOID va = MiStartAddressesForProcessorModes[MappingMode];
			  va < MiEndAddressesForProcessorModes[MappingMode];
			  *(ULONG*)&va += PAGE_SIZE )
		{
			if (MmIsAddressValidEx (va) == PageStatusFree)
			{
				bool free = true;
				ULONG i;

				//
				// Check that all pages are free
				//

				for (i=1; i<ViewPages; i++)
				{
					free &= (MmIsAddressValidEx ((PVOID)((ULONG)va + i*PAGE_SIZE)) == PageStatusFree);
				}

				if (free)
				{
					//
					// Found range of free pages.
					//

					PMMPTE PointerPte = MiGetPteAddress (va);
					for (i=0; i<ViewPages; i++)
					{
						MiZeroPte (PointerPte);

						PointerPte->u1.e3.PteType = PTE_TYPE_VIEW;	// file view
						PointerPte->u1.e3.FileDescriptorNumber = 0;	// not mapped yet
						PointerPte->u1.e3.Protection = 0;			// not protected yet
						
						PointerPte = MiNextPte (PointerPte);
					}

					// Return

					*RealAddress = va;
					return STATUS_SUCCESS;

				} // if(free)

			} // if (MmIsAddressValidEx(..) == PageStatusFree)

		} // for (..)

		return STATUS_INSUFFICIENT_RESOURCES;

	} // if (StartAddress == NULL)
}


VOID
KEAPI
MiInitializeViewPages (
	IN OUT PMAPPED_VIEW View
	)
/*++
	Initialize properly view pages
--*/
{
	PMMPTE PointerPte = View->StartPte;

	for (ULONG i=0; i < View->ViewSize/PAGE_SIZE; i++)
	{
		PointerPte->u1.e3.FileDescriptorNumber = (USHORT) View->hMapping;
		PointerPte->u1.e3.Protection = View->Protection;

		PointerPte = MiNextPte (PointerPte);
	}
}
	

KESYSAPI
STATUS
KEAPI
MmMapViewOfFile(
	IN HANDLE hMapping,
	IN ULONG OffsetStart,
	IN ULONG OffsetStartHigh,
	IN ULONG ViewSize,
	IN UCHAR Protection,
	IN OUT PVOID *VirtualAddress // on input optinally contains desired VA, on output - real mapped VA of view.
	)
/*++
	Map view of the specified file, which is already being mapped (MmCreateFileMapping should be called before this call)
	Offset and size of the view should be specified.
	Protection should be lower or equal strongest protection of the specified mapping.
	Virtual address is returned in *VirtualAddress
--*/
{
	PMAPPED_FILE Mapping;
	STATUS Status;
	
	Status = ObpMapHandleToPointer (hMapping, -1 /*any access*/, (PVOID*)&Mapping, FALSE);

	if (!SUCCESS(Status))
		return Status;

	if (ObIsObjectGoingAway(Mapping))
		return STATUS_DELETE_PENDING;

	if (Mapping->StrongestProtection < Protection)
	{
		return STATUS_ACCESS_DENIED;
	}

	PMMPTE StartPte;
	PVOID va;

	ExAcquireMutex (&MmPageDatabaseLock);

	ViewSize = ALIGN_UP (ViewSize, PAGE_SIZE);

	//
	// Find and reserve virtual address range for file view
	//

	Status = MiFindAndReserveVirtualAddressesForFileView (
		*VirtualAddress, 
		Mapping->TargetMode,
		ViewSize/PAGE_SIZE, 
		&va);

	if (!SUCCESS(Status))
	{
		ExReleaseMutex (&MmPageDatabaseLock);
		return Status;
	}

	StartPte = MiGetPteAddress (va);

	//
	// Allocate space for the structure
	//

	PMAPPED_VIEW View = (PMAPPED_VIEW) ExAllocateHeap (TRUE, sizeof(MAPPED_VIEW));
	if (!View)
	{
		ExReleaseMutex (&MmPageDatabaseLock);
		return STATUS_INSUFFICIENT_RESOURCES;
	}

	View->StartOffsetInFile.LowPart = OffsetStart;
	View->StartOffsetInFile.HighPart = OffsetStartHigh;
	View->ViewSize = ViewSize;
	View->StartPte = StartPte;
	View->StartVa = va;
	View->Mapping = Mapping;
	View->hMapping = hMapping; // save handle
	View->Protection = Protection & MM_MAXIMUM_PROTECTION;

	//
	// Insert structure
	//

	InterlockedInsertTailList (&Mapping->ViewList, &View->ViewListEntry);

	//
	// Initialize view properly
	//

	MiInitializeViewPages (View);

	//
	// Return
	//

	*VirtualAddress = va;

	ExReleaseMutex (&MmPageDatabaseLock);

	return STATUS_SUCCESS;
}

STATUS
KEAPI
MiUnmapViewOfFile(
	IN PMAPPED_VIEW View
	)
/*++
	Internal routine used to unmap view of file
	
	Environment: mapping list locked.
--*/
{
	return STATUS_NOT_SUPPORTED;
}


KESYSAPI
STATUS
KEAPI
MmUnmapViewOfFile(
	IN HANDLE hMapping,
	IN PVOID VirtualAddress
	)
/*++
	Unmap the specified view of file
--*/
{
	return STATUS_NOT_SUPPORTED;
}

