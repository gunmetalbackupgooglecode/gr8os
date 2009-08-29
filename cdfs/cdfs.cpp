/*
* CDFS.SYS Compact Disk File System driver source code.
*
* [C]oded for GR8OS by Great, 2008.
*
* It is a part of GR8OS project, see http://code.google.com/p/gr8os/ for details.
*/

//typedef unsigned short wchar_t;
#include "common.h"
#include "iso9660.h"
#include "cdfs.h"
#include "ide.h"

PDEVICE FsDeviceObject;
UNICODE_STRING FsDeviceName;

LOCKED_LIST FsCdfsVcbList;

#define CdPrint(x) KdPrint(x)

enum CDFS_VOLUME_DESCRIPTOR_TYPE
{
	VolumeDescriptorBootRecord,
	PrimaryVolumeDescriptor,
	SupplementaryVolumeDescriptor,
	VolumePartitionDescriptor,
	VolumeDescriptorSetTerminator = 255
};

char *CdfsVdTypes[] = {
	"Volume Descriptor Boot Record",
	"Primary Volume Descriptor",
	"Supplementary Volume Descriptor",
	"Volume Partition Descriptor"
};

STATUS
KEAPI
CdfsCreateVcb(
	PCDFS_VOLUME_OBJECT Volume
	)
/*++
	Creates the volume parameter block for the CDFS file system
--*/
{
	PCDFS_VCB Vcb = &Volume->Vcb;

	Vcb->Vpb = Volume->DeviceObject.Vpb;

	CdPrint(("CDFS: CdfsCreateVcb: opening volume\n"));

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

	CdPrint(("CDFS: CdfsCreateVcb: Volume name %Z\n", &VolumeName));

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

	CdPrint(("CDFS: CdfsCreateVcb: Volume opened. Parsing\n"));

	//
	// Read descriptors.
	//

	PCDFS_PRIMARY_VOLUME_DESCRIPTOR descriptor;
	//descriptor = (PCDFS_PRIMARY_VOLUME_DESCRIPTOR) ExAllocateHeap (TRUE, sizeof(CDFS_PACKED_DIRECTORY_RECORD));
	descriptor = (PCDFS_PRIMARY_VOLUME_DESCRIPTOR) MmAllocatePage ();

	if (!descriptor)
	{
		IoCloseFile (Vcb->RawFileObject);
		return STATUS_INSUFFICIENT_RESOURCES;
	}

	LARGE_INTEGER Offset = {0};

	Offset.LowPart = CDFS_SECTOR_SIZE * 16;

	int gcounter = 0;

	do 
	{
		Status = IoReadFile(
			Vcb->RawFileObject,
			descriptor,
			CDFS_SECTOR_SIZE,
			&Offset,
			IRP_FLAGS_TRACE_IRP,
			&IoStatus
			);

		if (!SUCCESS(Status))
		{
			ExFreeHeap (descriptor);
			IoCloseFile (Vcb->RawFileObject);
			return Status;
		}

		if (strncmp ((char*)descriptor->Identifier, "CD001", 5))
		{
#if DBG
			char signature[6];
			signature[5] = 0;
			strncpy (signature, (char*)descriptor->Identifier, 5);

			KdPrint(("CDFS: Bad signature found in descriptor [%s]\n", signature));
#endif
			ExFreeHeap (descriptor);
			IoCloseFile (Vcb->RawFileObject);
			return STATUS_FILE_SYSTEM_NOT_RECOGNIZED;
		}

		KdPrint(("CDFS: Read descriptor of type %d {%s}\n", descriptor->Type,
			descriptor->Type < 4 ? CdfsVdTypes[descriptor->Type] : "VolumeDescriptorSetTerminator"));

		if (descriptor->Type == PrimaryVolumeDescriptor)
		{
			break;
		}

		gcounter ++;
		if (gcounter == 10)
		{
			KdPrint(("CDFS: Couldn't find primary volume descriptor in first 10 extents\n"));
			ExFreeHeap (descriptor);
			IoCloseFile (Vcb->RawFileObject);
			return STATUS_FILE_SYSTEM_NOT_RECOGNIZED;
		}

		Offset.LowPart += CDFS_SECTOR_SIZE;
	}
	while (TRUE);

	//
	// Got primary descriptor
	//

	Vcb->PrimaryDescriptor = descriptor;



    KdPrint(("B POT MHE HOGI!!\n"));

	return STATUS_NOT_SUPPORTED;
}

VOID
KEAPI
CdfsCloseVcb(
	PCDFS_VOLUME_OBJECT Volume
	)
/*++
	Creates the volume parameter block for the FAT file system
--*/
{
	return;
}

STATUS
KEAPI
CdfsFsControl(
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
				PCDFS_VOLUME_OBJECT MountedDeviceObject;

				CdPrint (("CDFS: Mounting device %08x\n", RealDevice));

				Status = IoCreateDevice (
					DeviceObject->DriverObject,
					sizeof(CDFS_VCB),
					NULL,
					DEVICE_TYPE_DISK_FILE_SYSTEM,
					(PDEVICE*) &MountedDeviceObject
					);

				if (!SUCCESS(Status))
				{
					CdPrint (("CDFS: Failed to create mounted volume: Status=%08x\n", Status));
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
							CdPrint(("CDFS: ObCreateSymbolicLink failed for %S->%S with status %08x\n", LinkName.Buffer, DeviceName.Buffer, Status));
						}
						else
						{
							CdPrint(("CDFS: Symbolic link created: %S -> %S\n", LinkName.Buffer, DeviceName.Buffer));
						}

						RtlFreeUnicodeString( &DeviceName );
					}
					else
					{
						CdPrint(("CDFS: ObQueryObjectName failed for %08x with status %08x\n", DeviceObject, Status));
					}
				}
				else
				{
					CdPrint(("CDFS: IoAllocateMountDriveLetter failed with status %08x\n", Status));
				}


				//
				// Now MountedDeviceObject - new fs device, that will be attached to PDO
				//
				// First we should first mount PDO to this device object.
				//

				Status = IoMountVolume (RealDevice, &MountedDeviceObject->DeviceObject);

				if (!SUCCESS(Status))
				{
					CdPrint (("CDFS: Failed to mount volume: Status=%08x\n", Status));
					ObpDeleteObject (MountedDeviceObject);
					break;
				}

				CdPrint(("CDFS: Volume %08x mounted to %08x\n", RealDevice, MountedDeviceObject));

				//
				// Now, we can attach our MountedDeviceObject to the PDO
				//

				IoAttachDevice (&MountedDeviceObject->DeviceObject, RealDevice);

				CdPrint(("CDFS: Mounted device attached\n"));

				//
				// Okay, the volume is mounted successfully. Create the VCB
				//

				Status = CdfsCreateVcb (MountedDeviceObject);

				if (!SUCCESS(Status))
				{
					CdPrint(("CDFS: CdfsCreateVcb failed with status %08x\n", Status));

					IoDetachDevice (RealDevice);
					IoDismountVolume (RealDevice);
					ObpDeleteObject (MountedDeviceObject);
					break;
				}

				CdPrint (("CDFS: VCB created, volume mounted successfully\n"));
			}

			break;

		case IRP_MN_DISMOUNT:

			{
				//
				// The dismount should be performed.
				// Find & detach our unnamed device object
				//

				PCDFS_VOLUME_OBJECT MountedDeviceObject = (PCDFS_VOLUME_OBJECT) RealDevice->AttachedDevice;

				//
				// Close the VCB
				//

				CdfsCloseVcb (MountedDeviceObject);

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
CdfsCreate(
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

		KdPrint(("CDFS open req: %S\n", Irp->FileObject->RelativeFileName.Buffer));

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
CdfsClose(
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

		KdPrint(("CDFS close req: %S\n", Irp->FileObject->RelativeFileName.Buffer));

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
CdfsRead(
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

		KdPrint(("CDFS read req: %S\n", Irp->FileObject->RelativeFileName.Buffer));

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
CdfsIoControl(
	PDEVICE DeviceObject,
	PIRP Irp
	)
{
	COMPLETE_IRP (Irp, STATUS_NOT_SUPPORTED, 0); 
}

// Driver entry point
STATUS DriverEntry(IN PDRIVER DriverObject)
{
	CdPrint(("CDFS: In DriverEntry()\n"));

	InitializeLockedList (&FsCdfsVcbList);

	STATUS Status;
	
	RtlInitUnicodeString (&FsDeviceName, L"\\FileSystem\\Cdfs");

	Status = IoCreateDevice (
		DriverObject,
		0,
		&FsDeviceName,
		DEVICE_TYPE_DISK_FILE_SYSTEM,
		&FsDeviceObject
		);

	if (!SUCCESS(Status))
	{
		CdPrint(("CDFS: Failed to create FSD device object: %08x\n", Status));
		return Status;
	}

	IoRegisterFileSystem (FsDeviceObject);

	DriverObject->IrpHandlers [IRP_FSCTL]  = CdfsFsControl;
	DriverObject->IrpHandlers [IRP_CREATE] = CdfsCreate;
	DriverObject->IrpHandlers [IRP_CLOSE]  = CdfsClose;
	DriverObject->IrpHandlers [IRP_READ]   = CdfsRead;
	DriverObject->IrpHandlers [IRP_IOCTL]  = CdfsIoControl;

	//
	// Search all CDROM drives and mount them
	//

	PDEVICE Device = NULL;
	POBJECT_DIRECTORY DeviceDirectory = IoDeviceDirectory;

	ObQueryDirectoryObject (DeviceDirectory, NULL, (PVOID*)&Device);

	do
	{
		PHYSICAL_IDE_CONTROL_BLOCK	*pcb = (PHYSICAL_IDE_CONTROL_BLOCK*) (Device+1);

		if (pcb->Mask == PHYS_IDE_CB_MASK && pcb->PhysicalOrPartition == 1 && pcb->Identification->RemovableMedia)
		{
			//
			// Our device. Mount it
			//

			ObReferenceObject (Device);
			Status = IoRequestMount (Device, FsDeviceObject);

			if (!SUCCESS(Status))
			{
				CdPrint(("CDFS: Mounting %08x failed with status %08x\n", Device, Status));
				ObDereferenceObject (Device);
			}
		}

		ObQueryDirectoryObject (DeviceDirectory, Device, (PVOID*)&Device);
	}
	while (Device != NULL);

	CdPrint(("CDFS: Initialized successfully\n"));

	return STATUS_SUCCESS;
}
