/*
* vcrt.SYS driver source code.
*
* [C]oded for GR8OS by Great, 2009.
*
* It is a part of GR8OS project, see http://code.google.com/p/gr8os/ for details.
*/

#include "vcrt.h"

FILE *stdin, *stdout, *stderr;

void*
VCRTAPI
malloc(
	size_t size
	)
{
	return ExAllocateHeap (TRUE, size);
}

void
VCRTAPI
free(
	void *ptr
	)
{
	ExFreeHeap (ptr);
}

int __errno = ESUCCESS;

int * __cdecl _errno(void)
{
	return &__errno;
}

int
_st_error_codes[] = {
	ESUCCESS,
	EUNK,
	ENOMEM,
	EACCES,
	EUNK,
	ENOENT,
	EINVAL,
	EFAULT,
	EINVAL,
	ERANGE,
	EBUSY,
	EUNK,
	EFAULT,
	EINVAL,
	EINVAL,
	EINVAL,
	EINVAL,
	EBUSY,
	EINVAL,
	EPERM,
	ENOENT,
	EINTR,
	ENODEV,
	EEOF,
	EINVAL,

	EINVAL,
	EINVAL,
	EINVAL,
	EINVAL,
	EINVAL,
	EINVAL,
	EINVAL,
	EINVAL,
};

int
_st_warn_codes[] = {
	ESUCCESS,
	EEOF,
};


int
VCRTAPI
StatusToError(
	STATUS Status
	)
{
	ULONG StCode = Status & 0x0FFFFFFF;
	ULONG StType = Status & 0xF0000000;

	if (Status == STATUS_SUCCESS)
	{
		return ESUCCESS;
	}

	switch (StType)
	{
	case 0:				return ESUCCESS;
	case 0x80000000:	return _st_warn_codes[StCode];
	case 0xF0000000:	return _st_error_codes[StCode];
	}

	return EUNK;
}

LIST_ENTRY StreamListHead;
MUTEX StreamListLock;

ULONG
str_to_mode (
	char *mode
	)
{
	ULONG Flags1 = 0;
	ULONG Flags2 = FMODE_TEXT;
	USHORT Mode = 0;

	for (char *sp = mode; *sp; sp++)
	{
		switch (*sp)
		{
		case 'r':
			if (*(sp+1) == '+')
			{
				Mode = FMODE_READ_WRITE;
				sp++;
			}
			else
				Mode = FMODE_READ_EXISTING;
			break;

		case 'w':
			if (*(sp+1) == '+')
			{
				Mode = FMODE_RW_EMPTY;
				sp++;
			}
			else
				Mode = FMODE_WRITE_EMPTY;
			break;

		case 'a':
			if (*(sp+1) == '+')
			{
				Mode = FMODE_APPEND_EX;
				sp++;
			}
			else
				Mode = FMODE_APPEND;
			break;

		case 'b':
			Flags2 = FMODE_BINARY;
			break;

		case 't':
			Flags2 = FMODE_TEXT;
			break;

		case 'T':
			Flags1 |= FMODE_TEMP_NOTFLUSH;
			break;

		case 'D':
			Flags1 |= FMODE_TEMP_DELETE;
			break;
		}
	}

	return ((ULONG)Mode) | Flags1 | Flags2;
}

FILE*
_make_file_by_fd(
	int fd,
	ULONG Mode
	)
{
	FILE *fp = (FILE*) malloc (sizeof(FILE));
	
	fp->Mode = Mode;
	fp->Signature = FFILE_SIGNATURE;

	if (fd == 0 ||
		fd == 1 || 
		fd == 2)
	{
		fp->Mode |= FMODE_PREDEFINED_STREAM;
		fp->FileId = fd;
	}
	else
	{
		fp->hFile = (HANDLE) fd;
		fp->Size.QuadPart = 0; // BUGBUG: fill!
		fp->Position.QuadPart = 0;
		fp->Flags = 0;
	}

	fp->LastErr = ESUCCESS;
	fp->BeingDeleted = FALSE;
	ExInitializeMutex (&fp->Lock);

	InterlockedOp (&StreamListLock, 
		InsertHeadList (&StreamListHead, 
			&fp->StreamListEntry)
		);
	return fp;
}

FILE*
VCRTAPI
_fdopen(
	int fd,
	PCHAR mode
	)
{
	return _make_file_by_fd (str_to_mode (mode));
}

bool
_lock_stream(
	FILE *fp
	)
{
	if (fp->Signature != FFILE_SIGNATURE)
		return FALSE;

	ExAcquireMutex (&fp->Lock);
	if (fp->BeingDeleted)
	{
		ExReleaseMutex (&fp->Lock);
		return false;
	}

	return true;
}

#define _unlock_stream(f) ExReleaseMutex (&(f)->Lock)

FILE*
VCRTAPI
fopen(
	PCHAR filename,
	PCHAR mode
	)
{
	HANDLE hFile;
	STATUS Status;
	IO_STATUS_BLOCK IoStatus;
	UNICODE_STRING FileName;
	WCHAR *wFileName;
	ULONG DesiredAccess = 0;
	ULONG Mode;
	ULONG Disposition = 0;
	ULONG Options = 0;

	Mode = str_to_mode (mode);
	switch (Mode & FMODE_MODE)
	{
	case FMODE_READ_EXISTING:	// r
		DesiredAccess = FILE_READ_DATA;
		Disposition = FILE_OPEN_EXISTING;
		break;

	case FMODE_WRITE_EMPTY:		// w
		DesiredAccess = FILE_WRITE_DATA;
		Disposition = FILE_CREATE_ALWAYS;
		break;

	case FMODE_APPEND:			// a
		DesiredAccess = FILE_READ_DATA|FILE_WRITE_DATA;
		Disposition = FILE_OPEN_EXISTING;
		break;

	case FMODE_READ_WRITE:		// r+
		DesiredAccess = FILE_READ_DATA|FILE_WRITE_DATA;
		Disposition = FILE_OPEN_EXISTING;
		break;

	case FMODE_RW_EMPTY:		// w+
		DesiredAccess = FILE_READ_DATA|FILE_WRITE_DATA;
		Disposition = FILE_CREATE_NEW;
		break;

	case FMODE_APPEND_EX:		// a+
		DesiredAccess = FILE_READ_DATA|FILE_WRITE_DATA;


	wFileName = (WCHAR*) malloc (strlen(filename)*2+2);
	mbstowcs (wFileName, filename, -1);
	RtlInitUnicodeString (&FileName, wFileName);

	Status = KsCreateFile (
		&hFile,
		DesiredAccess,
		&FileName,
		&IoStatus,
		Disposition,
		Options
		);
}

// Driver entry point
STATUS DriverEntry(IN PDRIVER DriverObject)
{
	KdPrint(("vcrt: DriverEntry() enter\n"));

	InitializeListHead (&StreamListHead);
	ExInitializeMutex (&StreamListLock);

	KdPrint(("vcrt: DriverEntry() exit\n"));
	return STATUS_SUCCESS;
}
