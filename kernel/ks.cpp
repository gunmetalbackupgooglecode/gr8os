//
// FILE:		ks.cpp
// CREATED:		17-Feb-2008  by Great
// PART:        KS (kernel services)
// ABSTRACT:
//			Exported kernel services
//

#include "common.h"

KESYSAPI
STATUS
KEAPI
KsClose(
	IN HANDLE hObject
	)
{
	return ObClose (hObject);
}

KESYSAPI
STATUS
KEAPI
KsCreateFile(
	OUT PHANDLE FileHandle,
	IN ULONG DesiredAccess UNIMPLEMENTED,
	IN PUNICODE_STRING FileName,
	OUT PIO_STATUS_BLOCK IoStatus,
	IN ULONG Disposition UNIMPLEMENTED,
	IN ULONG Options UNIMPLEMENTED
	)
{
	PFILE FileObject;
	STATUS Status;

	Status = IoCreateFile (
		&FileObject,
		DesiredAccess,
		FileName,
		IoStatus,
		Disposition,
		Options
		);
	
	if (SUCCESS(Status))
	{
		*FileHandle = ObpCreateHandle (FileObject, DesiredAccess);
		if (!*FileHandle)
			Status = STATUS_UNSUCCESSFUL;

		ObDereferenceObject (FileObject);
	}

	return Status;
}


KESYSAPI
STATUS
KEAPI
KsCreateEvent (
	IN PHANDLE EventHandle,
	IN UCHAR Type,
	IN BOOLEAN InitialState,
	IN PUNICODE_STRING EventName
	)
{
	PEVENT Event;
	STATUS Status;

	Status = ObCreateObject (
		(PVOID*) &Event,
		sizeof (EVENT),
		KeEventObjectType,
		EventName,
		OB_OBJECT_OWNER_KS
		);

	if (EventName && SUCCESS(Status))
	{
		Status = ObInsertObject (KeBaseNamedObjectsDirectory, Event);
	}

	if (SUCCESS(Status))
	{
		KeInitializeEvent (Event, Type, InitialState);

		*EventHandle = ObpCreateHandle (Event, EVENT_ALL_ACCESS);

		if (!*EventHandle)
			Status = STATUS_UNSUCCESSFUL;

		ObDereferenceObject (Event);
	}

	return Status;
}

KESYSAPI
BOOLEAN
KEAPI
KsSetEvent (
	IN HANDLE hEvent,
	IN USHORT QuantumIncrement
	)
{
	PEVENT Event;
	STATUS Status;
	BOOLEAN State = 0;

	Status = ObReferenceObjectByHandle (hEvent, KernelMode, EVENT_SET_STATE, (PVOID*)&Event);

	if (SUCCESS(Status))
	{
		State = KeSetEvent (Event, QuantumIncrement);

		ObDereferenceObject (Event);
	}

	return State;
}