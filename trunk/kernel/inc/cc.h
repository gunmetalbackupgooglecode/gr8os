// begin_ddk
#pragma once

#define MIN_CACHED_PAGES		32
#define MAX_CACHED_PAGES		1024

// end_ddk

#pragma pack(1)

typedef struct CCFILE_CACHED_PAGE
{
	USHORT Cached : 1;
	USHORT Modified : 1;
	USHORT WriteError : 1;
	USHORT PageUseCount : 13;
	ULONG PageNumber;
	STATUS Status;
	PVOID Buffer;
} *PCCFILE_CACHED_PAGE;

#pragma pack()

// begin_ddk

typedef struct FILE *PFILE;

typedef STATUS (KEAPI *PCCFILE_CACHE_READ)(
	IN PFILE FileObject,
	IN ULONG PageNumber,
	OUT PVOID Buffer,
	IN ULONG Size
	);

typedef STATUS (KEAPI *PCCFILE_CACHE_WRITE)(
	IN PFILE FileObject,
	IN ULONG PageNumber,
	IN PVOID Buffer,
	IN ULONG Size
	);


typedef struct CCFILE_CACHE_CALLBACKS
{
	PCCFILE_CACHE_READ ActualRead;
	PCCFILE_CACHE_WRITE ActualWrite;
} *PCCFILE_CACHE_CALLBACKS;


typedef struct CCFILE_CACHE_MAP

// end_ddk

{
	MUTEX CacheMapLock;
	PFILE FileObject;
	ULONG CachedPages;		// Number of currently cached pages
	ULONG MaxCachedPages;	// Maximum size of the following array
	ULONG RebuildCount;
	ULONG ClusterSize;
	UCHAR ClustersPerPage;
	ULONG ShouldRebuild;
	CCFILE_CACHE_CALLBACKS Callbacks;
	PCCFILE_CACHED_PAGE PageCacheMap;	// Array of cached pages.
}

// begin_ddk

*PCCFILE_CACHE_MAP;

KESYSAPI
STATUS
KEAPI
CcInitializeFileCaching(
	IN PFILE File,
	IN ULONG PageSize,
	IN PCCFILE_CACHE_CALLBACKS Callbacks
	);

// end_ddk

VOID
KEAPI
CcpRebuildCacheMap(
	IN PCCFILE_CACHE_MAP CacheMap,
	IN BOOLEAN ForceReduceMap
	);

#define CC_CACHED_PAGE_USECOUNT_INITIAL_TRESHOLD	0
#define CC_CACHED_PAGE_REBUILD_COUNT_TRESHOLD	10
#define CC_CACHE_MAP_REBUILD_FREQUENCY				4

STATUS
KEAPI
CcpCacheFilePage(
	IN PFILE FileObject,
	IN ULONG PageNumber,
	IN PVOID PageBuffer
	);

// begin_ddk

KESYSAPI
STATUS
KEAPI
CcCacheReadFile(
	IN PFILE FileObject,
	IN ULONG Offset,
	OUT PVOID Buffer,
	IN ULONG Size
	);

KESYSAPI
STATUS
KEAPI
xCcCacheWriteFile(
	IN PFILE FileObject,
	IN ULONG PageNumber,
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

// end_ddk
