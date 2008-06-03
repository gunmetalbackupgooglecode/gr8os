//
// FILE:		cc.cpp
// CREATED:		25-Mar-2008  by Great
// PART:        Cache
// ABSTRACT:
//			Cache subsystem
//

#include "common.h"

#if CC_TRACE_CACHING
#define CcPrint(x) KdPrint(x)
#else
#define CcPrint(x)
#endif

KESYSAPI
STATUS
KEAPI
CcInitializeFileCaching(
	IN PFILE File,
	IN ULONG ClusterSize,
	IN PCCFILE_CACHE_CALLBACKS Callbacks
	)
/*++
	Initialize file cache map. Further read/write operating may be cached.
--*/
{
	if (PAGE_SIZE > ClusterSize)
	{
		if (PAGE_SIZE % ClusterSize)
			return STATUS_INVALID_PARAMETER; // cannot cache
	}

	PCCFILE_CACHE_MAP CacheMap = (PCCFILE_CACHE_MAP) ExAllocateHeap (TRUE, sizeof(CCFILE_CACHE_MAP));
	if (CacheMap == NULL)
	{
		//
		// Insufficient resources to allocate cache map structure
		// File operations will be non-cached.
		//

		File->CacheMap = NULL;
		
		SET_FILE_NONCACHED (File);

		File->DesiredAccess |= FILE_NONCACHED;
		return STATUS_INSUFFICIENT_RESOURCES;
	}

	CacheMap->CachedPages = 0;
	CacheMap->FileObject = File;
	CacheMap->ClusterSize = ClusterSize;

	CacheMap->ClustersPerPage = 0;
	if (ClusterSize < PAGE_SIZE)
	{
		CacheMap->ClustersPerPage = (UCHAR)(PAGE_SIZE / ClusterSize);
	}

	CacheMap->MaxCachedPages = MIN_CACHED_PAGES;
	CacheMap->RebuildCount = 0;
	CacheMap->ShouldRebuild = 0;
	CacheMap->Callbacks = *Callbacks;

	if (IS_FILE_CACHED(File))
	{
		CacheMap->PageCacheMap = (PCCFILE_CACHED_PAGE) ExAllocateHeap (TRUE, MIN_CACHED_PAGES*sizeof(CCFILE_CACHED_PAGE));

		if (CacheMap->PageCacheMap == NULL)
		{
			//
			// Insufficient resources to allocate cache map.
			// File operations will be non-cached.
			//

			ExFreeHeap (CacheMap);
			File->CacheMap = NULL;
			
			SET_FILE_NONCACHED (File);

			File->DesiredAccess |= FILE_NONCACHED;
			return STATUS_INSUFFICIENT_RESOURCES;
		}

		KeZeroMemory (CacheMap->PageCacheMap, MIN_CACHED_PAGES*sizeof(CCFILE_CACHED_PAGE));
	}
	else
	{
		CacheMap->MaxCachedPages = 0;
		CacheMap->PageCacheMap = NULL;
	}

	ExInitializeMutex (&CacheMap->CacheMapLock);

	File->CacheMap = CacheMap;
	return STATUS_SUCCESS;
}

KESYSAPI
STATUS
KEAPI
CcFreeCacheMap(
	IN PFILE File
	)
/*++
	Free all buffers allocated by CcInitializeFileCaching
--*/
{
	STATUS Status;

	if (IS_FILE_CACHED(File))
	{
		Status = CcPurgeCacheFile (File);
		if (!SUCCESS(Status))
		{
			return Status;
		}

		ExAcquireMutex (&File->CacheMap->CacheMapLock);

		for (ULONG i=0,j=0; i<File->CacheMap->MaxCachedPages && j<File->CacheMap->CachedPages; i++)
		{
			PCCFILE_CACHED_PAGE Page = &File->CacheMap->PageCacheMap[i];

			if (Page->Cached)
			{
				MmFreePage (Page->Buffer);
				Page->Cached = 0;
				j++;
			}
		}

		ExReleaseMutex (&File->CacheMap->CacheMapLock);
		ExFreeHeap (File->CacheMap->PageCacheMap);
	}

	ExFreeHeap (File->CacheMap);

	//
	// Set this in the assumtion that file object can be used later in synchronous operations
	//

	File->CacheMap = NULL;
	SET_FILE_NONCACHED (File);
	File->DesiredAccess |= FILE_NONCACHED;

	return STATUS_SUCCESS;
}

STATUS
KEAPI
CcpCacheFilePage(
	IN PFILE FileObject,
	IN ULONG PageNumber,
	IN PVOID PageBuffer
	)
/*++
	Add the specified page to cache map
--*/
{
	PCCFILE_CACHE_MAP CacheMap = FileObject->CacheMap;
	PCCFILE_CACHED_PAGE CachedPages;
	STATUS Status = STATUS_UNSUCCESSFUL;

	//ExAcquireMutex (&CacheMap->CacheMapLock);

	CacheMap->CachedPages ++;
	if (CacheMap->CachedPages == CacheMap->MaxCachedPages)
	{
		if (CacheMap->MaxCachedPages == MAX_CACHED_PAGES)
		{
			//
			// Cache map reached its maximum size. Reduce it
			//

			CcpRebuildCacheMap (CacheMap, TRUE);
		}

		CacheMap->MaxCachedPages *= 2;

		CachedPages = CacheMap->PageCacheMap;
		CacheMap->PageCacheMap = (PCCFILE_CACHED_PAGE) ExReallocHeap (
			CacheMap->PageCacheMap, 
			CacheMap->MaxCachedPages * sizeof(CCFILE_CACHED_PAGE));

		if (CacheMap->PageCacheMap == NULL)
		{
			//
			// Not enough memory to satisfy the allocation. Reduce cache map again
			//

			CacheMap->PageCacheMap = CachedPages; // restore old pointer
			CacheMap->MaxCachedPages /= 2;

			CcpRebuildCacheMap (CacheMap, TRUE);
		}
		else
		{
			// Zero new space
			KeZeroMemory (	&CacheMap->PageCacheMap[CacheMap->MaxCachedPages/2], 
							CacheMap->MaxCachedPages * sizeof(CCFILE_CACHED_PAGE) / 2 );
		}
	}

	//
	// Now there is some space to cache one page.
	//

	for (ULONG i=0; i<CacheMap->MaxCachedPages; i++)
	{
		if (CacheMap->PageCacheMap[i].Cached == FALSE)
		{
			CacheMap->PageCacheMap[i].Cached = TRUE;
			CacheMap->PageCacheMap[i].Modified = FALSE;
			CacheMap->PageCacheMap[i].PageNumber = PageNumber;
			CacheMap->PageCacheMap[i].PageUseCount = 1;

			//
			// Recode for MmAllocatePhysicalPages
			//

			CacheMap->PageCacheMap[i].Buffer = MmAllocatePage ();


			if (CacheMap->PageCacheMap[i].Buffer == NULL)
			{
				//
				// Not enough space to cache page..
				//

				CacheMap->PageCacheMap[i].Cached = FALSE;
				Status = STATUS_INSUFFICIENT_RESOURCES;
			}
			else
			{
				memcpy (CacheMap->PageCacheMap[i].Buffer, PageBuffer, PAGE_SIZE);

#ifndef GROSEMU
				PMMPTE PointerPte = MiGetPteAddress (CacheMap->PageCacheMap[i].Buffer);
				PointerPte->u1.e1.Dirty = FALSE;
#endif

				Status = STATUS_SUCCESS;
			}

			break;
		}
	}

	//ExReleaseMutex (&CacheMap->CacheMapLock);
	return Status;
}

STATUS
KEAPI
CcpActualWritePage(
	IN PCCFILE_CACHE_MAP CacheMap,
	IN PCCFILE_CACHED_PAGE Page
	)
/*++
	Performs actual writing of the modified page to the disk.
--*/
{
	PUCHAR pBuffer = (PUCHAR)Page->Buffer;
	STATUS Status;

	if (CacheMap->Callbacks.ActualWrite == NULL)
		return STATUS_NOT_SUPPORTED;

	for (ULONG i=0; i<CacheMap->ClustersPerPage; i++)
	{
		ULONG BytesWritten;

		Status = CacheMap->Callbacks.ActualWrite (
			CacheMap->FileObject, 
			Page->PageNumber*CacheMap->ClustersPerPage + i,
			pBuffer + i*CacheMap->ClusterSize, 
			CacheMap->ClusterSize,
			&BytesWritten
			);
	}

	return Status;
}

STATUS
KEAPI
CcpActualReadPage(
	IN PCCFILE_CACHE_MAP CacheMap,
	IN ULONG PageNumber,
	IN PVOID Buffer
	)
/*++
	Performs actual reading of the page from the disk.
--*/
{
	PUCHAR pBuffer = (PUCHAR)Buffer;
	STATUS Status;

	if (CacheMap->Callbacks.ActualRead == NULL)
		return STATUS_NOT_SUPPORTED;

	for (ULONG i=0; i<CacheMap->ClustersPerPage; i++)
	{
		ULONG BytesRead = 0;

		Status = CacheMap->Callbacks.ActualRead (
			CacheMap->FileObject, 
			PageNumber*CacheMap->ClustersPerPage + i,
			pBuffer + i*CacheMap->ClusterSize, 
			CacheMap->ClusterSize,
			&BytesRead
			);

		//KdPrint(("Actual read %04x\n", BytesRead));

		/*
		IO_STATUS_BLOCK IoStatus;
		LARGE_INTEGER Offset = {0,0};
		Offset.LowPart = (PageNumber*CacheMap->ClustersPerPage + i)*CacheMap->ClusterSize;

		Status = IoReadFile (
			CacheMap->FileObject,
			pBuffer + i*CacheMap->ClusterSize,
			CacheMap->ClusterSize,
			&Offset,
			IRP_FLAGS_SYNCHRONOUS_IO,
			&IoStatus);
		*/

		if (Status == STATUS_END_OF_FILE)
		{
			Status = STATUS_SUCCESS;
			break;
		}
	}

	return Status;
}


VOID
KEAPI
CcpRebuildCacheMap(
	IN PCCFILE_CACHE_MAP CacheMap,
	IN BOOLEAN ForceReduceMap
	)
/*++
	Rebuild file cache map and force reducing it if need.
	For any cached page the following actions will be performed:
		1. if use count is too small, try to delete this page.
		2. to delete page check if it is modified. If so, try to write it
		   on disk
	    3. don't delete modified pages, that failed to be written on disk.
		4. if page has become non-modified and there was no write error, 
		   delete this page from cache map.
   This function is usually very unfrequently, frequency of calls is determined by parameter 
    CC_CACHE_MAP_REBUILD_FREQUENCY
--*/
{
	ULONG PagesDeleted = 0;
	ULONG UseCountTreshold = CC_CACHED_PAGE_USECOUNT_INITIAL_TRESHOLD;

#if CC_DISABLE_CACHE_REBUILDING
	return;
#endif

	ExAcquireMutex (&CacheMap->CacheMapLock);

	CcPrint (("CC: Rebuild initiated for cache map %08x, rebuild count %d\n", CacheMap, CacheMap->RebuildCount));

_rebuild:

	for (ULONG i=0; i<CacheMap->MaxCachedPages; i++)
	{
		if (CacheMap->PageCacheMap[i].Cached)
		{
			//
			// Check use count. If it equals zero, when CacheMap->RebuildCount is greater than 2 or reducing requested,
			// than delete cached page.
			//

			PCCFILE_CACHED_PAGE Page = &CacheMap->PageCacheMap[i];

			CcPrint (("CC: Cached page %d, use count %d, modified %d\n",
				Page->PageNumber, Page->PageUseCount, Page->Modified));
				
			if (Page->PageUseCount)
				Page->PageUseCount --;

			//
			// Ensure that this page was not used recently, it is modified and we are not going
			//  to force reducing of cache map.
			// If so, write page to the disk.
			//

			if ( Page->PageUseCount <= UseCountTreshold &&
				 Page->Modified &&
				 CacheMap->RebuildCount >= CC_CACHED_PAGE_REBUILD_COUNT_TRESHOLD &&
				 !ForceReduceMap )
			{
				//
				// It's time to write this page to the disk.
				// Write page.
				//

				CcPrint (("CC: Writing cached page %d\n", Page->PageNumber));

				STATUS Status;

				/*Status = CacheMap->Callbacks.ActualWrite (
							  CacheMap->FileObject, 
							  Page->PageNumber,
							  Page->Buffer, 
							  CacheMap->PageSize
							  );*/

				Status = CcpActualWritePage (CacheMap, Page);

				if (!SUCCESS(Status))
				{
					//
					// Write failed..
					//

					Page->WriteError = 1;
					Page->Status = Status;

					CcPrint (("CC: Error while writing page %d: %08x\n", Page->PageNumber, Status));
				}
				else
				{
					Page->Modified = FALSE;
				}
			}

			if ( Page->PageUseCount <= UseCountTreshold &&
				 (CacheMap->RebuildCount >= CC_CACHED_PAGE_REBUILD_COUNT_TRESHOLD || ForceReduceMap) )
			{
				//
				// Delete cached page
				//

				CcPrint (("CC: Deleting unused cached page %d\n", Page->PageNumber));

				if (Page->Modified)
				{
					//
					// Write page.
					//

					CcPrint (("CC: Writing cached page being deleted %d\n", Page->PageNumber));

					STATUS Status;

					/*Status = CacheMap->Callbacks.ActualWrite (
								  CacheMap->FileObject, 
								  Page->PageNumber,
								  Page->Buffer, 
								  CacheMap->PageSize
								  );*/

					Status = CcpActualWritePage (CacheMap, Page);
					
					if (!SUCCESS(Status))
					{
						//
						// Write failed.. cannot delete this page
						//

						Page->WriteError = 1;
						Page->Status = Status;

						CcPrint (("CC: Error while writing page %d: %08x\n", Page->PageNumber, Status));
					}
					else
					{
						Page->Modified = FALSE;
					}
				}

				if ( Page->Modified == FALSE &&
					 Page->WriteError == FALSE )
				{
					//
					// We can delete this cached page
					//

					Page->Cached = FALSE;
					//ExFreeHeap (Page->Buffer);
					MmFreePage (Page->Buffer);

					CacheMap->CachedPages --;
					PagesDeleted ++;
				}
			}
		}
	}

	if (ForceReduceMap && PagesDeleted == 0)
	{
		//
		// If cache map reducing requested, but we did not delete any page,
		//  raise up use count delete treshold and rebuild cache map again.
		//

		CcPrint (("CC: Rebuild: no pages purged, re-rebuild (treshold=%d)\n", UseCountTreshold));

		UseCountTreshold ++;
		goto _rebuild;
	}

	CacheMap->RebuildCount ++;
	ExReleaseMutex (&CacheMap->CacheMapLock);
}


KESYSAPI
STATUS
KEAPI
CcPurgeCacheFile(
	IN PFILE FileObject
	)
/*++
	Write all modified pages to the disk.
--*/
{
	STATUS Status = STATUS_SUCCESS;
	PCCFILE_CACHE_MAP CacheMap = FileObject->CacheMap;

	ExAcquireMutex (&CacheMap->CacheMapLock);

	CcPrint (("CC: Purging cache map %08x\n", CacheMap));

	for (ULONG i=0; i<CacheMap->MaxCachedPages; i++)
	{
		PCCFILE_CACHED_PAGE Page = &CacheMap->PageCacheMap[i];

		if (Page->Cached &&
			Page->Modified)
		{
			//
			// Write to disk
			//

			CcPrint (("CC: Writing modified page %d\n", Page->PageNumber));

			/*Status = CacheMap->Callbacks.ActualWrite (
						  CacheMap->FileObject, 
						  Page->PageNumber,
						  Page->Buffer, 
						  CacheMap->PageSize
						  );*/

			Status = CcpActualWritePage (CacheMap, Page);

			if (!SUCCESS(Status))
			{
				//
				// Write failed.. cannot delete this page
				//

				Page->WriteError = 1;
				Page->Status = Status;
				break;
			}
			else
			{
				Page->Modified = FALSE;
			}
		}
	}

	ExReleaseMutex (&CacheMap->CacheMapLock);
	return Status;
}

//#define Cc2Print KdPrint
#define Cc2Print(x) __asm nop;

STATUS
KEAPI
CcpFindAndReadWrite(
	IN PCCFILE_CACHE_MAP CacheMap,
	IN ULONG *PageNumber,
	IN ULONG *PageOffset,
	IN BOOLEAN WriteOperation,
	OUT PVOID *Buffer OPTIONAL,
	IN ULONG *Size,
	OUT PULONG nBytesRead
	)
/*++
	Find page in cache map. If page exists, satisfy read/write request by copying.
	If not, return STATUS_NOT_FOUND.
--*/
{
	STATUS Status = STATUS_NOT_FOUND;

	*nBytesRead = 0;

_repeat_read:

	for (ULONG i=0; i<CacheMap->MaxCachedPages; i++)
	{
		if (CacheMap->PageCacheMap[i].Cached &&
			CacheMap->PageCacheMap[i].PageNumber == *PageNumber)
		{
			CcPrint(("CC: Found cached page %d{%d} (buff=%08x)\n", i, CacheMap->PageCacheMap[i].PageNumber,
				CacheMap->PageCacheMap[i].Buffer));

			if (!ARGUMENT_PRESENT(Buffer))
			{
				//
				// Don't read anything, just lookup
				//

				Status = STATUS_SUCCESS;
				break;
			}

			CacheMap->PageCacheMap[i].PageUseCount ++;

			if ( *Size <= (PAGE_SIZE-*PageOffset) )
			{
				if (!WriteOperation)
				{
					memcpy (*Buffer, (PUCHAR)CacheMap->PageCacheMap[i].Buffer + *PageOffset, *Size);
				}
				else
				{
					memcpy ((PUCHAR)CacheMap->PageCacheMap[i].Buffer + *PageOffset, *Buffer, *Size);
				}

				if (CacheMap->FileObject->RelativeFileName.Length > 0)
					Cc2Print(("[. [%04x] +%04x] ", (*nBytesRead), *Size));

				(*nBytesRead) += *Size;
			}
			else
			{
				ULONG readpart = PAGE_SIZE - (*PageOffset);

				if (!WriteOperation)
				{
					memcpy (*Buffer, (PUCHAR)CacheMap->PageCacheMap[i].Buffer + *PageOffset, readpart);
				}
				else
				{
					memcpy ((PUCHAR)CacheMap->PageCacheMap[i].Buffer + *PageOffset, *Buffer, readpart);
				}

				if (CacheMap->FileObject->RelativeFileName.Length > 0)
					Cc2Print(("[* [%04x] +%04x] ", (*nBytesRead), readpart));

				(*nBytesRead) += readpart;
				
				//KdPrint((" find and r/w: old loop [PN=%x, PO=%x, Size=%x]\n", *PageNumber, *PageOffset, *Size));

				(*PageNumber) ++;
				*PageOffset = 0;
				(*Size) -= readpart;
				(*(ULONG*)Buffer) += readpart;

				//KdPrint((" find and r/w: new loop [PN=%x, PO=%x, Size=%x]\n", *PageNumber, *PageOffset, *Size));

				goto _repeat_read;
			}

			Status = STATUS_SUCCESS;

			if (WriteOperation)
			{
				CacheMap->PageCacheMap[i].Modified = TRUE;
			}

			CcPrint (("CC: Cache %s satisfied from cache for the file %08x, page %d\n", 
				WriteOperation ? "write" : "read", CacheMap->FileObject, *PageNumber));

			break;
		}
	}

	return Status;
}

STATUS
KEAPI
CcpActualRead(
    IN PCCFILE_CACHE_MAP CacheMap,
	IN ULONG PageNumber,
	IN ULONG PageOffset,
	OUT PVOID Buffer OPTIONAL,
	IN ULONG Size,
	OUT PULONG nBytesRead,
	IN BOOLEAN Cache
	)
/*++
	Perform actual reading of page. This function may be recursive
--*/
{
	PVOID TempPage = MmAllocatePage ();
	STATUS Status;

	if (CacheMap->FileObject->RelativeFileName.Length > 0)
		Cc2Print(("CC: CcpActualRead: PN=%04x,POffs=%03x,Size=%04x, Read=%04x\n", PageNumber, PageOffset, Size, *nBytesRead));

	Status = CcpActualReadPage (CacheMap, PageNumber, TempPage);

	if (SUCCESS(Status))
	{
		if (Size > (PAGE_SIZE-PageOffset))
		{
			if (ARGUMENT_PRESENT(Buffer))
			{
				memcpy (Buffer, (PUCHAR)TempPage + PageOffset, (PAGE_SIZE-PageOffset));
			}

			if (CacheMap->FileObject->RelativeFileName.Length > 0)
				Cc2Print(("[$ [%04x] +%04x] ", *nBytesRead, (PAGE_SIZE-PageOffset)));

			(*nBytesRead) += (PAGE_SIZE-PageOffset);
		}
		else
		{
			if (ARGUMENT_PRESENT(Buffer))
			{
				memcpy (Buffer, (PUCHAR)TempPage + PageOffset, Size);
			}

			if (CacheMap->FileObject->RelativeFileName.Length > 0)	
				Cc2Print(("[^ [%04x] +%04x] ", *nBytesRead, Size));

			(*nBytesRead) += Size;
		}

		if (Cache)
		{
			CcpCacheFilePage (
				CacheMap->FileObject,
				PageNumber,
				TempPage
				);
		}
	}

	MmFreePage (TempPage);

	Size -= PAGE_SIZE-PageOffset;
	if ((LONG)Size > 0)
	{
		PVOID NewBuffer = (PUCHAR)Buffer + (PAGE_SIZE-PageOffset);

		if (!ARGUMENT_PRESENT(Buffer))
		{
			NewBuffer = NULL;
		}

		PageNumber ++;
		PageOffset = 0;

		Status = CcpFindAndReadWrite (CacheMap, &PageNumber, &PageOffset, FALSE, &NewBuffer, &Size, nBytesRead);

		if (Status == STATUS_NOT_FOUND)
		{
			Status = CcpActualRead (CacheMap, PageNumber, PageOffset, NewBuffer, Size, nBytesRead, Cache);
		}
	}

	return Status;
}

STATUS
KEAPI
CcpActualWrite(
    IN PCCFILE_CACHE_MAP CacheMap,
	IN ULONG PageNumber,
	IN ULONG PageOffset,
	OUT PVOID Buffer,
	IN ULONG Size,
	OUT PULONG nBytesWritten
	)
/*++
	Perform actual writing of page. This function may be recursive
--*/
{
	PVOID TempPage = MmAllocatePage ();
	STATUS Status;

	Status = CcpActualReadPage (CacheMap, PageNumber, TempPage);

	if (SUCCESS(Status))
	{
		if (Size > (PAGE_SIZE-PageOffset))
		{
			memcpy ((PUCHAR)TempPage + PageOffset, Buffer, (PAGE_SIZE-PageOffset));

			(*nBytesWritten) += (PAGE_SIZE-PageOffset);
		}
		else
		{
			memcpy ((PUCHAR)TempPage + PageOffset, Buffer, Size);
			
			(*nBytesWritten) += Size;
		}

		CcpCacheFilePage (
			CacheMap->FileObject,
			PageNumber,
			TempPage
			);
	}

	MmFreePage (TempPage);

	Size -= PAGE_SIZE-PageOffset;
	if ((LONG)Size > 0)
	{
		PVOID NewBuffer = (PUCHAR)Buffer + (PAGE_SIZE-PageOffset);

		PageNumber ++;
		PageOffset = 0;

		Status = CcpFindAndReadWrite (CacheMap, &PageNumber, &PageOffset, TRUE, &NewBuffer, &Size, nBytesWritten);

		if (Status == STATUS_NOT_FOUND)
		{
			Status = CcpActualWrite (CacheMap, PageNumber, PageOffset, NewBuffer, Size, nBytesWritten);
		}
	}

	return Status;
}

KESYSAPI
STATUS
KEAPI
CcCacheReadFile(
	IN PFILE FileObject,
	IN ULONG Offset,
	OUT PVOID Buffer,
	IN ULONG Size,
	OUT PULONG nBytesRead
	)
/*++
	Read file from the cache
--*/
{
	STATUS Status = STATUS_NOT_FOUND;
	PCCFILE_CACHE_MAP CacheMap = FileObject->CacheMap;
	
	/* -----------hekked by scrat---------- */

	ExAcquireMutex (&CacheMap->CacheMapLock);

	ULONG PageNumber = ALIGN_DOWN (Offset, PAGE_SIZE) / PAGE_SIZE;
	ULONG PageOffset = Offset & (PAGE_SIZE-1);

	if (FileObject->ReadThrough == 0)
	{
		CcPrint (("CC: Cache read requested for the file %08x [%S], offs=%08x [pg=%05x, ofs=%03x], sz=%08x\n", 
			FileObject, FileObject->RelativeFileName.Buffer, Offset, PageNumber, PageOffset, Size));
	}

	ASSERT (Size < PAGE_SIZE*20);

	

	if (FileObject->ReadThrough)
	{
		ExReleaseMutex (&CacheMap->CacheMapLock);

		return CcpActualRead (CacheMap, PageNumber, PageOffset, Buffer, Size, nBytesRead, FALSE);
	}

	//INT3

	// Try to satisfy reading from cache
	Status = CcpFindAndReadWrite (CacheMap, &PageNumber, &PageOffset, FALSE, &Buffer, &Size, nBytesRead);

	if (Status == STATUS_NOT_FOUND)
	{
		//
		// If not, read the pages.
		//

		Status = CcpActualRead (CacheMap, PageNumber, PageOffset, Buffer, Size, nBytesRead, 1);

		if (SUCCESS(Status))
		{
			Status = STATUS_CACHED;
		}
	}

	// Check if cache map should be rebuilded.
	BOOLEAN rebuild = FALSE;
	CacheMap->ShouldRebuild ++;
	if (CacheMap->ShouldRebuild % CC_CACHE_MAP_REBUILD_FREQUENCY == 0)
		rebuild = true;

	ExReleaseMutex (&CacheMap->CacheMapLock);

	if (rebuild)
		CcpRebuildCacheMap (CacheMap, FALSE);

	return Status;
}

/*
KESYSAPI
STATUS
KEAPI
CcRequestCachedPage(
	IN PFILE FileObject,
	IN ULONG PageNumber,
	OUT PVOID *Buffer
	)
{
	PCCFILE_CACHE_MAP CacheMap = FileObject->CacheMap;
	STATUS Status = STATUS_NOT_FOUND;

	ASSERT (CacheMap != NULL);

	ExAcquireMutex (&CacheMap->CacheMapLock);

	for (ULONG i=0; i<CacheMap->MaxCachedPages; i++)
	{
		if (CacheMap->PageCacheMap[i].Cached &&
			CacheMap->PageCacheMap[i].PageNumber == PageNumber)
		{
			*Buffer = CacheMap->PageCacheMap[i].Buffer;
			Status = STATUS_SUCCESS;
			break;
		}
	}

	ExReleaseMutex (&CacheMap->CacheMapLock);

	return Status;
}
*/


KESYSAPI
STATUS
KEAPI
CcCacheWriteFile(
	IN PFILE FileObject,
	IN ULONG Offset,
	IN PVOID Buffer,
	IN ULONG Size,
	OUT PULONG nBytesWritten
	)
/*++
	Writes file to the cache
--*/
{
	STATUS Status = STATUS_NOT_FOUND;
	PCCFILE_CACHE_MAP CacheMap = FileObject->CacheMap;

	ExAcquireMutex (&CacheMap->CacheMapLock);

	ULONG PageNumber = ALIGN_DOWN (Offset, PAGE_SIZE) / PAGE_SIZE;
	ULONG PageOffset = Offset & (PAGE_SIZE-1);

	if (FileObject->WriteThrough == 0)
	{
		CcPrint (("CC: Cache read requested for the file %08x [%S], offs=%08x [pg=%05x, ofs=%03x], sz=%08x\n", 
			FileObject, FileObject->RelativeFileName.Buffer, Offset, PageNumber, PageOffset, Size));
	}

	ASSERT (Size < PAGE_SIZE*20);

	if (FileObject->WriteThrough)
	{
		ExReleaseMutex (&CacheMap->CacheMapLock);

		return CcpActualWrite (CacheMap, PageNumber, PageOffset, Buffer, Size, nBytesWritten);
	}

	//INT3

	// Try to satisfy writing to cache
	Status = CcpFindAndReadWrite (CacheMap, &PageNumber, &PageOffset, TRUE, &Buffer, &Size, nBytesWritten);

	if (Status == STATUS_NOT_FOUND)
	{
		//
		// If not, read the pages and write to cache.
		//

		Status = CcpActualWrite (CacheMap, PageNumber, PageOffset, Buffer, Size, nBytesWritten);

		if (SUCCESS(Status))
		{
			Status = STATUS_CACHED;
		}
	}


	BOOLEAN rebuild = FALSE;
	CacheMap->ShouldRebuild ++;
	if (CacheMap->ShouldRebuild % CC_CACHE_MAP_REBUILD_FREQUENCY == 0)
		rebuild = true;

	ExReleaseMutex (&CacheMap->CacheMapLock);

	if (rebuild)
		CcpRebuildCacheMap (CacheMap, FALSE);

	return Status;
}