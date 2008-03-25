//
// FILE:		io.cpp
// CREATED:		16-Mar-2008  by Great
// PART:        IO
// ABSTRACT:
//			Input/Output support
//

#include "common.h"

POBJECT_DIRECTORY IoDeviceDirectory;
POBJECT_DIRECTORY IoDriverDirectory;
POBJECT_DIRECTORY IoFileSystemDirectory;


POBJECT_TYPE IoDeviceObjectType;
POBJECT_TYPE IoDriverObjectType;
POBJECT_TYPE IoFileObjectType;

PMUTEX IoDatabaseLock;

STATUS
KEAPI
IopParseDevice(
	IN POBJECT_HEADER Object,
	IN PUNICODE_STRING FullObjectPath,
	IN PUNICODE_STRING RemainingPath,
	OUT PUNICODE_STRING ReparsePath
	)
/*++
	See remarks to the PPARSE_OBJECT_ROUTINE typedef for the general explanations
	 of the type of such routines.

    Specifically this function always stops parsing with success.
	The rest of the path goes to the IRP_CREATE later
--*/
{
	KdPrint(("IopParseDevice\n"));
	return STATUS_FINISH_PARSING;
}

VOID
KEAPI
IopDeleteDevice(
	IN POBJECT_HEADER Object
	)
/*++
	See remarks to the PDELETE_OBJECT_ROUTINE typedef for the general explanations
	 of the type of such routines.

	This routine is called when the device object is being deleted.
	We should dereference the corresponding driver object
--*/
{
	PDEVICE DeviceObject = OBJECT_HEADER_TO_OBJECT (Object, DEVICE);

	KdPrint(("IopDeleteDevice\n"));

	ObDereferenceObject (DeviceObject->DriverObject);
}


VOID
KEAPI
IopDeleteFile(
	IN POBJECT_HEADER Object
	)
/*++
	See remarks to the PDELETE_OBJECT_ROUTINE typedef for the general explanations
	 of the type of such routines.

	This routine is called when the file object is being deleted.
	We should dereference the corresponding device and free some resources
--*/
{
	PFILE FileObject = OBJECT_HEADER_TO_OBJECT (Object, FILE);

	KdPrint(("IopDeleteFile\n"));

	if (FileObject->Synchronize == FALSE)
	{
		//
		// Purge cache and free cache map
		//

		CcFreeCacheMap (FileObject);
	}

	ObDereferenceObject (FileObject->DeviceObject);
	ExFreeHeap (FileObject->FileName.Buffer);
}

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

	RtlInitUnicodeString (&Name, L"Device");

	Status = ObCreateDirectory( &IoDeviceDirectory, &Name, OB_OBJECT_OWNER_IO, NULL);
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

	Status = ObCreateDirectory( &IoDriverDirectory, &Name, OB_OBJECT_OWNER_IO, NULL);
	if (!SUCCESS(Status))
	{
		KeBugCheck (IO_INITIALIZATION_FAILED,
					__LINE__,
					Status,
					0,
					0);
	}

	//
	// Create 'FileSystem' directory
	//

	RtlInitUnicodeString (&Name, L"FileSystem");

	Status = ObCreateDirectory( &IoFileSystemDirectory, &Name, OB_OBJECT_OWNER_IO, NULL);
	if (!SUCCESS(Status))
	{
		KeBugCheck (IO_INITIALIZATION_FAILED,
					__LINE__,
					Status,
					0,
					0);
	}

	//
	// Create device object type
	//

	UNICODE_STRING TypeName;
	RtlInitUnicodeString (&TypeName, L"Device");

	Status = ObCreateObjectType (
		&IoDeviceObjectType, 
		&TypeName,
		NULL,
		IopParseDevice,
		NULL,
		IopDeleteDevice,
		NULL,
		OB_OBJECT_OWNER_IO);

	if (!SUCCESS(Status))
	{
		KeBugCheck (IO_INITIALIZATION_FAILED,
					__LINE__,
					Status,
					0,
					0
					);
	}

	//
	// Create driver object type
	//

	RtlInitUnicodeString (&TypeName, L"Driver");

	Status = ObCreateObjectType (
		&IoDriverObjectType, 
		&TypeName,
		NULL,
		NULL,
		NULL,
		NULL,
		NULL,
		OB_OBJECT_OWNER_IO);

	if (!SUCCESS(Status))
	{
		KeBugCheck (IO_INITIALIZATION_FAILED,
					__LINE__,
					Status,
					0,
					0
					);
	}

	//
	// Create file object type
	//

	RtlInitUnicodeString (&TypeName, L"File");

	Status = ObCreateObjectType (
		&IoFileObjectType, 
		&TypeName,
		NULL,
		NULL,
		NULL,
		IopDeleteFile,
		NULL,
		OB_OBJECT_OWNER_IO);

	if (!SUCCESS(Status))
	{
		KeBugCheck (IO_INITIALIZATION_FAILED,
					__LINE__,
					Status,
					0,
					0
					);
	}

	//
	// Load FD driver
	//

	PDRIVER FdDriver;
	UNICODE_STRING FdDriverName;

	RtlInitUnicodeString (&FdDriverName, L"\\Driver\\floppy" );

	Status = IopCreateDriverObject ( 0, 0, DRV_FLAGS_BUILTIN|DRV_FLAGS_CRITICAL, FdDriverEntry, &FdDriverName, &FdDriver );
	if (!SUCCESS(Status))
	{
		KeBugCheck (IO_INITIALIZATION_FAILED,
					__LINE__,
					Status,
					0,
					0
					);
	}
}


KESYSAPI
PIRP
KEAPI
IoAllocateIrp(
	IN UCHAR StackSize
	)
/*++
	Allocate an IRP
--*/
{
	PIRP Irp;
	ULONG FullIrpSize = sizeof(IRP) + (StackSize-1)*sizeof(IRP_STACK_LOCATION);

	Irp = (PIRP) ExAllocateHeap (TRUE, FullIrpSize);
	memset (Irp, 0, sizeof(IRP));
	Irp->Size = FullIrpSize;
	Irp->StackSize = StackSize;
	InitializeListHead (&Irp->ThreadListEntry);

	return Irp;
}

KESYSAPI
STATUS
KEAPI
IoCallDriver(
	IN PDEVICE Device,
	IN PIRP Irp
	)
/*++
	--
--*/
{
	PDRIVER Driver;
	PIRP_STACK_LOCATION irpSl;
	STATUS Status;

	if (Irp->CurrentLocation >= Irp->StackSize)
	{
		KeBugCheck (IO_NO_MORE_IRP_STACK_LOCATIONS,
					(ULONG)Irp,
					(ULONG)Device,
					Irp->CurrentLocation,
					Irp->StackSize);
	}

	Driver = Device->DriverObject;

	irpSl = IoGetCurrentIrpStackLocation (Irp);
	irpSl->DeviceObject = Device;

	Status = (STATUS) (Driver->IrpHandlers[Irp->MajorFunction]) (Device, Irp);

	return Status;
}

KESYSAPI
PIRP
KEAPI
IoBuildDeviceIoControlRequest(
	IN PDEVICE DeviceObject,
	OUT PIO_STATUS_BLOCK IoStatus,
	IN PROCESSOR_MODE RequestorMode,
	IN ULONG IoControlCode,
	IN PVOID InputBuffer,
	IN ULONG InputBufferSize,
	IN PVOID OutputBuffer,
	IN ULONG OutputBufferSize
	)
/*++
	Build device IO control request IRP
--*/
{
	PIRP Irp = IoAllocateIrp (DeviceObject->StackSize);
	if (Irp == NULL)
		return Irp;
	
	//
	// BUGBUG: select i/o type from device flags
	//

	Irp->MajorFunction = IRP_IOCTL;
	Irp->UserBuffer = InputBuffer;
	Irp->Flags |= IRP_FLAGS_BUFFERED_IO | IRP_FLAGS_DEALLOCATE_BUFFER | IRP_FLAGS_INPUT_OPERATION;
	Irp->SystemBuffer = ExAllocateHeap (FALSE, max(InputBufferSize, OutputBufferSize));
	memcpy (Irp->SystemBuffer, InputBuffer, InputBufferSize);
	Irp->BufferLength = InputBufferSize;
	Irp->UserIosb = IoStatus;
	Irp->RequestorMode = RequestorMode;
	Irp->CallerThread = PsGetCurrentThread();
	Irp->FileObject = NULL;
	Irp->CurrentLocation = 0;
	Irp->CurrentStackLocation = &Irp->IrpStackLocations[Irp->CurrentLocation];
	Irp->CurrentStackLocation->Parameters.IoCtl.IoControlCode = IoControlCode;
	Irp->CurrentStackLocation->Parameters.IoCtl.OutputUserBuffer = OutputBuffer;
	Irp->CurrentStackLocation->Parameters.IoCtl.OutputBufferLength = OutputBufferSize;

	for (int i=1; i<Irp->StackSize; i++)
	{
		Irp->IrpStackLocations[i] = Irp->IrpStackLocations[i-1];
	}

	IopQueueThreadIrp (Irp);
	
	return Irp;
}

KESYSAPI
PIRP
KEAPI
IoBuildDeviceRequest(
	IN PDEVICE DeviceObject,
	IN ULONG MajorFunction,
	OUT PIO_STATUS_BLOCK IoStatus,
	IN PROCESSOR_MODE RequestorMode,
	IN OUT PVOID Buffer,
	IN ULONG BufferSize,
	OUT PULONG ReturnedLength UNIMPLEMENTED
	)
/*++
	This function builds IRP for create/read/write/close requests
--*/
{
	PIRP Irp = IoAllocateIrp (DeviceObject->StackSize);
	if (Irp == NULL)
		return Irp;

	//
	// BUGBUG: select i/o type from device flags
	//

	Irp->MajorFunction = MajorFunction;
	Irp->UserBuffer = Buffer;
	Irp->Flags |= IRP_FLAGS_BUFFERED_IO | IRP_FLAGS_DEALLOCATE_BUFFER;
	Irp->SystemBuffer = ExAllocateHeap (FALSE, BufferSize);
	Irp->BufferLength = BufferSize;
	Irp->UserIosb = IoStatus;
	Irp->RequestorMode = RequestorMode;
	Irp->CallerThread = PsGetCurrentThread();
	Irp->FileObject = NULL;
	Irp->CurrentLocation = 0;
	Irp->CurrentStackLocation = &Irp->IrpStackLocations[Irp->CurrentLocation];

	IopQueueThreadIrp (Irp);
	
	return Irp;
}


KESYSAPI
STATUS
KEAPI
IoCreateFile(
	OUT PFILE *FileObject,
	IN ULONG DesiredAccess UNIMPLEMENTED,
	IN PUNICODE_STRING FileName,
	OUT PIO_STATUS_BLOCK IoStatus,
	IN ULONG Disposition UNIMPLEMENTED,
	IN ULONG Options UNIMPLEMENTED
	)
/*++
	This function creates a file object and creates/opens a file/device or another i/o object
--*/
{
	PFILE File;
	PDEVICE thisDeviceObject, DeviceObject;
	STATUS Status;

	Status = ObCreateObject (
		(PVOID*) &File,
		sizeof(FILE),
		IoFileObjectType,
		FileName,
		OB_OBJECT_OWNER_IO
		);

	if (!SUCCESS(Status))
	{
		return Status;
	}

	Status = ObReferenceObjectByName (
		FileName,
		IoDeviceObjectType,
		KernelMode,
		DesiredAccess,
		(PVOID*) &thisDeviceObject
		);

	if( !SUCCESS(Status) )
	{
		ObpDeleteObjectInternal (File);
		return Status;
	}

	DeviceObject = IoGetAttachedDevice (thisDeviceObject);

	ObReferenceObject (DeviceObject);
	ObDereferenceObject (thisDeviceObject);
	
	File->DeviceObject = DeviceObject;
	File->FsContext = NULL;
	File->FsContext2 = NULL;
	
	RtlDuplicateUnicodeString( FileName, &File->FileName );

	File->CurrentOffset.QuadPart = 0;
	File->FinalStatus = STATUS_SUCCESS;
	KeInitializeEvent (&File->Event, SynchronizationEvent, FALSE);
	File->CacheMap = NULL;
	File->DesiredAccess = DesiredAccess;
	File->ReadAccess = !!(DesiredAccess & FILE_READ_DATA);
	File->WriteAccess = !!(DesiredAccess & FILE_WRITE_DATA);
	File->DeleteAccess = !!(DesiredAccess & FILE_DELETE);
	File->Synchronize = !!(DesiredAccess & SYNCHRONIZE);

	if (File->Synchronize == FALSE)
	{
		//BUGBUG: Fixme - pass there VPB::ClusterSize
		CcInitializeFileCaching (File, FD_SECTOR_SIZE);
	}

	PIRP Irp = IoBuildDeviceRequest (
		DeviceObject,
		IRP_CREATE,
		IoStatus,
		KernelMode,
		NULL,
		0,
		NULL
		);

	if (!Irp)
	{
		ObpDeleteObjectInternal (File);
		ObDereferenceObject (DeviceObject);
		return STATUS_INSUFFICIENT_RESOURCES;
	}

	Irp->FileObject = File;
	Irp->CurrentStackLocation->DeviceObject = DeviceObject;
	Irp->CurrentStackLocation->Parameters.Create.Path = *FileName;
	Irp->CurrentStackLocation->Parameters.Create.DesiredAccess = DesiredAccess;

	for (int i=1; i<Irp->StackSize; i++)
	{
		Irp->IrpStackLocations[i] = Irp->IrpStackLocations[i-1];
	}

	Status = IoCallDriver (DeviceObject, Irp);

	if (SUCCESS(Status) && !SUCCESS(Irp->IoStatus.Status))
	{
		Status = Irp->IoStatus.Status;
	}

	if (!SUCCESS(Status))
	{
		ObpDeleteObjectInternal (File);
		ObDereferenceObject (DeviceObject);
		return Status;
	}

	*FileObject = File;

	return STATUS_SUCCESS;
}

KESYSAPI
STATUS
KEAPI
IoCloseFile(
	IN PFILE FileObject
	)
/*++
	This function closes the file opened by IoCreateFile
--*/
{
	IO_STATUS_BLOCK IoStatus;
	STATUS Status;

	PIRP Irp = IoBuildDeviceRequest (
		FileObject->DeviceObject,
		IRP_CLOSE,
		&IoStatus,
		KeGetRequestorMode(),
		NULL,
		0,
		NULL
		);

	Irp->FileObject = FileObject;

	Status = IoCallDriver (FileObject->DeviceObject, Irp);

	//
	// FileObjectType deletion routine will free all resources and dereference the
	//  coppersponding device object
	//

	if (SUCCESS(Status))
	{
		Status = ObpDeleteObject (FileObject);
	}

	return Status;
}


KESYSAPI
STATUS
KEAPI
IoReadFile(
	IN PFILE FileObject,
	OUT PVOID Buffer,
	IN ULONG Length,
	IN PLARGE_INTEGER FileOffset OPTIONAL,
	OUT PIO_STATUS_BLOCK IoStatus
	)
/*++
	Perform the read operation on the specified file object
--*/
{
	STATUS Status;
	PIRP Irp;

	Irp = IoBuildDeviceRequest (
		FileObject->DeviceObject,
		IRP_READ,
		IoStatus,
		KeGetRequestorMode(),
		Buffer,
		Length,
		NULL
		);

	if (!Irp)
	{
		return STATUS_INSUFFICIENT_RESOURCES;
	}

	Irp->Flags |= IRP_FLAGS_INPUT_OPERATION;
	Irp->FileObject = FileObject;
	Irp->CurrentStackLocation->DeviceObject = FileObject->DeviceObject;

	if (ARGUMENT_PRESENT(FileOffset))
	{
		Irp->CurrentStackLocation->Parameters.ReadWrite.Offset = *FileOffset;
	}
	else
	{
		Irp->CurrentStackLocation->Parameters.ReadWrite.Offset = FileObject->CurrentOffset;
	}

	for (int i=1; i<Irp->StackSize; i++)
	{
		Irp->IrpStackLocations[i] = Irp->IrpStackLocations[i-1];
	}

	Status = IoCallDriver (FileObject->DeviceObject, Irp);

	if (SUCCESS(Status) && !SUCCESS(Irp->IoStatus.Status))
	{
		Status = Irp->IoStatus.Status;
	}

	return Status;
}


KESYSAPI
STATUS
KEAPI
IoWriteFile(
	IN PFILE FileObject,
	IN PVOID Buffer,
	IN ULONG Length,
	IN PLARGE_INTEGER FileOffset OPTIONAL,
	OUT PIO_STATUS_BLOCK IoStatus
	)
/*++
	Perform the write operation on the specified file object
--*/
{
	STATUS Status;
	PIRP Irp;

	Irp = IoBuildDeviceRequest (
		FileObject->DeviceObject,
		IRP_WRITE,
		IoStatus,
		KeGetRequestorMode(),
		Buffer,
		Length,
		NULL
		);

	if (!Irp)
	{
		return STATUS_INSUFFICIENT_RESOURCES;
	}

	Irp->FileObject = FileObject;
	Irp->CurrentStackLocation->DeviceObject = FileObject->DeviceObject;

	memcpy (Irp->SystemBuffer, Buffer, Length);

	if (ARGUMENT_PRESENT(FileOffset))
	{
		Irp->CurrentStackLocation->Parameters.ReadWrite.Offset = *FileOffset;
	}
	else
	{
		Irp->CurrentStackLocation->Parameters.ReadWrite.Offset = FileObject->CurrentOffset;
	}

	for (int i=1; i<Irp->StackSize; i++)
	{
		Irp->IrpStackLocations[i] = Irp->IrpStackLocations[i-1];
	}

	Status = IoCallDriver (FileObject->DeviceObject, Irp);

	if (SUCCESS(Status) && !SUCCESS(Irp->IoStatus.Status))
	{
		Status = Irp->IoStatus.Status;
	}

	return Status;
}



KESYSAPI
STATUS
KEAPI
IoDeviceIoControlFile(
	IN PFILE FileObject,
	IN ULONG IoControlCode,
	IN PVOID InputBuffer,
	IN ULONG InputBufferLength,
	OUT PVOID OutputBuffer,
	IN ULONG OutputBufferLength,
	OUT PIO_STATUS_BLOCK IoStatus
	)
/*++
	This function send IOCTL request to the appropriate driver.
--*/
{
	STATUS Status;

	PIRP Irp = IoBuildDeviceIoControlRequest(
		FileObject->DeviceObject,
		IoStatus,
		KeGetRequestorMode(),
		IoControlCode,
		InputBuffer,
		InputBufferLength,
		OutputBuffer,
		OutputBufferLength
		);

	if (!Irp)
	{
		return STATUS_INSUFFICIENT_RESOURCES;
	}

	Irp->FileObject = FileObject;

	Status = IoCallDriver (FileObject->DeviceObject, Irp);

	if (SUCCESS(Status) && !SUCCESS(Irp->IoStatus.Status))
	{
		Status = Irp->IoStatus.Status;
	}

	return Status;
}

KESYSAPI
STATUS
KEAPI
IoCreateDevice(
	IN PDRIVER DriverObject,
	IN PUNICODE_STRING DeviceName OPTIONAL,
	IN ULONG DeviceType,
	OUT PDEVICE *DeviceObject
	)
{
	STATUS Status;
	UNICODE_STRING RelativeDeviceName;
	
	PWSTR rel = wcsrchr(DeviceName->Buffer+1, L'\\');
	RtlInitUnicodeString (&RelativeDeviceName, rel+1);

	Status = ObCreateObject(
		(PVOID*)DeviceObject, 
		sizeof(DEVICE),
		IoDeviceObjectType,
		&RelativeDeviceName,
		OB_OBJECT_OWNER_IO
		);
	if (!SUCCESS(Status))
		return Status;

	if (ARGUMENT_PRESENT(DeviceName))
	{
		WCHAR *wDirectoryName = (WCHAR*) ExAllocateHeap(TRUE, 
			(ULONG)rel-(ULONG)DeviceName->Buffer+2);
		wcssubstr (DeviceName->Buffer, 0, ((ULONG)rel-(ULONG)DeviceName->Buffer)/2, wDirectoryName);

		POBJECT_DIRECTORY Directory;
		UNICODE_STRING DirectoryName;
		RtlInitUnicodeString (&DirectoryName, wDirectoryName);

		Status = ObReferenceObjectByName (
			&DirectoryName,
			ObDirectoryObjectType,
			KernelMode,
			0,
			(PVOID*) &Directory
			);

		ExFreeHeap (wDirectoryName);

		if (!SUCCESS(Status))
		{
			ObpDeleteObjectInternal (*DeviceObject);
			return Status;
		}

		Status = ObInsertObject (Directory, *DeviceObject);

		ObDereferenceObject (Directory);

		if (!SUCCESS(Status))
		{
			ObpDeleteObjectInternal (*DeviceObject);
			return Status;
		}
	}

	(*DeviceObject)->DriverObject = DriverObject;
	(*DeviceObject)->DeviceType = DeviceType;
	(*DeviceObject)->StackSize = 1;

	ObReferenceObject (DriverObject);	// It will be dereferenced in device deletion routine

	return Status;
}

KESYSAPI
PDEVICE
KEAPI
IoGetNextDevice(
	IN PDEVICE DeviceObject
	)
/*++
	Retrieves the pointer to the next lower device in the device stack
	 or NULL if there is no device lower than current.
--*/
{
	return DeviceObject->NextDevice;
}

KESYSAPI
PDEVICE
KEAPI
IoGetAttachedDevice(
	IN PDEVICE DeviceObject
	)
/*++
	Retrieves pointer to the highest device in the device stack
--*/
{
	PDEVICE Prev = DeviceObject->AttachedDevice;
	if (Prev == NULL)
		return DeviceObject;

	while (Prev->AttachedDevice)
	{
		Prev = Prev->AttachedDevice;
	}

	return Prev;	
}


KESYSAPI
VOID
KEAPI
IoAttachDevice(
	IN PDEVICE SourceDevice,
	IN PDEVICE TargetDevice
	)
/*++
	Attach device to the target device stack
--*/
{
	ExAcquireMutex (IoDatabaseLock);
	
	SourceDevice->NextDevice = TargetDevice;
	SourceDevice->StackSize = TargetDevice->StackSize + 1;
	TargetDevice->AttachedDevice = SourceDevice;
	
	ExReleaseMutex (IoDatabaseLock);
}

KESYSAPI
VOID
KEAPI
IoDetachDevice(
	IN PDEVICE TargetDevice
	)
/*++
	Detach device from the target device stack
--*/
{
	ExAcquireMutex (IoDatabaseLock);
	
	PDEVICE AttachedDevice = TargetDevice->AttachedDevice;
	TargetDevice->AttachedDevice = NULL;
	AttachedDevice->NextDevice = NULL;

	ExReleaseMutex (IoDatabaseLock);
}

KESYSAPI
VOID
KEAPI
IoDeleteDevice(
	IN PUNICODE_STRING DeviceName
	)
/*++
	This function deletes the device object
--*/
{
	ObDeleteObject (DeviceName);
}

STATUS
KEAPI 
IopInvalidDeviceRequest(
	PDEVICE DeviceObject,
	PIRP Irp
	)
/*++
	This is the default handler for all IRP requests
--*/
{
	Irp->IoStatus.Status = STATUS_INVALID_FUNCTION;
	IoCompleteRequest (Irp, 0);
	return STATUS_INVALID_FUNCTION;
}

STATUS
KEAPI
IopCreateDriverObject(
	IN PVOID DriverStart,
	IN PVOID DriverEnd,
	IN ULONG Flags,
	IN PDRIVER_ENTRY DriverEntry,
	IN PUNICODE_STRING DriverName,
	OUT PDRIVER *DriverObject
	)
/*++
	Create driver object for the built-in driver or the driver being loaded
--*/
{
	PDRIVER Driver;
	STATUS Status;

	Status = ObCreateObject (
		(PVOID*) &Driver,
		sizeof(DRIVER),
		IoDriverObjectType,
		DriverName,
		OB_OBJECT_OWNER_IO
		);

	if (!SUCCESS(Status))
	{
		return Status;
	}

	Driver->DriverStart = DriverStart;
	Driver->DriverEnd = DriverEnd;
	Driver->DriverEntry = DriverEntry;
	Driver->DriverUnload = NULL;
	Driver->Flags = Flags;
	
	for (int i=0; i<MAX_IRP; i++)
		Driver->IrpHandlers[i] = IopInvalidDeviceRequest;

	*DriverObject = Driver;

	Status = ObInsertObject (IoDriverDirectory, Driver);

	if (!SUCCESS(Status))
	{
		ObpDeleteObjectInternal (Driver);
		return Status;
	}

	Status = DriverEntry (Driver);
	if (!SUCCESS(Status))
	{
		ObpDeleteObjectInternal (Driver);
		return Status;
	}

	return STATUS_SUCCESS;
}

VOID
KEAPI
IopQueueThreadIrp(
	IN PIRP Irp
	)
{
	PTHREAD Thread = Irp->CallerThread;

	ExAcquireMutex (&Thread->IrpListLock);
	InsertTailList (&Thread->IrpList, &Irp->ThreadListEntry);
	ExReleaseMutex (&Thread->IrpListLock);
}

VOID
KEAPI
IopDequeueThreadIrp(
	IN PIRP Irp
	)
{
	PTHREAD Thread = Irp->CallerThread;

	ExAcquireMutex (&Thread->IrpListLock);
	RemoveEntryList (&Irp->ThreadListEntry);
	ExReleaseMutex (&Thread->IrpListLock);
	InitializeListHead (&Irp->ThreadListEntry);
}

KESYSAPI
VOID
KEAPI
IoCompleteRequest(
	IN PIRP Irp,
	IN ULONG QuantumIncrement
	)
{
	if (Irp->Size < sizeof(IRP) ||
		Irp->CurrentLocation > Irp->StackSize ||
		Irp->CurrentStackLocation > &Irp->IrpStackLocations[Irp->StackSize]
		)
	{
		//
		// IRP has already been completed.
		//

		KeBugCheck (IO_MULTIPLE_COMPLETE_REQUESTS,
					(ULONG)Irp,
					__LINE__,
					0,
					0);
	}

	if (Irp->IoStatus.Status == STATUS_PENDING)
	{
		KeBugCheck (IO_IRP_COMPLETION_WITH_PENDING,
					(ULONG) Irp,
					__LINE__,
					0,
					0);
	}

	if ( (Irp->Flags & (IRP_FLAGS_BUFFERED_IO|IRP_FLAGS_INPUT_OPERATION)) &&
		 SUCCESS(Irp->IoStatus.Status) )
	{
		//
		// Copy system buffer to user buffer
		//

		memcpy (
			Irp->UserBuffer,
			Irp->SystemBuffer,
			Irp->BufferLength
			);
	}

	if (Irp->Flags & IRP_FLAGS_DEALLOCATE_BUFFER)
	{
		ExFreeHeap (Irp->SystemBuffer);
	}

	Irp->Flags &= ~(IRP_FLAGS_BUFFERED_IO|IRP_FLAGS_DEALLOCATE_BUFFER);

	//
	// Copy status block to user block
	//

	*Irp->UserIosb = Irp->IoStatus;

	//
	// Set user event or file event
	//

	if (Irp->FileObject)
	{
		Irp->FileObject->FinalStatus = Irp->IoStatus.Status;
	}

	if (Irp->UserEvent)
	{
		KePulseEvent (Irp->UserEvent, (USHORT)QuantumIncrement);
	}
	
	if (Irp->FileObject)
	{
		KePulseEvent (&Irp->FileObject->Event, (USHORT)QuantumIncrement);
	}

	//
	// Dequeue IRP from the thread
	//

	IopDequeueThreadIrp (Irp);

	//
	// Free the IRP
	//

	ExFreeHeap (Irp);
}
