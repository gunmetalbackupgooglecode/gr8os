#include "common.h"

//
// Out byte to data port
//
void FdpOut(UCHAR parm)
{
	while (!(KiInPort(FD_STATUS) & FD_STATUS_RDY_FOR_IO));
	
	KiOutPort (FD_DATA, parm);
}

//
// Read byte from data port
//
UCHAR FdpIn()
{
	while (!(KiInPort(FD_STATUS) & FD_STATUS_RDY_FOR_IO));

	return KiInPort (FD_DATA);
}

//
// Wait for FD IRQ
//
void FdpWait()
{
	_enable();
	while (!FdpIrqState.FddIrqGot);
	FdpIrqState.FddIrqGot = 0;
}


// From linux-1.0 kernel
FD_MEDIA_TYPE FdpFloppyTypes[] = {
	{  720, 9,2,40,0,0x2A,0x02,0xDF,0x50,"360k/PC" }, /* 360kB PC diskettes */
	{  720, 9,2,40,0,0x2A,0x02,0xDF,0x50,"360k/PC" }, /* 360kB PC diskettes */
	{ 2400,15,2,80,0,0x1B,0x00,0xDF,0x54,"1.2M" },	  /* 1.2 MB AT-diskettes */
	{  720, 9,2,40,1,0x23,0x01,0xDF,0x50,"360k/AT" }, /* 360kB in 1.2MB drive */
	{ 1440, 9,2,80,0,0x2A,0x02,0xDF,0x50,"720k" },	  /* 3.5" 720kB diskette */
	{ 1440, 9,2,80,0,0x2A,0x02,0xDF,0x50,"720k" },	  /* 3.5" 720kB diskette */
	{ 2880,18,2,80,0,0x1B,0x00,0xCF,0x6C,"1.44M" },	  /* 1.44MB diskette */
	{ 1440, 9,2,80,0,0x2A,0x02,0xDF,0x50,"720k/AT" }, /* 3.5" 720kB diskette */
};

// disk type until the next media change occurs
FD_MEDIA_TYPE *FdpCurrentType;

void LbaToChs (ULONG LbaSector, ULONG *Cylinder, ULONG *Head, ULONG *Sector)
{
//	KdPrint(("CT: S=%x H=%x\n", FdpCurrentType->SectorsPerTrack, FdpCurrentType->NumberOfHeads));

	*Sector = ((LbaSector % FdpCurrentType->SectorsPerTrack) + 1);
	*Head = ((LbaSector / FdpCurrentType->SectorsPerTrack) % FdpCurrentType->NumberOfHeads);
	*Cylinder = ((LbaSector / FdpCurrentType->SectorsPerTrack) / FdpCurrentType->NumberOfHeads);
}

// FDC reply buffer
UCHAR FdpReplyBuffer [MAX_REPLIES];
#define ST0 (FdpReplyBuffer[0])
#define ST1 (FdpReplyBuffer[1])
#define ST2 (FdpReplyBuffer[2])
#define ST3 (FdpReplyBuffer[3])

ULONG FdpCurrentSector = 0;
ULONG FdpCurrentHead = 0;
ULONG FdpCurrentTrack = 0;


VOID
KEAPI
FdMotorOn(
	UCHAR Drive
	)
/*++
	Turns on the motor of the selected drive
--*/
{
	_enable();
	
	FD_3F2 port = {0};
	port.FdSelect = (Drive == 1);
	port.ControllerEnabled = 1;
	port.EnableIntsDma = 1;
	port.EnableMotor1 = 1;

	KiOutPort (FD_DOR, port.RawValue);
}

VOID
KEAPI
FdMotorOff(
	UCHAR Drive
	)
/*++
	Turns on the motor of the selected drive
--*/
{
	_enable();
	
	FD_3F2 port = {0};
	port.FdSelect = (Drive == 1);
	port.ControllerEnabled = 1;
	port.EnableIntsDma = 1;
	port.EnableMotor1 = 0;

	KiOutPort (FD_DOR, port.RawValue);
}

VOID
KEAPI
FdResetController(
	)
/*++
	Resets the FDC controller
--*/
{
	FD_3F2 port = {0};
	port.RawValue = KiInPort (FD_DOR);
	
	port.ControllerEnabled = 0;
	KiOutPort (FD_DOR, port.RawValue);

	port.ControllerEnabled = 1;
	KiOutPort (FD_DOR, port.RawValue);

	while (!(KiInPort(FD_STATUS) & FD_STATUS_RDY_FOR_IO));
}

VOID
KEAPI
FdPrepareController(
	UCHAR Drive
	)
/*++
	Prepares controller for the i/o
--*/
{
	FdMotorOn (Drive);
	while (!(KiInPort(FD_STATUS) & FD_STATUS_RDY_FOR_IO));

	//FdpWait();
}



STATUS
KEAPI
FdInit(
	)
{
	KdPrint(("FdInit()\n"));
	FdpIrqState.FddIrqGot = 0;

	FdpCurrentType = &FdpFloppyTypes[6];

	// Select & enable first controller
	FdResetController ();

	FdPrepareController (0);

	KdPrint(("FD: Controller prepared\n"));

	// Initialize controller
	FdpOut (FD_OPERATION_INITIALIZE);
	FdpOut (FD_OP2_INITIALIZE);

	//FdpWait();
	while (!(KiInPort(FD_STATUS) & FD_STATUS_RDY_FOR_IO));

	KdPrint(("FD: Controller initialized\n"));

	// 500 KB/s
	FDP_WRITE_FDC_CONTROL_REG (0);

	FdMotorOff (0);
	while (!(KiInPort(FD_STATUS) & FD_STATUS_RDY_FOR_IO));

	KdPrint(("FD: Controller off\n"));

	return STATUS_SUCCESS;
}


STATUS
KEAPI
FdPerformRead(
	IN PFILE FileObject,
	IN ULONG ClusterNumber,
	OUT PVOID Buffer,
	IN ULONG Size,
	OUT PULONG nBytesRead
	)
{
	ULONG LbaSector = /* Sector to cluster */ ClusterNumber;

	if (LbaSector > FdpCurrentType->NumberOfSectors)
	{
		KdPrint(("FDD: Read beyond the end of disk (LbaSector=%d(%x), NumberOfSectors=%d(%x)\n",
			LbaSector, LbaSector,
			FdpCurrentType->NumberOfSectors, FdpCurrentType->NumberOfSectors
			));
		return STATUS_INVALID_PARAMETER;
	}

	ULONG* BufferLength = &Size;

	ULONG Cylinder, Head, Sector;
	LbaToChs (LbaSector, &Cylinder, &Head, &Sector);

	if (Sector > FdpCurrentType->SectorsPerTrack)
	{
		KdPrint(("FDD: LBA->CHS conversion failure!! LBA was %d(%x), C=%d(%x), H=%d(%x), S=%d(%x), SpT=%d(%x)\n",
			LbaSector, LbaSector,
			Cylinder, Cylinder,
			Head, Head,
			Sector, Sector,
			FdpCurrentType->SectorsPerTrack, FdpCurrentType->SectorsPerTrack
			));
		return STATUS_INVALID_PARAMETER;
	}

//	ULONG Cylinder = LbaSector / (FdpCurrentType->NumberOfHeads * FdpCurrentType->NumberOfSectors);
//	ULONG Head = (LbaSector % (FdpCurrentType->NumberOfHeads * FdpCurrentType->NumberOfSectors)) / FdpCurrentType->NumberOfSectors;
//	ULONG Sector = (LbaSector % (FdpCurrentType->NumberOfHeads * FdpCurrentType->NumberOfSectors)) % FdpCurrentType->NumberOfSectors + 1;

	FdPrepareController (0);

//	KdPrint(("FdRead: LBA=%d, C=%d, H=%d, S=%d\n", LbaSector, Cylinder, Head, Sector));

	PDMA_REQUEST DmaReq;
	STATUS Status;

	Status = HalInitializeDmaRequest (DMA_MODE_READ, FLOPPY_DMA, Buffer, *BufferLength, &DmaReq);
	if (!SUCCESS(Status))
	{
		KdPrint(("HalInitializeDmaRequest failed with status %08x\n", Status));
		return Status;
	}

//	KdPrint(("FdRead: dma initialized (PageUsed=%08x,PageSize=%08x,Buff=%08x,Mapped=%08x)\n",
//		DmaReq->PageUsed,
//		DmaReq->PageCount,
//		DmaReq->Buffer,
//		DmaReq->MappedPhysical));

	FdpOut (FD_OPERATION_READ);
	FdpOut ((UCHAR)Head << 2); // HDS=0, DS=0

	FdpOut ((UCHAR)Cylinder); // CYL 0
	FdpOut ((UCHAR)Head); // HEAD 0
	FdpOut ((UCHAR)Sector); // SECTOR 1

	FdpOut (2); // sector size=512
	FdpOut ((UCHAR)FdpCurrentType->NumberOfSectors);
	FdpOut (FdpCurrentType->Gap1Size);
	FdpOut (0xFF);

	ULONG TryCount = 1000000;

	while ( (!(KiInPort(FD_STATUS) & FD_STATUS_RDY_FOR_IO)) && TryCount>0 )
	{
		TryCount --;
	}

	if (TryCount == 0)
	{
		KdPrint(("FdRead: Timed out\n"));
	}
	else
	{
//		KdPrint(("FdRead: Read completed\n"));
	}

	ST0 = FdpIn ();
	ST1 = FdpIn ();
	ST2 = FdpIn ();
	FdpReplyBuffer[3] = FdpIn (); // C
	FdpReplyBuffer[4] = FdpIn (); // H
	FdpReplyBuffer[5] = FdpIn (); // R
	FdpReplyBuffer[6] = FdpIn (); // N

//	KdPrint(("FD: ST0=%02x, ST1=%02x, ST2=%02x, C=%02x, H=%02x, R=%02x, N=%02x\n",
//		ST0,
//		ST1,
//		ST2,
//		FdpReplyBuffer[3],
//		FdpReplyBuffer[4],
//		FdpReplyBuffer[5],
//		FdpReplyBuffer[6]));

	HalCompleteDmaRequest (DmaReq);

//	KdPrint(("FdRead: Buffer: %02x %02x %02x %02x\n",
//		((UCHAR*)Buffer)[0],
//		((UCHAR*)Buffer)[1],
//		((UCHAR*)Buffer)[2],
//		((UCHAR*)Buffer)[3]));

	FD_ST0 st0;
	*(UCHAR*)&st0 = ST0;

	if (st0.InterruptCode == FD_NOT_READY)
	{
		Status = STATUS_DEVICE_NOT_READY;
	}

	if( st0.InterruptCode == FD_ABORT)
	{
		FD_ST1 st1;
		*(UCHAR*)&st1 = ST1;

		Status = STATUS_UNSUCCESSFUL;

		if (st1.SectorNotFound)
		{
			KdPrint(("FD: Sector not found\n"));
			Status = STATUS_INVALID_PARAMETER;
		}
	}

	FdMotorOff (0);

	if (SUCCESS(Status))
	{
		*nBytesRead = Size;
	}

	return Status;
}
