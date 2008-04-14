//
// <hal.h> built by header file parser at 08:46:13  14 Apr 2008
// This is a part of gr8os include files for GR8OS Driver & Extender Development Kit (DEDK)
//

#pragma once

KESYSAPI
STATUS
KEAPI
HalWriteComPort(
	IN UCHAR ComPortNumber,
	IN PVOID Data,
	IN ULONG DataSize
	);

KESYSAPI
STATUS
KEAPI
HalReadComPort(
	IN UCHAR ComPortNumber,
	OUT PVOID Data,
	IN OUT PULONG DataSize
	);

KESYSAPI
UCHAR
KEAPI
KdPortGetByte(
	UCHAR* Byte
	);

KESYSAPI
VOID
KEAPI
KdPortPutByte(
	UCHAR Byte
	);

#define PORT_COM1_BASE 0x3F8
#define PORT_COM2_BASE 0x2F8

// Reader states
#define COMPORT_DATAREADY			0x01
#define COMPORT_OVERFLOW_READERR	0x02
#define COMPORT_PARITY_READERR		0x04
#define COMPORT_SYNCH_READERR		0x08
#define COMPORT_READER_ERROR  \
			(COMPORT_OVERFLOW_READERR | COMPORT_PARITY_READERR | COMPORT_SYNCH_READERR)
#define COMPORT_READER  (COMPORT_DATAREADY | COMPORT_READER_ERROR)

// Writer states
#define COMPORT_READY_TO_WRITE		0x20
#define COMPORT_WRITE_COMPLETED		0x40
#define COMPORT_WRITER (COMPORT_READY_TO_WRITE | COMPORT_WRITE_COMPLETED)


#define READ_COM(N,OFS) KiInPort( ComPorts[N] + (OFS) ) 
#define WRITE_COM(N,OFS,V) KiOutPort( ComPorts[N] + (OFS), V )

#define GET_COMPORT_STATE(N) READ_COM( N, 5 )

#define COMPORT_BASEFREQ 1843200


KESYSAPI
ULONG
KEAPI
HalQueryTimerTickMult(
	);

#pragma pack(1)

typedef union APIC_LVT_ENTRY
{
	ULONG RawValue;
	struct
	{
		ULONG Vector : 8;
		ULONG Reserved1 : 4;
		ULONG DeliveryStatus : 1;
		ULONG Reserved2 : 3;
		ULONG Masked : 1;
		ULONG TimerMode : 1;
		ULONG Reserved3 : 14;
	};
} *PAPIC_LVT_ENTRY;

typedef struct APIC_TIMER_CONFIG
{
	ULONG Flags;
	ULONG InitialCounter;
	ULONG CurrentCounter;
	ULONG Divisor;
	APIC_LVT_ENTRY LvtTimer;
} *PAPIC_TIMER_CONFIG;

enum TIMER_MODE
{
	OneShot,
	Periodic
};

#pragma pack()

#define TIMER_MODIFY_INITIAL_COUNTER	0x01
#define TIMER_MODIFY_DIVISOR			0x02
#define TIMER_MODIFY_LVT_ENTRY			0x04

KESYSAPI
VOID
KEAPI
HalQueryApicTimerConf(
	PAPIC_TIMER_CONFIG Config
	);

KESYSAPI
VOID
KEAPI
HalSetApicTimerConf(
	PAPIC_TIMER_CONFIG Config
	);

KESYSAPI
ULONG
KEAPI
HalpReadApicConfig(
	ULONG Offset
	);

KESYSAPI
ULONG
KEAPI
HalpWriteApicConfig(
	ULONG Offset,
	ULONG Value
	);

KESYSAPI
ULONG
KEAPI
HalQueryBusClockFreq(
	);

extern ULONG HalBusClockFrequency;

#define APIC_TPR	0x0080
#define APIC_LVTTMR  0x0320
#define APIC_INITCNT 0x0380
#define APIC_CURRCNT 0x0390
#define APIC_DIVCONF 0x03E0

#pragma pack(1)

typedef union SYSTEM_PORT
{
	UCHAR RawValue;

	struct
	{
		// R/W
		UCHAR Gate2 : 1;
		UCHAR Speaker : 1;
		UCHAR RamControlErr : 1;
		UCHAR IsaControl : 1;

		// R
		UCHAR RamReg : 1;
		UCHAR Timer2Out : 1;
		UCHAR IsaControlFault : 1;
		UCHAR RamParityError : 1;
	};
} *PSYSTEM_PORT;

#pragma pack()

#define SYSTEM_PORT_NUMBER	0x61

KESYSAPI
SYSTEM_PORT
KEAPI
HalReadSystemPort(
	);

#define TIMER_GATE0			0x40
#define TIMER_GATE1			0x41
#define TIMER_GATE2			0x42
#define TIMER_CONTROL_PORT	0x43

#pragma pack(1)

enum TIMER_COUNTER_MODE
{
	CounterIrq = 0,
	WaitingMultivibrator = 1,
	ShortSignals = 2,
	MeandrGenerator = 3
	// 4
	// 5
};

enum TIMER_REQUEST_MODE
{
	LockCurrentValue = 0,
	LSB = 1,
	MSB = 2,
	LSBMSB = 3
};

typedef union TIMER_CONTROL
{
	UCHAR RawValue;

	struct
	{
		UCHAR  CountMode : 1; // 0=Binary, 1=BCD
		UCHAR  CounterMode : 3; // see TIMER_COUNTER_MODE

		UCHAR  RequestMode : 2; // see TIMER_REQUEST_MODE
		UCHAR  CounterSelector : 2; // 0, 1, 2
	};
} *PTIMER_CONTROL;

#pragma pack()

KESYSAPI
VOID
KEAPI
HalConfigureTimer(
	UCHAR Timer,
	ULONG Freq
	);


KESYSAPI
USHORT
KEAPI
HalQueryTimerCounter(
	UCHAR Timer
	);

#define TIMER_FREQ  1193180


KESYSAPI
VOID
KEAPI
HalReadConfigTimer(
	UCHAR Timer,
	ULONG *Freq
	);


#define MAX_DMA_CHANNELS	8

/* 8237 DMA controllers */
#define IO_DMA1_BASE	0x00	/* 8 bit slave DMA, channels 0..3 */
#define IO_DMA2_BASE	0xC0	/* 16 bit master DMA, ch 4(=slave input)..7 */

/* DMA controller registers */
#define DMA1_CMD_REG		0x08	/* command register (w) */
#define DMA1_STAT_REG		0x08	/* status register (r) */
#define DMA1_REQ_REG            0x09    /* request register (w) */
#define DMA1_MASK_REG		0x0A	/* single-channel mask (w) */
#define DMA1_MODE_REG		0x0B	/* mode register (w) */
#define DMA1_CLEAR_FF_REG	0x0C	/* clear pointer flip-flop (w) */
#define DMA1_TEMP_REG           0x0D    /* Temporary Register (r) */
#define DMA1_RESET_REG		0x0D	/* Master Clear (w) */
#define DMA1_CLR_MASK_REG       0x0E    /* Clear Mask */
#define DMA1_MASK_ALL_REG       0x0F    /* all-channels mask (w) */

#define DMA2_CMD_REG		0xD0	/* command register (w) */
#define DMA2_STAT_REG		0xD0	/* status register (r) */
#define DMA2_REQ_REG            0xD2    /* request register (w) */
#define DMA2_MASK_REG		0xD4	/* single-channel mask (w) */
#define DMA2_MODE_REG		0xD6	/* mode register (w) */
#define DMA2_CLEAR_FF_REG	0xD8	/* clear pointer flip-flop (w) */
#define DMA2_TEMP_REG           0xDA    /* Temporary Register (r) */
#define DMA2_RESET_REG		0xDA	/* Master Clear (w) */
#define DMA2_CLR_MASK_REG       0xDC    /* Clear Mask */
#define DMA2_MASK_ALL_REG       0xDE    /* all-channels mask (w) */

#define DMA_ADDR_0              0x00    /* DMA address registers */
#define DMA_ADDR_1              0x02
#define DMA_ADDR_2              0x04
#define DMA_ADDR_3              0x06
#define DMA_ADDR_4              0xC0
#define DMA_ADDR_5              0xC4
#define DMA_ADDR_6              0xC8
#define DMA_ADDR_7              0xCC

#define DMA_CNT_0               0x01    /* DMA count registers */
#define DMA_CNT_1               0x03
#define DMA_CNT_2               0x05
#define DMA_CNT_3               0x07
#define DMA_CNT_4               0xC2
#define DMA_CNT_5               0xC6
#define DMA_CNT_6               0xCA
#define DMA_CNT_7               0xCE

#define DMA_PAGE_0              0x87    /* DMA page registers */
#define DMA_PAGE_1              0x83
#define DMA_PAGE_2              0x81
#define DMA_PAGE_3              0x82
#define DMA_PAGE_5              0x8B
#define DMA_PAGE_6              0x89
#define DMA_PAGE_7              0x8A

#define DMA_MODE_READ	0x44	/* I/O to memory, no autoinit, increment, single mode */
#define DMA_MODE_WRITE	0x48	/* memory to I/O, no autoinit, increment, single mode */
#define DMA_MODE_CASCADE 0xC0   /* pass thru DREQ->HRQ, DACK<-HLDA only */



KESYSAPI
STATUS
KEAPI
HalRequestDma(
	UCHAR Channel
	);

KESYSAPI
STATUS
KEAPI
HalFreeDma(
	UCHAR Channel
	);

#define _1Meg 1048576
#define _1MegPages 256


typedef struct DMA_REQUEST
{
	BOOLEAN ReadOperation;
	UCHAR Channel;
	PVOID Buffer;
	ULONG BufferSize;
	UCHAR PageUsed;
	UCHAR PageCount;
	PVOID MappedPhysical;
} *PDMA_REQUEST;

KESYSAPI
UCHAR
KEAPI
HalAllocatePhysicalLowMegPages(
	UCHAR PageCount
	);

KESYSAPI
VOID
KEAPI
HalFreePhysicalLowMegPages(
	UCHAR StartPage,
	UCHAR PageCount
	);


#define _enable() { __asm sti }
#define _disable() { __asm cli }


KESYSAPI
VOID
KEAPI
HalEnableDma(
	UCHAR Channel
	);

KESYSAPI
VOID
KEAPI
HalDisableDma(
	UCHAR Channel
	);

KESYSAPI
VOID
KEAPI
HalClearDmaFf(
	UCHAR Channel
	);

KESYSAPI
VOID
KEAPI
HalSetDmaMode(
	UCHAR Channel,
	UCHAR Mode
	);

KESYSAPI
STATUS
KEAPI
HalInitializeDmaRequest(
	UCHAR DmaCommand,
	UCHAR Channel,
	PVOID Buffer,
	ULONG Size,
	PDMA_REQUEST *pDmaRequest
	);

KESYSAPI
VOID
KEAPI
HalCompleteDmaRequest(
	PDMA_REQUEST DmaReq
	);

KESYSAPI
UCHAR
KEAPI
HalCmosRead(
	UCHAR Offset
	);

#define CMOS_SELECTOR	0x70
#define CMOS_DATA		0x71

#define KEYB_CONTROLLER	0x64
#define KEYB_REBOOT		0xFE

KENORETURN
KESYSAPI
VOID
KEAPI
HalRebootMachine(
	);

