//
// FILE:		paging.cpp
// CREATED:		17-Feb-2008  by Great
// PART:        MM
// ABSTRACT:
//			Memory Management routines. Paging support
//

#include "common.h"

KESYSAPI
STATUS
KEAPI
MmCreatePagingFile(
	IN PUNICODE_STRING PagingFileName,
	IN OUT PULONG PagingFileNumber,
	OUT PMMPAGING_FILE *PagingFile
	)
{
	return STATUS_NOT_IMPLEMENTED;
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
		if (MmPagingFile[pf].Size)
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

