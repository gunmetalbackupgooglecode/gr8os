#pragma once

#define IDE_CHANNEL_1_BASE	0x1F0
#define IDE_CHANNEL_2_BASE	0x170

#define IDE_CHANNEL_1_CONTROL	0x3F6
#define IDE_CHANNEL_2_CONTROL	0x376

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


