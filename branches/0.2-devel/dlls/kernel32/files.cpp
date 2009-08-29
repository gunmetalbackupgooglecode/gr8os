#include "kernel32.h"

VOID
KEAPI
CloseHandle(
	HANDLE hObject
	)
{
	KsClose (hObject);
}

HANDLE 
KEAPI
CreateFileW(
  PWSTR lpFileName,
  ULONG dwDesiredAccess,
  ULONG dwShareMode,
  PVOID lpSecurityAttributes,
  DWORD dwCreationDisposition,
  DWORD dwFlagsAndAttributes,
  HANDLE hTemplateFile
)
{
	UNICODE_STRING FileName;
	ULONG KeDesiredAccess = 0;
	ULONG KeDisposition = 0;
	ULONG KeOptions = 0;
	STATUS Status;
	IO_STATUS_BLOCK IoStatus;
	HANDLE hFile;

	RtlInitUnicodeString (&FileName, lpFileName);

	Status = KsCreateFile (&hFile,
		KeDesiredAccess, 
		&FileName,
		&IoStatus, 
		KeDisposition,
		KeOptions
		);

	return SUCCESS(Status) ? hFile : INVALID_HANDLE_VALUE;
}

HANDLE 
KEAPI
CreateFileA(
  PSTR lpFileName,
  ULONG dwDesiredAccess,
  ULONG dwShareMode,
  PVOID lpSecurityAttributes,
  DWORD dwCreationDisposition,
  DWORD dwFlagsAndAttributes,
  HANDLE hTemplateFile
)
{
	HANDLE hFile;
	PWSTR UnicodeFileName = (PWSTR) ExAllocateHeap (TRUE, strlen(lpFileName)*2+2);
	mbstowcs (UnicodeFileName, lpFileName, -1);

	hFile = CreateFileW (
		UnicodeFileName,
		dwDesiredAccess,
		dwShareMode,
		lpSecurityAttributes,
		dwCreationDisposition,
		dwFlagsAndAttributes,
		hTemplateFile
		);

	ExFreeHeap (UnicodeFileName);

	return hFile;
}

BOOLEAN
KEAPI
ReadFile(
  HANDLE hFile,
  PVOID lpBuffer,
  DWORD nNumberOfBytesToRead,
  PDWORD lpNumberOfBytesRead,
  LPOVERLAPPED lpOverlapped
)
{
	IO_STATUS_BLOCK IoStatus;
	STATUS Status;
	PFILE FileObject;
	BOOLEAN Return = FALSE;

	Status = ObReferenceObjectByHandle (hFile, KernelMode, FILE_READ_DATA, (PVOID*)&FileObject);

	if (SUCCESS(Status))
	{
		Status = IoReadFile (
			FileObject,
			lpBuffer,
			nNumberOfBytesToRead,
			(PLARGE_INTEGER) &lpOverlapped->Offset,
			0,
			&IoStatus
			);

		if (SUCCESS(Status))
		{
			Return = TRUE;
			*lpNumberOfBytesRead = IoStatus.Information;
		}

		ObDereferenceObject (FileObject);
	}

	return Return;
}


BOOLEAN
KEAPI
WriteFile(
  HANDLE hFile,
  PVOID lpBuffer,
  DWORD nNumberOfBytesToWrite,
  PDWORD lpNumberOfBytesWritten,
  LPOVERLAPPED lpOverlapped
)
{
	IO_STATUS_BLOCK IoStatus;
	STATUS Status;
	PFILE FileObject;
	BOOLEAN Return = FALSE;

	Status = ObReferenceObjectByHandle (hFile, KernelMode, FILE_READ_DATA, (PVOID*)&FileObject);

	if (SUCCESS(Status))
	{
		Status = IoWriteFile (
			FileObject,
			lpBuffer,
			nNumberOfBytesToWrite,
			(PLARGE_INTEGER) &lpOverlapped->Offset,
			0,
			&IoStatus
			);

		if (SUCCESS(Status))
		{
			Return = TRUE;
			*lpNumberOfBytesWritten = IoStatus.Information;
		}

		ObDereferenceObject (FileObject);
	}

	return Return;
}
