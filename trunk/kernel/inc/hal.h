#pragma once

BOOLEAN
KEAPI
HalpCheckComPortConnected(
	IN UCHAR ComPortNumber
	);

BOOLEAN
KEAPI
HalpInitializeComPort(
	IN UCHAR Port,
	IN ULONG Frequency
	);

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

VOID
KEAPI
HalInitSystem(
	);

KESYSAPI
VOID
KEAPI
HalReadConfigTimer(
	UCHAR Timer,
	ULONG *Freq
	);
