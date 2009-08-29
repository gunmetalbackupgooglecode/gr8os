#ifndef _VCRT_H_
#define _VCRT_H_

#define FILE FILE_OBJECT
#define PFILE PFILE_OBJECT

#include "common.h"

#undef FILE
#undef PFILE

#ifdef __cplusplus
extern "C" {
#endif

#define VCRTAPI _cdecl

extern int __errno;
int * __cdecl _errno(void);
#define errno   (*_errno())

#define ESUCCESS		0
#define EPERM           1
#define ENOENT          2
#define ESRCH           3
#define EINTR           4
#define EIO             5
#define ENXIO           6
#define E2BIG           7
#define ENOEXEC         8
#define EBADF           9
#define ECHILD          10
#define EAGAIN          11
#define ENOMEM          12
#define EACCES          13
#define EFAULT          14
#define EBUSY           16
#define EEXIST          17
#define EXDEV           18
#define ENODEV          19
#define ENOTDIR         20
#define EISDIR          21
#define EINVAL          22
#define ENFILE          23
#define EMFILE          24
#define ENOTTY          25
#define EFBIG           27
#define ENOSPC          28
#define ESPIPE          29
#define EROFS           30
#define EMLINK          31
#define EPIPE           32
#define EDOM            33
#define ERANGE          34
#define EDEADLK         36
#define ENAMETOOLONG    38
#define ENOLCK          39
#define ENOSYS          40
#define ENOTEMPTY       41
#define EILSEQ          42
#define EDEADLOCK       EDEADLK
#define EEOF			43
#define EUNK			0xFFFFFFFF


extern LIST_ENTRY StreamListHead;
extern MUTEX StreamListLock;

#define FMODE_READ_EXISTING		0x00000001
#define FMODE_WRITE_EMPTY		0x00000002
#define FMODE_APPEND			0x00000003
#define FMODE_READ_WRITE		0x00000004
#define FMODE_RW_EMPTY			0x00000005
#define FMODE_APPEND_EX			0x00000006

#define FMODE_MODE				0x00000FFF

#define FMODE_TEMP_NOTFLUSH		0x00010000
#define FMODE_TEMP_DELETE		0x00020000

#define FMODE_TEMPORARY			0x00FF0000

#define FMODE_TEXT				0x10000000
#define FMODE_BINARY			0x20000000
#define FMODE_PREDEFINED_STREAM	0x80000000

#define FMODE_TYPE				0xFF000000

#define FFLAGS_EOF				0x00000001

#define FFILE_SIGNATURE 'FILE'

typedef struct _FILE
{	
	LIST_ENTRY StreamListEntry;
	ULONG Signature;
	ULONG Mode;
	union
	{
		struct
		{
			HANDLE hFile;
			LARGE_INTEGER Position;
			LARGE_INTEGER Size;
			ULONG Flags;
		};
		struct
		{
			ULONG FileId;
		};
	};
	ULONG LastErr;
	MUTEX Lock;
	BOOLEAN BeingDeleted;
} FILE, *PFILE;

extern FILE *stdin, *stdout, *stderr;

int
VCRTAPI
StatusToError(
	STATUS Status
	);

FILE*
VCRTAPI
fopen(
	PCHAR filename,
	PCHAR mode
	);

int
VCRTAPI
fclose(
	FILE *stream
	);

int
VCRTAPI
_fcloseall(
	);

FILE*
VCRTAPI
_fdopen(
	int fd,
	PCHAR mode
	);

#define fdopen(fd,mode) _fdopen(fd,mode)

void*
VCRTAPI
malloc(
	size_t size
	);

void
VCRTAPI
free(
	void *ptr
	);

#ifdef __cplusplus
}
#endif

#endif

