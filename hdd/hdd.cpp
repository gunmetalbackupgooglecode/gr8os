//
// Kernel-Mode driver for GR8OS
//
//  Generic Hard Disk  read/write driver
//

#include "common.h"

#define IDE_CHANNEL_1_BASE	0x1F0
#define IDE_CHANNEL_2_BASE	0x170

#define IDE_CHANNEL_1_CONTROL	0x3F6
#define IDE_CHANNEL_2_CONTROL	0x376

UCHAR HdReadPort( UCHAR channel, UCHAR offset )
{
	volatile USHORT Port = (channel ? IDE_CHANNEL_2_BASE : IDE_CHANNEL_1_BASE);
	return KiInPort (Port+offset);
}

void HdRead ( UCHAR channel, void *buffer, ULONG len)
{
	volatile USHORT Port = (channel ? IDE_CHANNEL_2_BASE : IDE_CHANNEL_1_BASE);
	__asm
	{
		push edi
		pushfd
		cld
		mov  edi, [buffer]
		mov  dx, [Port]
		mov  ecx, [len]
		rep  insw
		popfd
		pop  edi
	}
}

void HdWrite ( UCHAR channel, void *buffer, ULONG len)
{
	volatile USHORT Port = (channel ? IDE_CHANNEL_2_BASE : IDE_CHANNEL_1_BASE);
	__asm
	{
		push edi
		pushfd
		cld
		mov  edi, [buffer]
		mov  dx, [Port]
		mov  ecx, [len]
		rep  outsw
		popfd
		pop  edi
	}
}

void HdWritePort( UCHAR channel, UCHAR offset, UCHAR Value )
{
	volatile USHORT Port = (channel ? IDE_CHANNEL_2_BASE : IDE_CHANNEL_1_BASE);
	KiOutPort (Port+offset, Value);
}

USHORT HdReadPortW( UCHAR channel, UCHAR offset )
{
	volatile USHORT Port = (channel ? IDE_CHANNEL_2_BASE : IDE_CHANNEL_1_BASE);
	return KiInPortW (Port+offset);
}

VOID HdWritePortW( UCHAR channel, UCHAR offset, USHORT Value )
{
	volatile USHORT Port = (channel ? IDE_CHANNEL_2_BASE : IDE_CHANNEL_1_BASE);
	return KiOutPortW (Port+offset, Value);
}

void HdControlChannel( UCHAR channel, UCHAR byte )
{
	volatile USHORT Port = (channel ? IDE_CHANNEL_1_CONTROL : IDE_CHANNEL_2_CONTROL);
	KiOutPort (Port, byte);
}


//
//   IDE HDD ports map:
//
// ========================================================
//  Port              CHS       LBA         R        W
// ========================================================
// (+x means 
//    offset.
//  x means 
//   absolute
//   port number)
//
//          +0          -         -      DATA     DATA
//          +1          -         -    ERRORS  PROPERTIES
//          +2          -         -   NUMSECTS  NUMSECTS
//          +3     SECTOR   0-7 addr        -        -
//          +4    CYLINDER  8-15 addr       -        -
//          +5    CYLINDER  16-23 addr      -        -
//          +6    HEAD/DEV  24-27adr/dev    -        -
//          +7          -         -      STATUS    COMMAND
// 
//    3F6/376           -         -      STATUS    CONTROL
//    3F7/377    ******* NOT USED ***** NOT USED ******
// ========================================================
//

#define IDE_DATA	0
#define IDE_ERRS	1
#define IDE_PROPS	1
#define IDE_SECTOR	3
#define IDE_07LOW	3
#define IDE_CYL1	4
#define IDE_815MED	4
#define IDE_CYL2	5
#define IDE_1623MED 5
#define IDE_HEADDEV 6
#define IDE_2427DEV 6
#define IDE_STATUS	7
#define IDE_COMMAND	7

#pragma pack(1)

typedef struct IDE_PORT_6
{
	UCHAR Lba : 4;
	UCHAR Dev : 1;				// 0 = Master, 1 = Slave.
	UCHAR Reserved1 : 1;
	UCHAR AddressingMode : 1;	// 0 = CHS, 1 = LBA.
	UCHAR Reserved2 : 1;
} *PIDE_PORT_6;

typedef struct IDE_STATE
{
	UCHAR Error : 1;				// ERR
	UCHAR Index_Reserved : 1;		// IDX
	UCHAR Corrected : 1;			// CORR
	UCHAR DataRequestReady : 1;		// DRQ
	UCHAR DataScanCompleted : 1;	// DSC
	UCHAR DeviceFailure : 1;		// DF
	UCHAR DeviceReady : 1;			// DRDY
	UCHAR Busy : 1;					// BSY
} *PIDE_STATE;

IDE_STATE HdGetState(UCHAR ch)
{
	volatile UCHAR st = HdReadPort (ch, 7);
	return *(PIDE_STATE)&st;
}

//
// 3F6 port:
//   0
//   1  -  interrupts disabled
//   2  -  hardware reset
//   3
//   4
//   5
//   6
//   7
//

#define IDE_CMD_READ		0x20
#define IDE_CMD_READDNOERR	0x21
#define IDE_CMD_WRITE		0x30
#define IDE_IDENTIFY_DEVICE	0xEC
#define IDE_SLEEP			0xE6

#pragma pack()


BOOLEAN HdPulsedIrq[2] = { 0, 0 };

__declspec(naked)
VOID
Hd0ProcessIrq()
{
//	HdPrint(("~~~~~~~ HDD0 IRQ ~~~~~~~\n"));
	HdPulsedIrq[0] = 1;

	__asm jmp KiEoiHelper;
}

__declspec(naked)
VOID
Hd1ProcessIrq()
{
//	HdPrint(("~~~~~~~ HDD1 IRQ ~~~~~~~\n"));
	HdPulsedIrq[1] = 1;

	__asm jmp KiEoiHelper;
}

BOOLEAN
KEAPI
HdCheckDevicePresence(
	UCHAR Channel,
	UCHAR Device
	)
{
	const ULONG DefTimeout = 10000;
	ULONG Timeout = DefTimeout;
	while (HdGetState(Channel).Busy && Timeout>0)
		Timeout--;

	if (Timeout==0)
	{
		//HdControlChannel (Channel, 1);
		return 0;
	}

	IDE_PORT_6 port6 = {0};
	port6.Reserved1 = 1;
	port6.Reserved2 = 1;
	port6.Dev = Device;
	HdWritePort (Channel, 6, *(UCHAR*)&port6);

	Timeout = DefTimeout;
	while (!(HdGetState(Channel).Busy == 0 && HdGetState(Channel).DeviceReady == 1) && Timeout > 0)
		Timeout --;

	if (Timeout==0)
	{
		if (Device)
		{
			port6.Dev = 0;
			HdWritePort (Channel, 6, *(UCHAR*)&port6);
		}
		return 0;
	}

	return 1;
}

#define PHYS_IDE_CB_MASK	'_IDE'

struct PHYSICAL_IDE_CONTROL_BLOCK
{
	ULONG Mask;
	BOOLEAN PhysicalOrPartition;
	union
	{
		struct
		{
			UCHAR Channel;
			UCHAR Device;
		};
		struct
		{
			ULONG PartitionStart;
			ULONG PartitionEnd;
			PFILE RelatedPhysical;
		};
	};
};

#define HdPrint(x) KiDebugPrint x

#define DUMP_STRUC_FIELD(OBJECT, FNAME, TYPE) HdPrint(( #OBJECT "->" #FNAME " = " TYPE "\n", (OBJECT)->FNAME ));

void DUMP_PCB(PHYSICAL_IDE_CONTROL_BLOCK *OBJECT) 
{
	DUMP_STRUC_FIELD (OBJECT, Mask, "%08x");				
	DUMP_STRUC_FIELD (OBJECT, PhysicalOrPartition, "%d");	
	if ( (OBJECT)->PhysicalOrPartition == 0 )				
	{														
		DUMP_STRUC_FIELD (OBJECT, PartitionStart, "%08x");	
		DUMP_STRUC_FIELD (OBJECT, PartitionEnd, "%08x");	
		DUMP_STRUC_FIELD (OBJECT, RelatedPhysical, "%08x");		
	}														
	else													
	{														
		DUMP_STRUC_FIELD (OBJECT, Channel, "%d");			
		DUMP_STRUC_FIELD (OBJECT, Device, "%d");			
	}														
}

STATUS
KEAPI
HdPerformRead(
	IN PFILE FileObject,
	IN ULONG SectorNumber,
	OUT PVOID Buffer,
	IN ULONG Size,
	OUT PULONG nBytesWritten
	)
{
	ULONG Timeout;

	if (!MmIsAddressValid(FileObject))
	{
		HdPrint(("FileObject=%08x\n", FileObject));
	}
	ASSERT (MmIsAddressValid(FileObject));
	ASSERT (MmIsAddressValid(FileObject->DeviceObject));

	PHYSICAL_IDE_CONTROL_BLOCK *pcb = (PHYSICAL_IDE_CONTROL_BLOCK*)(FileObject->FsContext2);

#define CHANNEL (pcb->Channel)
#define DEVICE  (pcb->Device)

//	HdPrint(("HD: performing generic read, pcb=%08x\n", pcb));
	//DUMP_PCB (pcb);

	if (pcb->Mask != PHYS_IDE_CB_MASK)
	{
		HdPrint(("pcb->Mask=%08x\n", pcb->Mask));

		UNICODE_STRING name;
		STATUS Status = ObQueryObjectName (FileObject->DeviceObject, &name);
		HdPrint(("device name %S\n", name.Buffer));
	}
	ASSERT (pcb->Mask == PHYS_IDE_CB_MASK);

//	if (SectorNumber > 70)
//		INT3;

	if (pcb->PhysicalOrPartition == 0)
	{
		//HdPrint(("HD: [%08x] Recursion read for partition [%08x -> %08x]\n", FileObject, FileObject->DeviceObject, pcb->RelatedPhysical->DeviceObject));

		//HdPrint(("Dumping related PCB\n"));		
		//DUMP_PCB ( (PHYSICAL_IDE_CONTROL_BLOCK*)pcb->RelatedPhysical->FsContext2 );

		//return HdPerformRead (pcb->RelatedPhysical, SectorNumber /*already adjusted by HddRead()*/, Buffer, Size);
		return HdPerformRead (pcb->RelatedPhysical, SectorNumber + pcb->PartitionStart, Buffer, Size, nBytesWritten);
	}

//	HdPrint(("HD: performing read IDE[%d:%d], Sector %08x\n", CHANNEL, DEVICE, SectorNumber));

	ASSERT (CHANNEL < 2);
	ASSERT (DEVICE < 2);

	IDE_PORT_6 port6 = {0};
	port6.AddressingMode = 1;
	port6.Reserved1 = 1;
	port6.Reserved2 = 1;
	port6.Dev = DEVICE;

	HdWritePort (CHANNEL, 6, *(UCHAR*)&port6);

_retry:

	Timeout = 1000000;

//	HdPrint(("HD: ~BSY.. "));

	while (HdGetState(CHANNEL).Busy && Timeout>0)
		Timeout--;

	if (Timeout==0)
	{
		HdPrint(("TIMEDOUT\n"));
		return STATUS_DEVICE_NOT_READY;
	}

//	HdPrint(("~DEV.. "));

	Timeout = 1000000;
	while (!(HdGetState(CHANNEL).Busy == 0 && HdGetState(CHANNEL).DeviceReady == 1) && Timeout > 0)
	{
		Timeout --;
	}

	if (Timeout==0)
	{
		HdPrint(("TIMEDOUT\n"));

		HdControlChannel (CHANNEL, 4);
		for (int i=0; i<1000000; i++) __asm nop;
		HdControlChannel (CHANNEL, 0);

		goto _retry;

		return STATUS_DEVICE_NOT_READY;
	}

//	HdPrint(("OK. "));

	ASSERT ( (SectorNumber & 0xFF000000) == 0 );

	HdPulsedIrq[CHANNEL] = 0;

	HdWritePort (CHANNEL, 2, 1);								// 1 cluster
	HdWritePort (CHANNEL, 3, SectorNumber & 0xFF);
	HdWritePort (CHANNEL, 4, (SectorNumber >> 8) & 0xFF);
	HdWritePort (CHANNEL, 5, (SectorNumber >> 16) & 0xFF);

	HdWritePort (CHANNEL, 7, IDE_CMD_READ);

//	HdPrint(("~SENDING "));

	Timeout = 1000000;
	while (HdGetState(CHANNEL).Busy && Timeout > 0)
		Timeout --;

	if (Timeout==0)
	{
		HdPrint(("TIMEDOUT\n"));
		return STATUS_DEVICE_NOT_READY;
	}

	if (HdGetState(CHANNEL).DeviceFailure)
	{
		HdPrint(("FAILED\n"));
		return STATUS_INTERNAL_FAULT;
	}

	if (HdGetState(CHANNEL).Error)
	{
		UCHAR err = HdReadPort (CHANNEL, 1);
		HdPrint(("ERROR [%02x]\n", err));
		return STATUS_UNSUCCESSFUL;
	}

//	HdPrint(("~READING.. "));

	//Timeout = 1000000;
	//while (!HdGetState(CHANNEL).DataRequestReady && Timeout > 0)
	//	Timeout --;

	while (!HdPulsedIrq[CHANNEL]);
	HdPulsedIrq[CHANNEL] = 0;

	if (Timeout==0)
	{
		HdPrint(("TIMEDOUT\n"));
		return STATUS_DEVICE_NOT_READY;
	}

	if (HdGetState(CHANNEL).DeviceFailure)
	{
		HdPrint(("FAILED\n"));
		return STATUS_INTERNAL_FAULT;
	}

	if (HdGetState(CHANNEL).Error)
	{
		HdPrint(("ERROR\n"));
		return STATUS_UNSUCCESSFUL;
	}

	HdRead (CHANNEL, Buffer, ALIGN_UP(Size, SECTOR_SIZE)/2);

//	HdPrint(("OK\n"));

	/*
	HdPrint (("Read succedded for [%d:%d], Sector %d, [%02x %02x %02x %02x  %02x %02x %02x %02x]\n",
		CHANNEL,
		DEVICE,
		SectorNumber,
		((UCHAR*)Buffer)[0],
		((UCHAR*)Buffer)[1],
		((UCHAR*)Buffer)[2],
		((UCHAR*)Buffer)[3],
		((UCHAR*)Buffer)[4],
		((UCHAR*)Buffer)[5],
		((UCHAR*)Buffer)[6],
		((UCHAR*)Buffer)[7]
	));
	*/

	*nBytesWritten = Size;

	return STATUS_SUCCESS;
}



STATUS
KEAPI
HdPerformWrite(
	IN PFILE FileObject,
	IN ULONG SectorNumber,
	OUT PVOID Buffer,
	IN ULONG Size,
	OUT PULONG nBytesWritten
	)
{
	ULONG Timeout;

	if (!MmIsAddressValid(FileObject))
	{
		HdPrint(("FileObject=%08x\n", FileObject));
	}
	ASSERT (MmIsAddressValid(FileObject));
	ASSERT (MmIsAddressValid(FileObject->DeviceObject));

	PHYSICAL_IDE_CONTROL_BLOCK *pcb = (PHYSICAL_IDE_CONTROL_BLOCK*)(FileObject->FsContext2);

#define CHANNEL (pcb->Channel)
#define DEVICE  (pcb->Device)

	if (pcb->Mask != PHYS_IDE_CB_MASK)
	{
		HdPrint(("pcb->Mask=%08x\n", pcb->Mask));

		UNICODE_STRING name;
		STATUS Status = ObQueryObjectName (FileObject->DeviceObject, &name);
		HdPrint(("device name %S\n", name.Buffer));
	}
	ASSERT (pcb->Mask == PHYS_IDE_CB_MASK);

	if (pcb->PhysicalOrPartition == 0)
	{
		return HdPerformWrite (pcb->RelatedPhysical, SectorNumber + pcb->PartitionStart, Buffer, Size, nBytesWritten);
	}

	HdPrint(("HD: performing write IDE[%d:%d], Sector %08x\n", CHANNEL, DEVICE, SectorNumber));

	ASSERT (CHANNEL < 2);
	ASSERT (DEVICE < 2);

	IDE_PORT_6 port6 = {0};
	port6.AddressingMode = 1;
	port6.Reserved1 = 1;
	port6.Reserved2 = 1;
	port6.Dev = DEVICE;

	HdWritePort (CHANNEL, 6, *(UCHAR*)&port6);

_retry:

	Timeout = 1000000;

	while (HdGetState(CHANNEL).Busy && Timeout>0)
		Timeout--;

	if (Timeout==0)
	{
		HdPrint(("TIMEDOUT\n"));
		return STATUS_DEVICE_NOT_READY;
	}

	Timeout = 1000000;
	while (!(HdGetState(CHANNEL).Busy == 0 && HdGetState(CHANNEL).DeviceReady == 1) && Timeout > 0)
	{
		Timeout --;
	}

	if (Timeout==0)
	{
		HdPrint(("TIMEDOUT\n"));

		HdControlChannel (CHANNEL, 4);
		for (int i=0; i<1000000; i++) __asm nop;
		HdControlChannel (CHANNEL, 0);

		goto _retry;

		return STATUS_DEVICE_NOT_READY;
	}

	ASSERT ( (SectorNumber & 0xFF000000) == 0 );

	HdPulsedIrq[CHANNEL] = 0;

	HdWritePort (CHANNEL, 2, 1);								// 1 cluster
	HdWritePort (CHANNEL, 3, SectorNumber & 0xFF);
	HdWritePort (CHANNEL, 4, (SectorNumber >> 8) & 0xFF);
	HdWritePort (CHANNEL, 5, (SectorNumber >> 16) & 0xFF);

	HdWritePort (CHANNEL, 7, IDE_CMD_WRITE);


	Timeout = 1000000;
	while (HdGetState(CHANNEL).Busy && Timeout > 0)
		Timeout --;

	if (Timeout==0)
	{
		HdPrint(("TIMEDOUT\n"));
		return STATUS_DEVICE_NOT_READY;
	}

	if (HdGetState(CHANNEL).DeviceFailure)
	{
		HdPrint(("FAILED\n"));
		return STATUS_INTERNAL_FAULT;
	}

	if (HdGetState(CHANNEL).Error)
	{
		UCHAR err = HdReadPort (CHANNEL, 1);
		HdPrint(("ERROR [%02x]\n", err));
		return STATUS_UNSUCCESSFUL;
	}


	while (!HdPulsedIrq[CHANNEL]);
	HdPulsedIrq[CHANNEL] = 0;

	if (Timeout==0)
	{
		HdPrint(("TIMEDOUT\n"));
		return STATUS_DEVICE_NOT_READY;
	}

	if (HdGetState(CHANNEL).DeviceFailure)
	{
		HdPrint(("FAILED\n"));
		return STATUS_INTERNAL_FAULT;
	}

	if (HdGetState(CHANNEL).Error)
	{
		HdPrint(("ERROR\n"));
		return STATUS_UNSUCCESSFUL;
	}

	HdWrite (CHANNEL, Buffer, ALIGN_UP(Size, SECTOR_SIZE)/2);

	HdPrint (("HDD: Write succedded for [%d:%d], Sector %d, [%02x %02x %02x %02x  %02x %02x %02x %02x]\n",
		CHANNEL,
		DEVICE,
		SectorNumber,
		((UCHAR*)Buffer)[0],
		((UCHAR*)Buffer)[1],
		((UCHAR*)Buffer)[2],
		((UCHAR*)Buffer)[3],
		((UCHAR*)Buffer)[4],
		((UCHAR*)Buffer)[5],
		((UCHAR*)Buffer)[6],
		((UCHAR*)Buffer)[7]
	));

	*nBytesWritten = Size;

	return STATUS_SUCCESS;
}

UNICODE_STRING DeviceName;

#define COMPLETE_IRP(Irp,xStatus,Info) {			\
		(Irp)->IoStatus.Status = (xStatus);			\
		(Irp)->IoStatus.Information = (Info);		\
		IoCompleteRequest ((Irp), 0);				\
		return (xStatus);							\
	}

#pragma pack(1)

typedef struct MBR_PARTITION
{
	UCHAR Active;
	UCHAR HeadStart;
	UCHAR SectorStart : 6;
	UCHAR CylinderStartHigh : 2;
	UCHAR CylinderStartLow;
	UCHAR Type;
	UCHAR HeadEnd;
	UCHAR SectorEnd : 6;
	UCHAR CylinderEndHigh : 2;
	UCHAR CylinderEndLow;
	ULONG FirstSector;
	ULONG NumberOfSectors;
} *PMBR_PARTITION;

#pragma pack()


STATUS
KEAPI
HdAddDevice(
	PDRIVER DriverObject,
	UCHAR Number
	)
{
	STATUS Status;
	PDEVICE DeviceObject;

	RtlInitUnicodeString (&DeviceName, L"\\Device\\hda");
	DeviceName.Buffer[10] = 'a' + Number;

	HdPrint(("HDD: Creating device %S on IDE channel\n", DeviceName.Buffer));

	Status = IoCreateDevice (
		DriverObject,
		sizeof(PHYSICAL_IDE_CONTROL_BLOCK),
		&DeviceName,
		DEVICE_TYPE_DISK,
		&DeviceObject
		);

	if (!SUCCESS(Status))
	{
		HdPrint(("[~] HDD: Cannot create device, IoCreateDevice failed with status %08x\n", Status));
		return Status;
	}

	PHYSICAL_IDE_CONTROL_BLOCK* pcb = (PHYSICAL_IDE_CONTROL_BLOCK*)(DeviceObject+1);
	pcb->Mask = PHYS_IDE_CB_MASK;
	pcb->Channel = Number >> 1;
	pcb->Device = Number & 1;
	pcb->PhysicalOrPartition = 1;

	//
	// Read partition table
	//

	HdPrint(("[~] HDD: Reading partition table\n"));

	PFILE PhysicalFileObject;
	IO_STATUS_BLOCK IoStatus;

	// BUGBUG: This file object should be closed later.
	Status = IoCreateFile (
		&PhysicalFileObject,
		FILE_READ_DATA,
		&DeviceName,
		&IoStatus,
		FILE_OPEN_EXISTING,
		0);

	if (!SUCCESS(Status))
	{
		HdPrint(("[~] HDD: Cannot open created device, IoCreateFile failed with status %08x\n", Status));
		return Status;
	}

	UCHAR Buffer[512];
	
	//
	// Read MBR
	//

	Status = IoReadFile(
		PhysicalFileObject,
		Buffer,
		sizeof(Buffer),
		NULL,
		0,
		&IoStatus
		);
	
	if (!SUCCESS(Status))
	{
		HdPrint(("[~] HDD: Cannot read partition table, IoReadFile failed with status %08x\n", Status));
		IoCloseFile (PhysicalFileObject);
		return Status;
	}

	//
	// Parse MBR
	//

	if (Buffer[511] != 0xAA || Buffer[510] != 0x55)
	{
		HdPrint(("[~] HDD: Master Boot Record BAD on disk %S\n", DeviceName.Buffer));
		IoCloseFile (PhysicalFileObject);
		return STATUS_UNSUCCESSFUL;
	}

	PMBR_PARTITION PartitionTable = (PMBR_PARTITION) &Buffer[0x1BE];

	for (int i=0; i<4; i++)
	{
		if (!(PartitionTable[i].Active == 0x80 || PartitionTable[i].Active == 0x00))
		{
			HdPrint(("[~] HDD: Partition table BAD on disk %S\n", DeviceName.Buffer));
			IoCloseFile (PhysicalFileObject);
			return STATUS_UNSUCCESSFUL;
		}

		HdPrint(("Partition#%d: Active=%02x,Type=%02x,FirstSector=%d,NumberSectors=%d\n",
			i,
			PartitionTable[i].Active,
			PartitionTable[i].Type,
			PartitionTable[i].FirstSector,
			PartitionTable[i].NumberOfSectors
			));

		if (PartitionTable[i].Type == 0)
			continue;

		UNICODE_STRING PartitionName;
		PDEVICE Partition;

		RtlInitUnicodeString( &PartitionName, L"\\Device\\hdaX" );
		PartitionName.Buffer[10] = 'a' + Number;
		PartitionName.Buffer[11] = '0' + i;

		HdPrint(("HDD: Creating partition %S.. ", PartitionName.Buffer));

		Status = IoCreateDevice (
			DriverObject,
			sizeof(PHYSICAL_IDE_CONTROL_BLOCK),
			&PartitionName,
			DEVICE_TYPE_DISK,
			&Partition
			);

		if (!SUCCESS(Status))
		{
			HdPrint(("Cannot create device, IoCreateDevice failed with status %08x\n", Status));
			return Status;
		}

		pcb = (PHYSICAL_IDE_CONTROL_BLOCK*)(Partition+1);
		pcb->Mask = PHYS_IDE_CB_MASK;
		pcb->PhysicalOrPartition = 0;
		pcb->PartitionStart = PartitionTable[i].FirstSector;
		pcb->PartitionEnd = PartitionTable[i].FirstSector + PartitionTable[i].NumberOfSectors;
		pcb->RelatedPhysical = PhysicalFileObject;

		HdPrint(("Created partition %08x\n", Partition));

		switch (PartitionTable[i].Type)
		{
		case 0x01:
		case 0x04:
		case 0x06:
		case 0x0B:
			{
				HdPrint(("Fat partition found [%02x]. Mounting..\n", PartitionTable[i].Type));

				UNICODE_STRING FatName;
				RtlInitUnicodeString( &FatName, L"\\FileSystem\\Fat" );
				STATUS Status;
				PDEVICE FatFs;

				Status = ObReferenceObjectByName (&FatName, IoDeviceObjectType, KernelMode, FILE_READ_DATA, NULL, (PVOID*)&FatFs);
				if (!SUCCESS(Status))
				{
					HdPrint(("ObReferenceObjectByName failed for FAT fs: %08x\n", Status));
					continue;
				}

				Status = IoRequestMount (Partition, FatFs);

				ObDereferenceObject (FatFs);

				if (!SUCCESS(Status))
				{
					HdPrint(("HDD: IoRequestMount failed : %08x\n", Status));
					return Status;
				}
				else
				{
					HdPrint(("HDD: Mounted partition %S for FAT file system\n", PartitionName.Buffer));
				}
			}
			break;
		}
	}

	return Status;
}

STATUS
KEAPI
HddCreateClose(
	PDEVICE DeviceObject,
	PIRP Irp
	)
{
	HdPrint(("HDD: %s for %S\n", Irp->MajorFunction==IRP_CREATE ? "IRP_CREATE" : "IRP_CLOSE",
		Irp->FileObject->RelativeFileName.Buffer));

	if (Irp->MajorFunction == IRP_CREATE)
	{
		//
		// Initialize caching for the hdd (support only read-only access)
		//

		CCFILE_CACHE_CALLBACKS Callbacks = { HdPerformRead, HdPerformWrite };
		CcInitializeFileCaching (Irp->FileObject, SECTOR_SIZE, &Callbacks);

		KdPrint(("HDD CREAT: Irp->File->CacheMap %08x\n", Irp->FileObject->CacheMap));

		Irp->FileObject->FsContext2 = (DeviceObject+1); // IDE Drive Control Block

		PHYSICAL_IDE_CONTROL_BLOCK* pcb = (PHYSICAL_IDE_CONTROL_BLOCK*)(DeviceObject+1);

		HdPrint(("IDE Control Block saved: *=%08x, SIGN=%08x[%08x], PhysOrPart=%d\n", pcb, pcb->Mask,PHYS_IDE_CB_MASK, pcb->PhysicalOrPartition));
		if (pcb->PhysicalOrPartition == 0) // partition
		{
			if (!MmIsAddressValid (pcb->RelatedPhysical))
			{
				HdPrint(("PCB INVALID [RelatedPhysical=%08]\n", pcb->RelatedPhysical));
				INT3
			}
			if (!MmIsAddressValid (pcb->RelatedPhysical->DeviceObject))
			{
				HdPrint(("PCB INVALID [RelatedPhysical=%08x, RPH->DevObj=%08x]\n", pcb->RelatedPhysical->DeviceObject));
				INT3
			}
			
			HdPrint(("Related physical: File=%08x, Device=%08x", pcb->RelatedPhysical, pcb->RelatedPhysical->DeviceObject));

			pcb = (PHYSICAL_IDE_CONTROL_BLOCK*)(pcb->RelatedPhysical->DeviceObject + 1);

			HdPrint(("PCB=%08x [SIGN=%08x, CH=%d DV=%d]\n", pcb, pcb->Mask, pcb->Channel, pcb->Device));
		}
	}
	else
	{
		//
		// Purge & free cache map
		//

		CcFreeCacheMap (Irp->FileObject);
	}

	COMPLETE_IRP (Irp, STATUS_SUCCESS, 0);
}

STATUS
KEAPI
HddReadWrite(
	PDEVICE DeviceObject,
	PIRP Irp
	)
{
	ULONG Size = Irp->BufferLength;
	STATUS Status = STATUS_INVALID_PARAMETER;
	PVOID Buffer;
	ULONG Offset;

	if (Irp->Flags & IRP_FLAGS_BUFFERED_IO)
	{
		Buffer = Irp->SystemBuffer;
	}
	else if (Irp->Flags & IRP_FLAGS_NEITHER_IO)
	{
		Buffer = Irp->UserBuffer;
	}

	if (Irp->CurrentStackLocation->Parameters.ReadWrite.OffsetSpecified)
	{
		Offset = (ULONG)(Irp->CurrentStackLocation->Parameters.ReadWrite.Offset.LowPart);
	}
	else
	{
		Offset = Irp->FileObject->CurrentOffset.LowPart;
	}
	
	if ( (Offset & (SECTOR_SIZE-1)) || (Size % SECTOR_SIZE))
	{
		COMPLETE_IRP (Irp, STATUS_DATATYPE_MISALIGNMENT, 0);
	}

	PHYSICAL_IDE_CONTROL_BLOCK *pcb = (PHYSICAL_IDE_CONTROL_BLOCK*)(DeviceObject+1);

	/*
	if (pcb->PhysicalOrPartition == 0)
	{
		//
		// Partition on the hark disk.
		//

		HdPrint(("HD: Resolving partition [PS=%d] : Sector %d -> %d\n", pcb->PartitionStart, 
			Offset/SECTOR_SIZE, 
			Offset/SECTOR_SIZE + pcb->PartitionStart));

//		Offset += pcb->PartitionStart*SECTOR_SIZE;
	}
	*/

	if (Irp->MajorFunction == IRP_READ)
	{
		Status = CcCacheReadFile (
			Irp->FileObject,
			Offset,
			Buffer,
			Size,
			&Size
			);
	}
	else
	{
		ASSERT (FALSE);
		/*
		Status = CcCacheWriteFile (
			Irp->FileObject,
			Offset,
			Buffer,
			Size,
			&Size
			);
		*/
	}

	COMPLETE_IRP (Irp, Status, Size);
}


#if 0
STATUS
KEAPI
HddFsControl(
	PDEVICE DeviceObject,
	PIRP Irp
	)
/*++
	Handle IRP_FSCTL
--*/
{
	switch (Irp->MinorFunction)
	{
	case IRP_MN_REQUEST_CACHED_PAGE:
		{
			PHYSICAL_IDE_CONTROL_BLOCK *pcb = (PHYSICAL_IDE_CONTROL_BLOCK*)(Irp->FileObject->FsContext2);

			ASSERT (MmIsAddressValid (pcb));
			ASSERT (pcb->Mask == PHYS_IDE_CB_MASK);

			ULONG PageNumber = 

			if (pcb->PhysicalOrPartition == 0)
			{
				
				
			}
		}
		break;

	default:

		COMPLETE_IRP (Irp, STATUS_NOT_SUPPORTED, 0);
	}
}
#endif


STATUS
KEAPI
DriverEntry(
	IN PDRIVER DriverObject
	)
{
	HdPrint(("[~] LKM DriverEntry()\n"));

	KiConnectInterrupt (14, Hd0ProcessIrq);
	KiConnectInterrupt (15, Hd1ProcessIrq);

//	HdControlChannel (0, 2);
//	HdControlChannel (1, 2);

	BOOLEAN Presence[4];

	HdPrint(("Checking presence (0,0).. "));
	Presence[0] = HdCheckDevicePresence (0, 0);
	HdPrint(("%s\n", (Presence[0] ? "TRUE" : "FALSE")));

	HdPrint(("Checking presence (0,1).. "));
	Presence[1] = HdCheckDevicePresence (0, 1);
	HdPrint(("%s\n", (Presence[1] ? "TRUE" : "FALSE")));

	HdPrint(("Checking presence (1,0).. "));
	Presence[2] = HdCheckDevicePresence (1, 0);
	HdPrint(("%s\n", (Presence[2] ? "TRUE" : "FALSE")));

	HdPrint(("Checking presence (1,1).. "));
	Presence[3] = HdCheckDevicePresence (1, 1);
	HdPrint(("%s\n", (Presence[3] ? "TRUE" : "FALSE")));

	DriverObject->IrpHandlers[IRP_CREATE] =
	DriverObject->IrpHandlers[IRP_CLOSE]  = HddCreateClose;
	DriverObject->IrpHandlers[IRP_READ]   = 
	DriverObject->IrpHandlers[IRP_WRITE]  = HddReadWrite;
	//DriverObject->IrpHandlers[IRP_FSCTL]  = HddFsControl;

	//
	// Create four device objects
	//

	if (Presence[0])
		HdAddDevice (DriverObject, 0);

	if (Presence[1])
		HdAddDevice (DriverObject, 1);

	INT3

	if (Presence[2])
		HdAddDevice (DriverObject, 2);

	if (Presence[3])
		HdAddDevice (DriverObject, 3);


	HdPrint(("HDD: Finished\n"));
	return STATUS_SUCCESS;
}
