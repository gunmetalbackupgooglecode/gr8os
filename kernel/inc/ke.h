// begin_ddk
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
	ULONG  Flags;			// see EH_*
	EXCEPTION_ARGUMENTS* ExceptionArguments;
	ULONG  ExceptionAddress;
	ULONG  NumberParameters;
	ULONG  Parameters[EXCEPTION_MAXIMUM_PARAMETERS];
} *PEXCEPTION_ARGUMENTS;

#define EH_NONCONTINUABLE	1
#define EH_UNWINDING		2
#define EH_EXIT_UNWIND		4
#define EH_STACK_INVALID	8
#define EH_NESTED_CALL		16
#define EH_CONTINUING		32

enum EXCEPTION_RESOLUTION
{
	ExceptionContinueExecution = 0,
	//ExceptionExecuteHandler,
	ExceptionContinueSearch = 2
};

#define EXCEPTION_CONTINUE_EXECUTION	0
#define EXCEPTION_EXECUTE_HANDLER		1
#define EXCEPTION_CONTINUE_SEARCH		2
//#define EXCEPTION_NESTED_EXCEPTION		3
#define EXCEPTION_COLLIDED_UNWIND		4

typedef struct CONTEXT_FRAME *PCONTEXT_FRAME;
typedef struct EXCEPTION_FRAME *PEXCEPTION_FRAME;

typedef struct EXCEPTION_POINTERS
{
	PEXCEPTION_ARGUMENTS ExceptionArguments;
	PCONTEXT_FRAME ContextFrame;
} PEXCEPTION_POINTERS;

typedef UCHAR (_cdecl *EXCEPTION_HANDLER)(
	IN PEXCEPTION_ARGUMENTS ExceptionArguments,
	IN PEXCEPTION_FRAME EstablisherFrame,
	IN PCONTEXT_FRAME CallerContext,
	IN PVOID Reserved
	);

typedef UCHAR (KEAPI *VCPP_FILTER)(
	);

typedef UCHAR (KEAPI *VCPP_HANDLER)(
	);

typedef struct SCOPE_TABLE
{
	ULONG PreviousTryLevel;
	VCPP_FILTER Filter;
	VCPP_HANDLER Handler;
} *PSCOPE_TABLE;

typedef struct EXCEPTION_FRAME
{
	EXCEPTION_FRAME *Next;
	EXCEPTION_HANDLER Handler;
	PSCOPE_TABLE ScopeTable;
	int trylevel;
	ULONG _ebp;
	PEXCEPTION_POINTERS xPointers;
} *PEXCEPTION_FRAME;

#define TRYLEVEL_NONE		((ULONG)-1)

UCHAR
_cdecl
_except_handler3(
	IN PEXCEPTION_ARGUMENTS ExceptionArguments,
	IN PEXCEPTION_FRAME EstablisherFrame,
	IN PCONTEXT_FRAME CallerContext,
	IN PVOID Reserved
	);

#define GetExceptionCode            _exception_code
#define exception_code              _exception_code
#define GetExceptionInformation     (EXCEPTION_POINTERS *)_exception_info
#define exception_info              (EXCEPTION_POINTERS *)_exception_info

unsigned long __cdecl _exception_code(void);
void *        __cdecl _exception_info(void);

#define EXCEPTION_ACCESS_VIOLATION	0xC0000005
#define EXCEPTION_BREAKPOINT		0x80000003
#define EXCEPTION_SINGLE_STEP		0x80000004
#define EXCEPTION_DIVISION_BY_ZERO	0xC0000094
#define EXCEPTION_INVALID_RESULUTION 0xC00000FF
#define STATUS_INVALID_DISPOSITION 0xC0000026
#define STATUS_UNWIND 0xC0000027
#define STATUS_INVALID_UNWIND_TARGET 0xC0000029

KESYSAPI
VOID
KEAPI
KeContinue(
	PCONTEXT_FRAME ContextFrame
	);

KESYSAPI
VOID
KEAPI
KeDispatchException(
	PEXCEPTION_ARGUMENTS Args,
	CONTEXT_FRAME *ContextFrame
	);

KESYSAPI
VOID
KEAPI
KeCaptureContext(
	PCONTEXT_FRAME ContextFrame
	);

KESYSAPI
VOID
KEAPI
KeRaiseStatus(
	STATUS Status
	);

KESYSAPI
VOID
KEAPI
KeRaiseException(
	PEXCEPTION_ARGUMENTS ExceptionArguments
	);

VOID
_cdecl
_local_unwind2(
	IN PEXCEPTION_FRAME RegistrationFrame,
	IN int TryLevelPassed
	);

VOID
KEAPI
RtlUnwind(
	PEXCEPTION_FRAME RegistrationFrame,
	PEXCEPTION_ARGUMENTS Args
	);

VOID
_cdecl
_global_unwind2(
	IN PEXCEPTION_FRAME RegistrationFrame
	);

// end_ddk

#define KI_CHECK_IF() { \
	ULONG efl;			\
	{ __asm pushfd	}	\
	{ __asm pop  [efl] }\
	ASSERT (efl & 0x200); \
	}

// begin_ddk

typedef struct PCB
{
	PEXCEPTION_FRAME ExceptionList;
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

#define InterlockedOp(Lock, Operation) \
	{									\
		ExAcquireMutex (Lock);			\
		Operation;						\
		ExReleaseMutex (Lock);			\
	}


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
#define ASSERT(x) if(!(x)) KeAssertionFailed( #x, __FILE__, __LINE__, __FUNCTION__ );
#define ASSERT_EQUAL(x,y) if((x)!=(y)) KeAssertionEqualFailed ( #x, (ULONG)x, #y, (ULONG)y, __FILE__, __LINE__, __FUNCTION__ );
#else
#define ASSERT(x) ;
#define ASSERT_EQUAL(x,y) ;
#endif

KESYSAPI
VOID
KEAPI
KeAssertionFailed(
	PCHAR Message,
	PCHAR FileName,
	ULONG Line,
	PCHAR Function
	);

KESYSAPI
VOID
KEAPI
KeAssertionEqualFailed(
	PCHAR Name1,
	ULONG Value1,
	PCHAR Name2,
	ULONG Value2,
	PCHAR FileName,
	ULONG Line,
	PCHAR Function
	);



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
BOOLEAN
KEAPI
KeChangeWpState(
	BOOLEAN Wp
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

KESYSAPI
VOID
_cdecl
memset_far(
	IN ULONG SegmentTo,
	IN ULONG To,
	IN UCHAR Byte,
	IN ULONG Length
	);

KESYSAPI
VOID
_cdecl
memcopy_far(
	IN ULONG SegmentTo,
	IN ULONG To,
	IN ULONG SegmentFrom,
	IN ULONG From,
	IN ULONG Length
	);

KESYSAPI
VOID
_cdecl
memmove_far(
	IN ULONG SegmentTo,
	IN ULONG To,
	IN ULONG SegmentFrom,
	IN ULONG From,
	IN ULONG Length
	);

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

// end_ddk

extern PDPC_QUEUE KiDpcListHead;
extern UCHAR  KiNumberDpcs;
extern LOCK   KiDpcQueueLock;

// begin_ddk

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

KESYSAPI
VOID
KEFASTAPI
KiOutPortW(
	USHORT PortNumber,
	USHORT  Value
	);

KESYSAPI
USHORT
KEFASTAPI
KiInPortW(
	USHORT PortNumber
	);

KESYSAPI
VOID
KEFASTAPI
KiOutPortD(
	USHORT PortNumber,
	ULONG  Value
	);

KESYSAPI
ULONG
KEFASTAPI
KiInPortD(
	USHORT PortNumber
	);

KESYSAPI
VOID
KEAPI
KiConnectInterrupt(
	UCHAR IdtVector,
	PVOID Handler
	);

KESYSAPI
VOID
KEAPI
KiConnectVector(
	UCHAR IdtVector,
	PVOID Handler
	);

KESYSAPI
VOID
KEAPI
KiEoiHelper(
	);

KESYSAPI
PDPC_QUEUE
KEAPI
KeInsertQueueDpc(
	PDPC_ROUTINE Function,
	PVOID Context
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
KiProcessDpcQueue(
	);

// end_ddk

VOID
KEAPI
KiCallDpc(
	PDPC_QUEUE DpcEntry
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

KENORETURN
VOID
KEAPI
KiStopExecution(
	);

// begin_ddk

KESYSAPI
VOID
KEAPI
KeStallExecution(
	ULONG TickCount
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

#define EX_INITIALIZATION_FAILED	0x00000001		// ( 0, 0, 0, 0 )
#define KE_INITIALIZATION_FAILED	0x00000002		// ( 0, 0, 0, 0 )
#define PS_INITIALIZATION_FAILED	0x00000003		// ( 0, 0, 0, 0 )
#define KD_INITIALIZATION_FAILED	0x00000004		// ( reason, 0, 0, 0 )
#define MM_INITIALIZATION_FAILED	0x00000005		// // ( 0, 0, 0, 0 )
#define KERNEL_MODE_EXCEPTION_NOT_HANDLED 0x00000006	// ( exc code, arg1, arg2, arg3 )
#define MANUALLY_INITIATED_CRASH	0x00000007		// ( caused from dbgkd, 0, 0, 0 )
#define EX_KERNEL_HEAP_FAILURE		0x00000008
#define OB_INITIALIZATION_FAILED	0x00000009
#define IO_INITIALIZATION_FAILED	0x0000000A
#define IO_MULTIPLE_COMPLETE_REQUESTS	0x0000000B
#define IO_NO_MORE_IRP_STACK_LOCATIONS	0x0000000C
#define IO_IRP_COMPLETION_WITH_PENDING	0x0000000D
#define HAL_FREEING_ALREADY_FREE_PAGES	0x0000000E
#define HAL_FREEING_RESERVED_PAGES		0x0000000F
#define HAL_FREEING_INVALID_PAGES		0x00000010
#define MEMORY_MANAGEMENT				0x00000011
#define PS_SCHEDULER_GENERAL_FAILURE	0x00000012
#define UNMOUNTABLE_BOOT_VOLUME			0x00000013

#define MAXIMUM_BUGCHECK				0x00000014

// end_ddk

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

#define PSP_NO_READY_THREADS				1

// begin_ddk

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

typedef enum KE_OBJECT_TYPE
{
	NotificationEvent,
	SynchronizationEvent,
	SynchronizationMutex
} *PKE_OBJECT_TYPE;

#define EVENT_READ_STATE	0x00000001
#define EVENT_SET_STATE		0x00000002
#define EVENT_ALL_ACCESS	(EVENT_READ_STATE | EVENT_SET_STATE)

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
BOOLEAN
KEAPI
KePulseEvent(
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

// end_ddk

BOOLEAN
KEAPI
KiSetEvent(
	IN PEVENT Event,
	IN USHORT QuantumIncrement
	);

// begin_ddk

enum PROCESSOR_MODE;

KESYSAPI
PROCESSOR_MODE
KEAPI
KeGetRequestorMode(
	);

#define KeGetRequestorMode() (PsGetCurrentThread()->PreviousMode)

KESYSAPI
LONG
KEFASTAPI
InterlockedExchange(
	PLONG Variable,
	LONG NewValue
	);

KESYSAPI
LONG
KEFASTAPI
InterlockedExchangeAdd(
	PLONG Variable,
	LONG Increment
	);

KESYSAPI
LONG
KEFASTAPI
InterlockedCompareExchange(
	PLONG Variable,			// @ECX
	LONG Exchange,			// @EDX
	LONG Comperand
	);

// end_ddk

VOID
KEAPI
KiStallExecutionHalfSecond(
	);

CHAR
KEFASTAPI
KiReadChar( 
	ULONG Pos
	);

VOID
KEFASTAPI
KiWriteChar(
	ULONG Pos,
	CHAR chr
	);

extern ULONG KiInitializationPhase;

// begin_ddk

KESYSAPI
VOID
KEFASTAPI
InterlockedDecrement(
	PLONG Long
);

KESYSAPI
VOID
KEFASTAPI
InterlockedIncrement(
	PLONG Long
);

KESYSAPI
VOID
KEAPI
KePrintActiveConsole(
	PSTR OutString
	);

VOID
KEAPI
KiInitializeOnScreenStatusLine(
	);

KESYSAPI
VOID
KEAPI
KeSetOnScreenStatus(
	PSTR Status
	);

VOID
KEAPI
KiMoveLoadingProgressBar(
	UCHAR Percent
	);

VOID
KEFASTAPI
KiWriteCharAttribute(
	ULONG Pos,
	UCHAR Attribute
	);

KEVAR UCHAR KiKeyboardStatusByte;

typedef struct OBJECT_DIRECTORY *POBJECT_DIRECTORY;
KEVAR POBJECT_DIRECTORY KeBaseNamedObjectsDirectory;

KESYSAPI
VOID
KEAPI
KeWriteConsoleChar(
  IN UCHAR x,
  IN UCHAR y,
  IN CHAR chr,
  IN UCHAR attribute
  );

KESYSAPI
VOID
KEAPI
KeWriteConsole(
  IN UCHAR x,
  IN UCHAR y,
  IN USHORT ByteCount,
  IN PVOID Buffer
  );

KESYSAPI
VOID
KEAPI
KeScanConsole(
  IN UCHAR x,
  IN UCHAR y,
  IN USHORT ByteCount,
  OUT PVOID Buffer
  );

// end_ddk
