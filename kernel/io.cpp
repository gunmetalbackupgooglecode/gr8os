//
// FILE:		io.cpp
// CREATED:		16-Mar-2008  by Great
// PART:        IO
// ABSTRACT:
//			Input/Output support
//

#include "common.h"


VOID
KEAPI
IoInitSystem(
	)
/*++
	Initiaolize I/O subsystem
--*/
{
	//
	// Create 'Device' directory
	//

	UNICODE_STRING Name;
	STATUS Status;
	PVOID DeviceDirectory;
	PVOID DriverDirectory;

	RtlInitUnicodeString (&Name, L"Device");

	Status = ObCreateObject(
		&DeviceDirectory,
		sizeof(OBJECT_DIRECTORY),
		ObDirectoryObjectType,
		&Name,
		OB_OBJECT_OWNER_IO
		);
	if (!SUCCESS(Status))
	{
		KeBugCheck (IO_INITIALIZATION_FAILED,
					__LINE__,
					Status,
					0,
					0);
	}

	Status = ObInsertObject( NULL, DeviceDirectory );
	if (!SUCCESS(Status))
	{
		KeBugCheck (IO_INITIALIZATION_FAILED,
					__LINE__,
					Status,
					0,
					0);
	}

	//
	// Create 'Driver' directory
	//

	RtlInitUnicodeString (&Name, L"Driver");

	Status = ObCreateObject(
		&DriverDirectory,
		sizeof(OBJECT_DIRECTORY),
		ObDirectoryObjectType,
		&Name,
		OB_OBJECT_OWNER_IO
		);
	if (!SUCCESS(Status))
	{
		KeBugCheck (IO_INITIALIZATION_FAILED,
					__LINE__,
					Status,
					0,
					0);
	}

	Status = ObInsertObject (NULL, DriverDirectory);
	if (!SUCCESS(Status))
	{
		KeBugCheck (IO_INITIALIZATION_FAILED,
					__LINE__,
					Status,
					0,
					0);
	}
}