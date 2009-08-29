//
// FILE:		listsup.cpp
// CREATED:		17-Feb-2008  by Great
// PART:        MM
// ABSTRACT:
//			Memory Management routines. Memory lists support
//

#include "common.h"

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

