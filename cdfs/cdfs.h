/*
* CDFS.SYS include file with CDFS driver internal structures
*
* [C]oded by Great, 2008.
*/

#pragma once

extern PDEVICE FsDeviceObject;
extern UNICODE_STRING FsDeviceName;

#define COMPLETE_IRP(Irp,xStatus,Info) {			\
	(Irp)->IoStatus.Status = (xStatus);			\
	(Irp)->IoStatus.Information = (Info);		\
	IoCompleteRequest ((Irp), 0);				\
	return (xStatus);							\
}

extern LOCKED_LIST FsCdfsVcbList;

//
// CDFS volume control block
//

typedef struct CDFS_VCB
{
	PVPB Vpb;
	LIST_ENTRY VcbListEntry;

	char DriveLetter;
	BOOLEAN MediaInserted;

	// If MediaInserted == 1
	// {
	
	PFILE RawFileObject;
	PCDFS_PRIMARY_VOLUME_DESCRIPTOR PrimaryDescriptor;

	ULONG BootableImageExtent;
	ULONG RootDirectoryExtent;
	
	PCDFS_DIRECTORY_RECORD RootDirectory;
	ULONG PartOfLoadedRootdir;

	//BUGBUG: add path record table

	// }
} *PCDFS_VCB;

//
// File Control block
//

typedef struct CDFS_FCB
{
    PCDFS_VCB Vcb;
	PCDFS_DIRECTORY_RECORD DirectoryRecord;
} *PCDFS_FCB;


//
// Volume Object
//

typedef struct CDFS_VOLUME_OBJECT
{
	DEVICE DeviceObject;
	CDFS_VCB Vcb;
} *PCDFS_VOLUME_OBJECT;

