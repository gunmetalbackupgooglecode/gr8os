#pragma once

STATUS
KEAPI
FsFatDriverEntry (
	PDRIVER DriverObject
	);

extern LOCKED_LIST FsFatVcbList;

#pragma pack(1)

typedef struct FSFAT_HEADER
{
	UCHAR JmpNear[3];
	CHAR  Version[8];
	USHORT SectorSize;
	UCHAR SectorsPerCluster;
	USHORT ReservedSectors;
	UCHAR NumberOfFats;
	USHORT DirectoryEntries;
	USHORT Sectors;
	UCHAR Media;
	USHORT FatSectors;
	USHORT SectorsPerTrack;
	USHORT Heads;
	ULONG HiddenSectors;
	ULONG SectorsLong;

	union
	{
		// FAT12 & FAT16
		struct
		{
			UCHAR DriveNumber;
			UCHAR CurrentHead;
			UCHAR Signature;
			ULONG BootID;
			CHAR VolumeLabel[11];
			CHAR BootSystemID[8];
		};

		// FAT32
		struct
		{
			ULONG FatSectors32;
			//USHORT ExtendedFlags;
			struct
			{
				USHORT ActiveFat : 4;
				USHORT Reserved : 3;
				USHORT OneFatActive : 1;
				USHORT Reserved2 : 8;
			} ExtendedFlags;
			USHORT FsVersion;
			ULONG RootDirectoryCluster;
			USHORT FsInfoSector;
			USHORT BootSectorBackup;
			USHORT Reserved;
			UCHAR DriveNumber;
			UCHAR Reserved1;
			UCHAR Signature;
			ULONG VolumeID;
			CHAR VolumeLabel[11];
			CHAR BootSystemID[8];
		};
	};

} *PFSFAT_HEADER;

union FSFAT_DATE
{
	USHORT Date;

	struct
	{
		USHORT Day : 5;
		USHORT Month : 4;
		USHORT Year : 7;    // current - 1980
	};
};

union FSFAT_TIME
{
	USHORT Time;

	struct
	{
		USHORT Seconds2 : 5;
		USHORT Minutes : 6;
		USHORT Hours : 5;
	};
};

typedef struct FSFATDIR_ENT
{
	CHAR Filename[11];

	// UCHAR Attributes;
	union
	{
		UCHAR RawValue;

		struct
		{
			UCHAR ReadOnly : 1;
			UCHAR Hidden : 1;
			UCHAR System : 1;
			UCHAR VolumeLabel : 1;
			UCHAR Subdirectory : 1;
			UCHAR Archive : 1;
			UCHAR Device : 1;
			//UCHAR Unused : 1;

			UCHAR ReadOwner : 1;
		};
	} Attributes;

	//UCHAR NTReserved;
	union
	{
		UCHAR RawValue;

		struct
		{
			UCHAR ExecuteOther : 1;
			UCHAR WriteOther : 1;
			UCHAR ReadOther : 1;
			UCHAR ExecuteGroup : 1;
			UCHAR WriteGroup : 1;
			UCHAR ReadGroup : 1;
			UCHAR ExecuteOwner : 1;
			UCHAR WriteOwner : 1;
		};
	} Permissions;

	UCHAR CreateTimeTenth;
	FSFAT_TIME CreateTime;
	FSFAT_DATE CreateDate;
	FSFAT_DATE LastAccessDate;
	USHORT StartClusterHigh;

	FSFAT_TIME WriteTime;
	FSFAT_DATE WriteDate;
	USHORT StartCluster;
	ULONG FileSize;
} *PFSFATDIR_ENT;

#pragma pack()

#pragma pack(2)

//
// Volume control block
//

typedef struct FSFATVCB
{
	PVPB Vpb;
	LIST_ENTRY VcbListEntry;
	ULONG OpenCount;
	ULONG ClusterSize;

	PFILE RawFileObject;
	union
	{
		PVOID BootSector;
		PFSFAT_HEADER FatHeader;
	};

	ULONG Fat1StartSector;
	ULONG Fat2StartSector;
	ULONG DirStartSector;
	ULONG Cluster2StartSector;

	PUCHAR FirstFat;
	PUCHAR SecondFat;
	PUCHAR Fat;			// Points to first fat or second fat

	ULONG SizeOfFatLoaded;
	ULONG FullFatSize;

	PFSFATDIR_ENT RootDirectory;

	BOOLEAN FirstFatBad;

	UCHAR FatType;

	CHAR DriveLetter;

} *PFSFATVCB;

//
// File Control Block
//

typedef struct FSFATFCB
{
	PFSFATVCB Vcb;
	PFSFATDIR_ENT DirEnt;
} *PFSFATFCB;

#pragma pack()

typedef struct FSFAT_VOLUME_OBJECT
{
	DEVICE DeviceObject;
	FSFATVCB Vcb;
} *PFSFAT_VOLUME_OBJECT;


VOID
KEAPI
FsFatNtNameToDosName (
	char *filename,
	char* dosname
	);

UCHAR
FsFatGetFatType(
	PFSFATVCB Vcb
	);

ULONG
FsFatReadFatEntry(
	PFSFATVCB Vcb,
	ULONG Index
	);

ULONG
KEAPI
FsFatSizeOfClusterChain(
	PFSFATVCB Vcb,
	ULONG Head
	);

ULONG
FsFatFileClusterByPos (
	PFSFATVCB Vcb,
	ULONG ChainHead, 
	ULONG Pos
	);

STATUS
KEAPI
FsFatReadCluster(
	PFSFATVCB Vcb,
	ULONG Cluster,
	PVOID Buffer
	);
