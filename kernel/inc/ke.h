#pragma once


#pragma pack(1)

typedef struct LOCK
{
	UCHAR  Count;
	UCHAR  OldIrqState;
} *PLOCK;

struct THREAD;

#define EXCEPTION_MAXIMUM_PARAMETERS 4
typedef struct EXCEPTION_ARGUMENTS
{
	ULONG  ExceptionCode;
	ULONG  NumberParameters;
	ULONG  Parameters[EXCEPTION_MAXIMUM_PARAMETERS];
} *PEXCEPTION_ARGUMENTS;

enum EXCEPTION_RESOLUTION
{
	ExceptionContinueExecution = 0,
	//ExceptionExecuteHandler,
	ExceptionContinueSearch = 2
};

#define EXCEPTION_CONTINUE_SEARCH 2
#define EXCEPTION_CONTINUE_EXECUTION 0

typedef struct CONTEXT_FRAME *PCONTEXT_FRAME;

typedef UCHAR (KEAPI *EXCEPTION_HANDLER)(PEXCEPTION_ARGUMENTS, PCONTEXT_FRAME);

typedef struct EXCEPTION_RECORD
{
	EXCEPTION_RECORD *Next;
	EXCEPTION_HANDLER Handler;
} *PEXCEPTION_RECORD;

#define EXCEPTION_ACCESS_VIOLATION	0xC0000005
#define EXCEPTION_BREAKPOINT		0x80000003
#define EXCEPTION_SINGLE_STEP		0x80000004
#define EXCEPTION_DIVISION_BY_ZERO	0xC0000094
#define EXCEPTION_INVALID_RESULUTION 0xC00000FF

KESYSAPI
VOID
KEAPI
KeDispatchException(
	ULONG ExceptionCode,
	ULONG NumberParameters,
	ULONG Parameter1,
	ULONG Parameter2,
	ULONG Parameter3,
	ULONG Parameter4,
	CONTEXT_FRAME *ContextFrame
	);

typedef struct PCB
{
	PEXCEPTION_RECORD ExceptionList;
	PEXCEPTION_ARGUMENTS CurrentException;
	THREAD* CurrentThread;
	USHORT  QuantumLeft;
	PCB *Self;
} *PPCB;

#define PcExceptionList		0
#define PcCurrentException	4
#define PcCurrentThread		8
#define PcQuantumLeft		12

typedef struct LIST_ENTRY
{
	LIST_ENTRY* Flink;
	LIST_ENTRY* Blink;
} *PLIST_ENTRY;

#define InitializeListHead(ListHead) (ListHead)->Flink = (ListHead)->Blink = (ListHead);
#define IsListEmpty(ListHead) \
    ((ListHead)->Flink == (ListHead))

VOID
FORCEINLINE
RemoveEntryList(
    IN PLIST_ENTRY Entry
    )
{
    PLIST_ENTRY Blink;
    PLIST_ENTRY Flink;

    Flink = Entry->Flink;
    Blink = Entry->Blink;
    Blink->Flink = Flink;
    Flink->Blink = Blink;
}

PLIST_ENTRY
FORCEINLINE
RemoveHeadList(
    IN PLIST_ENTRY ListHead
    )
{
    PLIST_ENTRY Flink;
    PLIST_ENTRY Entry;

    Entry = ListHead->Flink;
    Flink = Entry->Flink;
    ListHead->Flink = Flink;
    Flink->Blink = ListHead;
    return Entry;
}



PLIST_ENTRY
FORCEINLINE
RemoveTailList(
    IN PLIST_ENTRY ListHead
    )
{
    PLIST_ENTRY Blink;
    PLIST_ENTRY Entry;

    Entry = ListHead->Blink;
    Blink = Entry->Blink;
    ListHead->Blink = Blink;
    Blink->Flink = ListHead;
    return Entry;
}


VOID
FORCEINLINE
InsertTailList(
    IN PLIST_ENTRY ListHead,
    IN PLIST_ENTRY Entry
    )
{
    PLIST_ENTRY Blink;

    Blink = ListHead->Blink;
    Entry->Flink = ListHead;
    Entry->Blink = Blink;
    Blink->Flink = Entry;
    ListHead->Blink = Entry;
}


VOID
FORCEINLINE
InsertHeadList(
    IN PLIST_ENTRY ListHead,
    IN PLIST_ENTRY Entry
    )
{
    PLIST_ENTRY Flink;

    Flink = ListHead->Flink;
    Entry->Flink = Flink;
    Entry->Blink = ListHead;
    Flink->Blink = Entry;
    ListHead->Flink = Entry;
}



extern PCB Pcb0;

#pragma pack()

#if DBG
#define ASSERT(x) if(!(x)) KeAssertionFailed( #x, __FILE__, __LINE__ );
#else
#define ASSERT(x)
#endif

KESYSAPI
BOOLEAN
KEFASTAPI
KeAcquireLock(
	PLOCK  Lock
	);

KESYSAPI
VOID
KEAPI
KeReleaseIrqState(
	BOOLEAN IrqState
	);

KESYSAPI
VOID
KEFASTAPI
KeReleaseLock(
	PLOCK  Lock
	);

KESYSAPI
VOID
KEAPI
KeAssertionFailed(
	PCHAR Message,
	PCHAR FileName,
	ULONG Line
	);

KESYSAPI
VOID
_cdecl
memcpy(
	IN PVOID To,
	IN PVOID From,
	IN ULONG Length
	);

KESYSAPI
VOID
KEFASTAPI
KeZeroMemory(
	IN PVOID To,
	IN ULONG Length
	);


KESYSAPI
VOID
_cdecl
memset(
	IN PVOID To,
	IN UCHAR Byte,
	IN ULONG Length
	);

KESYSAPI
VOID
_cdecl
memmove(
	IN PVOID To,
	IN PVOID From,
	IN ULONG Length
	);

char* strncpy(char*, const char*, int);

KESYSAPI
STATUS
KEAPI
KeConnectInterrupt(
	IN UCHAR Vector,
	IN PIDT_ENTRY IdtEntry
	);

KESYSAPI
STATUS
KEAPI
KeSetInterruptHandler(
	IN UCHAR Vector,
	IN PVOID Handler
	);

VOID
KEAPI
KiInitializeIdt(
	);

typedef VOID (KEAPI *PDPC_ROUTINE)(PVOID);

typedef struct DPC_QUEUE
{
	PDPC_ROUTINE  Function;
	PVOID  Context;
} *PDPC_QUEUE;

extern PDPC_QUEUE KiDpcListHead;
extern UCHAR  KiNumberDpcs;
extern LOCK   KiDpcQueueLock;


KESYSAPI
VOID
KEFASTAPI
KiOutPort(
	USHORT PortNumber,
	UCHAR  Value
	);

KESYSAPI
UCHAR
KEFASTAPI
KiInPort(
	USHORT PortNumber
	);

VOID
KEAPI
KiProcessDpcQueue(
	);

KESYSAPI
PDPC_QUEUE
KEAPI
KeInsertQueueDpc(
	PDPC_ROUTINE Function,
	PVOID Context
	);

VOID
KEAPI
KiCallDpc(
	PDPC_QUEUE DpcEntry
	);

KESYSAPI
VOID
KEAPI
KeRemoveQueueDpc(
	PDPC_QUEUE DpcEntry,
	BOOLEAN LockQueue
	);

VOID
KEAPI
KiClearDpcQueue(
	);

VOID
KiOutChar(
	USHORT x,
	USHORT y,
	CHAR chr
	);

KESYSAPI
VOID
KEAPI
KeStallExecution(
	ULONG TickCount
	);

KENORETURN
VOID
KEAPI
KiStopExecution(
	);

KESYSAPI
USHORT
KEAPI
KeAllocateGdtDescriptor(
	);

KESYSAPI
PSEG_DESCRIPTOR
KEAPI
KeGdtEntry(
	USHORT Selector
	);

KESYSAPI
PIDT_ENTRY
KEAPI
KeIdtEntry(
	USHORT Index
	);

KESYSAPI
VOID
KEAPI
KiFillDataSegment(
	PSEG_DESCRIPTOR Segment,
	ULONG Address,
	ULONG Limit,
	BOOLEAN Granularity,
	UCHAR Type
	);

#define ALIGN_DOWN(x, align) ( (x) & (~(align-1)) )
#define ALIGN_UP(x, align)	((x & (align-1))?ALIGN_DOWN(x,align)+align:x)

KENORETURN
KESYSAPI
VOID
KEAPI
KeBugCheck(
	ULONG StopCode,
	ULONG Argument1,
	ULONG Argument2,
	ULONG Argument3,
	ULONG Argument4
	);

#define EX_INITIALIZATION_FAILED 0x00000001		// ( 0, 0, 0, 0 )
#define KE_INITIALIZATION_FAILED 0x00000002		// ( 0, 0, 0, 0 )
#define PS_INITIALIZATION_FAILED 0x00000003		// ( 0, 0, 0, 0 )
#define KD_INITIALIZATION_FAILED 0x00000004		// ( reason, 0, 0, 0 )
#define MM_INITIALIZATION_FAILED 0x00000005		// // ( 0, 0, 0, 0 )
#define KERNEL_MODE_EXCEPTION_NOT_HANDLED 0x00000006	// ( exc code, arg1, arg2, arg3 )
#define MANUALLY_INITIATED_CRASH 0x00000007		// ( caused from dbgkd, 0, 0, 0 )
#define EX_KERNEL_HEAP_FAILURE 0x00000008

#define MAXIMUM_BUGCHECK		 0x00000009

extern PCHAR KeBugCheckDescriptions[];

// First argument
#define HEAP_BLOCK_VALIDITY_CHECK_FAILED	1			 // ( 2 = see below, 3 = block, 4 = 0)
#define HEAP_DOUBLE_FREEING					2			 // ( 2 = 0, 3 = block, 4 = 0)
#define HEAP_LOCKED_BLOCK_FREEING			3			 // ( 2 = 0-freing/1-reallocating, 3 = block, 4 = 0)
#define HEAP_FREE_BLOCK_REALLOCATING		4			 // ( 2 = 0, 3 = block, 4 = 0)
#define HEAP_DOUBLE_LOCKING					5			 // ( 2 = 0, 3 = block, 4 = 0)
#define HEAP_FREE_BLOCK_LOCKING				6			 // ( 2 = 0, 3 = block, 4 = 0)
#define HEAP_NOTLOCKED_BLOCK_UNLOCKING		7			 // ( 2 = type, 3 = block, 4 = 0)
#define HEAP_GUARD_FAILURE					8			 // ( 2 = see below, 3 = table, 4 = block)

// Second argument
#define HEAP_VALIDATION_COOKIE_MISMATCH		1
#define HEAP_VALIDATION_CHECKSUM_MISMATCH	2
#define HEAP_VALIDATION_NO_FREE_PATTERN		3		// 4 = pattern found
#define HEAP_VALIDATION_OVERFLOW_DETECTED	4		// 4 = pattern found

#define HEAP_GUARD_ALREADY_GUARDED			1
#define HEAP_GUARD_NOT_GUARDED				2
#define HEAP_GUARD_LEAK_DETECTED			3


#pragma pack(1)

typedef struct CPU_FEATURES
{
	char ProcessorId[16];
	ULONG MaximumEax;

	union
	{
		ULONG Version;
		struct
		{
			ULONG SteppingID : 4;
			ULONG Model : 4;
			ULONG FamilyID : 4;
			ULONG Type : 2;
			ULONG Reserved1 : 2;
			ULONG ExModelID : 4;
			ULONG ExFamilyID : 8;
			ULONG Reserved2 : 4;
		};
	};

	union
	{
		ULONG  EbxAddInfo;
		struct
		{
			UCHAR BrandIndex;
			UCHAR CacheLineSize;
			UCHAR Reserved3;
			UCHAR LocalAPICID;
		};
	};

	union
	{
		ULONG FeatureInfo[2];

		struct
		{
			USHORT Sse3Exts : 1;
			USHORT Reserved1 : 2;
			USHORT Monitor : 1;
			USHORT DsCpl : 1;
			USHORT Vmx : 1;
			USHORT Reserved2 : 1;
			USHORT Est : 1;
			USHORT Tm2 : 1;
			USHORT Ssse3 : 1;
			USHORT CnxtId : 1;
			USHORT Reserved3 : 2;
			USHORT Cmpxchg16b : 1;
			USHORT xTpr : 1;
			USHORT Reserved4 : 1;
			USHORT Reserved5;
			ULONG FpuOnChip : 1;
			ULONG V8086 : 1;
			ULONG DebugExts : 1;
			ULONG PageSizeExts : 1;
			ULONG TimeStampCounter : 1;
			ULONG MSR : 1;
			ULONG PAE : 1;
			ULONG MachineCheck : 1;
			ULONG Cmpxchg8b : 1;
			ULONG ApicOnChip : 1;
			ULONG Reserved6 : 1;
			ULONG SysEnter : 1;
			ULONG MTRR : 1;
			ULONG PteGlobalBit : 1;
			ULONG MachineCheckArch : 1;
			ULONG CondMov : 1;
			ULONG PageAttributeTable : 1;
			ULONG PageSizeExt36 : 1;
			ULONG ProcessorSerialNumber : 1;
			ULONG Cflush : 1;
			ULONG Reserved7 : 1;
			ULONG DebugStore : 1;
			ULONG ACPI : 1;
			ULONG MMX : 1;
			ULONG FpuSave : 1;
			ULONG SseExts : 1;
			ULONG Sse2Exts : 1;
			ULONG SelfSnoop : 1;
			ULONG HyperThreading : 1;
			ULONG ThremalMonitor : 1;
			ULONG Reserved8 : 1;
			ULONG PBE : 1;
		} Features;
	};

	char BrandString[48];

} *PCPU_FEATURES;

#pragma pack()

KEVAR  CPU_FEATURES KeProcessorInfo;

KESYSAPI
VOID
KEAPI
KeGetCpuFeatures(
	PCPU_FEATURES IdentInfo
	);

typedef struct SCHEDULER_HEADER
{
	UCHAR ObjectType;
	BOOLEAN SignaledState;
	LIST_ENTRY WaitListHead;
} *PSCHEDULER_HEADER;

typedef struct EVENT
{
	SCHEDULER_HEADER Header;
} *PEVENT;

typedef enum OBJECT_TYPE
{
	NotificationEvent,
	SynchronizationEvent
} *POBJECT_TYPE;

KESYSAPI
VOID
KEAPI
KeInitializeEvent(
	PEVENT Event,
	UCHAR Type,
	BOOLEAN InitialState
	);

KESYSAPI
BOOLEAN
KEAPI
KeSetEvent(
	PEVENT Event,
	USHORT QuantumIncrement
	);

KESYSAPI
VOID
KEAPI
KeClearEvent(
	PEVENT Event
	);

KESYSAPI
BOOLEAN
KEAPI
KeWaitForSingleObject(
	IN PVOID Object,
	IN BOOLEAN Alertable,
	IN PLARGE_INTEGER Timeout
	);

KESYSAPI
ULONG
KEAPI
KeQueryTimerTickMult(
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
KeQueryApicTimerConf(
	PAPIC_TIMER_CONFIG Config
	);

KESYSAPI
VOID
KEAPI
KeSetApicTimerConf(
	PAPIC_TIMER_CONFIG Config
	);

KESYSAPI
ULONG
KEAPI
KiReadApicConfig(
	ULONG Offset
	);

KESYSAPI
ULONG
KEAPI
KiWriteApicConfig(
	ULONG Offset,
	ULONG Value
	);

#define APIC_TPR	0x0080

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
KeReadSystemPort(
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
KeConfigureTimer(
	UCHAR Timer,
	ULONG Freq
	);


KESYSAPI
USHORT
KEAPI
KeQueryTimerCounter(
	UCHAR Timer
	);

	
#define TIMER_FREQ  1193180