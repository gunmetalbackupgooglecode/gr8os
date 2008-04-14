#include "common.h"

PDEVICE FsDeviceObject;
UNICODE_STRING FsDeviceName;

#define COMPLETE_IRP(Irp,xStatus,Info) {			\
		(Irp)->IoStatus.Status = (xStatus);			\
		(Irp)->IoStatus.Information = (Info);		\
		IoCompleteRequest ((Irp), 0);				\
		return (xStatus);							\
	}

LOCKED_LIST FsFatVcbList;

#define FsPrint(x) KdPrint(x)

STATUS
KEAPI
FsFatCreateVcb(
	PFSFAT_VOLUME_OBJECT Volume
	)
/*++
	Creates the volume parameter block for the FAT file system
--*/
{
	PFSFATVCB Vcb = &Volume->Vcb;

	Vcb->Vpb = Volume->DeviceObject.Vpb;

	FsPrint(("FSFAT: FsFatCreateVcb: Opening volume\n"));

	//
	// Load FatHeader & other metadata
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

	FsPrint(("FSFAT: FsFatCreateVcb: Volume name %S\n", VolumeName.Buffer));

	Status = IoCreateFile (
		&Vcb->RawFileObject,
		FILE_READ_DATA | FILE_WRITE_DATA,
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

	FsPrint(("FSFAT: FsFatCreateVcb: Volume opened\n"));

	//
	// Read bootsector
	//

	Vcb->BootSector = ExAllocateHeap (TRUE, SECTOR_SIZE);

	if (!Vcb->BootSector)
	{
		IoCloseFile (Vcb->RawFileObject);
		return STATUS_INSUFFICIENT_RESOURCES;
	}

	LARGE_INTEGER Offset;

	Offset.LowPart = 0;

	Status = IoReadFile (
		Vcb->RawFileObject,
		Vcb->BootSector,
		SECTOR_SIZE,
		&Offset,
		&IoStatus
		);

	if (!SUCCESS(Status))
	{
		ExFreeHeap (Vcb->BootSector);
		IoCloseFile (Vcb->RawFileObject);
		return Status;
	}

	PUCHAR buff = (PUCHAR)Vcb->BootSector;
	FsPrint(("FSFAT: FsFatCreateVcb: Bootsector read: %02x %02x %02x %02x [%s]\n", 
		buff[0], buff[1], buff[2], buff[3], buff));

	PFSFAT_HEADER fh = Vcb->FatHeader;

	ULONG FatSectors = (fh->FatSectors ? fh->FatSectors : fh->FatSectors32);

	FsPrint((
		"SectorsPerCluster = %d\n"
		"SectorSize = %d\n"
		"ReservedSectors = %d\n"
		"NumberOfFats = %d\n"
		"DirectoryEntries = %d\n"
		"Sectors = %d\n"
		"Media = %02x\n"
		"FatSectors = %d\n"
		"SectorsPerTrack = %d\n"
		"Heads = %d\n"
		"HiddenSectors = %d\n"
		"DriveNumber = %d\n"
		"Signature = %d\n"
		"SerialNumber = %08x\n"
		"\n"
		"FatSectors16/32 = %08x\n"
		,
		fh->SectorsPerCluster,
		fh->SectorSize,
		fh->ReservedSectors,
		fh->NumberOfFats,
		fh->DirectoryEntries,
		fh->Sectors,
		fh->Media,
		fh->FatSectors,
		fh->SectorsPerTrack,
		fh->Heads,
		fh->HiddenSectors,
		fh->DriveNumber,
		fh->Signature,
		fh->BootID,
		FatSectors
		));

//	if (KiInitializationPhase >= 2)
//		INT3

	Vcb->Fat1StartSector = (USHORT) (fh->ReservedSectors /*+ fh->HiddenSectors*/);
	Vcb->Fat2StartSector = Vcb->Fat1StartSector + FatSectors;
	Vcb->DirStartSector = Vcb->Fat2StartSector + FatSectors;
	Vcb->Cluster2StartSector = Vcb->DirStartSector + ALIGN_UP(fh->DirectoryEntries*sizeof(FSFATDIR_ENT),fh->SectorSize)/fh->SectorSize;
	Vcb->ClusterSize = fh->SectorsPerCluster*fh->SectorSize;

	Vcb->Vpb->SerialNumber = fh->BootID;

	Vcb->FatType = FsFatGetFatType (Vcb);

	//
	// Read fat
	//

	Offset.LowPart = (Vcb->Fat1StartSector) * fh->SectorSize;

	FsPrint(("FSFAT: Reading fat, offset=%08x [F1=%08x, SS=%08x]\n", Offset.LowPart,
		Vcb->Fat1StartSector, fh->SectorSize));

	Vcb->Fat = Vcb->FirstFat = (PUCHAR) ExAllocateHeap (TRUE, FatSectors * fh->SectorSize);

	if (!Vcb->FirstFat)
	{
		ExFreeHeap (Vcb->BootSector);
		IoCloseFile (Vcb->RawFileObject);
		return STATUS_INSUFFICIENT_RESOURCES;
	}

	Status = IoReadFile (
		Vcb->RawFileObject,
		Vcb->Fat,
		FatSectors * fh->SectorSize,
		&Offset,
		&IoStatus
		);

	if (!SUCCESS(Status))
	{
		FsPrint(("FSFAT: FsFatCreateVcb: First FAT is bad. Trying to read second fat\n"));
		Vcb->Fat = Vcb->SecondFat = Vcb->FirstFat;
		Vcb->FirstFat = NULL;

		Offset.LowPart = (Vcb->Fat2StartSector) * fh->SectorSize;

		Status = IoReadFile (
			Vcb->RawFileObject,
			Vcb->Fat,
			FatSectors * fh->SectorSize,
			&Offset,
			&IoStatus
			);

		if (!SUCCESS(Status))
		{
			FsPrint(("FSFAT: FsFatCreateVcb: Second FAT is bad too. Fatal: Cannot mount\n"));

			ExFreeHeap (Vcb->Fat);
			ExFreeHeap (Vcb->BootSector);
			IoCloseFile (Vcb->RawFileObject);
			return Status;
		}
	}

	FsPrint(("FSFAT: FsFatCreateVcb: FAT loaded\n"));
	buff = (PUCHAR)Vcb->Fat;
	FsPrint(("FSFAT: FAT: %02x %02x %02x %02x [%s]\n", 
		buff[0], buff[1], buff[2], buff[3], buff));

	//
	// FAT loaded.
	// Load root directory.
	//

	if (fh->DirectoryEntries)
	{
		//
		// Reading root directory for FAT12/16 - fixed size.
		//

		Vcb->RootDirectory = (PFSFATDIR_ENT) ExAllocateHeap (TRUE, fh->DirectoryEntries*sizeof(FSFATDIR_ENT));

		if (!Vcb->RootDirectory)
		{
			ExFreeHeap (Vcb->Fat);
			ExFreeHeap (Vcb->BootSector);
			IoCloseFile (Vcb->RawFileObject);
			return STATUS_INSUFFICIENT_RESOURCES;
		}

		Offset.LowPart = Vcb->DirStartSector * fh->SectorSize;

		FsPrint(("FSFAT: Reading root directory, offset=%08x\n", Offset.LowPart));

		Status = IoReadFile (
			Vcb->RawFileObject,
			Vcb->RootDirectory,
			fh->DirectoryEntries*sizeof(FSFATDIR_ENT),
			&Offset,
			&IoStatus
			);
	}
	else
	{
		//
		// Read dynamic FAT32 root directory
		//

		ULONG RootDirSize = FsFatSizeOfClusterChain (Vcb, fh->RootDirectoryCluster);

		FsPrint(("FSFAT: Root directory cluster chain size %08x\n", RootDirSize));

		Vcb->RootDirectory = (PFSFATDIR_ENT) ExAllocateHeap (TRUE, RootDirSize*Vcb->ClusterSize);

		if (!Vcb->RootDirectory)
		{
			ExFreeHeap (Vcb->Fat);
			ExFreeHeap (Vcb->BootSector);
			IoCloseFile (Vcb->RawFileObject);
			return STATUS_INSUFFICIENT_RESOURCES;
		}

		PUCHAR iBufferPos = (PUCHAR) Vcb->RootDirectory;

		for (	ULONG Cluster = fh->RootDirectoryCluster; 
				Cluster >= 2 && Cluster < 0xFF0; 
				Cluster = FsFatReadFatEntry (Vcb, Cluster) )
		{
			FsPrint(("FSFAT: Reading root directory cluster %08x\n", Cluster));

			Status = FsFatReadCluster (Vcb, Cluster, iBufferPos);
			iBufferPos += Vcb->ClusterSize;
		}
	}

	if (!SUCCESS(Status))
	{
		FsPrint(("FSFAT: FsFatCreateVcb: Cannot read root directory\n"));
		ExFreeHeap (Vcb->RootDirectory);
		ExFreeHeap (Vcb->Fat);
		ExFreeHeap (Vcb->BootSector);
		IoCloseFile (Vcb->RawFileObject);
		return Status;
	}

	buff = (PUCHAR)Vcb->RootDirectory;
	FsPrint(("FSFAT: ROOT: %02x %02x %02x %02x [%s]\n", 
		buff[0], buff[1], buff[2], buff[3], buff));

	//
	// Root directory loaded.
	//

	FsPrint(("FSFAT: FsFatCreateVcb: Metadata loaded\n"));

	// Synchronously insert this VCB into the global VCB list
	InterlockedInsertTailList (&FsFatVcbList, &Vcb->VcbListEntry);

	return STATUS_SUCCESS;
}

VOID
KEAPI
FsFatCloseVcb(
	PFSFAT_VOLUME_OBJECT Volume
	)
/*++
	Creates the volume parameter block for the FAT file system
--*/
{
	PFSFATVCB Vcb = &Volume->Vcb;

	KdPrint(("FSFAT: FsFatCloseVcb: Deleting VCB\n"));

	InterlockedRemoveEntryList (&FsFatVcbList, &Vcb->VcbListEntry);

	//
	// Unload FatHeader & other metadata
	//

	// Free root directory buffer
	ExFreeHeap (Vcb->RootDirectory);
	
	// Free FATs
	if (Vcb->FirstFat)
	{
		ExFreeHeap (Vcb->FirstFat);
	}
	if (Vcb->SecondFat)
	{
		ExFreeHeap (Vcb->SecondFat);
	}
	
	// Free bootsector
	ExFreeHeap (Vcb->BootSector);

	// Close file object
	IoCloseFile (Vcb->RawFileObject);
}

STATUS
KEAPI
FsFatFsControl(
	PDEVICE DeviceObject,
	PIRP Irp
	)
/*++
	FileSystem Control requests handler.
	It handles two cases:
		
		1. The mount request pending. We should create unnamed device object,
		 which will handle all file operation requests, attach it to the real object
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
				PFSFAT_VOLUME_OBJECT MountedDeviceObject;

				KdPrint (("FSFAT: Mounting device %08x\n", RealDevice));

				Status = IoCreateDevice (
					DeviceObject->DriverObject,
					sizeof(FSFATVCB),
					NULL,
					DEVICE_TYPE_DISK_FILE_SYSTEM,
					(PDEVICE*) &MountedDeviceObject
					);

				if (!SUCCESS(Status))
				{
					KdPrint (("FSFAT: Failed to create mounted volume: Status=%08x\n", Status));
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
							KdPrint(("FSFAT: ObCreateSymbolicLink failed for %S->%S with status %08x\n", LinkName.Buffer, DeviceName.Buffer, Status));
						}
						else
						{
							KdPrint(("FSFAT: Symbolic link created: %S -> %S\n", LinkName.Buffer, DeviceName.Buffer));
						}

						RtlFreeUnicodeString( &DeviceName );
					}
					else
					{
						KdPrint(("FSFAT: ObQueryObjectName failed for %08x with status %08x\n", DeviceObject, Status));
					}
				}
				else
				{
					KdPrint(("FSFAT: IoAllocateMountDriveLetter failed with status %08x\n", Status));
				}


				//
				// Now MountedDeviceObject - new fs device, that will be attached to PDO
				//
				// First we should first mount PDO to this device object.
				//

				Status = IoMountVolume (RealDevice, &MountedDeviceObject->DeviceObject);

				if (!SUCCESS(Status))
				{
					KdPrint (("FSFAT: Failed to mount volume: Status=%08x\n", Status));
					ObpDeleteObject (MountedDeviceObject);
					break;
				}

				KdPrint(("FSFAT: Volume %08x mounted to %08x\n", RealDevice, MountedDeviceObject));

				//
				// Now, we can attach our MountedDeviceObject to the PDO
				//

				IoAttachDevice (&MountedDeviceObject->DeviceObject, RealDevice);

				KdPrint(("FSFAT: Mounted device attached\n"));

				//
				// Okay, the volume is mounted successfully. Create the VCB
				//

				Status = FsFatCreateVcb (MountedDeviceObject);

				if (!SUCCESS(Status))
				{
					KdPrint(("FSFAT: FsFatCreateVcb failed with status %08x\n", Status));

					IoDetachDevice (RealDevice);
					IoDismountVolume (RealDevice);
					ObpDeleteObject (MountedDeviceObject);
					break;
				}

				KdPrint (("FSFAT: VCB created, volume mounted successfully\n"));
			}

			break;

		case IRP_MN_DISMOUNT:

			{
				//
				// The dismount should be performed.
				// Find & detach our unnamed device object
				//

				PFSFAT_VOLUME_OBJECT MountedDeviceObject = (PFSFAT_VOLUME_OBJECT) RealDevice->AttachedDevice;

				//
				// Close the VCB
				//

				FsFatCloseVcb (MountedDeviceObject);
				
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
		}
	}

	COMPLETE_IRP (Irp, Status, 0);
}

void strtoupr(char* string)
{
	for(;*string;string++)
	{
		if (*string >= 'a' && *string <= 'z')
			*string += 'A' - 'a';
	}
}

VOID
KEAPI
FsFatNtNameToDosName (
	char *filename,
	char* dosname
	)
{
	char *NameToConvert = filename;

	strtoupr (NameToConvert);

	for (int i=0;i<11;i++)
		dosname[i] = ' ';

	char *dot = strchr(NameToConvert, '.');
	if (dot)
	{
		int dotpos = dot - NameToConvert;

		strncpy (dosname, NameToConvert, dotpos);
		strncpy (dosname+8, NameToConvert+dotpos+1, 3);
	}
	else
	{
		strncpy (dosname, NameToConvert, strlen(NameToConvert));
	}
	dosname[11] = 0;
}

STATUS
KEAPI
FsFatCreate(
	PDEVICE DeviceObject,
	PIRP Irp
	)
/*++
	Handle the create operation.
--*/
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

		KdPrint(("FSFAT open req: %S\n", Irp->FileObject->RelativeFileName.Buffer));

		//
		// Now, pass the request down if Path is empty.
		// This means that caller wants to open disk directly.
		//

		if (Irp->FileObject->RelativeFileName.Length == 0)
		{
			IoSkipCurrentIrpStackLocation (Irp);
			return IoCallDriver (RealDevice, Irp);
		}
		else
		{
			//
			// Search the file
			//

			char *path = (char*) ExAllocateHeap (TRUE, Irp->FileObject->RelativeFileName.Length/2 /* -1 for \ + 1 for NULL */ );

			wcstomb (path, Irp->FileObject->RelativeFileName.Buffer+1, -1);

			if (strchr (path, '\\'))
			{
				KdPrint(("FSFAT: Subdirectorires not supported\n"));
				Status = STATUS_NOT_SUPPORTED;
				goto finally;
			}

			char dosname[12];

			FsFatNtNameToDosName (path, dosname);

			ExFreeHeap (path);

			PFSFAT_VOLUME_OBJECT dev = (PFSFAT_VOLUME_OBJECT) DeviceObject;
			PFSFAT_HEADER fh = dev->Vcb.FatHeader;

			ULONG numEntries = fh->DirectoryEntries ? fh->DirectoryEntries : (((ULONG)fh->SectorsPerCluster*(ULONG)fh->SectorSize)/(ULONG)sizeof(FSFATDIR_ENT));

			KdPrint(("CREAT: %d entries\n", numEntries));

			for (ULONG i=0; i<numEntries; i++)
			{
				PFSFATDIR_ENT dirent = &dev->Vcb.RootDirectory[i];

				KdPrint(("CREAT: [%s] %s\n", dosname, dirent->Filename));

				if (!strncmp (dosname, dirent->Filename, 11))
				{
					//
					// Found file.
					// Check permissions.
					//

					if (dirent->Attributes.ReadOnly &&
						(Irp->CurrentStackLocation->Parameters.Create.DesiredAccess & FILE_WRITE_DATA))
					{
						Status = STATUS_ACCESS_DENIED;
						goto finally;
					}

					//
					// Allow access.
					//
					// Create FCB
					//

					PFSFATFCB Fcb = (PFSFATFCB) ExAllocateHeap (TRUE, sizeof(FSFATFCB));
					Fcb->Vcb = &dev->Vcb;
					Fcb->DirEnt = dirent;

					// Save FCB pointer
					Irp->FileObject->FsContext = Fcb;

					Status = STATUS_SUCCESS;
					goto finally;
				}
			}

			Status = STATUS_NOT_FOUND;
		}
	}

finally:
	COMPLETE_IRP (Irp, Status, 0);
}


STATUS
KEAPI
FsFatClose(
	PDEVICE DeviceObject,
	PIRP Irp
	)
/*++
	Handle the close operation.
--*/
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

		KdPrint(("FSFAT close req: %S\n", Irp->FileObject->RelativeFileName.Buffer));

		//
		// Now, pass the request down if Path is empty.
		// This means that caller wants to close disk directly.
		//

		if (Irp->FileObject->RelativeFileName.Length == 0)
		{
			IoSkipCurrentIrpStackLocation (Irp);
			return IoCallDriver (RealDevice, Irp);
		}
		else
		{
			//
			// Close opened file.
			//

			// The one thing we should do - delete file control block. Do it
			
			PFSFATFCB Fcb = (PFSFATFCB) Irp->FileObject->FsContext;
			ExFreeHeap (Fcb);
			Irp->FileObject->FsContext = NULL;
			
			Status = STATUS_SUCCESS;
		}
	}

	COMPLETE_IRP (Irp, Status, 0);
}

UCHAR
FsFatGetFatType(
	PFSFATVCB Vcb
	)
{
	PFSFAT_HEADER fh = Vcb->FatHeader;
	ULONG RootDirSectors = (
		(fh->DirectoryEntries*sizeof(FSFATDIR_ENT)) + 
		(fh->SectorSize - 1)) / fh->SectorSize;

	ULONG FatSz;

	if ( fh->FatSectors != 0)
		FatSz = fh->FatSectors;
	else
		FatSz = fh->FatSectors32;

	ULONG TotalSectors;

	if (fh->Sectors != 0)
		TotalSectors = fh->Sectors;
	else
		TotalSectors = fh->SectorsLong;

	ULONG DataSec = TotalSectors - (fh->ReservedSectors + (fh->NumberOfFats*FatSz) + RootDirSectors);
	ULONG CountOfClusters = DataSec / fh->SectorsPerCluster;

	if (CountOfClusters < 4085) {
		return 12;
	} else if (CountOfClusters < 65525) {
		return 16;
	} else {
		return 32;
	}
}

ULONG
FsFatReadFatEntry(
	PFSFATVCB Vcb,
	ULONG Index
	)
{
	ULONG nByte;
	ULONG Cluster = 0;
	PUCHAR Fat = Vcb->Fat;
	UCHAR FatType = Vcb->FatType;

	switch(FatType)
	{
	case 12:
		if ( (Index % 2)==0 )
		{
			// Index is a multiple of two.

			nByte = (Index*3)/2;

			Cluster = (*(USHORT*)&Fat[nByte]) & 0xFFF;
		}
		else
		{
			// Index is not a multiple of two
			
			nByte = (Index+1)*3/2 - 2;

			Cluster = (*(USHORT*)&Fat[nByte]) >> 4;
		}

	//	KdPrint(("nByte=%08x, FAT=%08x, short=%08x, Cluster=%08x\n", nByte, Fat, *(USHORT*)&Fat[nByte], Cluster));

		break;

	case 16:
		Cluster = (*(USHORT*)&Fat[Index*2]);
		break;

	case 32:
		Cluster = (*(ULONG*)&Fat[Index*4]) & 0x0FFFFFFF; // 24 bits
		break;
	}

	return Cluster;
}

struct FSFAT_CLUSTERS {
	ULONG Mask;
	ULONG ReservedStart;
	ULONG ReservedEnd;
	ULONG BadCluster;
	ULONG LastClusterStart;
};

FSFAT_CLUSTERS FsFatClusters[] = {
	{ 0xFFF, 0xFF0, 0xFF6, 0xFF7, 0xFF8 },
	{ 0xFFFF, 0xFFFF0, 0xFFFF6, 0xFFFF7, 0xFFFF8 },
	{ 0x0FFFFFFF, 0x0FFFFFF0, 0x0FFFFFF6, 0x0FFFFFF7, 0x0FFFFFF8 }
};

FSFAT_CLUSTERS* FsFatGetClTable(UCHAR Size)
{
	switch (Size)
	{
	case 12:
		return &FsFatClusters[0];
	case 16:
		return &FsFatClusters[1];
	case 32:
		return &FsFatClusters[2];
	}

	return NULL;
}

#define CLUSTER(x) ((x) & FsFatGetClTable(Vcb->FatType)->Mask)
#define FATTBL(x) (FsFatGetClTable(Vcb->FatType)->x)

ULONG
FsFatFileClusterByPos (
	PFSFATVCB Vcb,
	ULONG ChainHead, 
	ULONG Pos
	)
{
	ULONG i=0;
	ULONG Cluster;

	ChainHead = CLUSTER(ChainHead);

	for (Cluster = (ChainHead); 
		 i < Pos;
		 i++)
	{
		Cluster = FsFatReadFatEntry(Vcb, Cluster);

//		KdPrint(("FSFAT: Got %8x cluster (#%d) from cluster chain head %08x\n", (Cluster), i, ChainHead));

		if ((Cluster) >= FATTBL(LastClusterStart))
		{
			Cluster = -1;
			break;
		}

		if ((Cluster) >= FATTBL(ReservedStart)
			&& (Cluster) <= FATTBL(ReservedEnd))
		{
			// Reserved sector
			KdPrint(("FSFAT: Warn: Resevred cluster %08x found in file, cluster chain head %08x\n", (Cluster), ChainHead));
			Cluster = -3;
			break;
		}

		if (Cluster == FATTBL(BadCluster))
		{
			KdPrint(("FSFAT: Warn: Bad cluster %08x found in file (chain %08x), file will be unreadable\n", Cluster, ChainHead));
			Cluster = -2;
			break;
		}
	}

	return Cluster;
}

ULONG
KEAPI
FsFatSizeOfClusterChain(
	PFSFATVCB Vcb,
	ULONG Head
	)
{
	ULONG i=1;
	ULONG Cluster;

	Head = CLUSTER(Head);

	for (Cluster = (Head); 
		 ;
		 i++)
	{
		Cluster = FsFatReadFatEntry(Vcb, Cluster);

//		KdPrint(("FSFAT: Got %8x cluster (#%d) from cluster chain head %08x\n", (Cluster), i-1, Head));

		if ((Cluster) >= FATTBL(LastClusterStart))
		{
			Cluster = -1;
			break;
		}

		if ((Cluster) >= FATTBL(ReservedStart)
			&& (Cluster) <= FATTBL(ReservedEnd))
		{
			// Reserved sector
			KdPrint(("FSFAT: Warn: Resevred cluster %08x found in chain, cluster chain head %08x\n", (Cluster), Head));
			Cluster = -3;
			break;
		}

		if (Cluster == FATTBL(BadCluster))
		{
			KdPrint(("FSFAT: Warn: Bad cluster %08x found in cluster chain %08x, file will be unreadable\n", Cluster, Head));
			Cluster = -2;
			break;
		}
	}

	return i;
}

STATUS
KEAPI
FsFatReadCluster(
	PFSFATVCB Vcb,
	ULONG Cluster,
	PVOID Buffer
	)
/*++
	Read cluster from disk
--*/
{
	ULONG StartSector;
	IO_STATUS_BLOCK IoStatus;
	STATUS Status;
	LARGE_INTEGER Offset;

	Cluster -= 2;
	StartSector = Vcb->Cluster2StartSector + Cluster*Vcb->FatHeader->SectorsPerCluster;

	Offset.LowPart = StartSector * Vcb->FatHeader->SectorSize;

	Status = IoReadFile (
		Vcb->RawFileObject,
		Buffer,
		Vcb->ClusterSize,
		&Offset,
		&IoStatus
		);

	return Status;
}

ULONG
KEAPI
FsFatSEHandler(
	PEXCEPTION_ARGUMENTS Args,
	PCONTEXT_FRAME ContextFrame
	)
{
	KdPrint(("FSFAT: Caught exception %08x\n", Args->ExceptionCode));
	
	KeBugCheck (KERNEL_MODE_EXCEPTION_NOT_HANDLED,
				Args->ExceptionCode,
				ContextFrame->Eip,
				0,
				0
				);
}

#if FSFAT_TRACE_READING
#define FrPrint(x) KiDebugPrint x
#else
#define FrPrint(x)
#endif

STATUS
KEAPI
FsFatRead(
	PDEVICE DeviceObject,
	PIRP Irp
	)
/*++
	Handle the read operation.
--*/
{
	STATUS Status = STATUS_INTERNAL_FAULT;
	ULONG Read = 0;

	if (DeviceObject == FsDeviceObject)
	{
		//
		// Disallow all open/close operations for the FSD device object
		//

		Status = STATUS_INVALID_FUNCTION;
	}
	else
	{
		PDEVICE RealDevice = DeviceObject->Vpb->PhysicalDeviceObject;

//		KdPrint(("FSFAT read req: %S\n", Irp->FileObject->RelativeFileName.Buffer));

		if (Irp->FileObject->RelativeFileName.Length == 0)
		{
			//
			// Someone wants to read the disk directly.
			// Pass IRP down.
			//

			IoSkipCurrentIrpStackLocation (Irp);
			return IoCallDriver (RealDevice, Irp);
		}
		else
		{
			//
			// Handle file reading.
			//

			__asm
			{
				push dword ptr fs:[0]

				push FsFatSEHandler
				push dword ptr fs:[0]
				mov fs:[0], esp
			}

			PFSFATFCB Fcb = (PFSFATFCB) Irp->FileObject->FsContext;
			PFSFATVCB Vcb = (PFSFATVCB) &((PFSFAT_VOLUME_OBJECT)DeviceObject)->Vcb;

			ULONG Length = Irp->BufferLength;
			ULONG FilePos = Irp->CurrentStackLocation->Parameters.ReadWrite.Offset.LowPart;
			if (Irp->BufferLength + FilePos >= Fcb->DirEnt->FileSize)
			{
				Length = Fcb->DirEnt->FileSize - FilePos;
			}

			ULONG AlignedPos = ALIGN_DOWN(FilePos, Vcb->ClusterSize) / Vcb->ClusterSize;
			ULONG AlignedSize = ALIGN_UP (FilePos+Length, Vcb->ClusterSize)/Vcb->ClusterSize - AlignedPos + 1;

			if ( (AlignedSize + AlignedPos)*Vcb->ClusterSize >= ALIGN_UP(Fcb->DirEnt->FileSize,Vcb->ClusterSize) )
			{
				AlignedSize --;
			}

			PVOID InternalBuffer = ExAllocateHeap (TRUE, AlignedSize*Vcb->ClusterSize);
			PUCHAR iBufferPos = (PUCHAR)InternalBuffer;

			FrPrint(("Pos=%08x, Len=%08x, AlignedPos = %08x, AlignedSize = %08x\n", FilePos, Length, AlignedPos, AlignedSize));

			if( AlignedSize == 0 )
				Status = STATUS_SUCCESS;

			for (ULONG i=AlignedPos; i<AlignedPos+AlignedSize; i++)
			{
				ULONG Cluster = FsFatFileClusterByPos (Vcb,
					Fcb->DirEnt->StartCluster | (Fcb->DirEnt->StartClusterHigh<<16),
					i
					);

				if (Cluster == -1)
				{
					//
					// Reading truncated because the end-of-file occurred.
					//
					KdPrint(("FSFAT: End-of-file occurred, read truncated\n"));

					//
					// This is the internal fault
					//

					Status = STATUS_INTERNAL_FAULT;
					ExFreeHeap (InternalBuffer);
					goto finally;
				}
				else if (Cluster == -2)
				{
					//
					// Bad cluster found.
					//

					KdPrint(("FSFAT: Bad cluster #%d in file\n", i));
					Status = STATUS_PARTIAL_COMPLETION;
					ExFreeHeap (InternalBuffer);
					goto finally;
				}
				else if (Cluster == -3)
				{
					//
					// Reserved cluster found.
					//

					KdPrint(("FSFAT: Reserved cluster #%d in file\n", i));
					Status = STATUS_PARTIAL_COMPLETION;
					ExFreeHeap (InternalBuffer);
					goto finally;
				}

				FrPrint(("FSFAT: Reading cluster #%d: %08x (offset %08x)\n", i, Cluster, i*Vcb->ClusterSize));

				if (Cluster < 2)
				{
					KdPrint(("FSFAT: Internal check failed (Cluster<2)\n"));
					Status = STATUS_INVALID_PARAMETER;
					ExFreeHeap (InternalBuffer);
					goto finally;
				}

				Status = FsFatReadCluster (Vcb, Cluster, iBufferPos);

				if (!SUCCESS(Status))
				{
					KdPrint(("FSFAT: Reading cluster %08x failed with status %08x\n", Cluster, Status));
					ExFreeHeap (InternalBuffer);
					goto finally;
				}

				iBufferPos += Vcb->ClusterSize;
			}

			memcpy (
				Irp->SystemBuffer,
				(PUCHAR)InternalBuffer + (FilePos - AlignedPos*Vcb->ClusterSize),
				Length
				);

			Status = STATUS_SUCCESS;
			ExFreeHeap (InternalBuffer);
			Read = Length;

			__asm pop dword ptr fs:[0];
		}
	}

finally:
	COMPLETE_IRP (Irp, Status, Read);
}

STATUS
KEAPI
FsFatDriverEntry (
	PDRIVER DriverObject
	)
/*++
	Initialize the FS FAT driver
--*/
{
	KdPrint(("FSFAT: INIT\n"));

	InitializeLockedList (&FsFatVcbList);

	STATUS Status;

	RtlInitUnicodeString (&FsDeviceName, L"\\FileSystem\\Fat" );
	Status = IoCreateDevice (
		DriverObject,
		0,
		&FsDeviceName,
		DEVICE_TYPE_DISK_FILE_SYSTEM,
		&FsDeviceObject
		);

	if (!SUCCESS(Status))
	{
		KdPrint (("FSFAT: Failed to create FSD device: Status=%08x\n", Status));
		return Status;
	}

	IoRegisterFileSystem (FsDeviceObject);

	DriverObject->IrpHandlers [IRP_FSCTL] = FsFatFsControl;
	DriverObject->IrpHandlers [IRP_CREATE] = FsFatCreate;
	DriverObject->IrpHandlers [IRP_CLOSE] = FsFatClose;
	DriverObject->IrpHandlers [IRP_READ] = FsFatRead;
//	DriverObject->IrpHandlers [IRP_WRITE] = FsFatWrite;
//	DriverObject->IrpHandlers [IRP_IOCTL] = FsFatIoctl;

	//
	// Mount floppy we booted from to fat file system
	//

	KdPrint (("FSFAT: Mounting floppy..\n"));

	PDEVICE Fdd0;
	UNICODE_STRING DeviceName;

	RtlInitUnicodeString (&DeviceName, L"\\Device\\fdd0");

	Status = ObReferenceObjectByName (
		&DeviceName,
		IoDeviceObjectType,
		KernelMode,
		0,
		0,
		(PVOID*) &Fdd0
		);

	if (!SUCCESS(Status))
	{
		IoDeleteDevice (&FsDeviceName);
		return Status;
	}

	Status = IoRequestMount ( Fdd0, FsDeviceObject );

	ObDereferenceObject (Fdd0);

	if (!SUCCESS(Status))
	{
		KdPrint (("FSFAT: Failed to mount fdd0 : Status=%08x\n", Status));
		IoDeleteDevice (&FsDeviceName);
		
		//return Status;

		KeBugCheck (UNMOUNTABLE_BOOT_VOLUME,
					__LINE__,
					Status,
					0,
					0);
	}

	//
	// Create \SystemRoot symlink
	//

	UNICODE_STRING SymlinkName;
	
	RtlInitUnicodeString (&SymlinkName, L"\\SystemRoot");

	Status = ObCreateSymbolicLink (&SymlinkName, &DeviceName);
	if (!SUCCESS(Status))
	{
		KdPrint (("FSFAT: Failed to create symlink \\SystemRoot : Status=%08x\n", Status));
		IoRequestDismount ( Fdd0, FsDeviceObject );
		IoDeleteDevice (&FsDeviceName);
		return Status;
	}

	KdPrint (("FSFAT: Initialized successfully\n"));

	return STATUS_SUCCESS;
}