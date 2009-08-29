#include "common.h"
#include "ntfs.h"
#include "driver.h"

//
// NTFS error values
//

#define S_OK				STATUS_SUCCESS
#define S_FALSE				STATUS_UNSUCCESSFUL
#define ERROR_BAD_FORMAT	STATUS_FILE_SYSTEM_NOT_RECOGNIZED
#define ERROR_NOT_FOUND		STATUS_NOT_FOUND

#define ECOMPR  0xF1000001
#define EINVAL  0xF1000002
#define ENOENT  0xF1000003
#define EBADF   ERROR_BAD_FORMAT
#define ENOTDIR 0xF1000004
#define ESUCCESS S_OK
#define EISDIR  0xF1000005
#define EROFS   0xF1000006


//
//  Some important manifest constants.  These are the maximum byte size we'll ever
//  see for a file record or an index allocation buffer.
//

#define MAXIMUM_FILE_RECORD_SIZE		 (4096)

#define MAXIMUM_INDEX_ALLOCATION_SIZE	(4096)

#define MAXIMUM_COMPRESSION_UNIT_SIZE	(65536)


typedef STATUS ERRTYPE, *PERRTYPE;

ERRTYPE
NtfsLookupAttribute (
	IN PNTFS_STRUCTURE_CONTEXT StructureContext,
	IN LONGLONG FileRecord,
	IN ATTRIBUTE_TYPE_CODE TypeCode,
	OUT PBOOLEAN FoundAttribute,
	OUT PNTFS_ATTRIBUTE_CONTEXT AttributeContext
	);

ERRTYPE
NtfsReadResidentAttribute (
	IN PNTFS_STRUCTURE_CONTEXT StructureContext,
	IN PNTFS_ATTRIBUTE_CONTEXT AttributeContext,
	IN VBO Vbo,
	IN ULONG Length,
	IN PVOID Buffer
	);

ERRTYPE
NtfsReadNonresidentAttribute (
	IN PNTFS_STRUCTURE_CONTEXT StructureContext,
	IN PNTFS_ATTRIBUTE_CONTEXT AttributeContext,
	IN VBO Vbo,
	IN ULONG Length,
	IN PVOID Buffer
	);

ERRTYPE
NtfsReadAndDecodeFileRecord (
	IN PNTFS_STRUCTURE_CONTEXT StructureContext,
	IN LONGLONG FileRecord,
	OUT PULONG Index
	);

ERRTYPE
NtfsDecodeUsa (
	IN PVOID UsaBuffer,
	IN ULONG Length
	);

/*
typedef struct _STRING
{
	USHORT Length;
	USHORT MaximumLength;
	PCHAR Buffer;
} STRING, *PSTRING;
*/

ERRTYPE
NtfsSearchForFileName (
	IN PNTFS_STRUCTURE_CONTEXT StructureContext,
	IN PNTFS_ATTRIBUTE_CONTEXT IndexRoot,
	IN PNTFS_ATTRIBUTE_CONTEXT IndexAllocation OPTIONAL,
	IN PNTFS_ATTRIBUTE_CONTEXT AllocationBitmap OPTIONAL,
	IN STRING FileName,
	OUT PBOOLEAN FoundFileName,
	OUT PLONGLONG FileRecord,
	OUT PBOOLEAN IsDirectory
	);

ERRTYPE
NtfsIsRecordAllocated (
	IN PNTFS_STRUCTURE_CONTEXT StructureContext,
	IN PNTFS_ATTRIBUTE_CONTEXT AttributeContext,
	IN ULONG BitOffset,
	OUT PBOOLEAN IsAllocated
	);
	
#define FileReferenceToLargeInteger(FR,LI) {	 \
	*(LI) = *(PLONGLONG)&(FR);			  \
	((PFILE_REFERENCE)(LI))->SequenceNumber = 0; \
}

#define COMPRESSION_FORMAT_NONE          (0x0000)   
#define COMPRESSION_FORMAT_LZNT1         (0x0002)   

#define InitializeAttributeContext(SC,FRB,AH,FR,AC) {						\
	(AC)->TypeCode = (AH)->TypeCode;										 \
	(AC)->FileRecord = (FR);												 \
	(AC)->FileRecordOffset = (USHORT)PtrOffset((FRB),(AH));				  \
	if ((AC)->IsAttributeResident = ((AH)->FormCode == RESIDENT_FORM)) {	 \
		(AC)->DataSize = /*xxFromUlong*/((AH)->Form.Resident.ValueLength);   \
	} else {																 \
		(AC)->DataSize = (AH)->Form.Nonresident.FileSize;					\
	}																		\
	(AC)->CompressionFormat = COMPRESSION_FORMAT_NONE;					   \
	if ((AH)->Flags & ATTRIBUTE_FLAG_COMPRESSION_MASK) {					 \
		ULONG _i;															\
		(AC)->CompressionFormat = COMPRESSION_FORMAT_LZNT1;				  \
		(AC)->CompressionUnit = (SC)->BytesPerCluster;					   \
		for (_i = 0; _i < (AH)->Form.Nonresident.CompressionUnit; _i += 1) { \
			(AC)->CompressionUnit *= 2;									  \
		}																	\
	}																		\
}

#define NtfsReadAttribute(A,B,C,D,E) { ERRTYPE _s;									\
	if ((B)->IsAttributeResident) {												  \
		if ((_s = NtfsReadResidentAttribute(A,B,C,D,E)) != S_OK) {return _s;}	\
	} else {																		 \
		if ((_s = NtfsReadNonresidentAttribute(A,B,C,D,E)) != S_OK) {return _s;} \
	}																				\
}

#define FlagOn(Flags,SingleFlag) ((BOOLEAN)(((Flags) & (SingleFlag)) != 0))
#define SetFlag(Flags,SingleFlag) { (Flags) |= (SingleFlag); }
#define ClearFlag(Flags,SingleFlag) { (Flags) &= ~(SingleFlag); }

#define Add2Ptr(POINTER,INCREMENT) ((PVOID)((PUCHAR)(POINTER) + (INCREMENT)))
#define PtrOffset(BASE,OFFSET) ((ULONG)((ULONG)(OFFSET) - (ULONG)(BASE)))

#define Minimum(X,Y) ((X) < (Y) ? (X) : (Y))

#define IsCharZero(C)	(((C) & 0x000000ff) == 0x00000000)
#define IsCharLtrZero(C) (((C) & 0x00000080) == 0x00000080)

typedef union _UCHAR1 { UCHAR  Uchar[1]; UCHAR  ForceAlignment; } UCHAR1, *PUCHAR1;
typedef union _UCHAR2 { UCHAR  Uchar[2]; USHORT ForceAlignment; } UCHAR2, *PUCHAR2;
typedef union _UCHAR4 { UCHAR  Uchar[4]; ULONG  ForceAlignment; } UCHAR4, *PUCHAR4;

//
//  This macro copies an unaligned src byte to an aligned dst byte
//

#define CopyUchar1(Dst,Src) {								\
	*((UCHAR1 *)(Dst)) = *((UNALIGNED UCHAR1 *)(Src)); \
	}

//
//  This macro copies an unaligned src word to an aligned dst word
//

#define CopyUchar2(Dst,Src) {								\
	*((UCHAR2 *)(Dst)) = *((UNALIGNED UCHAR2 *)(Src)); \
	}

//
//  This macro copies an unaligned src longword to an aligned dsr longword
//

#define CopyUchar4(Dst,Src) {								\
	*((UCHAR4 *)(Dst)) = *((UNALIGNED UCHAR4 *)(Src)); \
	}

UCHAR NtfsBuffer0[MAXIMUM_FILE_RECORD_SIZE+256];
UCHAR NtfsBuffer1[MAXIMUM_FILE_RECORD_SIZE+256];
UCHAR NtfsBuffer2[MAXIMUM_INDEX_ALLOCATION_SIZE+256];
//UCHAR NtfsBuffer3[MAXIMUM_COMPRESSION_UNIT_SIZE+256];
//UCHAR NtfsBuffer4[MAXIMUM_COMPRESSION_UNIT_SIZE+256];

PACKED_BOOT_SECTOR BootSector;
UCHAR NtfsMftBuffer[MAXIMUM_FILE_RECORD_SIZE+256];
PFILE_RECORD_SEGMENT_HEADER NtfsMftFrs = 
	(PFILE_RECORD_SEGMENT_HEADER) &NtfsMftBuffer;

PFILE_RECORD_SEGMENT_HEADER NtfsFrs0 = 
	(PFILE_RECORD_SEGMENT_HEADER) &NtfsBuffer1;

PINDEX_ALLOCATION_BUFFER NtfsIndexAllocationBuffer = 
	(PINDEX_ALLOCATION_BUFFER) &NtfsBuffer2;

#define BUFFER_COUNT (64)

USHORT NtfsFileRecordBufferPinned[BUFFER_COUNT];
VBO NtfsFileRecordBufferVbo[BUFFER_COUNT];
PFILE_RECORD_SEGMENT_HEADER NtfsFileRecordBuffer[BUFFER_COUNT];

//UCHAR NtfsBuffers[BUFFER_COUNT][MAXIMUM_FILE_RECORD_SIZE+256];

VOID
NtfsInitialize(
	)
{
	for (ULONG i=0; i<BUFFER_COUNT; i++)
	{
		NtfsFileRecordBuffer[i] = NULL;
			//(PFILE_RECORD_SEGMENT_HEADER) &NtfsBuffers[i];

		NtfsFileRecordBufferVbo[i] = -1;
		NtfsFileRecordBufferPinned[i] = 0;
	}
}

UCHAR Buffer [SECTOR_SIZE];

ERRTYPE
NtfsReadDisk (
	IN PNTFS_VCB Vcb,
	IN ULONGLONG Offset,
	IN ULONG Size,
	OUT PVOID Buffer
	)
{
	ULONG SizeOffset = Size % SECTOR_SIZE;
	ULONG SizeAligned = Size - SizeOffset;
	IO_STATUS_BLOCK IoStatus;
	LARGE_INTEGER FileOffset;
	STATUS Status;

	FileOffset.QuadPart = Offset;

	if (SizeOffset == 0)
	{
		Status = IoReadFile (Vcb->RawFileObject, 
			Buffer,
			Size,
			&FileOffset,
			0,
			&IoStatus
			);

		return Status;
	}

	// Read aligned buffer
	Status = IoReadFile (Vcb->RawFileObject,
		Buffer,
		SizeAligned,
		&FileOffset,
		0,
		&IoStatus
		);

	if (SUCCESS(Status))
	{
		Status = IoReadFile (Vcb->RawFileObject,
			::Buffer,
			SECTOR_SIZE,
			NULL,
			0,
			&IoStatus
			);

		if (SUCCESS(Status))
		{
			memcpy ((PUCHAR)Buffer - SizeAligned, ::Buffer, SizeOffset);
		}
	}

	return Status;
}

VOID
NtfsFirstComponent (
	IN OUT PSTRING String,
	OUT PSTRING FirstComponent
	)
{
	ULONG Index;

	//
	//  Copy over the string variable into the first component variable
	//

	*FirstComponent = *String;

	//
	//  Now if the first character in the name is a backslash then
	//  simply skip over the backslash.
	//

	if (FirstComponent->Buffer[0] == '\\') {

		FirstComponent->Buffer += 1;
		FirstComponent->Length -= 1;
	}

	//
	//  Now search the name for a backslash
	//

	for (Index = 0; Index < FirstComponent->Length; Index += 1) {

		if (FirstComponent->Buffer[Index] == '\\') {

			break;
		}
	}

	//
	//  At this point Index denotes a backslash or is equal to the length of the
	//  string.  So update string to be the remaining part.  Decrement the length of
	//  the first component by the approprate amount
	//

	String->Buffer = &FirstComponent->Buffer[Index];
	String->Length = (SHORT)(FirstComponent->Length - Index);

	FirstComponent->Length = (SHORT)Index;

	//
	//  And return to our caller.
	//

	return;
}

ERRTYPE
NtfsDecodeRetrievalInformation (
	IN PNTFS_STRUCTURE_CONTEXT StructureContext,
	IN PNTFS_MCB Mcb,
	IN VBO Vbo,
	IN PATTRIBUTE_RECORD_HEADER AttributeHeader
	);

__declspec(naked)
ULONG
RtlCompareMemory(
	PVOID buf1,
	PVOID buf2,
	ULONG Count
	)
{
	__asm
	{
		push esi
		push edi

		mov ecx, [Count]
		mov esi, [buf1]
		mov edi, [buf2]

		rep cmpsb

		pop esi
		pop edi
		mov eax, ecx

		ret
	}
}
	

BOOLEAN
IsNtfsFileStructure (
	IN PNTFS_VCB Vcb
	)
{
	BIOS_PARAMETER_BLOCK Bpb;
	PNTFS_STRUCTURE_CONTEXT StructureContext = &Vcb->StructureContext;

	if (NtfsReadDisk (Vcb, 0, sizeof(PACKED_BOOT_SECTOR), &BootSector) != S_OK)
	{
		return FALSE;
	}

	NtfsUnpackBios (&Bpb, &BootSector.PackedBpb);

	if (RtlCompareMemory (&BootSector.Oem[0], "NTFS    ", 8) != 8)
	{
		return FALSE;
	}

	if (Bpb.ReservedSectors != 0 ||
		Bpb.Fats != 0 ||
		Bpb.RootEntries != 0 ||
		Bpb.Sectors != 0 ||
		Bpb.SectorsPerFat != 0 ||
		Bpb.LargeSectors != 0)
	{
		return FALSE;
	}

	switch (Bpb.Media)
	{
	case 0xf0:
	case 0xf8:
	case 0xf9:
	case 0xfc:
	case 0xfd:
	case 0xfe:
	case 0xff:
		break;
	default:
		return FALSE;
	}

	switch (Bpb.BytesPerSector)
	{
	case 128:
	case 256:
	case 512:
	case 1024:
	case 2048:
		break;
	default:
		return FALSE;
	}

	switch (Bpb.SectorsPerCluster)
	{
	case 1:
	case 2:
	case 4:
	case 8:
	case 16:
	case 32:
	case 64:
	case 128:
		break;
	default:
		return FALSE;
	}

	if (BootSector.ClustersPerFileRecordSegment < 0 &&
		(BootSector.ClustersPerFileRecordSegment > -9 ||
		BootSector.ClustersPerFileRecordSegment < -31))
	{
		return FALSE;
	}

	ULONG ClusterSize, FileRecordSize;

	StructureContext->Vcb = Vcb;
	StructureContext->BytesPerCluster = 
	ClusterSize = Bpb.SectorsPerCluster * Bpb.BytesPerSector;

	if (BootSector.ClustersPerFileRecordSegment > 0)
	{
		StructureContext->BytesPerFileRecord = 
		FileRecordSize = BootSector.ClustersPerFileRecordSegment * ClusterSize;
	}
	else
	{
		StructureContext->BytesPerFileRecord = 
		FileRecordSize = 1 << (-1 * BootSector.ClustersPerFileRecordSegment);
	}

	if (NtfsReadDisk (
		Vcb,
		BootSector.MftStartLcn * ClusterSize,
		FileRecordSize,
		&NtfsMftBuffer
		) != S_OK)
	{
		return FALSE;
	}

	if (NtfsDecodeUsa (&NtfsMftBuffer, FileRecordSize) != S_OK)
	{
		return FALSE;
	}

	if (!FlagOn (NtfsMftFrs->Flags, FILE_RECORD_SEGMENT_IN_USE))
	{
		return FALSE;
	}

	PATTRIBUTE_RECORD_HEADER AttributeHeader;

	for (AttributeHeader = NtfsFirstAttribute (NtfsMftFrs);
		AttributeHeader->TypeCode != $DATA || AttributeHeader->NameLength != 0;
		AttributeHeader = NtfsGetNextRecord (AttributeHeader))
	{
		if (AttributeHeader->TypeCode == $END)
		{
			return FALSE;
		}
	}

	if (AttributeHeader->FormCode != NONRESIDENT_FORM)
	{
		return FALSE;
	}

	InitializeAttributeContext (StructureContext,
								NtfsMftFrs,
								AttributeHeader,
								0,
								&StructureContext->MftAttributeContext);


	if (NtfsDecodeRetrievalInformation( StructureContext,
										&StructureContext->MftBaseMcb,
										0,
										AttributeHeader ) != S_OK)
	{
		return NULL;
	}

	return TRUE;
}


#define ToUpper(x) (((x) >= 'a' && (x) <= 'z') ? ((x) + 'A' - 'a') : (x))

BOOLEAN
NtfsAreNamesEqual (
	IN STRING AnsiString,
	IN UNICODE_STRING UnicodeString
	)
{
	ULONG i;

	if (AnsiString.Length*2 != UnicodeString.Length)
	{
		return FALSE;
	}

	for (i = 0; i < AnsiString.Length; i += 1)
	{
		if (ToUpper((USHORT)AnsiString.Buffer[i]) != ToUpper(UnicodeString.Buffer[i]))
		{
			return FALSE;
		}
	}

	return TRUE;
}

extern "C" extern VOID KEAPI RtlInitString (PSTRING, PCHAR);

ERRTYPE
NtfsSearchForFileName (
	IN PNTFS_STRUCTURE_CONTEXT StructureContext,
	IN PNTFS_ATTRIBUTE_CONTEXT IndexRoot,
	IN PNTFS_ATTRIBUTE_CONTEXT IndexAllocation OPTIONAL,
	IN PNTFS_ATTRIBUTE_CONTEXT AllocationBitmap OPTIONAL,
	IN STRING FileName,
	OUT PBOOLEAN FoundFileName,
	OUT PLONGLONG FileRecord,
	OUT PBOOLEAN IsDirectory
	)
{
	PATTRIBUTE_RECORD_HEADER IndexAttributeHeader;
	PINDEX_ROOT IndexRootValue;
	PINDEX_HEADER IndexHeader;
	ERRTYPE Err;

	ULONG NextIndexBuffer;
	ULONG BytesPerIndexBuffer;
	ULONG BufferIndex;

	*FoundFileName = FALSE;

	Err = NtfsReadAndDecodeFileRecord ( StructureContext,
										IndexRoot->FileRecord,
										&BufferIndex );

	if (Err != S_OK)
		return Err;

	IndexAttributeHeader = (PATTRIBUTE_RECORD_HEADER)
		Add2Ptr (NtfsFileRecordBuffer[BufferIndex], IndexRoot->FileRecordOffset);

	IndexRootValue = NtfsGetValue (IndexAttributeHeader);
	IndexHeader = &IndexRootValue->IndexHeader;

	NextIndexBuffer = 0;
	BytesPerIndexBuffer = IndexRootValue->BytesPerIndexBuffer;

	while (TRUE)
	{
		PINDEX_ENTRY IndexEntry;
		BOOLEAN IsAllocated;
		VBO Vbo;

		for (IndexEntry = (PINDEX_ENTRY)Add2Ptr(IndexHeader,IndexHeader->FirstIndexEntry);
			!FlagOn (IndexEntry->Flags, INDEX_ENTRY_END);
			IndexEntry = (PINDEX_ENTRY)Add2Ptr(IndexEntry, IndexEntry->Length))
		{
			PFILE_NAME FileNameEntry;
			UNICODE_STRING UnicodeFileName;

			FileNameEntry = (PFILE_NAME) Add2Ptr (IndexEntry, sizeof(INDEX_ENTRY));

			UnicodeFileName.Length = FileNameEntry->FileNameLength * 2;
			UnicodeFileName.Buffer = &FileNameEntry->FileName[0];

			if (NtfsAreNamesEqual (FileName, UnicodeFileName))
			{
				*FoundFileName = TRUE;

				FileReferenceToLargeInteger (IndexEntry->FileReference, FileRecord);

				*IsDirectory = FlagOn (FileNameEntry->Info.FileAttributes,
					DUP_FILE_NAME_INDEX_PRESENT);

				NtfsFileRecordBufferPinned[BufferIndex] --;

				return S_OK;
			}
		}

		if (!(IndexAllocation) ||
			!(AllocationBitmap))
		{
			NtfsFileRecordBufferPinned[BufferIndex] --;
			return S_OK;
		}

		IsAllocated = FALSE;

		while (!IsAllocated)
		{

			Vbo = BytesPerIndexBuffer * NextIndexBuffer;
			if (Vbo >= IndexAllocation->DataSize)
				return S_OK;

			Err = NtfsIsRecordAllocated(StructureContext,
										AllocationBitmap,
										NextIndexBuffer,
										&IsAllocated);
			if (Err != S_OK)
			{
				return Err;
			}

			NextIndexBuffer += 1;
		}

		NtfsReadAttribute (	StructureContext,
							IndexAllocation,
							Vbo,
							BytesPerIndexBuffer,
							NtfsIndexAllocationBuffer);

		Err = NtfsDecodeUsa (NtfsIndexAllocationBuffer, BytesPerIndexBuffer);
		if (Err != S_OK)
			return Err;

		IndexHeader = &NtfsIndexAllocationBuffer->IndexHeader;
	}
}

ERRTYPE
NtfsIsRecordAllocated (
	IN PNTFS_STRUCTURE_CONTEXT StructureContext,
	IN PNTFS_ATTRIBUTE_CONTEXT AllocationBitmap,
	IN ULONG BitOffset,
	OUT PBOOLEAN IsAllocated
	)
{
	ULONG ByteIndex;
	ULONG BitIndex;
	UCHAR LocalByte;

	ByteIndex = BitOffset / 8;
	BitIndex = BitOffset % 8;

	*IsAllocated = FALSE;

	NtfsReadAttribute (	StructureContext,
						AllocationBitmap,
						ByteIndex,
						1,
						&LocalByte );

	if (FlagOn(LocalByte >> BitIndex, 0x01))
	{
		*IsAllocated = TRUE;
	}

	return S_OK;
}

ERRTYPE
NtfsDecodeUsa (
	IN PVOID UsaBuffer,
	IN ULONG Length
	)
{
	PMULTI_SECTOR_HEADER Hdr;
	PUSHORT UsaOffset;
	ULONG UsaSize;
	ULONG i;
	PUSHORT ProtectedUshort;

	Hdr = (PMULTI_SECTOR_HEADER)UsaBuffer;

	UsaOffset = (PUSHORT) Add2Ptr (UsaBuffer, Hdr->UpdateSequenceArrayOffset);
	UsaSize = Hdr->UpdateSequenceArraySize;

	for (i = 1; i < UsaSize; i += 1 )
	{
		ProtectedUshort = (PUSHORT) Add2Ptr (UsaBuffer, SEQUENCE_NUMBER_STRIDE * i - sizeof(USHORT));
		if (*ProtectedUshort != UsaOffset[0])
		{
			return ERROR_BAD_FORMAT;
		}

		*ProtectedUshort = UsaOffset[i];
	}

	return S_OK;
}

ERRTYPE
NtfsReadResidentAttribute (
	IN PNTFS_STRUCTURE_CONTEXT StructureContext,
	IN PNTFS_ATTRIBUTE_CONTEXT AttributeContext,
	IN VBO Vbo,
	IN ULONG Length,
	IN PVOID Buffer
	)
{
	PATTRIBUTE_RECORD_HEADER AttributeHeader;
	ULONG BufferIndex;
	ERRTYPE Err;

	Err =
	NtfsReadAndDecodeFileRecord (StructureContext,
								 AttributeContext->FileRecord,
								 &BufferIndex);

	if (Err != S_OK) return Err;

	AttributeHeader = (PATTRIBUTE_RECORD_HEADER) 
		Add2Ptr (NtfsFileRecordBuffer[BufferIndex],
				 AttributeContext->FileRecordOffset);

	memmove (Buffer, Add2Ptr(NtfsGetValue(AttributeHeader), ((ULONG)Vbo)), Length);

	NtfsFileRecordBufferPinned[BufferIndex] --;
	return S_OK;
}

ERRTYPE
NtfsDecodeRetrievalInformation (
	IN PNTFS_STRUCTURE_CONTEXT StructureContext,
	IN PNTFS_MCB Mcb,
	IN VBO Vbo,
	IN PATTRIBUTE_RECORD_HEADER AttributeHeader
	)
{
	ULONG BytesPerCluster;

	VBO NextVbo;
	LBO CurrentLbo;
	VBO CurrentVbo;

	LONGLONG Change;
	PCHAR ch;
	ULONG VboBytes;
	ULONG LboBytes;

	//
	//  Initialize our locals
	//

	BytesPerCluster = StructureContext->BytesPerCluster;

	//
	//  Setup the next vbo and current lbo and ch for the following loop that decodes
	//  the retrieval information
	//

	NextVbo = /*xxXMul*/(AttributeHeader->Form.Nonresident.LowestVcn * BytesPerCluster);

	CurrentLbo = 0;

	ch = (PCHAR) Add2Ptr( AttributeHeader,
				  AttributeHeader->Form.Nonresident.MappingPairsOffset );

	Mcb->InUse = 0;

	//
	//  Loop to process mapping pairs
	//

	while (!IsCharZero(*ch)) {

		//
		//  Set current Vbo from initial value or last pass through loop
		//

		CurrentVbo = NextVbo;

		//
		//  Extract the counts from the two nibbles of this byte
		//

		VboBytes = *ch & 0x0f;
		LboBytes = *ch++ >> 4;

		//
		//  Extract the Vbo change and update next vbo
		//

		Change = 0;

		if (IsCharLtrZero(*(ch + VboBytes - 1))) {

			return EINVAL;
		}

		memmove( &Change, ch, VboBytes );

		ch += VboBytes;

		NextVbo = /*xxAdd*/(NextVbo + /*xXMul*/(Change * BytesPerCluster));

		//
		//  If we have reached the maximum for this mcb then it is time
		//  to return and not decipher any more retrieval information
		//

		if (Mcb->InUse >= MAXIMUM_NUMBER_OF_MCB_ENTRIES - 1) {

			break;
		}

		//
		//  Now check if there is an lbo change.  If there isn't
		//  then we only need to update the vbo, because this
		//  is sparse/compressed file.
		//

		if (LboBytes != 0) {

			//
			//  Extract the Lbo change and update current lbo
			//

			Change = 0;

			if (IsCharLtrZero(*(ch + LboBytes - 1))) {

				Change = /*xxSub*/( Change - 1 );
			}

			memmove( &Change, ch, LboBytes );

			ch += LboBytes;

			CurrentLbo = /*xxAdd*/( CurrentLbo + /*xxXMul*/(Change * BytesPerCluster));
		}

		//
		//  Now check if the Next Vbo is greater than the Vbo we after
		//

		if (/*xxGeq*/(NextVbo >= Vbo)) {

			//
			//  Load this entry into the mcb and advance our in use counter
			//

			Mcb->Vbo[Mcb->InUse]	 = CurrentVbo;
			Mcb->Lbo[Mcb->InUse]	 = (LboBytes != 0 ? CurrentLbo : 0);
			Mcb->Vbo[Mcb->InUse + 1] = NextVbo;

			Mcb->InUse += 1;
		}
	}

	return S_OK;
}

ERRTYPE
NtfsLoadMcb (
	IN PNTFS_STRUCTURE_CONTEXT StructureContext,
	IN PNTFS_ATTRIBUTE_CONTEXT AttributeContext,
	IN VBO Vbo,
	IN PNTFS_MCB Mcb
	)
{
	PATTRIBUTE_RECORD_HEADER AttributeHeader;
	ULONG BytesPerCluster;
	VBO LowestVbo;
	VBO HighestVbo;
	ERRTYPE Err;

	LONGLONG FileRecord;

	NTFS_ATTRIBUTE_CONTEXT AttributeContext1;
	PNTFS_ATTRIBUTE_CONTEXT AttributeList;

	LONGLONG li;
	LONGLONG Previousli;
	ATTRIBUTE_LIST_ENTRY AttributeListEntry;

	ATTRIBUTE_TYPE_CODE TypeCode;

	ULONG BufferIndex;
	ULONG SavedBufferIndex;

	//
	//  Load our local variables
	//

	BytesPerCluster = StructureContext->BytesPerCluster;

	//
	//  Setup a pointer to the cached mcb, indicate the attribute context that is will
	//  now own the cached mcb, and zero out the mcb
	//

	Mcb->InUse = 0;

	//
	//  Read in the file record that contains the non-resident attribute and get a
	//  pointer to the attribute header
	//

	Err = NtfsReadAndDecodeFileRecord( StructureContext,
							 AttributeContext->FileRecord,
							 &BufferIndex );

	if (Err != S_OK)
		return Err;

	AttributeHeader = (PATTRIBUTE_RECORD_HEADER) Add2Ptr( NtfsFileRecordBuffer[BufferIndex],
							   AttributeContext->FileRecordOffset );

	//
	//  Compute the lowest and highest vbo that is described by this attribute header
	//

	LowestVbo  = /*xxXMul*/( AttributeHeader->Form.Nonresident.LowestVcn *
						 BytesPerCluster );

	HighestVbo = /*xxXMul*/( AttributeHeader->Form.Nonresident.HighestVcn *
						 BytesPerCluster );

	//
	//  Now check if the vbo we are after is within the range of this attribute header
	//  and if so then decode the retrieval information and return to our caller
	//

	if (/*xxLeq*/(LowestVbo <= Vbo) && /*xxLeq*/(Vbo <= HighestVbo)) {

		Err = NtfsDecodeRetrievalInformation( StructureContext, Mcb, Vbo, AttributeHeader );
		if (Err != S_OK)
			return Err;

		NtfsFileRecordBufferPinned[BufferIndex] -= 1;

		return S_OK;
	}

	//
	//  At this point the attribute header does not contain the range we need so read
	//  in the base file record and we'll search the attribute list for a attribute
	//  header that we need.
	//

	//****if (!xxEqlZero(NtfsFileRecordBuffer[BufferIndex]->BaseFileRecordSegment)) {

		FileReferenceToLargeInteger( NtfsFileRecordBuffer[BufferIndex]->BaseFileRecordSegment,
									 &FileRecord );

		NtfsFileRecordBufferPinned[BufferIndex] -= 1;

		Err = NtfsReadAndDecodeFileRecord( StructureContext,
								 FileRecord,
								 &BufferIndex );
		if (Err != S_OK)
			return Err;
	//****}

	//
	//  Now we have read in the base file record so search for the attribute list
	//  attribute
	//

	AttributeList = NULL;

	for (AttributeHeader = NtfsFirstAttribute( NtfsFileRecordBuffer[BufferIndex] );
		 AttributeHeader->TypeCode != $END;
		 AttributeHeader = NtfsGetNextRecord( AttributeHeader )) {

		//
		//  Check if this is the attribute list attribute and if so then setup a local
		//  attribute context
		//

		if (AttributeHeader->TypeCode == $ATTRIBUTE_LIST) {

			InitializeAttributeContext( StructureContext,
										NtfsFileRecordBuffer[BufferIndex],
										AttributeHeader,
										FileRecord,
										AttributeList = &AttributeContext1 );
		}
	}

	//
	//  We have better located an attribute list otherwise we're in trouble
	//

	if (AttributeList == NULL) {

		NtfsFileRecordBufferPinned[BufferIndex] -= 1;

		return EINVAL;
	}

	//
	//  Setup a local for the type code
	//

	TypeCode = AttributeContext->TypeCode;

	//
	//  Now that we've located the attribute list we need to continue our search.  So
	//  what this outer loop does is search down the attribute list looking for a
	//  match.
	//

	NtfsFileRecordBufferPinned[SavedBufferIndex = BufferIndex] += 1;

	for (Previousli = li = 0;
		 /*xxLtr*/(li < AttributeList->DataSize);
		 li = /*xxAdd*/(li + /*xxFromUlong*/(AttributeListEntry.RecordLength))) {

		//
		//  Read in the attribute list entry.  We don't need to read in the name,
		//  just the first part of the list entry.
		//

		NtfsReadAttribute( StructureContext,
					   AttributeList,
					   li,
					   sizeof(ATTRIBUTE_LIST_ENTRY),
					   &AttributeListEntry );

		if (Err != S_OK)
			return Err;

		//
		//  Now check if the attribute matches, and either it is not $data or if it
		//  is $data then it is unnamed
		//

		if ((AttributeListEntry.AttributeTypeCode == TypeCode)

					&&

			((TypeCode != $DATA) ||
			 ((TypeCode == $DATA) && (AttributeListEntry.AttributeNameLength == 0)))) {

			//
			//  If the lowest vcn is is greater than the vbo we've after then
			//  we are done and can use previous li otherwise set previous li accordingly.

			if (Vbo < AttributeListEntry.LowestVcn * BytesPerCluster) {

				break;
			}

			Previousli = li;
		}
	}

	//
	//  Now we should have found the offset for the attribute list entry
	//  so read it in and verify that it is correct
	//

	NtfsReadAttribute( StructureContext,
				   AttributeList,
				   Previousli,
				   sizeof(ATTRIBUTE_LIST_ENTRY),
				   &AttributeListEntry );

	if (Err != S_OK)
		return Err;

	if ((AttributeListEntry.AttributeTypeCode == TypeCode)

				&&

		((TypeCode != $DATA) ||
		 ((TypeCode == $DATA) && (AttributeListEntry.AttributeNameLength == 0)))) {

		//
		//  We found a match so now compute the file record containing this
		//  attribute and read in the file record
		//

		FileReferenceToLargeInteger( AttributeListEntry.SegmentReference, &FileRecord );

		NtfsFileRecordBufferPinned[BufferIndex] -= 1;

		Err = NtfsReadAndDecodeFileRecord( StructureContext,
								 FileRecord,
								 &BufferIndex );

		if (Err != S_OK)
			return Err;

		//
		//  Now search down the file record for our matching attribute, and it
		//  better be there otherwise the attribute list is wrong.
		//

		for (AttributeHeader = NtfsFirstAttribute( NtfsFileRecordBuffer[BufferIndex] );
			 AttributeHeader->TypeCode != $END;
			 AttributeHeader = NtfsGetNextRecord( AttributeHeader )) {

			//
			//  As a quick check make sure that this attribute is non resident
			//

			if (AttributeHeader->FormCode == NONRESIDENT_FORM) {

				//
				//  Compute the range of this attribute header
				//

				LowestVbo  = /*xxXMul*/( AttributeHeader->Form.Nonresident.LowestVcn *
									 BytesPerCluster );

				HighestVbo = /*xxXMul*/( AttributeHeader->Form.Nonresident.HighestVcn *
									 BytesPerCluster);

				//
				//  We have located the attribute in question if the type code
				//  match, it is within the proper range, and if it is either not
				//  the data attribute or if it is the data attribute then it is
				//  also unnamed
				//

				if ((AttributeHeader->TypeCode == TypeCode)

							&&

					/*xxLeq*/(LowestVbo <= Vbo) && /*xxLeq*/(Vbo <= HighestVbo)

							&&

					((TypeCode != $DATA) ||
					 ((TypeCode == $DATA) && (AttributeHeader->NameLength == 0)))) {

					//
					//  We've located the attribute so now it is time to decode
					//  the retrieval information and return to our caller
					//

					Err = NtfsDecodeRetrievalInformation( StructureContext,
												Mcb,
												Vbo,
												AttributeHeader );

					if (Err != S_OK)
						return Err;

					NtfsFileRecordBufferPinned[BufferIndex] -= 1;
					NtfsFileRecordBufferPinned[SavedBufferIndex] -= 1;

					return S_OK;
				}
			}
		}
	}

	NtfsFileRecordBufferPinned[BufferIndex] -= 1;
	NtfsFileRecordBufferPinned[SavedBufferIndex] -= 1;

	return EINVAL;
}

ULONG LastMcb = 0;

ERRTYPE
NtfsVboToLbo (
	IN PNTFS_STRUCTURE_CONTEXT StructureContext,
	IN PNTFS_ATTRIBUTE_CONTEXT AttributeContext,
	IN VBO Vbo,
	OUT PLBO Lbo,
	OUT PULONG ByteCount
	)
{
	PNTFS_MCB Mcb;
	ULONG i;
	ERRTYPE Err;

	Mcb = NULL;

	if (AttributeContext == &StructureContext->MftAttributeContext)
	{
		Mcb = &StructureContext->MftBaseMcb;

		if (Vbo < Mcb->Vbo[0] || Vbo >= Mcb->Vbo[Mcb->InUse])
		{
			Mcb = NULL;
		}
	}

	if (Mcb == NULL)
	{
		for (i = 0; i < 16; i += 1)
		{
			Mcb = &StructureContext->CachedMcb[i];

			if (AttributeContext->FileRecord == StructureContext->CachedMcbFileRecord[i] &&
				AttributeContext->FileRecordOffset == StructureContext->CachedMcbFileRecordOffset[i] &&
				Mcb->Vbo[0] <= Vbo &&
				Vbo < Mcb->Vbo[Mcb->InUse])
			{
				break;
			}

			Mcb = NULL;
		}

		if (Mcb == NULL)
		{
			Mcb = &StructureContext->CachedMcb[LastMcb % 16];
			StructureContext->CachedMcbFileRecord[LastMcb % 16] = AttributeContext->FileRecord;
			StructureContext->CachedMcbFileRecordOffset[LastMcb % 16] = AttributeContext->FileRecordOffset;

			LastMcb ++;

			Err = NtfsLoadMcb ( StructureContext,
								AttributeContext,
								Vbo,
								Mcb );

			if (Err != S_OK)
				return Err;
		}
	}

	for (i = 0; i < Mcb->InUse; i += 1)
	{
		if (Vbo < Mcb->Vbo[i+1])
		{
			if (Mcb->Lbo[i] != 0)
			{
				*Lbo = Mcb->Lbo[i] + (Vbo - Mcb->Vbo[i]);
			}
			else
			{
				*Lbo = 0;
			}

			*ByteCount = ((ULONG)(Mcb->Vbo[i+1] - Vbo));

			return S_OK;
		}
	}

	return EINVAL;
}

ERRTYPE
NtfsReadNonresidentAttribute (
	IN PNTFS_STRUCTURE_CONTEXT StructureContext,
	IN PNTFS_ATTRIBUTE_CONTEXT AttributeContext,
	IN VBO Vbo,
	IN ULONG Length,
	IN PVOID Buffer
	)
{
	ERRTYPE Err;

	if (AttributeContext->CompressionFormat != 0)
	{
		//
		// File is compressed!?
		//

		return ECOMPR;
	}

	while (Length > 0)
	{
		LBO Lbo;
		ULONG CurrentRunByteCount;

		if((Err = NtfsVboToLbo (StructureContext,
								AttributeContext,
								Vbo,
								&Lbo,
								&CurrentRunByteCount)) != S_OK)
		{
			return Err;
		}

		while ((Length > 0) && (CurrentRunByteCount > 0))
		{
			LONG SingleReadSize;

			SingleReadSize = Minimum (Length, 32*1024);
			SingleReadSize = Minimum ((ULONG)SingleReadSize, CurrentRunByteCount);

			if ((Vbo + SingleReadSize) > AttributeContext->DataSize)
			{
				SingleReadSize = ((ULONG)(AttributeContext->DataSize - Vbo));

				if (SingleReadSize <= 0)
				{
					return S_OK;
				}

				Length = SingleReadSize;
			}
			
			Err = NtfsReadDisk (StructureContext->Vcb,
								Lbo,
								SingleReadSize,
								Buffer);
			if (Err != S_OK)
				return Err;

			Length -= SingleReadSize;
			CurrentRunByteCount -= SingleReadSize;
			Lbo = Lbo + SingleReadSize;
			Vbo = Vbo + SingleReadSize;

			Buffer = (PCHAR)Buffer + SingleReadSize;
		}
	}

	return S_OK;
}

ERRTYPE
NtfsReadAndDecodeFileRecord (
	IN PNTFS_STRUCTURE_CONTEXT StructureContext,
	IN LONGLONG FileRecord,
	OUT PULONG Index
	)
{
	ERRTYPE Err;

	for (*Index = 0; (*Index < BUFFER_COUNT) && (NtfsFileRecordBuffer[*Index] != NULL); *Index += 1)
	{
		if (NtfsFileRecordBufferVbo[*Index] == FileRecord)
		{
			++ NtfsFileRecordBufferPinned[*Index];
			return S_OK;
		}
	}

	for (*Index = 0; (*Index < BUFFER_COUNT) && (NtfsFileRecordBufferPinned[*Index] != 0); *Index += 1)
	{
		;
	}

	if (*Index == BUFFER_COUNT)
		return ERROR_NOT_FOUND;

	if (NtfsFileRecordBuffer[*Index] == NULL)
	{
//		KdPrint(( __FUNCTION__ " : general failure (NULL pointer)\n"));
//		return S_FALSE;

		NtfsFileRecordBuffer[*Index] = (PFILE_RECORD_SEGMENT_HEADER) ExAllocateHeap (TRUE, MAXIMUM_FILE_RECORD_SIZE+256);
		memset (NtfsFileRecordBuffer[*Index], 0, MAXIMUM_FILE_RECORD_SIZE+256);
		KdPrint(("Allocated new file record buffer [%08x] at %08x\n", *Index, NtfsFileRecordBuffer[*Index]));
	}

	NtfsFileRecordBufferPinned[*Index] += 1;

	if((Err = NtfsReadNonresidentAttribute (StructureContext,
											&StructureContext->MftAttributeContext,
											FileRecord * StructureContext->BytesPerFileRecord,
											StructureContext->BytesPerFileRecord,
											NtfsFileRecordBuffer[*Index])) != S_OK)
	{
		return Err;
	}

	if((Err = NtfsDecodeUsa (NtfsFileRecordBuffer[*Index],
							 StructureContext->BytesPerFileRecord)) != S_OK)
	{
		return Err;
	}

	NtfsFileRecordBufferVbo[*Index] = FileRecord;

	return S_OK;
}

ERRTYPE
NtfsLookupAttribute (
	IN PNTFS_STRUCTURE_CONTEXT StructureContext,
	IN LONGLONG FileRecord,
	IN ATTRIBUTE_TYPE_CODE TypeCode,
	OUT PBOOLEAN FoundAttribute,
	OUT PNTFS_ATTRIBUTE_CONTEXT AttributeContext
	)
{
	ERRTYPE Err;
	PATTRIBUTE_RECORD_HEADER AttributeHeader;

	NTFS_ATTRIBUTE_CONTEXT AttributeContext1;
	PNTFS_ATTRIBUTE_CONTEXT AttributeList;

	LONGLONG li;
	ATTRIBUTE_LIST_ENTRY AttributeListEntry;

	ULONG BufferIndex;

	//
	//  Unless other noted we will assume we haven't found the attribute
	//

	*FoundAttribute = FALSE;

	//
	//  Read in the file record and if necessary move ourselves up to the base file
	//  record
	//

	Err = NtfsReadAndDecodeFileRecord( StructureContext,
							 FileRecord,
							 &BufferIndex );

	if (Err != S_OK)
		return Err;

	if (/*!xxEqlZero*/(*((PLONGLONG)&(NtfsFileRecordBuffer[BufferIndex]->BaseFileRecordSegment)) != 0)) {

		//
		//  This isn't the base file record so now extract the base file record
		//  number and read it in
		//

		FileReferenceToLargeInteger( NtfsFileRecordBuffer[BufferIndex]->BaseFileRecordSegment,
									 &FileRecord );

		NtfsFileRecordBufferPinned[BufferIndex] -= 1;

		Err = NtfsReadAndDecodeFileRecord( StructureContext,
								 FileRecord,
								 &BufferIndex );

		if (Err != S_OK)
			return Err;
	}

	//
	//  Now we have read in the base file record so search for the target attribute
	//  type code and also remember if we find the attribute list attribute
	//

	AttributeList = NULL;

	for (AttributeHeader = NtfsFirstAttribute( NtfsFileRecordBuffer[BufferIndex] );
		 AttributeHeader->TypeCode != $END;
		 AttributeHeader = NtfsGetNextRecord( AttributeHeader )) {

		//
		//  We have located the attribute in question if the type code match and if
		//  it is either not the data attribute or if it is the data attribute then
		//  it is also unnamed
		//

		if ((AttributeHeader->TypeCode == TypeCode)

					&&

			((TypeCode != $DATA) ||
			 ((TypeCode == $DATA) && (AttributeHeader->NameLength == 0)))) {

			//
			//  Indicate that we have found the attribute and setup the output
			//  attribute context and then return to our caller
			//

			*FoundAttribute = TRUE;

			InitializeAttributeContext( StructureContext,
										NtfsFileRecordBuffer[BufferIndex],
										AttributeHeader,
										FileRecord,
										AttributeContext );

			NtfsFileRecordBufferPinned[BufferIndex] -= 1;

			return S_OK;
		}

		//
		//  Check if this is the attribute list attribute and if so then setup a
		//  local attribute context to use just in case we don't find the attribute
		//  we're after in the base file record
		//

		if (AttributeHeader->TypeCode == $ATTRIBUTE_LIST) {

			InitializeAttributeContext( StructureContext,
										NtfsFileRecordBuffer[BufferIndex],
										AttributeHeader,
										FileRecord,
										AttributeList = &AttributeContext1 );
		}
	}

	//
	//  If we reach this point then the attribute has not been found in the base file
	//  record so check if we have located an attribute list.  If not then the search
	//  has not been successful
	//

	if (AttributeList == NULL) {

		NtfsFileRecordBufferPinned[BufferIndex] -= 1;

		return S_OK;
	}

	//
	//  Now that we've located the attribute list we need to continue our search.  So
	//  what this outer loop does is search down the attribute list looking for a
	//  match.
	//

	for (li = 0;
		 /*xxLtr*/(li < AttributeList->DataSize);
		 li = /*xxAdd*/(li + /*xxFromUlong*/(AttributeListEntry.RecordLength))) {

		//
		//  Read in the attribute list entry.  We don't need to read in the name,
		//  just the first part of the list entry.
		//

		NtfsReadAttribute( StructureContext,
					   AttributeList,
					   li,
					   sizeof(ATTRIBUTE_LIST_ENTRY),
					   &AttributeListEntry );

		//
		//  Now check if the attribute matches, and it is the first of multiple
		//  segments, and either it is not $data or if it is $data then it is unnamed
		//

		if ((AttributeListEntry.AttributeTypeCode == TypeCode)

					&&

			/*xxEqlZero*/(AttributeListEntry.LowestVcn == 0)

					&&

			((TypeCode != $DATA) ||
			 ((TypeCode == $DATA) && (AttributeListEntry.AttributeNameLength == 0)))) {

			//
			//  We found a match so now compute the file record containing the
			//  attribute we're after and read in the file record
			//

			FileReferenceToLargeInteger( AttributeListEntry.SegmentReference,
										 &FileRecord );

			NtfsFileRecordBufferPinned[BufferIndex] -= 1;

			Err = NtfsReadAndDecodeFileRecord( StructureContext,
									 FileRecord,
									 &BufferIndex );

			if (Err != S_OK)
				return Err;

			//
			//  Now search down the file record for our matching attribute, and it
			//  better be there otherwise the attribute list is wrong.
			//

			for (AttributeHeader = NtfsFirstAttribute( NtfsFileRecordBuffer[BufferIndex] );
				 AttributeHeader->TypeCode != $END;
				 AttributeHeader = NtfsGetNextRecord( AttributeHeader )) {

				//
				//  We have located the attribute in question if the type code match
				//  and if it is either not the data attribute or if it is the data
				//  attribute then it is also unnamed
				//

				if ((AttributeHeader->TypeCode == TypeCode)

							&&

					((TypeCode != $DATA) ||
					 ((TypeCode == $DATA) && (AttributeHeader->NameLength == 0)))) {

					//
					//  Indicate that we have found the attribute and setup the
					//  output attribute context and return to our caller
					//

					*FoundAttribute = TRUE;

					InitializeAttributeContext( StructureContext,
												NtfsFileRecordBuffer[BufferIndex],
												AttributeHeader,
												FileRecord,
												AttributeContext );

					NtfsFileRecordBufferPinned[BufferIndex] -= 1;

					return S_OK;
				}
			}

			NtfsFileRecordBufferPinned[BufferIndex] -= 1;

			return ERROR_BAD_FORMAT;
		}
	}

	//
	//  If we reach this point we've exhausted the attribute list without finding the
	//  attribute
	//

	NtfsFileRecordBufferPinned[BufferIndex] -= 1;

	return S_OK;
}

//////////////////////////////////////////////////////////////////////

typedef struct _BL_FILE_FLAGS {
	ULONG Open : 1;
	ULONG Read : 1;
	ULONG Write : 1;
} BL_FILE_FLAGS, *PBL_FILE_FLAGS;

#define MAXIMUM_FILE_NAME_LENGTH 32

typedef struct _BL_FILE_TABLE {
	BL_FILE_FLAGS Flags;
	ULONG DeviceId;
	LARGE_INTEGER Position;
	PVOID StructureContext;
	UCHAR FileNameLength;
	CHAR FileName[MAXIMUM_FILE_NAME_LENGTH];
	union {
		NTFS_FILE_CONTEXT NtfsFileContext;
	} u;
} BL_FILE_TABLE, *PBL_FILE_TABLE;

#define BL_FILE_TABLE_SIZE 512

BL_FILE_TABLE BlFileTable[BL_FILE_TABLE_SIZE];

////////////

enum OPEN_MODE
{
	ArcOpenDirectory,
	ArcCreateDirectory,
	ArcOpenReadOnly
};

ERRTYPE
NtfsOpen (
	IN PCHAR FileName,
	IN OPEN_MODE OpenMode,
	IN PULONG FileId
	)
{
	PBL_FILE_TABLE FileTableEntry;
	PNTFS_STRUCTURE_CONTEXT StructureContext;
	PNTFS_FILE_CONTEXT FileContext;
	ERRTYPE Err;

	STRING PathName;
	STRING Name;

	LONGLONG FileRecord;
	BOOLEAN IsDirectory;
	BOOLEAN Found;

	NTFS_ATTRIBUTE_CONTEXT AttributeContext1;
	NTFS_ATTRIBUTE_CONTEXT AttributeContext2;
	NTFS_ATTRIBUTE_CONTEXT AttributeContext3;

	PNTFS_ATTRIBUTE_CONTEXT IndexRoot;
	PNTFS_ATTRIBUTE_CONTEXT IndexAllocation;
	PNTFS_ATTRIBUTE_CONTEXT AllocationBitmap;

	//NtfsPrint("%d NtfsOpen(\"%s\")\n\r", __LINE__, FileName);

	//
	//  Load our local variables
	//

	FileTableEntry = &BlFileTable[*FileId];
	StructureContext = (PNTFS_STRUCTURE_CONTEXT)FileTableEntry->StructureContext;
	FileContext = &FileTableEntry->u.NtfsFileContext;

	//
	//  Zero out the file context and position information in the file table entry
	//

	FileTableEntry->Position.QuadPart = 0;

	memset(FileContext, 0, sizeof(NTFS_FILE_CONTEXT));

	//
	//  Construct a file name descriptor from the input file name
	//

	RtlInitString( &PathName, FileName );

	//
	//  Open the root directory as our starting point,  The root directory file
	//  reference number is 5.
	//

	FileRecord = 5;
	IsDirectory = TRUE;

	//
	//  While the path name has some characters left in it and current attribute
	//  context is a directory we will continue our search
	//

	while ((PathName.Length > 0) && IsDirectory) {

		//
		//  Extract the first component and search the directory for a match, but
		//  first copy the first part to the file name buffer in the file table entry
		//

		if (PathName.Buffer[0] == '\\') {

			PathName.Buffer +=1;
			PathName.Length -=1;
		}

		for (FileTableEntry->FileNameLength = 0;
			 (((USHORT)FileTableEntry->FileNameLength < PathName.Length) &&
			  (PathName.Buffer[FileTableEntry->FileNameLength] != '\\'));
			 FileTableEntry->FileNameLength += 1) {

			FileTableEntry->FileName[FileTableEntry->FileNameLength] =
									  PathName.Buffer[FileTableEntry->FileNameLength];
		}

		NtfsFirstComponent( &PathName, &Name );

		//
		//  The current file record must be a directory so now lookup the index root,
		//  allocation and bitmap for the directory and then we can do our search.
		//

		IndexRoot = &AttributeContext1;

		Err = NtfsLookupAttribute( StructureContext,
						 FileRecord,
						 $INDEX_ROOT,
						 &Found,
						 IndexRoot);
		if (Err != S_OK)
			return Err;

		if (!Found) { return ERROR_BAD_FORMAT; }

		IndexAllocation = &AttributeContext2;

		Err = NtfsLookupAttribute( StructureContext,
						 FileRecord,
						 $INDEX_ALLOCATION,
						 &Found,
						 IndexAllocation);

		if (Err != S_OK)
			return Err;

		if (!Found) { IndexAllocation = NULL; }

		AllocationBitmap = &AttributeContext3;

		Err = NtfsLookupAttribute( StructureContext,
						 FileRecord,
						 $BITMAP,
						 &Found,
						 AllocationBitmap);
		if (Err != S_OK)
			return Err;

		if (!Found) { AllocationBitmap = NULL; }

		//
		//  Search for the name in the current directory
		//

		Err = NtfsSearchForFileName( StructureContext,
						   IndexRoot,
						   IndexAllocation,
						   AllocationBitmap,
						   Name,
						   &Found,
						   &FileRecord,
						   &IsDirectory );

		if (Err != S_OK)
			return Err;

		//
		//  If we didn't find it then we should get out right now
		//

		if (!Found) { return ENOENT; }
	}

	//
	//  At this point we have exhausted our pathname or we did not get a directory
	//  Check if we didn't get a directory and we still have a name to crack
	//

	if (PathName.Length > 0) {

		return ENOTDIR;
	}

	//
	//  Now FileRecord is the one we wanted to open.  Check the various open modes
	//  against what we have located
	//

	if (IsDirectory) {

		switch (OpenMode) {

		case ArcOpenDirectory:

			//
			//  To open the directory we will lookup the index root as our file
			//  context and then increment the appropriate counters.
			//

			Err = NtfsLookupAttribute( StructureContext,
							 FileRecord,
							 $INDEX_ROOT,
							 &Found,
							 FileContext );

			if (Err != S_OK)
				return Err;

			if (!Found) { return EBADF; }

			FileTableEntry->Flags.Open = 1;
			FileTableEntry->Flags.Read = 1;

			return ESUCCESS;

		case ArcCreateDirectory:

			return EROFS;

		default:

			return EISDIR;
		}

	}

	switch (OpenMode) {

	case ArcOpenReadOnly:

		//
		//  To open the file we will lookup the $data as our file context and then
		//  increment the appropriate counters.
		//

		Err = NtfsLookupAttribute( StructureContext,
						 FileRecord,
						 $DATA,
						 &Found,
						 FileContext );

		if (Err != S_OK)
			return Err;

		if (!Found) { return EBADF; }

		FileTableEntry->Flags.Open = 1;
		FileTableEntry->Flags.Read = 1;

		return ESUCCESS;

	case ArcOpenDirectory:

		return ENOTDIR;

	default:

		return EROFS;
	}
}

ERRTYPE
NtfsClose(
	IN ULONG FileId
	)
{
	BlFileTable[FileId].Flags.Open = 0;
	return S_OK;
}

ERRTYPE
NtfsRead (
	IN ULONG FileId,
	OUT PVOID Buffer,
	IN ULONG Length,
	OUT PULONG Transfer
	)
{
	PBL_FILE_TABLE FileTableEntry;
	PNTFS_STRUCTURE_CONTEXT StructureContext;
	PNTFS_FILE_CONTEXT FileContext;
	
	LONGLONG AmountLeft;

	FileTableEntry = &BlFileTable[FileId];
	StructureContext = (PNTFS_STRUCTURE_CONTEXT) FileTableEntry->StructureContext;
	FileContext = &FileTableEntry->u.NtfsFileContext;

	AmountLeft = FileContext->DataSize - FileTableEntry->Position.QuadPart;

	if (Length <= AmountLeft)
	{
		*Transfer = Length;
	}
	else
	{
		*Transfer = ((ULONG)AmountLeft);
	}

	NtfsReadAttribute ( StructureContext,
						FileContext,
						FileTableEntry->Position.QuadPart,
						*Transfer,
						Buffer );

	FileTableEntry->Position.QuadPart += *Transfer;
	return S_OK;
}