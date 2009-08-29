//
// FILE:		map.cpp
// CREATED:		17-Feb-2008  by Great
// PART:        MM
// ABSTRACT:
//			Memory Management routines. Mapping
//

#include "common.h"

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
