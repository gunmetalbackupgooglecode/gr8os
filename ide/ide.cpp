//
// Kernel-Mode driver for GR8OS
//
//  Generic IDE  read/write driver
//

#include "common.h"

#define IDE_CHANNEL_1_BASE	0x1F0
#define IDE_CHANNEL_2_BASE	0x170

#define IDE_CHANNEL_1_CONTROL	0x3F6
#define IDE_CHANNEL_2_CONTROL	0x376

UCHAR IdeReadPort( UCHAR channel, UCHAR offset )
{
	volatile USHORT Port = (channel ? IDE_CHANNEL_2_BASE : IDE_CHANNEL_1_BASE);
	return KiInPort (Port+offset);
}

void IdeRead ( UCHAR channel, void *buffer, ULONG len)
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

void IdeWrite ( UCHAR channel, void *buffer, ULONG len)
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

void IdeWritePort( UCHAR channel, UCHAR offset, UCHAR Value )
{
	volatile USHORT Port = (channel ? IDE_CHANNEL_2_BASE : IDE_CHANNEL_1_BASE);
	KiOutPort (Port+offset, Value);
}

USHORT IdeReadPortW( UCHAR channel, UCHAR offset )
{
	volatile USHORT Port = (channel ? IDE_CHANNEL_2_BASE : IDE_CHANNEL_1_BASE);
	return KiInPortW (Port+offset);
}

VOID IdeWritePortW( UCHAR channel, UCHAR offset, USHORT Value )
{
	volatile USHORT Port = (channel ? IDE_CHANNEL_2_BASE : IDE_CHANNEL_1_BASE);
	return KiOutPortW (Port+offset, Value);
}

void IdeControlChannel( UCHAR channel, UCHAR byte )
{
	volatile USHORT Port = (channel ? IDE_CHANNEL_1_CONTROL : IDE_CHANNEL_2_CONTROL);
	KiOutPort (Port, byte);
}


//
//   IDE IDE ports map:
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

IDE_STATE IdeGetState(UCHAR ch)
{
	volatile UCHAR st = IdeReadPort (ch, 7);
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


BOOLEAN IdePulsedIrq[2] = { 0, 0 };

__declspec(naked)
VOID
Ide0ProcessIrq()
{
//	IdePrint(("~~~~~~~ IDE0 IRQ ~~~~~~~\n"));
	IdePulsedIrq[0] = 1;

	__asm jmp KiEoiHelper;
}

__declspec(naked)
VOID
Ide1ProcessIrq()
{
//	IdePrint(("~~~~~~~ IDE1 IRQ ~~~~~~~\n"));
	IdePulsedIrq[1] = 1;

	__asm jmp KiEoiHelper;
}

BOOLEAN
KEAPI
IdeCheckDevicePresence(
	UCHAR Channel,
	UCHAR Device
	)
{
	const ULONG DefTimeout = 10000;
	ULONG Timeout = DefTimeout;
	while (IdeGetState(Channel).Busy && Timeout>0)
		Timeout--;

	if (Timeout==0)
	{
		//IdeControlChannel (Channel, 1);
		return 0;
	}

	IDE_PORT_6 port6 = {0};
	port6.Reserved1 = 1;
	port6.Reserved2 = 1;
	port6.Dev = Device;
	IdeWritePort (Channel, 6, *(UCHAR*)&port6);

	Timeout = DefTimeout;
	while (!(IdeGetState(Channel).Busy == 0 && IdeGetState(Channel).DeviceReady == 1) && Timeout > 0)
		Timeout --;

	if (Timeout==0)
	{
		if (Device)
		{
			port6.Dev = 0;
			IdeWritePort (Channel, 6, *(UCHAR*)&port6);
		}
		return 0;
	}

	return 1;
}

#define PHYS_IDE_CB_MASK	'_IDE'

struct IDE_DEVICE_IDENTIFICATION;

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
			IDE_DEVICE_IDENTIFICATION *Identification;
		};
		struct
		{
			ULONG PartitionStart;
			ULONG PartitionEnd;
			PFILE RelatedPhysical;
		};
	};
};

#define IdePrint(x) KiDebugPrint x

#define DUMP_STRUC_FIELD(OBJECT, FNAME, TYPE) IdePrint(( #OBJECT "->" #FNAME " = " TYPE "\n", (OBJECT)->FNAME ));

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

#pragma pack(2)

struct IDE_DEVICE_IDENTIFICATION
{
	USHORT Reserved1 : 2;
	USHORT ResonseIncomplete : 1;
	USHORT Reserved2 : 3;
	USHORT NotRemovableController : 1;
	USHORT RemovableMedia : 1;
	USHORT Reserved3 : 7;
	USHORT ATADevice : 1;
	USHORT NumberCylinders;
	USHORT SpecificConfiguration;
	USHORT NumberHeads;
	USHORT Reserved4[2];
	USHORT SectorsPerTrack;
	USHORT Reserved5[3];
	USHORT SerialNumber [10];
	USHORT Reserved6[3];
	USHORT FirmwareRevision [4];
	USHORT ModelNumber [20];
	//47

	USHORT MaxSectPerIrqMult : 8;
	USHORT Reserved7 : 8;
	
	USHORT Reserved8;
	
	USHORT Reserved9 : 10;
	USHORT IORDYMayBeDisabled : 1;
	USHORT IORDYSupported : 1;
	USHORT Reserved10 : 1;
	USHORT StandyTimerSupported : 1;
	USHORT Reserved11 : 2;
	
	USHORT Reserved12 [3];

	USHORT Fields5458Valid : 1;
	USHORT Fields6470Valid : 1;
	USHORT Fields88Valid : 1;
	USHORT Reserved : 13;
	USHORT NumberCurrentCylinders;
	USHORT NumberCurrentHeads;
	USHORT NumberCurrentSectorsPerTrack;
	ULONG CapacityInSectors;

	// ...
};

STATIC_ASSERT (sizeof(IDE_DEVICE_IDENTIFICATION) == 118);
#pragma pack()


void
IdeStringToCString(
	USHORT *IdeString,
	char *CString,
	ULONG nWords
	)
{
	ULONG i;
	for (i=0; i<nWords; i++)
	{
		CString[i*2] = IdeString[i] >> 8;
		CString[i*2 + 1] = IdeString[i] & 0xFF;
	}
	CString[i*2] = 0;
}

STATUS
KEAPI
IdeIdentifyDevice(
	IN UCHAR Channel,
	IN UCHAR Device,
	OUT IDE_DEVICE_IDENTIFICATION **pID
	)
/*++
	Perform device identification
--*/
{
	IDE_PORT_6 port6 = {0};
	port6.Reserved1 = 1;
	port6.Reserved2 = 1;
	port6.Dev = Device;

	IdeWritePort (Channel, 6, *(UCHAR*)&port6);


	ULONG Timeout = 1000000;
	while (!(IdeGetState(Channel).Busy == 0 && IdeGetState(Channel).DeviceReady == 1) && Timeout > 0)
	{
		Timeout --;
	}

	if (Timeout==0)
	{
		IdePrint(("TIMEDOUT\n"));
		return STATUS_DEVICE_NOT_READY;
	}


	IdeWritePort (Channel, 7, IDE_IDENTIFY_DEVICE);


	Timeout = 1000000;
	while (!(IdeGetState(Channel).Busy == 0 && IdeGetState(Channel).DeviceReady == 1) && Timeout > 0)
	{
		Timeout --;
	}

	if (Timeout==0)
	{
		IdePrint(("TIMEDOUT\n"));
		return STATUS_DEVICE_NOT_READY;
	}

	
	char *buf = (char*) MmAllocatePage ();
	IdeRead (Channel, buf, 512);


	KdPrint(("Device on IDE[%d:%d] identified:\n", Channel, Device));

	IDE_DEVICE_IDENTIFICATION *id = (IDE_DEVICE_IDENTIFICATION*) buf;
	
	KdPrint(("Removable Media : %d\n", id->RemovableMedia));
	KdPrint(("ResonseIncomplete : %d\n", id->ResonseIncomplete));
	KdPrint(("NotRemovableController : %d\n", id->NotRemovableController));
	KdPrint(("ATADevice : %d\n", id->ATADevice));
	KdPrint(("NumberCylinders : %d\n", id->NumberCylinders));
	KdPrint(("NumberHeads : %d\n", id->NumberHeads));
	KdPrint(("SectorsPerTrack : %d\n", id->SectorsPerTrack));

	char Serial[21];
	IdeStringToCString (id->SerialNumber, Serial, 10);
	KdPrint(("SerialNumber : %s\n", Serial));

	char Revision[7];
	IdeStringToCString (id->FirmwareRevision, Revision, 3);
	KdPrint(("FirmwareRevision : %s\n", Revision));

	char Model[41];
	IdeStringToCString (id->ModelNumber, Model, 20);
	KdPrint(("ModelNumber : %s\n", Model));

	*pID = id;

	return STATUS_SUCCESS;
}

STATUS
KEAPI
IdePerformRead(
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
		IdePrint(("FileObject=%08x\n", FileObject));
	}
	ASSERT (MmIsAddressValid(FileObject));
	ASSERT (MmIsAddressValid(FileObject->DeviceObject));

	PHYSICAL_IDE_CONTROL_BLOCK *pcb = (PHYSICAL_IDE_CONTROL_BLOCK*)(FileObject->FsContext2);

#define CHANNEL (pcb->Channel)
#define DEVICE  (pcb->Device)

//	IdePrint(("HD: performing generic read, pcb=%08x\n", pcb));
	//DUMP_PCB (pcb);

	if (pcb->Mask != PHYS_IDE_CB_MASK)
	{
		IdePrint(("pcb->Mask=%08x\n", pcb->Mask));

		UNICODE_STRING name;
		STATUS Status = ObQueryObjectName (FileObject->DeviceObject, &name);
		IdePrint(("device name %S\n", name.Buffer));
	}
	ASSERT (pcb->Mask == PHYS_IDE_CB_MASK);

//	if (SectorNumber > 70)
//		INT3;

	if (pcb->PhysicalOrPartition == 0)
	{
		//IdePrint(("HD: [%08x] Recursion read for partition [%08x -> %08x]\n", FileObject, FileObject->DeviceObject, pcb->RelatedPhysical->DeviceObject));

		//IdePrint(("Dumping related PCB\n"));		
		//DUMP_PCB ( (PHYSICAL_IDE_CONTROL_BLOCK*)pcb->RelatedPhysical->FsContext2 );

		//return IdePerformRead (pcb->RelatedPhysical, SectorNumber /*already adjusted by HddRead()*/, Buffer, Size);
		return IdePerformRead (pcb->RelatedPhysical, SectorNumber + pcb->PartitionStart, Buffer, Size, nBytesWritten);
	}

//	IdePrint(("HD: performing read IDE[%d:%d], Sector %08x\n", CHANNEL, DEVICE, SectorNumber));

	ASSERT (CHANNEL < 2);
	ASSERT (DEVICE < 2);

	IDE_PORT_6 port6 = {0};
	port6.AddressingMode = 1;
	port6.Reserved1 = 1;
	port6.Reserved2 = 1;
	port6.Dev = DEVICE;

	IdeWritePort (CHANNEL, 6, *(UCHAR*)&port6);

_retry:

	Timeout = 1000000;

//	IdePrint(("HD: ~BSY.. "));

	while (IdeGetState(CHANNEL).Busy && Timeout>0)
		Timeout--;

	if (Timeout==0)
	{
		IdePrint(("TIMEDOUT\n"));
		return STATUS_DEVICE_NOT_READY;
	}

//	IdePrint(("~DEV.. "));

	Timeout = 1000000;
	while (!(IdeGetState(CHANNEL).Busy == 0 && IdeGetState(CHANNEL).DeviceReady == 1) && Timeout > 0)
	{
		Timeout --;
	}

	if (Timeout==0)
	{
		IdePrint(("TIMEDOUT\n"));

		IdeControlChannel (CHANNEL, 4);
		for (int i=0; i<1000000; i++) __asm nop;
		IdeControlChannel (CHANNEL, 0);

		goto _retry;

		return STATUS_DEVICE_NOT_READY;
	}

//	IdePrint(("OK. "));

	ASSERT ( (SectorNumber & 0xFF000000) == 0 );

	IdePulsedIrq[CHANNEL] = 0;

	IdeWritePort (CHANNEL, 2, 1);								// 1 cluster
	IdeWritePort (CHANNEL, 3, SectorNumber & 0xFF);
	IdeWritePort (CHANNEL, 4, (SectorNumber >> 8) & 0xFF);
	IdeWritePort (CHANNEL, 5, (SectorNumber >> 16) & 0xFF);

	IdeWritePort (CHANNEL, 7, IDE_CMD_READ);

//	IdePrint(("~SENDING "));

	Timeout = 1000000;
	while (IdeGetState(CHANNEL).Busy && Timeout > 0)
		Timeout --;

	if (Timeout==0)
	{
		IdePrint(("TIMEDOUT\n"));
		return STATUS_DEVICE_NOT_READY;
	}

	if (IdeGetState(CHANNEL).DeviceFailure)
	{
		IdePrint(("FAILED\n"));
		return STATUS_INTERNAL_FAULT;
	}

	if (IdeGetState(CHANNEL).Error)
	{
		UCHAR err = IdeReadPort (CHANNEL, 1);
		IdePrint(("ERROR [%02x]\n", err));

		if (err == 0x20)
			return STATUS_NO_MEDIA_IN_DEVICE;

		return STATUS_UNSUCCESSFUL;
	}

//	IdePrint(("~READING.. "));

	//Timeout = 1000000;
	//while (!IdeGetState(CHANNEL).DataRequestReady && Timeout > 0)
	//	Timeout --;

	while (!IdePulsedIrq[CHANNEL]);
	IdePulsedIrq[CHANNEL] = 0;

	if (Timeout==0)
	{
		IdePrint(("TIMEDOUT\n"));
		return STATUS_DEVICE_NOT_READY;
	}

	if (IdeGetState(CHANNEL).DeviceFailure)
	{
		IdePrint(("FAILED\n"));
		return STATUS_INTERNAL_FAULT;
	}

	if (IdeGetState(CHANNEL).Error)
	{
		IdePrint(("ERROR\n"));
		return STATUS_UNSUCCESSFUL;
	}

	IdeRead (CHANNEL, Buffer, ALIGN_UP(Size, SECTOR_SIZE)/2);

//	IdePrint(("OK\n"));

	/*
	IdePrint (("Read succedded for [%d:%d], Sector %d, [%02x %02x %02x %02x  %02x %02x %02x %02x]\n",
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
IdePerformWrite(
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
		IdePrint(("FileObject=%08x\n", FileObject));
	}
	ASSERT (MmIsAddressValid(FileObject));
	ASSERT (MmIsAddressValid(FileObject->DeviceObject));

	PHYSICAL_IDE_CONTROL_BLOCK *pcb = (PHYSICAL_IDE_CONTROL_BLOCK*)(FileObject->FsContext2);

#define CHANNEL (pcb->Channel)
#define DEVICE  (pcb->Device)

	if (pcb->Mask != PHYS_IDE_CB_MASK)
	{
		IdePrint(("pcb->Mask=%08x\n", pcb->Mask));

		UNICODE_STRING name;
		STATUS Status = ObQueryObjectName (FileObject->DeviceObject, &name);
		IdePrint(("device name %S\n", name.Buffer));
	}
	ASSERT (pcb->Mask == PHYS_IDE_CB_MASK);

	if (pcb->PhysicalOrPartition == 0)
	{
		return IdePerformWrite (pcb->RelatedPhysical, SectorNumber + pcb->PartitionStart, Buffer, Size, nBytesWritten);
	}

	IdePrint(("HD: performing write IDE[%d:%d], Sector %08x\n", CHANNEL, DEVICE, SectorNumber));

	ASSERT (CHANNEL < 2);
	ASSERT (DEVICE < 2);

	IDE_PORT_6 port6 = {0};
	port6.AddressingMode = 1;
	port6.Reserved1 = 1;
	port6.Reserved2 = 1;
	port6.Dev = DEVICE;

	IdeWritePort (CHANNEL, 6, *(UCHAR*)&port6);

_retry:

	Timeout = 1000000;

	while (IdeGetState(CHANNEL).Busy && Timeout>0)
		Timeout--;

	if (Timeout==0)
	{
		IdePrint(("TIMEDOUT\n"));
		return STATUS_DEVICE_NOT_READY;
	}

	Timeout = 1000000;
	while (!(IdeGetState(CHANNEL).Busy == 0 && IdeGetState(CHANNEL).DeviceReady == 1) && Timeout > 0)
	{
		Timeout --;
	}

	if (Timeout==0)
	{
		IdePrint(("TIMEDOUT\n"));

		IdeControlChannel (CHANNEL, 4);
		for (int i=0; i<1000000; i++) __asm nop;
		IdeControlChannel (CHANNEL, 0);

		goto _retry;

		return STATUS_DEVICE_NOT_READY;
	}

	ASSERT ( (SectorNumber & 0xFF000000) == 0 );

	IdePulsedIrq[CHANNEL] = 0;

	IdeWritePort (CHANNEL, 2, 1);								// 1 cluster
	IdeWritePort (CHANNEL, 3, SectorNumber & 0xFF);
	IdeWritePort (CHANNEL, 4, (SectorNumber >> 8) & 0xFF);
	IdeWritePort (CHANNEL, 5, (SectorNumber >> 16) & 0xFF);

	IdeWritePort (CHANNEL, 7, IDE_CMD_WRITE);


	Timeout = 1000000;
	while (IdeGetState(CHANNEL).Busy && Timeout > 0)
		Timeout --;

	if (Timeout==0)
	{
		IdePrint(("TIMEDOUT\n"));
		return STATUS_DEVICE_NOT_READY;
	}

	if (IdeGetState(CHANNEL).DeviceFailure)
	{
		IdePrint(("FAILED\n"));
		return STATUS_INTERNAL_FAULT;
	}

	if (IdeGetState(CHANNEL).Error)
	{
		UCHAR err = IdeReadPort (CHANNEL, 1);
		IdePrint(("ERROR [%02x]\n", err));
		return STATUS_UNSUCCESSFUL;
	}


	while (!IdePulsedIrq[CHANNEL]);
	IdePulsedIrq[CHANNEL] = 0;

	if (Timeout==0)
	{
		IdePrint(("TIMEDOUT\n"));
		return STATUS_DEVICE_NOT_READY;
	}

	if (IdeGetState(CHANNEL).DeviceFailure)
	{
		IdePrint(("FAILED\n"));
		return STATUS_INTERNAL_FAULT;
	}

	if (IdeGetState(CHANNEL).Error)
	{
		IdePrint(("ERROR\n"));
		return STATUS_UNSUCCESSFUL;
	}

	IdeWrite (CHANNEL, Buffer, ALIGN_UP(Size, SECTOR_SIZE)/2);

	IdePrint (("IDE: Write succedded for [%d:%d], Sector %d, [%02x %02x %02x %02x  %02x %02x %02x %02x]\n",
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
IdeAddDevice(
	PDRIVER DriverObject,
	UCHAR Number
	)
{
	STATUS Status;
	PDEVICE DeviceObject;

	RtlInitUnicodeString (&DeviceName, L"\\Device\\hda");
	DeviceName.Buffer[10] = 'a' + Number;

	IdePrint(("IDE: Creating device %S on IDE channel\n", DeviceName.Buffer));

	Status = IoCreateDevice (
		DriverObject,
		sizeof(PHYSICAL_IDE_CONTROL_BLOCK),
		&DeviceName,
		DEVICE_TYPE_DISK,
		&DeviceObject
		);

	if (!SUCCESS(Status))
	{
		IdePrint(("[~] IDE: Cannot create device, IoCreateDevice failed with status %08x\n", Status));
		return Status;
	}

	UCHAR Channel = Number >> 1;
	UCHAR Device = Number & 1;

	IdePrint(("IDE: Identifying device [%d:%d]\n", Channel, Device));

	IDE_DEVICE_IDENTIFICATION *id;

	Status = IdeIdentifyDevice (Channel, Device, &id);
	if (!SUCCESS(Status))
	{
		IdePrint(("[~] IDE device identification failed with status %08x\n", Status));
		return Status;
	}


	PHYSICAL_IDE_CONTROL_BLOCK* pcb = (PHYSICAL_IDE_CONTROL_BLOCK*)(DeviceObject+1);
	pcb->Mask = PHYS_IDE_CB_MASK;
	pcb->Channel = Channel;
	pcb->Device = Device;
	pcb->PhysicalOrPartition = 1;
	pcb->Identification = id;


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
		IdePrint(("[~] IDE: Cannot open created device, IoCreateFile failed with status %08x\n", Status));
		return Status;
	}

	if (id->RemovableMedia)
	{
		IdePrint(("[~] IDE: Removable-Media device. Trying to find ECMA-119\n"));

		char FirstExtent[2048];
		LARGE_INTEGER Offset = {0};
		Offset.LowPart = 2048 * 16;

		Status = IoReadFile (
			PhysicalFileObject,
			FirstExtent,
			2048,
			&Offset,
			0,
			&IoStatus
			);

		if (!SUCCESS(Status))
		{
			IdePrint(("[-] IDE: Couln't read CDROM first extent [IoReadFile returned %08x]\n", Status));
			IoCloseFile (PhysicalFileObject);
			return Status;
		}

		IdePrint(("[~] IDE:  Signature = %s\n", &FirstExtent[1]));

		return STATUS_SUCCESS;
	}

	//
	// Read partition table
	//

	IdePrint(("[~] IDE: Reading partition table\n"));

	UCHAR Buffer[512];

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
		IdePrint(("[~] IDE: Cannot read partition table, IoReadFile failed with status %08x\n", Status));
		IoCloseFile (PhysicalFileObject);
		return Status;
	}

	//
	// Parse MBR
	//

	if (Buffer[511] != 0xAA || Buffer[510] != 0x55)
	{
		IdePrint(("[~] IDE: Master Boot Record BAD on disk %S\n", DeviceName.Buffer));
		IoCloseFile (PhysicalFileObject);
		return STATUS_UNSUCCESSFUL;
	}

	PMBR_PARTITION PartitionTable = (PMBR_PARTITION) &Buffer[0x1BE];

	for (int i=0; i<4; i++)
	{
		if (!(PartitionTable[i].Active == 0x80 || PartitionTable[i].Active == 0x00))
		{
			IdePrint(("[~] IDE: Partition table BAD on disk %S\n", DeviceName.Buffer));
			IoCloseFile (PhysicalFileObject);
			return STATUS_UNSUCCESSFUL;
		}

		IdePrint(("Partition#%d: Active=%02x,Type=%02x,FirstSector=%d,NumberSectors=%d\n",
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

		IdePrint(("IDE: Creating partition %S.. ", PartitionName.Buffer));

		Status = IoCreateDevice (
			DriverObject,
			sizeof(PHYSICAL_IDE_CONTROL_BLOCK),
			&PartitionName,
			DEVICE_TYPE_DISK,
			&Partition
			);

		if (!SUCCESS(Status))
		{
			IdePrint(("Cannot create device, IoCreateDevice failed with status %08x\n", Status));
			return Status;
		}

		pcb = (PHYSICAL_IDE_CONTROL_BLOCK*)(Partition+1);
		pcb->Mask = PHYS_IDE_CB_MASK;
		pcb->PhysicalOrPartition = 0;
		pcb->PartitionStart = PartitionTable[i].FirstSector;
		pcb->PartitionEnd = PartitionTable[i].FirstSector + PartitionTable[i].NumberOfSectors;
		pcb->RelatedPhysical = PhysicalFileObject;

		IdePrint(("Created partition %08x\n", Partition));

		switch (PartitionTable[i].Type)
		{
		case 0x01:
		case 0x04:
		case 0x06:
		case 0x0B:
			{
				IdePrint(("Fat partition found [%02x]. Mounting..\n", PartitionTable[i].Type));

				UNICODE_STRING FatName;
				RtlInitUnicodeString( &FatName, L"\\FileSystem\\Fat" );
				STATUS Status;
				PDEVICE FatFs;

				Status = ObReferenceObjectByName (&FatName, IoDeviceObjectType, KernelMode, FILE_READ_DATA, NULL, (PVOID*)&FatFs);
				if (!SUCCESS(Status))
				{
					IdePrint(("ObReferenceObjectByName failed for FAT fs: %08x\n", Status));
					continue;
				}

				Status = IoRequestMount (Partition, FatFs);

				ObDereferenceObject (FatFs);

				if (!SUCCESS(Status))
				{
					IdePrint(("IDE: IoRequestMount failed : %08x\n", Status));
					return Status;
				}
				else
				{
					IdePrint(("IDE: Mounted partition %S for FAT file system\n", PartitionName.Buffer));
				}
			}
			break;
		}
	}

	return Status;
}

STATUS
KEAPI
IdeCreateClose(
	PDEVICE DeviceObject,
	PIRP Irp
	)
{
	IdePrint(("IDE: %s for %S\n", Irp->MajorFunction==IRP_CREATE ? "IRP_CREATE" : "IRP_CLOSE",
		Irp->FileObject->RelativeFileName.Buffer));

	if (Irp->MajorFunction == IRP_CREATE)
	{
		//
		// Initialize caching for the hdd (support only read-only access)
		//

		CCFILE_CACHE_CALLBACKS Callbacks = { IdePerformRead, IdePerformWrite };
		CcInitializeFileCaching (Irp->FileObject, SECTOR_SIZE, &Callbacks);

		KdPrint(("IDE CREAT: Irp->File->CacheMap %08x\n", Irp->FileObject->CacheMap));

		Irp->FileObject->FsContext2 = (DeviceObject+1); // IDE Drive Control Block

		PHYSICAL_IDE_CONTROL_BLOCK* pcb = (PHYSICAL_IDE_CONTROL_BLOCK*)(DeviceObject+1);

		IdePrint(("IDE Control Block saved: *=%08x, SIGN=%08x[%08x], PhysOrPart=%d\n", pcb, pcb->Mask,PHYS_IDE_CB_MASK, pcb->PhysicalOrPartition));
		if (pcb->PhysicalOrPartition == 0) // partition
		{
			if (!MmIsAddressValid (pcb->RelatedPhysical))
			{
				IdePrint(("PCB INVALID [RelatedPhysical=%08]\n", pcb->RelatedPhysical));
				INT3
			}
			if (!MmIsAddressValid (pcb->RelatedPhysical->DeviceObject))
			{
				IdePrint(("PCB INVALID [RelatedPhysical=%08x, RPH->DevObj=%08x]\n", pcb->RelatedPhysical->DeviceObject));
				INT3
			}
			
			IdePrint(("Related physical: File=%08x, Device=%08x", pcb->RelatedPhysical, pcb->RelatedPhysical->DeviceObject));

			pcb = (PHYSICAL_IDE_CONTROL_BLOCK*)(pcb->RelatedPhysical->DeviceObject + 1);

			IdePrint(("PCB=%08x [SIGN=%08x, CH=%d DV=%d]\n", pcb, pcb->Mask, pcb->Channel, pcb->Device));
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
IdeReadWrite(
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

		IdePrint(("HD: Resolving partition [PS=%d] : Sector %d -> %d\n", pcb->PartitionStart, 
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
IdeFsControl(
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
	IdePrint(("[~] LKM DriverEntry()\n"));

	KiConnectInterrupt (14, Ide0ProcessIrq);
	KiConnectInterrupt (15, Ide1ProcessIrq);

//	IdeControlChannel (0, 2);
//	IdeControlChannel (1, 2);

	BOOLEAN Presence[4];

	IdePrint(("Checking presence (0,0).. "));
	Presence[0] = IdeCheckDevicePresence (0, 0);
	IdePrint(("%s\n", (Presence[0] ? "TRUE" : "FALSE")));

	IdePrint(("Checking presence (0,1).. "));
	Presence[1] = IdeCheckDevicePresence (0, 1);
	IdePrint(("%s\n", (Presence[1] ? "TRUE" : "FALSE")));

	IdePrint(("Checking presence (1,0).. "));
	Presence[2] = IdeCheckDevicePresence (1, 0);
	IdePrint(("%s\n", (Presence[2] ? "TRUE" : "FALSE")));

	IdePrint(("Checking presence (1,1).. "));
	Presence[3] = IdeCheckDevicePresence (1, 1);
	IdePrint(("%s\n", (Presence[3] ? "TRUE" : "FALSE")));

	DriverObject->IrpHandlers[IRP_CREATE] =
	DriverObject->IrpHandlers[IRP_CLOSE]  = IdeCreateClose;
	DriverObject->IrpHandlers[IRP_READ]   = 
	DriverObject->IrpHandlers[IRP_WRITE]  = IdeReadWrite;
	//DriverObject->IrpHandlers[IRP_FSCTL]  = IdeFsControl;

	//
	// Create four device objects
	//

	if (Presence[0])
		IdeAddDevice (DriverObject, 0);

	if (Presence[1])
		IdeAddDevice (DriverObject, 1);

	if (Presence[2])
		IdeAddDevice (DriverObject, 2);

	if (Presence[3])
		IdeAddDevice (DriverObject, 3);


	IdePrint(("IDE: Finished\n"));
	return STATUS_SUCCESS;
}
