//
// FILE:		cc.cpp
// CREATED:		25-Mar-2008  by Great
// PART:        Cache
// ABSTRACT:
//			Cache subsystem
//

#include "common.h"

KESYSAPI
VOID
KEAPI
CcInitializeFileCaching(
	IN PFILE File,
	IN ULONG ClusterSize
	)
/*++
	Initialize file cache map. Further read/write operating may be cached.
--*/
{
	PCCFILE_CACHE_MAP CacheMap = (PCCFILE_CACHE_MAP) ExAllocateHeap (TRUE, sizeof(CCFILE_CACHE_MAP));
	if (CacheMap == NULL)
	{
		//
		// Insufficient resources to allocate cache map structure
		// File operations will be non-cached.
		//

		File->CacheMap = NULL;
		File->Synchronize = TRUE;
		File->DesiredAccess |= SYNCHRONIZE;
		return;
	}

	CacheMap->CachedClusters = 0;
	CacheMap->FileObject = File;
	CacheMap->ClusterSize = ClusterSize;
	CacheMap->MaxCachedClusters = MIN_CACHED_CLUSTERS;
	CacheMap->RebuildCount = 0;
	CacheMap->ClusterCacheMap = (PCCFILE_CACHED_CLUSTER) ExAllocateHeap (TRUE, MIN_CACHED_CLUSTERS*sizeof(CCFILE_CACHED_CLUSTER));

	if (CacheMap->ClusterCacheMap == NULL)
	{
		//
		// Insufficient resources to allocate cache map.
		// File operations will be non-cached.
		//

		ExFreeHeap (CacheMap);
		File->CacheMap = NULL;
		File->Synchronize = TRUE;
		File->DesiredAccess |= SYNCHRONIZE;
		return;
	}

	KeZeroMemory (CacheMap->ClusterCacheMap, MIN_CACHED_CLUSTERS*sizeof(CCFILE_CACHED_CLUSTER));

	ExInitializeMutex (&CacheMap->CacheMapLock);

	File->CacheMap = CacheMap;
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

	Status = CcPurgeCacheFile (File);
	if (!SUCCESS(Status))
	{
		return Status;
	}

	ExAcquireMutex (&File->CacheMap->CacheMapLock);
	ExFreeHeap (File->CacheMap->ClusterCacheMap);
	ExFreeHeap (File->CacheMap);

	//
	// Set this in the assumtion that file object can be used later in synchronous operations
	//

	File->CacheMap = NULL;
	File->Synchronize = TRUE;
	File->DesiredAccess |= SYNCHRONIZE;

	ExReleaseMutex (&File->CacheMap->CacheMapLock);
	return STATUS_SUCCESS;
}

KESYSAPI
STATUS
KEAPI
CcCacheFileCluster(
	IN PFILE FileObject,
	IN ULONG ClusterNumber,
	IN PVOID ClusterBuffer
	)
/*++
	Add the specified cluster to cache map
--*/
{
	PCCFILE_CACHE_MAP CacheMap = FileObject->CacheMap;
	PCCFILE_CACHED_CLUSTER CachedClusters;
	STATUS Status = STATUS_UNSUCCESSFUL;

	ExAcquireMutex (&CacheMap->CacheMapLock);

	CacheMap->CachedClusters ++;
	if (CacheMap->CachedClusters == CacheMap->MaxCachedClusters)
	{
		if (CacheMap->MaxCachedClusters == MAX_CACHED_CLUSTERS)
		{
			//
			// Cache map reached its maximum size. Reduce it
			//

			CcpRebuildCacheMap (CacheMap, TRUE);
		}

		CacheMap->MaxCachedClusters *= 2;

		CachedClusters = CacheMap->ClusterCacheMap;
		CacheMap->ClusterCacheMap = (PCCFILE_CACHED_CLUSTER) ExReallocHeap (
			CacheMap->ClusterCacheMap, 
			CacheMap->MaxCachedClusters * sizeof(CCFILE_CACHED_CLUSTER));

		if (CacheMap->ClusterCacheMap == NULL)
		{
			//
			// Not enough memory to satisfy the allocation. Reduce cache map again
			//

			CacheMap->ClusterCacheMap = CachedClusters; // restore old pointer
			CacheMap->MaxCachedClusters /= 2;

			CcpRebuildCacheMap (CacheMap, TRUE);
		}
		else
		{
			// Zero new space
			KeZeroMemory (	&CacheMap->ClusterCacheMap[CacheMap->MaxCachedClusters/2], 
							CacheMap->MaxCachedClusters * sizeof(CCFILE_CACHED_CLUSTER) / 2 );
		}
	}

	//
	// Now there is some space to cache one cluster.
	//

	for (ULONG i=0; i<CacheMap->MaxCachedClusters; i++)
	{
		if (CacheMap->ClusterCacheMap[i].Cached == FALSE)
		{
			CacheMap->ClusterCacheMap[i].Cached = TRUE;
			CacheMap->ClusterCacheMap[i].Modified = FALSE;
			CacheMap->ClusterCacheMap[i].ClusterNumber = ClusterNumber;
			CacheMap->ClusterCacheMap[i].ClusterUseCount = 0;
			CacheMap->ClusterCacheMap[i].Buffer = ExAllocateHeap (TRUE, CacheMap->ClusterSize);

			if (CacheMap->ClusterCacheMap[i].Buffer == NULL)
			{
				//
				// Not enough space to cache cluster..
				//

				CacheMap->ClusterCacheMap[i].Cached = FALSE;
				Status = STATUS_INSUFFICIENT_RESOURCES;
			}
			else
			{
				memcpy (CacheMap->ClusterCacheMap[i].Buffer, ClusterBuffer, CacheMap->ClusterSize);
				Status = STATUS_SUCCESS;
			}

			break;
		}
	}

	ExReleaseMutex (&CacheMap->CacheMapLock);
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
	For any cached clusters following actions will be performed:
		1. if use count is too small, try to delete this cluster.
		2. to delete cluster check if it is modified. If so, try to write it
		   on disk
	    3. don't delete modified clusters, that failed to be written on disk.
		4. if cluster has become non-modified and there was no write error, 
		   delete this cluster from cache map.
--*/
{
	ULONG ClustersDeleted = 0;
	ULONG UseCountTreshold = CC_CACHED_CLUSTER_USECOUNT_INITIAL_TRESHOLD;

	ExAcquireMutex (&CacheMap->CacheMapLock);

_rebuild:

	for (ULONG i=0; i<CacheMap->MaxCachedClusters; i++)
	{
		if (CacheMap->ClusterCacheMap[i].Cached)
		{
			//
			// Check use count. If it equals zero, when CacheMap->RebuildCount is greater than 2 or reducing requested,
			// than delete cached cluster.
			//

			if ( CacheMap->ClusterCacheMap[i].ClusterUseCount <= UseCountTreshold &&
				 (CacheMap->RebuildCount >= CC_CACHED_CLUSTER_REBUILD_COUNT_TRESHOLD || ForceReduceMap) )
			{
				//
				// Delete cached cluster
				//

				if (CacheMap->ClusterCacheMap[i].Modified)
				{
					//
					// Write cluster.
					//

					IO_STATUS_BLOCK IoStatus;
					LARGE_INTEGER FileOffset = {0};
					STATUS Status;

					FileOffset.LowPart = CacheMap->ClusterCacheMap[i].ClusterNumber * CacheMap->ClusterSize;

					Status = IoWriteFile ( CacheMap->FileObject, 
								  CacheMap->ClusterCacheMap[i].Buffer, 
								  CacheMap->ClusterSize,
								  &FileOffset,
								  &IoStatus
								  );

					if (!SUCCESS(Status))
					{
						//
						// Write failed.. cannot delete this cluster
						//

						CacheMap->ClusterCacheMap[i].WriteError = 1;
						CacheMap->ClusterCacheMap[i].Status = Status;
					}
					else
					{
						CacheMap->ClusterCacheMap[i].Modified = FALSE;
					}
				}

				if ( CacheMap->ClusterCacheMap[i].Modified == FALSE &&
					 CacheMap->ClusterCacheMap[i].WriteError == FALSE )
				{
					//
					// We can delete this cached cluster
					//

					CacheMap->ClusterCacheMap[i].Cached = FALSE;
					ExFreeHeap (CacheMap->ClusterCacheMap[i].Buffer);

					CacheMap->CachedClusters --;
					ClustersDeleted ++;
				}
			}
		}
	}

	if (ForceReduceMap && ClustersDeleted == 0)
	{
		//
		// If cache map reducing requested, but we did not delete any cluster,
		//  raise up use count delete treshold and rebuild cache map again.
		//

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
	Write all modified clusters to the disk.
--*/
{
	STATUS Status = STATUS_SUCCESS;
	PCCFILE_CACHE_MAP CacheMap = FileObject->CacheMap;

	ExAcquireMutex (&CacheMap->CacheMapLock);

	for (ULONG i=0; i<CacheMap->MaxCachedClusters; i++)
	{
		if (CacheMap->ClusterCacheMap[i].Cached &&
			CacheMap->ClusterCacheMap[i].Modified)
		{
			//
			// Write to disk
			//

			IO_STATUS_BLOCK IoStatus;
			LARGE_INTEGER FileOffset = {0};

			FileOffset.LowPart = CacheMap->ClusterCacheMap[i].ClusterNumber * CacheMap->ClusterSize;

			Status = IoWriteFile ( CacheMap->FileObject, 
						  CacheMap->ClusterCacheMap[i].Buffer, 
						  CacheMap->ClusterSize,
						  &FileOffset,
						  &IoStatus
						  );

			if (!SUCCESS(Status))
			{
				//
				// Write failed.. cannot delete this cluster
				//

				CacheMap->ClusterCacheMap[i].WriteError = 1;
				CacheMap->ClusterCacheMap[i].Status = Status;
				break;
			}
			else
			{
				CacheMap->ClusterCacheMap[i].Modified = FALSE;
			}
		}
	}

	ExReleaseMutex (&CacheMap->CacheMapLock);
	return Status;
}

KESYSAPI
STATUS
KEAPI
CcCacheReadFile(
	IN PFILE FileObject,
	IN ULONG ClusterNumber,
	OUT PVOID Buffer,
	IN ULONG Size,
	OUT ULONG* ReturnedLength
	)
/*++
	Reads cluster from the cache
--*/
{
	STATUS Status = STATUS_NOT_FOUND;
	PCCFILE_CACHE_MAP CacheMap = FileObject->CacheMap;

	CcpRebuildCacheMap (CacheMap, FALSE);

	ExAcquireMutex (&CacheMap->CacheMapLock);

	for (ULONG i=0; i<CacheMap->MaxCachedClusters; i++)
	{
		if (CacheMap->ClusterCacheMap[i].Cached &&
			CacheMap->ClusterCacheMap[i].ClusterNumber == ClusterNumber)
		{
			CacheMap->ClusterCacheMap[i].ClusterUseCount ++;
			memcpy (Buffer, CacheMap->ClusterCacheMap[i].Buffer, Size);
			*ReturnedLength = Size;
			Status = STATUS_SUCCESS;
			break;
		}
	}

	ExReleaseMutex (&CacheMap->CacheMapLock);
	return Status;
}

KESYSAPI
STATUS
KEAPI
CcCacheWriteFile(
	IN PFILE FileObject,
	IN ULONG ClusterNumber,
	IN PVOID Buffer,
	IN ULONG Size
	)
/*++
	Writes cluster to the cache
--*/
{
	STATUS Status = STATUS_NOT_FOUND;
	PCCFILE_CACHE_MAP CacheMap = FileObject->CacheMap;

	CcpRebuildCacheMap (CacheMap, FALSE);

	ExAcquireMutex (&CacheMap->CacheMapLock);

	for (ULONG i=0; i<CacheMap->MaxCachedClusters; i++)
	{
		if (CacheMap->ClusterCacheMap[i].Cached &&
			CacheMap->ClusterCacheMap[i].ClusterNumber == ClusterNumber)
		{
			CacheMap->ClusterCacheMap[i].ClusterUseCount ++;
			memcpy (CacheMap->ClusterCacheMap[i].Buffer, Buffer, Size);
			CacheMap->ClusterCacheMap[i].Modified = TRUE;
			Status = STATUS_SUCCESS;
			break;
		}
	}

	ExReleaseMutex (&CacheMap->CacheMapLock);
	return Status;
}