
//
// FD driver header
//

#pragma once

STATUS
KEAPI
FdDriverEntry(
	PDRIVER DriverObject
	);

STATUS
KEAPI
FdPerformRead(
	IN PFILE FileObject,
	IN ULONG ClusterNumber,
	OUT PVOID Buffer,
	IN ULONG Size
	);

STATUS
KEAPI
FdPerformWrite(
	OUT PVOID Buffer,
	IN OUT PULONG BufferLength,
	IN ULONG LbaSector,
	IN BOOLEAN SynchronousOperation
	);

STATUS
KEAPI
FdInit(
	);


#define FD_SECTOR_SIZE	512
#define FD_SECTOR_SHIFT	9


#define FD_BASE 0x3f0
//#define FD_STATUS_A	(FD_BASE + 0)	/* Status register A */
//#define FD_STATUS_B	(FD_BASE + 1)	/* Status register B */
#define FD_DOR		(FD_BASE + 2)	/* Digital Output Register */

//
// 3F2 port bit map (w/o)
//
//   0-1    FD select. (IBM PC/AT do not use 1 bit)
//   2      0 - controller reset, 1 - controller enable
//   3      1 - enable interrupts & DMA
//   4-7    Enable motor (6-7 unused)
//

typedef union FD_3F2
{
	UCHAR RawValue;
	struct
	{
		UCHAR FdSelect : 1;
		UCHAR Reserved : 1;
		UCHAR ControllerEnabled : 1;
		UCHAR EnableIntsDma : 1;
		UCHAR EnableMotor1 : 1;
		UCHAR EnableMorot2 : 1;
		UCHAR Reserved2 : 2;
	};
} *PFD_3F2;


//#define FD_TDR		(FD_BASE + 3)	/* Tape Drive Register */
#define FD_STATUS	(FD_BASE + 4)	/* Main Status Register */

//
// 3F4 port bit map (r/o)
//
//   0-3    FD busy (2-3 unused)
//   4      FD busy with i/o operation
//   5      0 - DMA used, 1 - DMA is not used
//   6      Data flow direction (0 - FD<-CPU, 1 - FD->CPU)
//   7      FD ready for i/o
//

#define FD_STATUS_FD0_BUSY	0x01
#define FD_STATUS_FD1_BUSY	0x02
#define FD_STATUS_BUSY_IO	0x10
#define FD_STATUS_DMA_OFF	0x20
#define FD_STATUS_FD_READ	0x40
#define FD_STATUS_RDY_FOR_IO 0x80

#define FD_DSR		(FD_BASE + 4)	/* Data Rate Select Register (old) */
#define FD_DATA		(FD_BASE + 5)	/* Data Transfer (FIFO) register */
#define FD_DIR		(FD_BASE + 7)	/* Digital Input Register (read) */
#define FD_DCR		(FD_BASE + 7)	/* Diskette Control Register (write)*/

//
//  3F7 port bit map (r/w)
//
//  When writing:
//
//    0-1    Data rate selector:
//           00  -  500 KB/s
//           01  -  300 KB/s
//           10  -  250 KB/s
//           11  -  reserved
//    
//  When reading:
//
//     0     1 - FD0 selected
//     1     1 - FD0 selected
//     2-5   Heads selected (2 - head0, 3 - head1, ....)
//     6     Write selector
//     7     Disk change bit
//

#define FDP_WRITE_FDC_CONTROL_REG(X) KiOutPort (FD_DCR,X)

//
// FD control registers
//

// ST0
typedef struct FD_ST0
{
	UCHAR DS1  : 1;				// US1
	UCHAR DS0  : 1;				// US0
	UCHAR Head : 1;				// HD
	UCHAR NotReady  : 1;		// NC
	UCHAR HwError : 1;			// EC
	UCHAR SearchCompleted : 1;	// SE
	UCHAR InterruptCode : 2;	// IC
	//
	// 00 - normal exit
	// 01 - abort
	// 10 - invalid command
	// 11 - FD not ready
	//
} *PFD_ST0;

#define FD_NORMAL_EXIT	0
#define FD_ABORT		1
#define FD_INVALID_CMD	2
#define FD_NOT_READY	3

// ST1
typedef struct FD_ST1
{
	UCHAR AddressMarkerAbsent : 1;	// MA
	UCHAR WriteProtect : 1;			// NN
	UCHAR SectorNotFound : 1;		// ND
	UCHAR Reserved : 1;
	UCHAR Overflow : 1;				// OR
	UCHAR DataError : 1;			// DE
	UCHAR Reserved2 : 1;
	UCHAR SectorIncorrect : 1;		// EN
} *PFD_ST1;

// ST2
typedef struct FD_ST2
{
	UCHAR AddressMarkerAbsent : 1;	// MD
	UCHAR TrackNotReadable : 1;		// BC
	UCHAR ScanError : 1;			// SN
	UCHAR ScanCompleted : 1;		// SH
	UCHAR TrackAddressError : 1;	// WC
	UCHAR DataFieldError : 1;		// DD
	UCHAR DeletedMarkerAbsent : 1;	// CM
} *PFD_ST2;

// ST3
typedef struct FD_ST3
{
	UCHAR DS1 : 1;					// US1
	UCHAR DS0 : 1;					// US0
	UCHAR HeadSelected : 1;			// HD
	UCHAR DoubleSideWriting : 1;	// TS
	UCHAR Track0 : 1;				// T0
	UCHAR Ready : 1;				// RDY
	UCHAR WriteProtect : 1;			// WP
	UCHAR InternalFailure : 1;		// FT
} *PFD_ST3;


typedef struct FD_INT_STATE
{
	UCHAR FddIrqGot : 1;
	UCHAR FddMediaChanged : 1;
	UCHAR Reserved : 6;
} *PFD_INT_STATE;

extern FD_INT_STATE FdpIrqState;

typedef struct FD_MEDIA_TYPE {
	ULONG NumberOfSectors;
	UCHAR SectorsPerTrack;
	ULONG NumberOfHeads;
	ULONG NumberOfTracks;
	ULONG Stretch;
	UCHAR Gap1Size;
	UCHAR DataRate;
	UCHAR SteppingRate;
	UCHAR Gap2Size;
	char *FormatName;
} *PFD_MEDIA_TYPE;

#define MAX_REPLIES 7


#define FD_OPERATION_READ		0xE6
#define FD_OPERATION_READDEL	0x6C
#define FD_OPERATION_WRITE		0xC5
#define FD_OPERATION_WRITEDEL	0x49
#define FD_OPERATION_READTRACK	0x62
#define FD_OPERATION_SCANEQ		0x71
#define FD_OPERATION_SCANBEQ	0x79
#define FD_OPERATION_SCANAEQ	0x7D
#define FD_OPERATION_FMTTRACK	0x4D
#define FD_OPERATION_READINDEX	0x4B
#define FD_OPERATION_INITIALIZE	0x07
#define FD_OPERATION_READIRQSTE	0x08
#define FD_OPERATION_QUERYPARMS	0x03
#define FD_OPERATION_READFDCSTE	0x04
#define FD_OPERATION_SEARCH		0x0F

typedef union FD_OP2
{
	struct
	{
		UCHAR DS0 : 1;
		UCHAR DS1 : 1;
		UCHAR Head : 1;
		UCHAR Reserved : 5;
	};

	UCHAR RawValue;
} *PFD_OP2;

#define FD_OP2_INITIALIZE		0x02

#define FLOPPY_DMA 2