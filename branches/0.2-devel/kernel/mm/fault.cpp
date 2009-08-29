//
// FILE:		fault.cpp
// CREATED:		17-Feb-2008  by Great
// PART:        MM
// ABSTRACT:
//			Memory Management routines. MmAccessFault() resolver
//

#include "common.h"


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


BOOLEAN
KEAPI
MiResolvePagingFault(
	IN PMMPTE PointerPte,
	IN PVOID VirtualAddress,
	IN PVOID FaultingAddress,
	IN PF_ERROR_CODE ErrorCode
	)
{
	KdPrint(( "MM: Access violation in %08x [paging detected : PFN in pagefile = %05x]\n", 
		VirtualAddress, PointerPte->u1.e2.PageFrameNumber ));

	return FALSE;
}


BOOLEAN
KEAPI
MiResolveMappedFault(
	IN PMMPTE PointerPte,
	IN PVOID VirtualAddress,
	IN PVOID FaultingAddress,
	IN PF_ERROR_CODE ErrorCode
	)
{
	KdPrint(( "MM: Access violation in %08x [view detected : FileDescriptor=%04x]\n", 
		VirtualAddress, PointerPte->u1.e3.FileDescriptorNumber ));


	//
	// Find the appropriate MAPPED_FILE
	//

	PMAPPED_FILE Mapping;
	STATUS Status;

	Status = ObpMapHandleToPointer ( (HANDLE)PointerPte->u1.e3.FileDescriptorNumber, -1, (PVOID*)&Mapping, FALSE);
	if (!SUCCESS(Status))
	{
		KdPrint(("MM: #PF: Invalid view [ObpMapHandleToPointer returned status %08x for mapping handle]\n", Status));
		
		return FALSE;
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
					KdPrint(("MM: #PF: Found cached page for the view\n"));

					if (PointerPte->u1.e1.Valid == 0)
					{
						KdPrint(("MM: #PF: R/W (%X) fault (invalid page) in file view. Mapping page\n", ErrorCode.e1.WriteFault));

						PMMPTE TempPte = MiGetPteAddress (CacheMap->PageCacheMap[i].Buffer);

						ASSERT (TempPte->u1.e1.Valid == 1);

						ExAcquireMutex (&MmPageDatabaseLock);

						PointerPte->u1.e1.Valid = 1;

//						if (ErrorCode.e1.WriteFault)
						{
							// Writing request is pending for page.
							PointerPte->u1.e1.Write = (View->Protection == MM_READWRITE || View->Protection == MM_EXECUTE_READWRITE);
//							CacheMap->PageCacheMap[i].Modified = 1;
						}
//						else
//						{
//							PointerPte->u1.e1.Write = 0;
//						}
						
						// Notice: PointerPte->u1.e1.PteType  is still PTE_TYPE_VIEW !!

						PointerPte->u1.e1.Owner = (Mapping->TargetMode == KernelMode);
						PointerPte->u1.e1.PageFrameNumber = TempPte->u1.e1.PageFrameNumber;

						PointerPte->u1.e1.Dirty = 0;
						PointerPte->u1.e1.Accessed = 0;

						KdPrint(("MM: #PF: Write=%d, Owner=%d, PFN=%x, Protection=%d, Mode=%d\n", PointerPte->u1.e1.Write, PointerPte->u1.e1.Owner,
							PointerPte->u1.e1.PageFrameNumber, View->Protection, Mapping->TargetMode));

						MmInvalidateTlb (VirtualAddress);

						ExReleaseMutex (&MmPageDatabaseLock);
					}
					else
					{
						KdPrint(("MM: #PF: R/W (%X) fault in VALID file view\n", ErrorCode.e1.WriteFault));
						ASSERT (FALSE);
					}

					ExReleaseMutex (&CacheMap->CacheMapLock);

					KdPrint(("MM: #PF: Resolved cached page for view\n"));

					ExReleaseMutex (&Mapping->ViewList.Lock);

					return TRUE;
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

			return TRUE;
		}

		View = CONTAINING_RECORD (View->ViewListEntry.Flink, MAPPED_VIEW, ViewListEntry);
	}

	ExReleaseMutex (&Mapping->ViewList.Lock);

	KdPrint(("MM: #PF: Can't find MAPPED_VIEW for the faulted view\n"));

	INT3
	return FALSE;
}

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

	KdPrint(( "MM: Page fault at %08x, buggy instruction: %p [ErrCode=%x] P=%d,W=%d,O=%d\n", 
		VirtualAddress, FaultingAddress, ErrorCode,
		PointerPte->u1.e1.Valid,
		PointerPte->u1.e1.Write,
		PointerPte->u1.e1.Owner));

	switch (PointerPte->u1.e1.PteType)
	{
	case PTE_TYPE_NORMAL_OR_NOTMAPPED:
		{
			KdPrint(( "MM: Access violation in %08x [invalid page]\n", VirtualAddress ));

			if (MmIsAddressValid(FaultingAddress))
			{
				KdPrint((" [EIP]: %08x: %02x %02x %02x  %02x %02x %02x\n",
					FaultingAddress,
					((UCHAR*)FaultingAddress)[0],
					((UCHAR*)FaultingAddress)[1],
					((UCHAR*)FaultingAddress)[2],
					((UCHAR*)FaultingAddress)[3],
					((UCHAR*)FaultingAddress)[4],
					((UCHAR*)FaultingAddress)[5]
				));
			}


			break;
		}

	case PTE_TYPE_PAGEDOUT:
		if (MiResolvePagingFault (PointerPte,
								  VirtualAddress,
								  FaultingAddress, 
								  Err))
			return;

	case PTE_TYPE_TRIMMED:
		{
			KdPrint(( "MM: Access violation in %08x [trimming detected : PFN=%05x]\n", 
				VirtualAddress, PointerPte->u1.e4.PageFrameNumber ));
			
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
		if (MiResolveMappedFault (PointerPte,
								  VirtualAddress,
								  FaultingAddress, 
								  Err))
			return;
	}


	KeBugCheck (KERNEL_MODE_EXCEPTION_NOT_HANDLED,
				STATUS_ACCESS_VIOLATION,
				(ULONG)VirtualAddress,
				(ULONG)FaultingAddress,
				Err.e1.WriteFault
				);
	
	KiStopExecution( );
}


