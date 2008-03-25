#pragma once

#define MIN_CACHED_CLUSTERS		32
#define MAX_CACHED_CLUSTERS		1024

#pragma pack(1)

typedef struct CCFILE_CACHED_CLUSTER
{
	USHORT Cached : 1;
	USHORT Modified : 1;
	USHORT WriteError : 1;
	USHORT ClusterUseCount : 13;
	ULONG ClusterNumber;
	STATUS Status;
	PVOID Buffer;
} *PCCFILE_CACHED_CLUSTER;

#pragma pack()

typedef STATUS (KEAPI *PCCFILE_CACHE_READ)(
	IN PFILE FileObject,
	IN ULONG ClusterNumber,
	OUT PVOID Buffer,
	IN ULONG Size
	);

typedef STATUS (KEAPI *PCCFILE_CACHE_WRITE)(
	IN PFILE FileObject,
	IN ULONG ClusterNumber,
	IN PVOID Buffer,
	IN ULONG Size
	);


typedef struct CCFILE_CACHE_CALLBACKS
{
	PCCFILE_CACHE_READ ActualRead;
	PCCFILE_CACHE_WRITE ActualWrite;
} *PCCFILE_CACHE_CALLBACKS;

typedef struct CCFILE_CACHE_MAP
{
	MUTEX CacheMapLock;
	PFILE FileObject;
	ULONG CachedClusters;		// Number of currently cached clusters
	ULONG MaxCachedClusters;	// Maximum size of the following array
	ULONG RebuildCount;
	ULONG ClusterSize;
	ULONG ShouldRebuild;
	CCFILE_CACHE_CALLBACKS Callbacks;
	PCCFILE_CACHED_CLUSTER ClusterCacheMap;	// Array of cached clusters.
} *PCCFILE_CACHE_MAP;


KESYSAPI
VOID
KEAPI
CcInitializeFileCaching(
	IN PFILE File,
	IN ULONG ClusterSize,
	IN PCCFILE_CACHE_CALLBACKS Callbacks
	);

VOID
KEAPI
CcpRebuildCacheMap(
	IN PCCFILE_CACHE_MAP CacheMap,
	IN BOOLEAN ForceReduceMap
	);

#define CC_CACHED_CLUSTER_USECOUNT_INITIAL_TRESHOLD	0
#define CC_CACHED_CLUSTER_REBUILD_COUNT_TRESHOLD	10
#define CC_CACHE_MAP_REBUILD_FREQUENCY				4

STATUS
KEAPI
CcpCacheFileCluster(
	IN PFILE FileObject,
	IN ULONG ClusterNumber,
	IN PVOID ClusterBuffer
	);

KESYSAPI
STATUS
KEAPI
CcCacheReadFile(
	IN PFILE FileObject,
	IN ULONG ClusterNumber,
	OUT PVOID Buffer,
	IN ULONG Size
	);

KESYSAPI
STATUS
KEAPI
CcCacheWriteFile(
	IN PFILE FileObject,
	IN ULONG ClusterNumber,
	IN PVOID Buffer,
	IN ULONG Size
	);

KESYSAPI
STATUS
KEAPI
CcPurgeCacheFile(
	IN PFILE FileObject
	);

KESYSAPI
STATUS
KEAPI
CcFreeCacheMap(
	IN PFILE FileObject
	);
