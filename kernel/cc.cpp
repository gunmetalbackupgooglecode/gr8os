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

	CacheMap->CachedClusters = 0;
	CacheMap->FileObject = File;
	CacheMap->ClusterSize = ClusterSize;

	CacheMap->ClustersPerPage = 0;
	if (ClusterSize < PAGE_SIZE)
	{
		CacheMap->ClustersPerPage = (UCHAR)(PAGE_SIZE / ClusterSize);
	}

	CacheMap->MaxCachedClusters = MIN_CACHED_CLUSTERS;
	CacheMap->RebuildCount = 0;
	CacheMap->ShouldRebuild = 0;
	CacheMap->Callbacks = *Callbacks;

	if (IS_FILE_CACHED(File))
	{
		CacheMap->ClusterCacheMap = (PCCFILE_CACHED_CLUSTER) ExAllocateHeap (TRUE, MIN_CACHED_CLUSTERS*sizeof(CCFILE_CACHED_CLUSTER));

		if (CacheMap->ClusterCacheMap == NULL)
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

		KeZeroMemory (CacheMap->ClusterCacheMap, MIN_CACHED_CLUSTERS*sizeof(CCFILE_CACHED_CLUSTER));
	}
	else
	{
		CacheMap->MaxCachedClusters = 0;
		CacheMap->ClusterCacheMap = NULL;
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

		for (ULONG i=0,j=0; i<File->CacheMap->MaxCachedClusters && j<File->CacheMap->CachedClusters; i++)
		{
			PCCFILE_CACHED_CLUSTER Cluster = &File->CacheMap->ClusterCacheMap[i];

			if (Cluster->Cached)
			{
				ExFreeHeap (Cluster->Buffer);
				Cluster->Cached = 0;
				j++;
			}
		}

		ExReleaseMutex (&File->CacheMap->CacheMapLock);
		ExFreeHeap (File->CacheMap->ClusterCacheMap);
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
CcpCacheFileCluster(
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

	//ExAcquireMutex (&CacheMap->CacheMapLock);

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
			CacheMap->ClusterCacheMap[i].ClusterUseCount = 1;
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

#ifndef GROSEMU
				PMMPTE PointerPte = MiGetPteAddress (CacheMap->ClusterCacheMap[i].Buffer);
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

VOID
KEAPI
CcpRebuildCacheMap(
	IN PCCFILE_CACHE_MAP CacheMap,
	IN BOOLEAN ForceReduceMap
	)
/*++
	Rebuild file cache map and force reducing it if need.
	For any cached cluster the following actions will be performed:
		1. if use count is too small, try to delete this cluster.
		2. to delete cluster check if it is modified. If so, try to write it
		   on disk
	    3. don't delete modified clusters, that failed to be written on disk.
		4. if cluster has become non-modified and there was no write error, 
		   delete this cluster from cache map.
   This function is usually very unfrequently, frequency of calls is determined by parameter 
    CC_CACHE_MAP_REBUILD_FREQUENCY
--*/
{
	ULONG ClustersDeleted = 0;
	ULONG UseCountTreshold = CC_CACHED_CLUSTER_USECOUNT_INITIAL_TRESHOLD;

	ExAcquireMutex (&CacheMap->CacheMapLock);

	CcPrint (("CC: Rebuild initiated for cache map %08x, rebuild count %d\n", CacheMap, CacheMap->RebuildCount));

_rebuild:

	for (ULONG i=0; i<CacheMap->MaxCachedClusters; i++)
	{
		if (CacheMap->ClusterCacheMap[i].Cached)
		{
			//
			// Check use count. If it equals zero, when CacheMap->RebuildCount is greater than 2 or reducing requested,
			// than delete cached cluster.
			//

			PCCFILE_CACHED_CLUSTER Cluster = &CacheMap->ClusterCacheMap[i];

			CcPrint (("CC: Cached cluster %d, use count %d, modified %d\n",
				Cluster->ClusterNumber, Cluster->ClusterUseCount, Cluster->Modified));
				
			if (Cluster->ClusterUseCount)
				Cluster->ClusterUseCount --;

			//
			// Ensure that this cluster was not used recently, it is modified and we are not going
			//  to force reducing of cache map.
			// If so, write cluster to the disk.
			//

			if ( Cluster->ClusterUseCount <= UseCountTreshold &&
				 Cluster->Modified &&
				 CacheMap->RebuildCount >= CC_CACHED_CLUSTER_REBUILD_COUNT_TRESHOLD &&
				 !ForceReduceMap )
			{
				//
				// It's time to write this cluster to the disk.
				// Write cluster.
				//

				CcPrint (("CC: Writing cached cluster %d\n", Cluster->ClusterNumber));

				STATUS Status;

				Status = CacheMap->Callbacks.ActualWrite (
							  CacheMap->FileObject, 
							  Cluster->ClusterNumber,
							  Cluster->Buffer, 
							  CacheMap->ClusterSize
							  );

				if (!SUCCESS(Status))
				{
					//
					// Write failed..
					//

					Cluster->WriteError = 1;
					Cluster->Status = Status;

					CcPrint (("CC: Error while writing cluster %d: %08x\n", Cluster->ClusterNumber, Status));
				}
				else
				{
					Cluster->Modified = FALSE;
				}
			}

			if ( Cluster->ClusterUseCount <= UseCountTreshold &&
				 (CacheMap->RebuildCount >= CC_CACHED_CLUSTER_REBUILD_COUNT_TRESHOLD || ForceReduceMap) )
			{
				//
				// Delete cached cluster
				//

				CcPrint (("CC: Deleting unused cached cluster %d\n", Cluster->ClusterNumber));

				if (Cluster->Modified)
				{
					//
					// Write cluster.
					//

					CcPrint (("CC: Writing cached cluster being deleted %d\n", Cluster->ClusterNumber));

					STATUS Status;

					Status = CacheMap->Callbacks.ActualWrite (
								  CacheMap->FileObject, 
								  Cluster->ClusterNumber,
								  Cluster->Buffer, 
								  CacheMap->ClusterSize
								  );

					if (!SUCCESS(Status))
					{
						//
						// Write failed.. cannot delete this cluster
						//

						Cluster->WriteError = 1;
						Cluster->Status = Status;

						CcPrint (("CC: Error while writing cluster %d: %08x\n", Cluster->ClusterNumber, Status));
					}
					else
					{
						Cluster->Modified = FALSE;
					}
				}

				if ( Cluster->Modified == FALSE &&
					 Cluster->WriteError == FALSE )
				{
					//
					// We can delete this cached cluster
					//

					Cluster->Cached = FALSE;
					ExFreeHeap (Cluster->Buffer);

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

		CcPrint (("CC: Rebuild: no clusters purged, re-rebuild (treshold=%d)\n", UseCountTreshold));

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

	CcPrint (("CC: Purging cache map %08x\n", CacheMap));

	for (ULONG i=0; i<CacheMap->MaxCachedClusters; i++)
	{
		PCCFILE_CACHED_CLUSTER Cluster = &CacheMap->ClusterCacheMap[i];

		if (Cluster->Cached &&
			Cluster->Modified)
		{
			//
			// Write to disk
			//

			CcPrint (("CC: Writing modified cluster %d\n", Cluster->ClusterNumber));

			Status = CacheMap->Callbacks.ActualWrite (
						  CacheMap->FileObject, 
						  Cluster->ClusterNumber,
						  Cluster->Buffer, 
						  CacheMap->ClusterSize
						  );

			if (!SUCCESS(Status))
			{
				//
				// Write failed.. cannot delete this cluster
				//

				Cluster->WriteError = 1;
				Cluster->Status = Status;
				break;
			}
			else
			{
				Cluster->Modified = FALSE;
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
	IN ULONG Size
	)
/*++
	Reads cluster from the cache
--*/
{
	STATUS Status = STATUS_NOT_FOUND;
	PCCFILE_CACHE_MAP CacheMap = FileObject->CacheMap;

	if (FileObject->ReadThrough)
	{
		return CacheMap->Callbacks.ActualRead (
			FileObject,
			ClusterNumber,
			Buffer,
			Size
			);
	}

	ExAcquireMutex (&CacheMap->CacheMapLock);

	CcPrint (("CC: Cache read requested for the file %08x, cluster %d\n", FileObject, ClusterNumber));

	for (ULONG i=0; i<CacheMap->MaxCachedClusters; i++)
	{
		if (CacheMap->ClusterCacheMap[i].Cached &&
			CacheMap->ClusterCacheMap[i].ClusterNumber == ClusterNumber)
		{
			CacheMap->ClusterCacheMap[i].ClusterUseCount ++;
			memcpy (Buffer, CacheMap->ClusterCacheMap[i].Buffer, Size);
			Status = STATUS_SUCCESS;

			CcPrint (("CC: Cache read satisfied from cache for the file %08x, cluster %d\n", FileObject, ClusterNumber));

			break;
		}
	}

	if (Status == STATUS_NOT_FOUND)
	{
		//
		// Read the cluster
		//

		Status = CacheMap->Callbacks.ActualRead (
			FileObject,
			ClusterNumber,
			Buffer,
			Size
			);

		if (SUCCESS(Status))
		{
			Status = CcpCacheFileCluster (
				FileObject,
				ClusterNumber,
				Buffer
				);
		}

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

	if (FileObject->WriteThrough)
	{
		Status = CacheMap->Callbacks.ActualWrite (
			FileObject,
			ClusterNumber,
			Buffer,
			Size
			);

		if (!SUCCESS(Status))
			return Status;

		if (IS_FILE_CACHED(FileObject) == FALSE)
			return Status;
	}

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

	if (Status == STATUS_NOT_FOUND)
	{
		//
		// Write the cluster
		//

		if (FileObject->WriteThrough == FALSE)
		{
			Status = CacheMap->Callbacks.ActualWrite (
				FileObject,
				ClusterNumber,
				Buffer,
				Size
				);
		}

		if (SUCCESS(Status))
		{
			Status = CcpCacheFileCluster (
				FileObject,
				ClusterNumber,
				Buffer
				);
		}

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