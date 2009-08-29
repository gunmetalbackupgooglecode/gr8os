/*
* NTFS.SYS New Technology File System (NTFS) driver source code.
*
* [C]oded for GR8OS by Great, 2008.
*
* It is a part of GR8OS project, see http://code.google.com/p/gr8os/ for details.
*/

#include "common.h"
#include "ntfs.h"
#include "driver.h"
#include "ide.h"

PDEVICE FsDeviceObject;
UNICODE_STRING FsDeviceName;

LOCKED_LIST FsNtfsVcbList;

BOOLEAN
IsNtfsFileStructure (
	IN PNTFS_VCB Vcb
	);

STATUS
KEAPI
NtfsCreateVcb(
	PNTFS_VOLUME_OBJECT Volume
	)
/*++
	Creates the volume parameter block for the NTFS file system
--*/
{
	PNTFS_VCB Vcb = &Volume->Vcb;

	Vcb->Vpb = Volume->DeviceObject.Vpb;

	KdPrint(("NTFS: NtfsCreateVcb: opening volume\n"));

	//
	// Load primary volume descriptor and other metadata
	//
	// 1. Open the volume
	//

	STATUS Status;
	UNICODE_STRING VolumeName;
	IO_STATUS_BLOCK IoStatus;

	Status = ObQueryObjectName (Vcb->Vpb->PhysicalDeviceObject, &VolumeName);

	if (!SUCCESS(Status))
	{
		return Status;
	}

	KdPrint(("NTFS: NtfsCreateVcb: Volume name %Z\n", &VolumeName));

	Status = IoCreateFile(
		&Vcb->RawFileObject,
		FILE_READ_DATA,
		&VolumeName,
		&IoStatus,
		0,
		0
		);

	RtlFreeUnicodeString (&VolumeName);

	if (!SUCCESS(Status))
	{
		return Status;
	}

	KdPrint(("NTFS: NtfsCreateVcb: Volume opened. Parsing\n"));

	if (!IsNtfsFileStructure (Vcb))
	{
		KdPrint(("NTFS: File system not recognized\n"));
		IoCloseFile (Vcb->RawFileObject);
		return STATUS_FILE_SYSTEM_NOT_RECOGNIZED;
	}

    KdPrint(("B POT MHE HOGI!!\n"));
	INT3

	return STATUS_NOT_SUPPORTED;
}

VOID
KEAPI
NtfsCloseVcb(
	PNTFS_VOLUME_OBJECT Volume
	)
/*++
	Closes the volume parameter block for the NTFS file system
--*/
{
	return;
}

STATUS
KEAPI
NtfsFsControl(
	PDEVICE DeviceObject,
	PIRP Irp
	)
/*++
	FileSystem Control requests handler.
	It handles two cases:

	1. The mount request pending. We should create unnamed device object,
	which will handle all file op
	eration requests, attach it to the real object
	(which we can get from IRP_STACK_LOCATION::Parameters.MountVolume.DeviceObject)
	and mount real device
	( mount operation performs the following:
	RealDevice->Vpb->FsDevice = MountedDeviceObject;
	MountedDeviceObject->Vpb = RealDevice->Vpb;
	)

	2. The dismount request pending. All operations are being cancelled:
	- find the unnamed device object attached to the real device object
	- detach it from the real device object
	- delete this unnamed object
	- dismout real object (RealDevice->Vpb->FsDevice = NULL)
--*/
{
	PIRP_STACK_LOCATION irpSl = IoGetCurrentIrpStackLocation (Irp);
	STATUS Status = STATUS_INVALID_FUNCTION;

	PDEVICE RealDevice = irpSl->Parameters.MountVolume.DeviceObject;

	if (DeviceObject == FsDeviceObject)
	{
		//
		// Handle this only if the request is going to the
		//  our FSD device object.
		//
		// Don't handle this request for the mounted volumes.
		//

		switch (Irp->MinorFunction)
		{
		case IRP_MN_MOUNT:
			{
				PNTFS_VOLUME_OBJECT MountedDeviceObject;

				KdPrint (("NTFS: Mounting device %08x\n", RealDevice));

				Status = IoCreateDevice (
					DeviceObject->DriverObject,
					sizeof(NTFS_VCB),
					NULL,
					DEVICE_TYPE_DISK_FILE_SYSTEM,
					(PDEVICE*) &MountedDeviceObject
					);

				if (!SUCCESS(Status))
				{
					KdPrint (("NTFS: Failed to create mounted volume: Status=%08x\n", Status));
					break;
				}

				MountedDeviceObject->Vcb.DriveLetter = 0;

				Status = IoAllocateMountDriveLetter (&MountedDeviceObject->Vcb.DriveLetter);

				if (SUCCESS(Status))
				{
					UNICODE_STRING LinkName;
					UNICODE_STRING DeviceName;

					RtlInitUnicodeString (&LinkName, L"\\Global\\ :");
					LinkName.Buffer[8] = MountedDeviceObject->Vcb.DriveLetter;

					Status = ObQueryObjectName (RealDevice, &DeviceName);

					if (SUCCESS(Status))
					{
						Status = ObCreateSymbolicLink (&LinkName, &DeviceName);

						if (!SUCCESS(Status))
						{
							KdPrint(("NTFS: ObCreateSymbolicLink failed for %S->%S with status %08x\n", LinkName.Buffer, DeviceName.Buffer, Status));
						}
						else
						{
							KdPrint(("NTFS: Symbolic link created: %S -> %S\n", LinkName.Buffer, DeviceName.Buffer));
						}

						RtlFreeUnicodeString( &DeviceName );
					}
					else
					{
						KdPrint(("NTFS: ObQueryObjectName failed for %08x with status %08x\n", DeviceObject, Status));
					}
				}
				else
				{
					KdPrint(("NTFS: IoAllocateMountDriveLetter failed with status %08x\n", Status));
				}


				//
				// Now MountedDeviceObject - new fs device, that will be attached to PDO
				//
				// First we should first mount PDO to this device object.
				//

				Status = IoMountVolume (RealDevice, &MountedDeviceObject->DeviceObject);

				if (!SUCCESS(Status))
				{
					KdPrint (("NTFS: Failed to mount volume: Status=%08x\n", Status));
					ObpDeleteObject (MountedDeviceObject);
					break;
				}

				KdPrint(("NTFS: Volume %08x mounted to %08x\n", RealDevice, MountedDeviceObject));

				//
				// Now, we can attach our MountedDeviceObject to the PDO
				//

				IoAttachDevice (&MountedDeviceObject->DeviceObject, RealDevice);

				KdPrint(("NTFS: Mounted device attached\n"));

				//
				// Okay, the volume is mounted successfully. Create the VCB
				//

				Status = NtfsCreateVcb (MountedDeviceObject);

				if (!SUCCESS(Status))
				{
					KdPrint(("NTFS: NtfsCreateVcb failed with status %08x\n", Status));

					IoDetachDevice (RealDevice);
					IoDismountVolume (RealDevice);
					ObpDeleteObject (MountedDeviceObject);
					break;
				}

				KdPrint (("NTFS: VCB created, volume mounted successfully\n"));
			}

			break;

		case IRP_MN_DISMOUNT:

			{
				//
				// The dismount should be performed.
				// Find & detach our unnamed device object
				//

				PNTFS_VOLUME_OBJECT MountedDeviceObject = (PNTFS_VOLUME_OBJECT) RealDevice->AttachedDevice;

				//
				// Close the VCB
				//

				NtfsCloseVcb (MountedDeviceObject);

				//
				// Detach device
				//

				IoDetachDevice (RealDevice);

				//
				// Delete mounted drive letter
				//

				if( MountedDeviceObject->Vcb.DriveLetter )
				{
					UNICODE_STRING LinkName;

					RtlInitUnicodeString (&LinkName, L"\\Global\\ :");
					LinkName.Buffer[8] = MountedDeviceObject->Vcb.DriveLetter;

					IoFreeMountedDriveLetter (MountedDeviceObject->Vcb.DriveLetter);
					ObDeleteObject (&LinkName);
				}


				//
				// Delete this unnamed object
				//

				ObpDeleteObject (MountedDeviceObject);

				//
				// Now, dismount the volume
				//

				Status = IoDismountVolume (RealDevice);

			}

			break;

		default:

			//
			// Pass down. Maybe it is IRP_MN_REQUEST_CACHED_PAGE or something else.
			//

			IoSkipCurrentIrpStackLocation (Irp);
			return IoCallDriver (RealDevice, Irp);
		}
	}

	COMPLETE_IRP (Irp, Status, 0);
}


STATUS
KEAPI
NtfsCreate(
	PDEVICE DeviceObject,
	PIRP Irp
	)
{
	STATUS Status = STATUS_INTERNAL_FAULT;

	if (DeviceObject == FsDeviceObject)
	{
		//
		// Allow all open operation for the FSD device object
		//

		Status = STATUS_SUCCESS;
	}
	else
	{
		//
		// Create request for some mounted volume.
		// First, retrieve pointer to it.
		//

		PDEVICE RealDevice = DeviceObject->Vpb->PhysicalDeviceObject;

		KdPrint(("NTFS open req: %S\n", Irp->FileObject->RelativeFileName.Buffer));

		//
		// Now, pass the request down if Path is empty.
		// This means that caller wants to open disk directly.
		//

		if (Irp->FileObject->RelativeFileName.Length == 0)
		{
			IoSkipCurrentIrpStackLocation (Irp);
			return IoCallDriver (RealDevice, Irp);
		}

		Status = STATUS_NOT_SUPPORTED;
	}

	COMPLETE_IRP (Irp, Status, 0);
}

STATUS
KEAPI
NtfsClose(
	PDEVICE DeviceObject,
	PIRP Irp
	)
{
	STATUS Status = STATUS_INTERNAL_FAULT;

	if (DeviceObject == FsDeviceObject)
	{
		//
		// Allow all close operation for the FSD device object
		//

		Status = STATUS_SUCCESS;
	}
	else
	{
		//
		// close request for some mounted volume.
		// First, retrieve pointer to it.
		//

		PDEVICE RealDevice = DeviceObject->Vpb->PhysicalDeviceObject;

		KdPrint(("NTFS close req: %S\n", Irp->FileObject->RelativeFileName.Buffer));

		//
		// Now, pass the request down if Path is empty.
		// This means that caller wants to close disk directly.
		//

		if (Irp->FileObject->RelativeFileName.Length == 0)
		{
			IoSkipCurrentIrpStackLocation (Irp);
			return IoCallDriver (RealDevice, Irp);
		}
		
		Status = STATUS_NOT_SUPPORTED;
	}

	COMPLETE_IRP (Irp, Status, 0);
}

STATUS
KEAPI
NtfsRead(
	PDEVICE DeviceObject,
	PIRP Irp
	)
{
	STATUS Status = STATUS_INTERNAL_FAULT;
	ULONG Read = 0;

	if (DeviceObject == FsDeviceObject)
	{
		//
		// Forbid all open/close operations for the FSD device object
		//

		Status = STATUS_INVALID_FUNCTION;
	}
	else
	{
		PDEVICE RealDevice = DeviceObject->Vpb->PhysicalDeviceObject;

		KdPrint(("NTFS read req: %S\n", Irp->FileObject->RelativeFileName.Buffer));

		if (Irp->FileObject->RelativeFileName.Length == 0)
		{
			//
			// Someone wants to read the disk directly.
			// Pass IRP down.
			//

			IoSkipCurrentIrpStackLocation (Irp);
			return IoCallDriver (RealDevice, Irp);
		}

		Status = STATUS_NOT_SUPPORTED;
	}

	COMPLETE_IRP (Irp, Status, Read);
}

STATUS
KEAPI
NtfsIoControl(
	PDEVICE DeviceObject,
	PIRP Irp
	)
{
	COMPLETE_IRP (Irp, STATUS_NOT_SUPPORTED, 0); 
}

VOID
NtfsInitialize(
	);

// Driver entry point
STATUS DriverEntry(IN PDRIVER DriverObject)
{
	KdPrint(("NTFS: In DriverEntry()\n"));
	InitializeLockedList (&FsNtfsVcbList);

	NtfsInitialize ();

	STATUS Status;	
	RtlInitUnicodeString (&FsDeviceName, L"\\FileSystem\\Ntfs");

	Status = IoCreateDevice (
		DriverObject,
		0,
		&FsDeviceName,
		DEVICE_TYPE_DISK_FILE_SYSTEM,
		&FsDeviceObject
		);

	if (!SUCCESS(Status))
	{
		KdPrint(("NTFS: Failed to create FSD device object: %08x\n", Status));
		return Status;
	}

	IoRegisterFileSystem (FsDeviceObject);

	DriverObject->IrpHandlers [IRP_FSCTL]  = NtfsFsControl;
	DriverObject->IrpHandlers [IRP_CREATE] = NtfsCreate;
	DriverObject->IrpHandlers [IRP_CLOSE]  = NtfsClose;
	DriverObject->IrpHandlers [IRP_READ]   = NtfsRead;
	DriverObject->IrpHandlers [IRP_IOCTL]  = NtfsIoControl;

	KdPrint(("NTFS: Initialized successfully\n"));

	return STATUS_SUCCESS;
}
