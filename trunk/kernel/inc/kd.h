// begin_ddk
#pragma once

KEVAR char *KdpErrorStatusValues[];

KESYSAPI
VOID
KEAPI
KiDebugPrintRaw(
	PCHAR String
	);

KESYSAPI
VOID
KECDECL
KiDebugPrint(
	PCHAR FormatString,
	...
	);

// end_ddk

#define BOCHS_LOGGER_PORT 0xE9

KEVAR  LOCK KdDebugLock;
KEVAR  BOOLEAN KdDebuggerEnabled;
KEVAR  BOOLEAN KdControlCPending, KdControlCPressed;

#define KD_COM_PORT_FAILED_INITIALIZATION 1

VOID
KEAPI
KdInitSystem(
	);

VOID
KEAPI
KdLockPort(
    PBOOLEAN OldIrqState
	);

VOID
KEAPI
KdUnlockPort(
	BOOLEAN OldIrqState
	);

KESYSAPI
BOOLEAN
KEAPI
KdPollBreakIn(
	);

#define DbgBreakPoint() { __asm int 3 }

enum PACKET_TYPE
{
	DbgKdManipulateStatePacket = 2,
	DbgKdDebugIOPacket = 3,
	DbgKdSynchronizePacket = 4,
	DbgKdDataCorruptPacket = 5,
	DbgKdPingPacket = 6,
	DbgKdBreakInPacket = 8,
	DbgKdTraceInfoPacket = 9,
	DbgKdRemoteFileControlPacket = 11
};

UCHAR
KEAPI
KdpReceiveString(
	PCBUFFER Buffer,
	ULONG *ActualReadBytes
	);

#define KdpReceiveStringR(CODE,BUFF,LEN)	\
	{										\
		CBUFFER _TempBuff;					\
		ULONG ActualRead;					\
		_TempBuff.Buffer = (PVOID)(BUFF);	\
		_TempBuff.Length = (USHORT)(LEN);	\
		*(CODE) = KdpReceiveString (&_TempBuff, &ActualRead);	\
	}

VOID
KEAPI
KdpSendString( 
	PCBUFFER Buffer
	);

#define KdpSendStringR(BUFF,LEN)		\
	{										\
		CBUFFER _TempBuff;					\
		_TempBuff.Buffer = (PVOID)(BUFF);	\
		_TempBuff.Length = (USHORT)(LEN);	\
		KdpSendString (&_TempBuff);			\
	}

VOID
KEAPI
KdpSendControlPacket (
    IN USHORT PacketType,
    IN ULONG PacketId OPTIONAL
    );

#define KDP_PACKET_RECEIVED		0
#define KDP_PACKET_TIMEOUT		1
#define KDP_PACKET_RESEND		2

UCHAR
KEAPI
KdpReceivePacketLeader (
    IN ULONG PacketType,
    OUT PULONG PacketLeader
    );

KESYSAPI
STATUS
KEAPI
KdReceivePacketWithType(
	IN USHORT* PacketType,
	OUT PCBUFFER MessageHeader,
	OUT PCBUFFER MessageData,
	OUT PULONG DataLength OPTIONAL
	);

KESYSAPI
STATUS
KEAPI
KdReceivePacket(
	IN USHORT PacketType,
	OUT PCBUFFER MessageHeader,
	OUT PCBUFFER MessageData,
	OUT PULONG DataLength OPTIONAL
	);

KESYSAPI
ULONG
KEAPI
KdCheckSumPacket(
	IN PBYTE Buffer,
	IN ULONG Length
	);

KESYSAPI
VOID
KEAPI
KdSendPacket(
	IN UCHAR PacketType,
	IN PCBUFFER MessageHeader,
	IN PCBUFFER MessageData
	);

//====================================================================
//                         MICROSOFT DBGKD
//====================================================================


//
// DbgKd APIs are for the portable kernel debugger
//

#define DBGKD_PROTOCOL_VERSION 6

//
// KD_PACKETS are the low level data format used in KD. All packets
// begin with a packet leader, byte count, packet type. The sequence
// for accepting a packet is:
//
//  - read 4 bytes to get packet leader.  If read times out (10 seconds)
//    with a short read, or if packet leader is incorrect, then retry
//    the read.
//
//  - next read 2 byte packet type.  If read times out (10 seconds) with
//    a short read, or if packet type is bad, then start again looking
//    for a packet leader.
//
//  - next read 4 byte packet Id.  If read times out (10 seconds)
//    with a short read, or if packet Id is not what we expect, then
//    ask for resend and restart again looking for a packet leader.
//
//  - next read 2 byte count.  If read times out (10 seconds) with
//    a short read, or if byte count is greater than PACKET_MAX_SIZE,
//    then start again looking for a packet leader.
//
//  - next read 4 byte packet data checksum.
//
//  - The packet data immediately follows the packet.  There should be
//    ByteCount bytes following the packet header.  Read the packet
//    data, if read times out (10 seconds) then start again looking for
//    a packet leader.
//


typedef struct _KD_PACKET {
    ULONG PacketLeader;
    USHORT PacketType;
    USHORT ByteCount;
    ULONG PacketId;
    ULONG Checksum;
} KD_PACKET, *PKD_PACKET;


#define PACKET_MAX_SIZE 4000
#define INITIAL_PACKET_ID 0x80800000    // Don't use 0
#define SYNC_PACKET_ID    0x00000800    // Or in with INITIAL_PACKET_ID
                                        // to force a packet ID reset.

//
// BreakIn packet
//

#define BREAKIN_PACKET                  0x62626262
#define BREAKIN_PACKET_BYTE             0x62

//
// Packet lead in sequence
//

#define PACKET_LEADER                   0x30303030 //0x77000077
#define PACKET_LEADER_BYTE              0x30

#define CONTROL_PACKET_LEADER           0x69696969
#define CONTROL_PACKET_LEADER_BYTE      0x69

//
// Packet Trailing Byte
//

#define PACKET_TRAILING_BYTE            0xAA

//
// Packet Types
//

#define PACKET_TYPE_UNUSED              0
#define PACKET_TYPE_KD_STATE_CHANGE32   1
#define PACKET_TYPE_KD_STATE_MANIPULATE 2
#define PACKET_TYPE_KD_DEBUG_IO         3
#define PACKET_TYPE_KD_ACKNOWLEDGE      4       // Packet-control type
#define PACKET_TYPE_KD_RESEND           5       // Packet-control type
#define PACKET_TYPE_KD_RESET            6       // Packet-control type
#define PACKET_TYPE_KD_STATE_CHANGE64   7
#define PACKET_TYPE_KD_POLL_BREAKIN     8
#define PACKET_TYPE_KD_TRACE_IO         9
#define PACKET_TYPE_KD_CONTROL_REQUEST  10
#define PACKET_TYPE_KD_FILE_IO          11
#define PACKET_TYPE_MAX                 12

//
// If the packet type is PACKET_TYPE_KD_STATE_CHANGE, then
// the format of the packet data is as follows:
//

#define DbgKdMinimumStateChange       0x00003030L

#define DbgKdExceptionStateChange     0x00003030L
#define DbgKdLoadSymbolsStateChange   0x00003031L
#define DbgKdCommandStringStateChange 0x00003032L

#define DbgKdMaximumStateChange       0x00003033L

// If the state change is from an alternate source
// then this bit is combined with the basic state change code.
#define DbgKdAlternateStateChange     0x00010000L

#define KD_REBOOT    (-1)
#define KD_HIBERNATE (-2)
//
// Pathname Data follows directly
//

typedef struct _DBGKD_LOAD_SYMBOLS32 {
    ULONG PathNameLength;
    ULONG BaseOfDll;
    ULONG ProcessId;
    ULONG CheckSum;
    ULONG SizeOfImage;
    BOOLEAN UnloadSymbols;
} DBGKD_LOAD_SYMBOLS32, *PDBGKD_LOAD_SYMBOLS32;

typedef struct _DBGKD_LOAD_SYMBOLS64 {
    ULONG PathNameLength;
    ULONG64 BaseOfDll;
    ULONG64 ProcessId;
    ULONG CheckSum;
    ULONG SizeOfImage;
    BOOLEAN UnloadSymbols;
} DBGKD_LOAD_SYMBOLS64, *PDBGKD_LOAD_SYMBOLS64;


typedef struct _DBGKD_COMMAND_STRING {
    ULONG Flags;
    ULONG Reserved1;
    ULONG64 Reserved2[7];
} DBGKD_COMMAND_STRING, *PDBGKD_COMMAND_STRING;

typedef struct _EXCEPTION_RECORD32 {
    ULONG ExceptionCode;
    ULONG ExceptionFlags;
    ULONG ExceptionRecord;
    ULONG ExceptionAddress;
    ULONG NumberParameters;
    ULONG ExceptionInformation[EXCEPTION_MAXIMUM_PARAMETERS];
} EXCEPTION_RECORD32, *PEXCEPTION_RECORD32;

typedef struct _EXCEPTION_RECORD64 {
    ULONG ExceptionCode;
    ULONG ExceptionFlags;
    ULONG64 ExceptionRecord;
    ULONG64 ExceptionAddress;
    ULONG NumberParameters;
    ULONG __unusedAlignment;
    ULONG64 ExceptionInformation[EXCEPTION_MAXIMUM_PARAMETERS];
} EXCEPTION_RECORD64, *PEXCEPTION_RECORD64;

typedef struct _DBGKM_EXCEPTION32 {
    EXCEPTION_RECORD32 ExceptionRecord;
    ULONG FirstChance;
} DBGKM_EXCEPTION32, *PDBGKM_EXCEPTION32;

typedef struct _DBGKM_EXCEPTION64 {
    EXCEPTION_RECORD64 ExceptionRecord;
    ULONG FirstChance;
} DBGKM_EXCEPTION64, *PDBGKM_EXCEPTION64;

typedef struct _DBGKD_WAIT_STATE_CHANGE32 {
    ULONG NewState;
    USHORT ProcessorLevel;
    USHORT Processor;
    ULONG NumberProcessors;
    ULONG Thread;
    ULONG ProgramCounter;
    union {
        DBGKM_EXCEPTION32 Exception;
        DBGKD_LOAD_SYMBOLS32 LoadSymbols;
    } u;
    // A processor-specific control report and context follows.
} DBGKD_WAIT_STATE_CHANGE32, *PDBGKD_WAIT_STATE_CHANGE32;

// Protocol version 5 64-bit state change.
typedef struct _DBGKD_WAIT_STATE_CHANGE64 {
    ULONG NewState;
    USHORT ProcessorLevel;
    USHORT Processor;
    ULONG NumberProcessors;
    ULONG64 Thread;
    ULONG64 ProgramCounter;
    union {
        DBGKM_EXCEPTION64 Exception;
        DBGKD_LOAD_SYMBOLS64 LoadSymbols;
    } u;
    // A processor-specific control report and context follows.
} DBGKD_WAIT_STATE_CHANGE64, *PDBGKD_WAIT_STATE_CHANGE64;

#define DBGKD_MAXSTREAM 16

typedef struct _DBGKD_CONTROL_REPORT {
    ULONG   Dr6;
    ULONG   Dr7;
    USHORT  InstructionCount;
    USHORT  ReportFlags;
    UCHAR   InstructionStream[DBGKD_MAXSTREAM];
    USHORT  SegCs;
    USHORT  SegDs;
    USHORT  SegEs;
    USHORT  SegFs;
    ULONG   EFlags;
} DBGKD_CONTROL_REPORT, *PDBGKD_CONTROL_REPORT;

// Protocol version 6 state change.
typedef struct _DBGKD_ANY_WAIT_STATE_CHANGE {
    ULONG NewState;
    USHORT ProcessorLevel;
    USHORT Processor;
    ULONG NumberProcessors;
    ULONG64 Thread;
    ULONG64 ProgramCounter;
    union {
        DBGKM_EXCEPTION64 Exception;
        DBGKD_LOAD_SYMBOLS64 LoadSymbols;
        DBGKD_COMMAND_STRING CommandString;
    } u;
    // The ANY control report is unioned here to
    // ensure that this structure is always large
    // enough to hold any possible state change.
    union {
        DBGKD_CONTROL_REPORT ControlReport;
//        DBGKD_ANY_CONTROL_REPORT AnyControlReport;
    };
	ULONG __padding[3];
} DBGKD_ANY_WAIT_STATE_CHANGE, *PDBGKD_ANY_WAIT_STATE_CHANGE;

//
// If the packet type is PACKET_TYPE_KD_STATE_MANIPULATE, then
// the format of the packet data is as follows:
//
// Api Numbers for state manipulation
//

#define DbgKdMinimumManipulate              0x00003130L

#define DbgKdReadVirtualMemoryApi           0x00003130L
#define DbgKdWriteVirtualMemoryApi          0x00003131L
#define DbgKdGetContextApi                  0x00003132L
#define DbgKdSetContextApi                  0x00003133L
#define DbgKdWriteBreakPointApi             0x00003134L
#define DbgKdRestoreBreakPointApi           0x00003135L
#define DbgKdContinueApi                    0x00003136L
#define DbgKdReadControlSpaceApi            0x00003137L
#define DbgKdWriteControlSpaceApi           0x00003138L
#define DbgKdReadIoSpaceApi                 0x00003139L
#define DbgKdWriteIoSpaceApi                0x0000313AL
#define DbgKdRebootApi                      0x0000313BL
#define DbgKdContinueApi2                   0x0000313CL
#define DbgKdReadPhysicalMemoryApi          0x0000313DL
#define DbgKdWritePhysicalMemoryApi         0x0000313EL
//#define DbgKdQuerySpecialCallsApi           0x0000313FL
#define DbgKdSetSpecialCallApi              0x00003140L
#define DbgKdClearSpecialCallsApi           0x00003141L
#define DbgKdSetInternalBreakPointApi       0x00003142L
#define DbgKdGetInternalBreakPointApi       0x00003143L
#define DbgKdReadIoSpaceExtendedApi         0x00003144L
#define DbgKdWriteIoSpaceExtendedApi        0x00003145L
#define DbgKdGetVersionApi                  0x00003146L
#define DbgKdWriteBreakPointExApi           0x00003147L
#define DbgKdRestoreBreakPointExApi         0x00003148L
#define DbgKdCauseBugCheckApi               0x00003149L
#define DbgKdSwitchProcessor                0x00003150L
#define DbgKdPageInApi                      0x00003151L // obsolete
#define DbgKdReadMachineSpecificRegister    0x00003152L
#define DbgKdWriteMachineSpecificRegister   0x00003153L
#define OldVlm1                             0x00003154L
#define OldVlm2                             0x00003155L
#define DbgKdSearchMemoryApi                0x00003156L
#define DbgKdGetBusDataApi                  0x00003157L
#define DbgKdSetBusDataApi                  0x00003158L
#define DbgKdCheckLowMemoryApi              0x00003159L
#define DbgKdClearAllInternalBreakpointsApi 0x0000315AL
#define DbgKdFillMemoryApi                  0x0000315BL
#define DbgKdQueryMemoryApi                 0x0000315CL
#define DbgKdSwitchPartition                0x0000315DL

#define DbgKdMaximumManipulate              0x0000315DL

//
// Physical memory caching flags.
// These flags can be passed in on physical memory
// access requests in the ActualBytes field.
//

#define DBGKD_CACHING_UNKNOWN        0
#define DBGKD_CACHING_CACHED         1
#define DBGKD_CACHING_UNCACHED       2
#define DBGKD_CACHING_WRITE_COMBINED 3

//
// Response is a read memory message with data following
//

typedef struct _DBGKD_READ_MEMORY32 {
    ULONG TargetBaseAddress;
    ULONG TransferCount;
    ULONG ActualBytesRead;
} DBGKD_READ_MEMORY32, *PDBGKD_READ_MEMORY32;

typedef struct _DBGKD_READ_MEMORY64 {
    ULONG64 TargetBaseAddress;
    ULONG TransferCount;
    ULONG ActualBytesRead;
} DBGKD_READ_MEMORY64, *PDBGKD_READ_MEMORY64;

//
// Data follows directly
//

typedef struct _DBGKD_WRITE_MEMORY32 {
    ULONG TargetBaseAddress;
    ULONG TransferCount;
    ULONG ActualBytesWritten;
} DBGKD_WRITE_MEMORY32, *PDBGKD_WRITE_MEMORY32;

typedef struct _DBGKD_WRITE_MEMORY64 {
    ULONG64 TargetBaseAddress;
    ULONG TransferCount;
    ULONG ActualBytesWritten;
} DBGKD_WRITE_MEMORY64, *PDBGKD_WRITE_MEMORY64;

//
// Response is a get context message with a full context record following
//

typedef struct _DBGKD_GET_CONTEXT {
    ULONG Unused;
} DBGKD_GET_CONTEXT, *PDBGKD_GET_CONTEXT;

//
// Full Context record follows
//

typedef struct _DBGKD_SET_CONTEXT {
    ULONG ContextFlags;
} DBGKD_SET_CONTEXT, *PDBGKD_SET_CONTEXT;

#define BREAKPOINT_TABLE_SIZE   32      // max number supported by kernel

typedef struct _DBGKD_WRITE_BREAKPOINT32 {
    ULONG BreakPointAddress;
    ULONG BreakPointHandle;
} DBGKD_WRITE_BREAKPOINT32, *PDBGKD_WRITE_BREAKPOINT32;

typedef struct _DBGKD_WRITE_BREAKPOINT64 {
    ULONG64 BreakPointAddress;
    ULONG BreakPointHandle;
} DBGKD_WRITE_BREAKPOINT64, *PDBGKD_WRITE_BREAKPOINT64;

typedef struct _DBGKD_RESTORE_BREAKPOINT {
    ULONG BreakPointHandle;
} DBGKD_RESTORE_BREAKPOINT, *PDBGKD_RESTORE_BREAKPOINT;

typedef struct _DBGKD_BREAKPOINTEX {
    ULONG     BreakPointCount;
    ULONG     ContinueStatus;
} DBGKD_BREAKPOINTEX, *PDBGKD_BREAKPOINTEX;

typedef struct _DBGKD_CONTINUE {
    ULONG     ContinueStatus;
} DBGKD_CONTINUE, *PDBGKD_CONTINUE;

#pragma pack(4)
typedef struct _DBGKD_CONTROL_SET {
    ULONG   TraceFlag;
    ULONG   Dr7;
    ULONG   CurrentSymbolStart;
    ULONG   CurrentSymbolEnd;
} DBGKD_CONTROL_SET, *PDBGKD_CONTROL_SET;

typedef struct _DBGKD_CONTINUE2 {
    ULONG   ContinueStatus;
    // The ANY control set is unioned here to
    // ensure that this structure is always large
    // enough to hold any possible continue.
    union {
        DBGKD_CONTROL_SET ControlSet;
//        DBGKD_ANY_CONTROL_SET AnyControlSet;
    };
} DBGKD_CONTINUE2, *PDBGKD_CONTINUE2;
#pragma pack()

typedef struct _DBGKD_READ_WRITE_IO32 {
    ULONG DataSize;                     // 1, 2, 4
    ULONG IoAddress;
    ULONG DataValue;
} DBGKD_READ_WRITE_IO32, *PDBGKD_READ_WRITE_IO32;

typedef struct _DBGKD_READ_WRITE_IO64 {
    ULONG64 IoAddress;
    ULONG DataSize;                     // 1, 2, 4
    ULONG DataValue;
} DBGKD_READ_WRITE_IO64, *PDBGKD_READ_WRITE_IO64;

typedef struct _DBGKD_READ_WRITE_IO_EXTENDED32 {
    ULONG DataSize;                     // 1, 2, 4
    ULONG InterfaceType;
    ULONG BusNumber;
    ULONG AddressSpace;
    ULONG IoAddress;
    ULONG DataValue;
} DBGKD_READ_WRITE_IO_EXTENDED32, *PDBGKD_READ_WRITE_IO_EXTENDED32;

typedef struct _DBGKD_READ_WRITE_IO_EXTENDED64 {
    ULONG DataSize;                     // 1, 2, 4
    ULONG InterfaceType;
    ULONG BusNumber;
    ULONG AddressSpace;
    ULONG64 IoAddress;
    ULONG DataValue;
} DBGKD_READ_WRITE_IO_EXTENDED64, *PDBGKD_READ_WRITE_IO_EXTENDED64;

typedef struct _DBGKD_READ_WRITE_MSR {
    ULONG Msr;
    ULONG DataValueLow;
    ULONG DataValueHigh;
} DBGKD_READ_WRITE_MSR, *PDBGKD_READ_WRITE_MSR;


typedef struct _DBGKD_QUERY_SPECIAL_CALLS {
    ULONG NumberOfSpecialCalls;
    // ULONG64 SpecialCalls[];
} DBGKD_QUERY_SPECIAL_CALLS, *PDBGKD_QUERY_SPECIAL_CALLS;

typedef struct _DBGKD_SET_SPECIAL_CALL32 {
    ULONG SpecialCall;
} DBGKD_SET_SPECIAL_CALL32, *PDBGKD_SET_SPECIAL_CALL32;

typedef struct _DBGKD_SET_SPECIAL_CALL64 {
    ULONG64 SpecialCall;
} DBGKD_SET_SPECIAL_CALL64, *PDBGKD_SET_SPECIAL_CALL64;

#define DBGKD_MAX_INTERNAL_BREAKPOINTS 20

typedef struct _DBGKD_SET_INTERNAL_BREAKPOINT32 {
    ULONG BreakpointAddress;
    ULONG Flags;
} DBGKD_SET_INTERNAL_BREAKPOINT32, *PDBGKD_SET_INTERNAL_BREAKPOINT32;

typedef struct _DBGKD_SET_INTERNAL_BREAKPOINT64 {
    ULONG64 BreakpointAddress;
    ULONG Flags;
} DBGKD_SET_INTERNAL_BREAKPOINT64, *PDBGKD_SET_INTERNAL_BREAKPOINT64;

typedef struct _DBGKD_GET_INTERNAL_BREAKPOINT32 {
    ULONG BreakpointAddress;
    ULONG Flags;
    ULONG Calls;
    ULONG MaxCallsPerPeriod;
    ULONG MinInstructions;
    ULONG MaxInstructions;
    ULONG TotalInstructions;
} DBGKD_GET_INTERNAL_BREAKPOINT32, *PDBGKD_GET_INTERNAL_BREAKPOINT32;

typedef struct _DBGKD_GET_INTERNAL_BREAKPOINT64 {
    ULONG64 BreakpointAddress;
    ULONG Flags;
    ULONG Calls;
    ULONG MaxCallsPerPeriod;
    ULONG MinInstructions;
    ULONG MaxInstructions;
    ULONG TotalInstructions;
} DBGKD_GET_INTERNAL_BREAKPOINT64, *PDBGKD_GET_INTERNAL_BREAKPOINT64;

#define DBGKD_INTERNAL_BP_FLAG_COUNTONLY 0x00000001 // don't count instructions
#define DBGKD_INTERNAL_BP_FLAG_INVALID   0x00000002 // disabled BP
#define DBGKD_INTERNAL_BP_FLAG_SUSPENDED 0x00000004 // temporarily suspended
#define DBGKD_INTERNAL_BP_FLAG_DYING     0x00000008 // kill on exit


#define DBGKD_64BIT_PROTOCOL_VERSION1 5
#define DBGKD_64BIT_PROTOCOL_VERSION2 6


typedef struct _DBGKD_SEARCH_MEMORY {
    union {
        ULONG64 SearchAddress;
        ULONG64 FoundAddress;
    };
    ULONG64 SearchLength;
    ULONG PatternLength;
} DBGKD_SEARCH_MEMORY, *PDBGKD_SEARCH_MEMORY;


typedef struct _DBGKD_GET_SET_BUS_DATA {
    ULONG BusDataType;
    ULONG BusNumber;
    ULONG SlotNumber;
    ULONG Offset;
    ULONG Length;
} DBGKD_GET_SET_BUS_DATA, *PDBGKD_GET_SET_BUS_DATA;


#define DBGKD_FILL_MEMORY_VIRTUAL  0x00000001
#define DBGKD_FILL_MEMORY_PHYSICAL 0x00000002

typedef struct _DBGKD_FILL_MEMORY {
    ULONG64 Address;
    ULONG Length;
    USHORT Flags;
    USHORT PatternLength;
} DBGKD_FILL_MEMORY, *PDBGKD_FILL_MEMORY;

// Input AddressSpace values.
#define DBGKD_QUERY_MEMORY_VIRTUAL 0x00000000

// Output AddressSpace values.
#define DBGKD_QUERY_MEMORY_PROCESS 0x00000000
#define DBGKD_QUERY_MEMORY_SESSION 0x00000001
#define DBGKD_QUERY_MEMORY_KERNEL  0x00000002

// Output Flags.
// Currently the kernel always returns rwx.
#define DBGKD_QUERY_MEMORY_READ    0x00000001
#define DBGKD_QUERY_MEMORY_WRITE   0x00000002
#define DBGKD_QUERY_MEMORY_EXECUTE 0x00000004
#define DBGKD_QUERY_MEMORY_FIXED   0x00000008

typedef struct _DBGKD_QUERY_MEMORY {
    ULONG64 Address;
    ULONG64 Reserved;
    ULONG AddressSpace;
    ULONG Flags;
} DBGKD_QUERY_MEMORY, *PDBGKD_QUERY_MEMORY;


#define DBGKD_PARTITION_DEFAULT   0x00000000
#define DBGKD_PARTITION_ALTERNATE 0x00000001

typedef struct _DBGKD_SWITCH_PARTITION {
    ULONG Partition;
} DBGKD_SWITCH_PARTITION;

#pragma pack(1)
typedef struct _DBGKD_GET_VERSION64 {
	USHORT	MajorVersion;
	USHORT	MinorVersion;
	USHORT	ProtocolVersion;
	USHORT	Flags;
	USHORT	MachineType;
	UCHAR	MaxPacketType;
	UCHAR	MaxStateChange;
	UCHAR	MaxManipulate;
	UCHAR	Simulation;
	USHORT	Unused;
	ULONG64	KernelBase;
	ULONG64	PsLoadedModuleList;
	ULONG64	DebuggerDataList;
} DBGKD_GET_VERSION64, *PDBGKD_GET_VERSION64;
#pragma pack()

#pragma pack(8)
typedef struct _DBGKD_MANIPULATE_STATE32 {
    ULONG ApiNumber;
    USHORT ProcessorLevel;
    USHORT Processor;
    ULONG ReturnStatus;
    union {
        DBGKD_READ_MEMORY32 ReadMemory;
        DBGKD_WRITE_MEMORY32 WriteMemory;
        DBGKD_READ_MEMORY64 ReadMemory64;
        DBGKD_WRITE_MEMORY64 WriteMemory64;
        DBGKD_GET_CONTEXT GetContext;
        DBGKD_SET_CONTEXT SetContext;
        DBGKD_WRITE_BREAKPOINT32 WriteBreakPoint;
        DBGKD_RESTORE_BREAKPOINT RestoreBreakPoint;
        DBGKD_CONTINUE Continue;
        DBGKD_CONTINUE2 Continue2;
        DBGKD_READ_WRITE_IO32 ReadWriteIo;
        DBGKD_READ_WRITE_IO_EXTENDED32 ReadWriteIoExtended;
        DBGKD_QUERY_SPECIAL_CALLS QuerySpecialCalls;
        DBGKD_SET_SPECIAL_CALL32 SetSpecialCall;
        DBGKD_SET_INTERNAL_BREAKPOINT32 SetInternalBreakpoint;
        DBGKD_GET_INTERNAL_BREAKPOINT32 GetInternalBreakpoint;
//        DBGKD_GET_VERSION32 GetVersion32;
		DBGKD_GET_VERSION64 GetVersion64;
        DBGKD_BREAKPOINTEX BreakPointEx;
        DBGKD_READ_WRITE_MSR ReadWriteMsr;
        DBGKD_SEARCH_MEMORY SearchMemory;
		DBGKD_QUERY_MEMORY QueryMemory;
    } u;
} DBGKD_MANIPULATE_STATE32, *PDBGKD_MANIPULATE_STATE32;

#pragma pack()


typedef struct _DBGKD_MANIPULATE_STATE64 {
    ULONG ApiNumber;
    USHORT ProcessorLevel;
    USHORT Processor;
    ULONG ReturnStatus;
    union {
        DBGKD_READ_MEMORY64 ReadMemory;
        DBGKD_WRITE_MEMORY64 WriteMemory;
        DBGKD_GET_CONTEXT GetContext;
        DBGKD_SET_CONTEXT SetContext;
        DBGKD_WRITE_BREAKPOINT64 WriteBreakPoint;
        DBGKD_RESTORE_BREAKPOINT RestoreBreakPoint;
        DBGKD_CONTINUE Continue;
        DBGKD_CONTINUE2 Continue2;
        DBGKD_READ_WRITE_IO64 ReadWriteIo;
        DBGKD_READ_WRITE_IO_EXTENDED64 ReadWriteIoExtended;
        DBGKD_QUERY_SPECIAL_CALLS QuerySpecialCalls;
        DBGKD_SET_SPECIAL_CALL64 SetSpecialCall;
        DBGKD_SET_INTERNAL_BREAKPOINT64 SetInternalBreakpoint;
        DBGKD_GET_INTERNAL_BREAKPOINT64 GetInternalBreakpoint;
//        DBGKD_GET_VERSION64 GetVersion64;
        DBGKD_BREAKPOINTEX BreakPointEx;
        DBGKD_READ_WRITE_MSR ReadWriteMsr;
        DBGKD_SEARCH_MEMORY SearchMemory;
        DBGKD_GET_SET_BUS_DATA GetSetBusData;
        DBGKD_FILL_MEMORY FillMemory;
        DBGKD_QUERY_MEMORY QueryMemory;
        DBGKD_SWITCH_PARTITION SwitchPartition;
    } u;
} DBGKD_MANIPULATE_STATE64, *PDBGKD_MANIPULATE_STATE64;

typedef union _DBGKD_TRACE_DATA {
    struct {
        UCHAR SymbolNumber;
        CHAR LevelChange;
        USHORT Instructions;
    } s;
    ULONG LongNumber;
} DBGKD_TRACE_DATA, *PDBGKD_TRACE_DATA;

#define TRACE_DATA_INSTRUCTIONS_BIG 0xffff

#define TRACE_DATA_BUFFER_MAX_SIZE 40

//
// If the packet type is PACKET_TYPE_KD_DEBUG_IO, then
// the format of the packet data is as follows:
//

#define DbgKdPrintStringApi     0x00003230L
#define DbgKdGetStringApi       0x00003231L

//
// For print string, the Null terminated string to print
// immediately follows the message
//
typedef struct _DBGKD_PRINT_STRING {
    ULONG LengthOfString;
} DBGKD_PRINT_STRING, *PDBGKD_PRINT_STRING;

//
// For get string, the Null terminated prompt string
// immediately follows the message. The LengthOfStringRead
// field initially contains the maximum number of characters
// to read. Upon reply, this contains the number of bytes actually
// read. The data read immediately follows the message.
//
//
typedef struct _DBGKD_GET_STRING {
    ULONG LengthOfPromptString;
    ULONG LengthOfStringRead;
} DBGKD_GET_STRING, *PDBGKD_GET_STRING;

typedef struct _DBGKD_DEBUG_IO {
    ULONG ApiNumber;
    USHORT ProcessorLevel;
    USHORT Processor;
    union {
        DBGKD_PRINT_STRING PrintString;
        DBGKD_GET_STRING GetString;
    } u;
} DBGKD_DEBUG_IO, *PDBGKD_DEBUG_IO;


//
// If the packet type is PACKET_TYPE_KD_TRACE_IO, then
// the format of the packet data is as follows:
//

#define DbgKdPrintTraceApi      0x00003330L

//
// For print trace, the trace buffer data
// immediately follows the message
//
typedef struct _DBGKD_PRINT_TRACE {
    ULONG LengthOfData;
} DBGKD_PRINT_TRACE, *PDBGKD_PRINT_TRACE;

typedef struct _DBGKD_TRACE_IO {
    ULONG ApiNumber;
    USHORT ProcessorLevel;
    USHORT Processor;
    union {
        ULONG64 ReserveSpace[7];
        DBGKD_PRINT_TRACE PrintTrace;
    } u;
} DBGKD_TRACE_IO, *PDBGKD_TRACE_IO;


//
// If the packet type is PACKET_TYPE_KD_CONTROL_REQUEST, then
// the format of the packet data is as follows:
//

#define DbgKdRequestHardwareBp  0x00004300L
#define DbgKdReleaseHardwareBp  0x00004301L

typedef struct _DBGKD_REQUEST_BREAKPOINT {
    ULONG HardwareBreakPointNumber;
    ULONG Available;
} DBGKD_REQUEST_BREAKPOINT, *PDBGKD_REQUEST_BREAKPOINT;

typedef struct _DBGKD_RELEASE_BREAKPOINT {
    ULONG HardwareBreakPointNumber;
    ULONG Released;
} DBGKD_RELEASE_BREAKPOINT, *PDBGKD_RELEASE_BREAKPOINT;


typedef struct _DBGKD_CONTROL_REQUEST {
    ULONG ApiNumber;
    union {
        DBGKD_REQUEST_BREAKPOINT RequestBreakpoint;
        DBGKD_RELEASE_BREAKPOINT ReleaseBreakpoint;
    } u;
} DBGKD_CONTROL_REQUEST, *PDBGKD_CONTROL_REQUEST;


//
// If the packet type is PACKET_TYPE_KD_FILE_IO, then
// the format of the packet data is as follows:
//

#define DbgKdCreateFileApi      0x00003430L
#define DbgKdReadFileApi        0x00003431L
#define DbgKdWriteFileApi       0x00003432L
#define DbgKdCloseFileApi       0x00003433L

// Unicode filename follows as additional data.
typedef struct _DBGKD_CREATE_FILE {
    ULONG DesiredAccess;
    ULONG FileAttributes;
    ULONG ShareAccess;
    ULONG CreateDisposition;
    ULONG CreateOptions;
    // Return values.
    ULONG64 Handle;
    ULONG64 Length;
} DBGKD_CREATE_FILE, *PDBGKD_CREATE_FILE;

// Data is returned as additional data in the response.
typedef struct _DBGKD_READ_FILE {
    ULONG64 Handle;
    ULONG64 Offset;
    ULONG Length;
} DBGKD_READ_FILE, *PDBGKD_READ_FILE;

// Data is given as additional data.
typedef struct _DBGKD_WRITE_FILE {
    ULONG64 Handle;
    ULONG64 Offset;
    ULONG Length;
} DBGKD_WRITE_FILE, *PDBGKD_WRITE_FILE;

typedef struct _DBGKD_CLOSE_FILE {
    ULONG64 Handle;
} DBGKD_CLOSE_FILE, *PDBGKD_CLOSE_FILE;

typedef struct _DBGKD_FILE_IO {
    ULONG ApiNumber;
    ULONG Status;
    union {
        ULONG64 ReserveSpace[7];
        DBGKD_CREATE_FILE CreateFile;
        DBGKD_READ_FILE ReadFile;
        DBGKD_WRITE_FILE WriteFile;
        DBGKD_CLOSE_FILE CloseFile;
    } u;
} DBGKD_FILE_IO, *PDBGKD_FILE_IO;


#define CP_GET_SUCCESS	0
#define CP_GET_NODATA	1
#define CP_GET_ERROR	2


#pragma  pack(1)

struct DESCRIPTOR {
  USHORT Pad;
  USHORT Limit;
  ULONG Base;
};

struct KSPECIAL_REGISTERS {
  ULONG Cr0;
  ULONG Cr2;
  ULONG Cr3;
  ULONG Cr4;
  ULONG KernelDr0;
  ULONG KernelDr1;
  ULONG KernelDr2;
  ULONG KernelDr3;
  ULONG KernelDr6;
  ULONG KernelDr7;
  DESCRIPTOR Gdtr;
  DESCRIPTOR Idtr;
  USHORT Tr;
  USHORT Ldtr;
  ULONG Reserved[6];
};

struct KPROCESSOR_STATE {
  //NTCONTEXT ContextFrame;
  UCHAR ContextFrame[0x2CC];

  KSPECIAL_REGISTERS SpecialRegisters;
};

#pragma pack()


//====================================================================
//                              GR8OS
//====================================================================

KEVAR  ULONG KdpNextPacketIdToSend;
KEVAR  ULONG KdpPacketIdExpected;
KEVAR  UCHAR KdpComPortPreferred;
KEVAR  BOOLEAN KdpKernelDebuggerActive;

#define NTSTATUS_WAIT_0                    ((DWORD   )0x00000000L)    
#define NTSTATUS_ABANDONED_WAIT_0          ((DWORD   )0x00000080L)    
#define NTSTATUS_USER_APC                  ((DWORD   )0x000000C0L)    
#define NTSTATUS_TIMEOUT                   ((DWORD   )0x00000102L)    
#define NTSTATUS_PENDING                   ((DWORD   )0x00000103L)    
#define NTSTATUS_SEGMENT_NOTIFICATION      ((DWORD   )0x40000005L)    
#define NTSTATUS_GUARD_PAGE_VIOLATION      ((DWORD   )0x80000001L)    
#define NTSTATUS_DATATYPE_MISALIGNMENT     ((DWORD   )0x80000002L)    
#define NTSTATUS_BREAKPOINT                ((DWORD   )0x80000003L)    
#define NTSTATUS_SINGLE_STEP               ((DWORD   )0x80000004L) 
#define NTSTATUS_NOT_IMPLEMENTED           ((ULONG   )0xC0000002L)
#define NTSTATUS_ACCESS_VIOLATION          ((DWORD   )0xC0000005L)    
#define NTSTATUS_IN_PAGE_ERROR             ((DWORD   )0xC0000006L)    
#define NTSTATUS_INVALID_HANDLE            ((DWORD   )0xC0000008L)    
#define NTSTATUS_NO_MEMORY                 ((DWORD   )0xC0000017L)    
#define NTSTATUS_ILLEGAL_INSTRUCTION       ((DWORD   )0xC000001DL)    
#define NTSTATUS_NONCONTINUABLE_EXCEPTION  ((DWORD   )0xC0000025L)    
#define NTSTATUS_INVALID_DISPOSITION       ((DWORD   )0xC0000026L)    
#define NTSTATUS_ARRAY_BOUNDS_EXCEEDED     ((DWORD   )0xC000008CL)    
#define NTSTATUS_FLOAT_DENORMAL_OPERAND    ((DWORD   )0xC000008DL)    
#define NTSTATUS_FLOAT_DIVIDE_BY_ZERO      ((DWORD   )0xC000008EL)    
#define NTSTATUS_FLOAT_INEXACT_RESULT      ((DWORD   )0xC000008FL)    
#define NTSTATUS_FLOAT_INVALID_OPERATION   ((DWORD   )0xC0000090L)    
#define NTSTATUS_FLOAT_OVERFLOW            ((DWORD   )0xC0000091L)    
#define NTSTATUS_FLOAT_STACK_CHECK         ((DWORD   )0xC0000092L)    
#define NTSTATUS_FLOAT_UNDERFLOW           ((DWORD   )0xC0000093L)    
#define NTSTATUS_INTEGER_DIVIDE_BY_ZERO    ((DWORD   )0xC0000094L)    
#define NTSTATUS_INTEGER_OVERFLOW          ((DWORD   )0xC0000095L)    
#define NTSTATUS_PRIVILEGED_INSTRUCTION    ((DWORD   )0xC0000096L)    
#define NTSTATUS_STACK_OVERFLOW            ((DWORD   )0xC00000FDL)    
#define NTSTATUS_CONTROL_C_EXIT            ((DWORD   )0xC000013AL)    
#define NTSTATUS_FLOAT_MULTIPLE_FAULTS     ((DWORD   )0xC00002B4L)    
#define NTSTATUS_FLOAT_MULTIPLE_TRAPS      ((DWORD   )0xC00002B5L)    
#define NTSTATUS_ILLEGAL_VLM_REFERENCE     ((DWORD   )0xC00002C0L)     

#define IMAGE_FILE_MACHINE_I386              0x014c  // Intel 386.

BOOLEAN
KEAPI
KdpReceiveAndReplyPackets(
	);

VOID
KEAPI
KdpSendStateChange(
	ULONG ExceptionCode,
	ULONG NumberParameters,
	ULONG Parameter1,
	ULONG Parameter2,
	ULONG Parameter3,
	ULONG Parameter4,
	ULONG ExceptionAddress,
	BOOLEAN FirstChance
	);

KESYSAPI
VOID
KEAPI
KdWakeUpDebugger(
	ULONG ExceptionCode,
	ULONG NumberParameters,
	ULONG Parameter1,
	ULONG Parameter2,
	ULONG Parameter3,
	ULONG Parameter4,
	ULONG ExceptionAddress,
	BOOLEAN FirstChance
	);

/*
	TO DO:
		KdWakeUpDebugger should set KdpKernelDebuggerActive, disable all interrupts and listen for the packets.
		If user requested to continue the execution KdWakeUpDebugger should unmask interrtupts and return.
*/

#define KD_WAKE_UP_DEBUGGER_BP() KdWakeUpDebugger (NTSTATUS_BREAKPOINT, 0, 0, 0, 0, 0, 0, TRUE);

#if TRACE_PACKETS
void DumpMemory( ULONG base, ULONG length, ULONG DisplayBase );
#endif

//#pragma pack()