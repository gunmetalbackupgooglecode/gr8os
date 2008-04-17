//
// <cc.h> built by header file parser at 09:59:07  17 Apr 2008
// This is a part of gr8os include files for GR8OS Driver & Extender Development Kit (DEDK)
//

#pragma once

#define MIN_CACHED_CLUSTERS		32
#define MAX_CACHED_CLUSTERS		1024


typedef struct FILE *PFILE;

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


*PCCFILE_CACHE_MAP;

KESYSAPI
STATUS
KEAPI
CcInitializeFileCaching(
	IN PFILE File,
	IN ULONG ClusterSize,
	IN PCCFILE_CACHE_CALLBACKS Callbacks
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

