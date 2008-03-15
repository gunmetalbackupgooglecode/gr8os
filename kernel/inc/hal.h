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

