/*
 * CDFS.SYS include file with ISO-9660 data structures
 *
 * [C]oded by Great, 2008.
 */

#pragma once

#define ISODCL(x,y) ( (y) - (x) + 1 )

#pragma pack(1)
#pragma warning(disable: 4200)

typedef struct CDFS_PACKED_PRIMARY_VOLUME_DESCRIPTOR
{
	UCHAR Type;								// 1
	UCHAR Identifier [ISODCL(2,6)];
	UCHAR Version;							// 7
	UCHAR Unused;							// 8
	UCHAR SystemIdentifier[ISODCL(9,40)];
	UCHAR VolumeIdentifier[ISODCL(41,72)];
	UCHAR Unused2[ISODCL(73,80)];
	UCHAR VolumeSpaceSize[ISODCL(81,88)];
	UCHAR Unused3[ISODCL(89,120)];
	UCHAR VolumeSetSize[ISODCL(121,124)];
	UCHAR VolumeSequenceNumber[ISODCL(125,128)];
	UCHAR LogicalBlockSize[ISODCL(129,132)];
	UCHAR PathTableSize[ISODCL(133,140)];
	UCHAR LPathTableOccurrence[ISODCL(141,144)];
	UCHAR LPathTableOptionalOccurrence[ISODCL(145,148)];
	UCHAR MPathTableOccurrence[ISODCL(149,152)];
	UCHAR MPathTableOptionalOccurrence[ISODCL(153,156)];
	UCHAR RootDirectoryRecord[ISODCL(157,190)];
	UCHAR VolumeSetIdentifier[ISODCL(191,318)];
	UCHAR PublisherIdentifier[ISODCL(319,446)];
	UCHAR DataPreparerIdentifier[ISODCL(447,574)];
	UCHAR ApplicationIdentifier[ISODCL(575,702)];
	UCHAR CopyrightFileIdentifier[ISODCL(703,739)];
	UCHAR AbstractFileIdentifier[ISODCL(740,776)];
	UCHAR BibliographicFileIdentifier[ISODCL(777,813)];
	UCHAR VolumeCreationDateTime[ISODCL(814,830)];
	UCHAR VolumeModificationDateTime[ISODCL(831,847)];
	UCHAR VolumeExpirationDateTime[ISODCL(848,864)];
	UCHAR VolumeEffectiveDateTime[ISODCL(865,881)];
	UCHAR FileStructureVersion;
	UCHAR Reserved;
	UCHAR ApplicationUse[ISODCL(884,1395)];
	UCHAR Reserved2[ISODCL(1396,2048)];
} *PCDFS_PACKED_PRIMARY_VOLUME_DESCRIPTOR;

STATIC_ASSERT (sizeof(CDFS_PACKED_PRIMARY_VOLUME_DESCRIPTOR) == 2048);

typedef struct CDFS_DOUBLE_USHORT
{
	USHORT LittleEndian;
	USHORT BigEndian;
} *PCDFS_DOUBLE_USHORT;

STATIC_ASSERT (sizeof(CDFS_DOUBLE_USHORT) == 4);

typedef struct CDFS_DOUBLE_ULONG
{
	ULONG LittleEndian;
	ULONG BigEndian;
} *PCDFS_DOUBLE_ULONG;

STATIC_ASSERT (sizeof(CDFS_DOUBLE_ULONG) == 8);

typedef struct CDFS_PACKED_DIRECTORY_RECORD
{
	UCHAR Length;
	UCHAR ExtAttrLength;
	UCHAR LocationOfExtent[ISODCL(3,10)];
	UCHAR DataLength[ISODCL(11,18)];
	UCHAR RecordingDateTime[ISODCL(19,25)];
	UCHAR FileFlags;
	UCHAR FileUnitSize;
	UCHAR InterleaveGapSize;
	UCHAR VolumeSequenceNumber[ISODCL(29,32)];
	UCHAR LengthOfFileIdentifier;
	UCHAR FileIdentifier[0];
} *PCDFS_PACKED_DIRECTORY_RECORD;

STATIC_ASSERT (sizeof(CDFS_PACKED_DIRECTORY_RECORD) == 33);

typedef struct CDFS_DIRECTORY_RECORD
{
	UCHAR Length;
	UCHAR ExtAttrLength;
	CDFS_DOUBLE_ULONG LocationOfExtent;
	CDFS_DOUBLE_ULONG DataLength;
	struct  
	{
		UCHAR YearsSince1900;
		UCHAR Month;	// 1-12
		UCHAR Day;		// 1-31
		UCHAR Hour;		// 0-23
		UCHAR Minute;	// 0-59
		UCHAR Second;	// 0-59
		UCHAR OffsetGMTin15minIntervals;
	} RecordingDateTime;
	struct  
	{
		UCHAR Existence : 1;
		UCHAR Directory : 1;
		UCHAR AssosiatedFile : 1;
		UCHAR Record : 1;
		UCHAR Protection : 1;
		UCHAR Reserved : 2;
		UCHAR MultiExtent : 1;
	} FileFlags;
	UCHAR FileUnitSize;
	UCHAR InterleaveGapSize;
	CDFS_DOUBLE_USHORT VolumeSequenceNumber;
	UCHAR LengthOfFileIdentifier;
	UCHAR FileIdentifier[0];
} *PCDFS_DIRECTORY_RECORD;

STATIC_ASSERT (sizeof(CDFS_DIRECTORY_RECORD) == 33);

typedef struct CDFS_PRIMARY_VOLUME_DESCRIPTOR
{
	UCHAR Type;								// 1
	UCHAR Identifier [ISODCL(2,6)];
	UCHAR Version;							// 7
	UCHAR Unused;							// 8
	UCHAR SystemIdentifier[ISODCL(9,40)];
	UCHAR VolumeIdentifier[ISODCL(41,72)];
	UCHAR Unused2[ISODCL(73,80)];
	CDFS_DOUBLE_ULONG VolumeSpaceSize;
	UCHAR Unused3[ISODCL(89,120)];
	CDFS_DOUBLE_USHORT VolumeSetSize;
	CDFS_DOUBLE_USHORT VolumeSequenceNumber;
	CDFS_DOUBLE_USHORT LogicalBlockSize;
	CDFS_DOUBLE_ULONG PathTableSize;
	ULONG LPathTableOccurrence;
	ULONG LPathTableOptionalOccurrence;
	ULONG MPathTableOccurrence;
	ULONG MPathTableOptionalOccurrence;
	UCHAR RootDirectoryRecord[ISODCL(157,190)];
	UCHAR VolumeSetIdentifier[ISODCL(191,318)];
	UCHAR PublisherIdentifier[ISODCL(319,446)];
	UCHAR DataPreparerIdentifier[ISODCL(447,574)];
	UCHAR ApplicationIdentifier[ISODCL(575,702)];
	UCHAR CopyrightFileIdentifier[ISODCL(703,739)];
	UCHAR AbstractFileIdentifier[ISODCL(740,776)];
	UCHAR BibliographicFileIdentifier[ISODCL(777,813)];
	UCHAR VolumeCreationDateTime[ISODCL(814,830)];
	UCHAR VolumeModificationDateTime[ISODCL(831,847)];
	UCHAR VolumeExpirationDateTime[ISODCL(848,864)];
	UCHAR VolumeEffectiveDateTime[ISODCL(865,881)];
	UCHAR FileStructureVersion;
	UCHAR Reserved;
	UCHAR ApplicationUse[ISODCL(884,1395)];
	UCHAR Reserved2[ISODCL(1396,2048)];
} *PCDFS_PRIMARY_VOLUME_DESCRIPTOR;

#pragma warning(default: 4200)
#pragma pack()

STATIC_ASSERT (sizeof(CDFS_PRIMARY_VOLUME_DESCRIPTOR) == 2048);

#define CDFS_SECTOR_SIZE 2048
