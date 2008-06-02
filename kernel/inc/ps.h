// begin_ddk
#pragma once

typedef struct PROCESS *PPROCESS;
typedef struct THREAD *PTHREAD;

//
// Thread structure.
//

typedef struct WAIT_BLOCK
{
	PTHREAD BackLink;
	LIST_ENTRY WaitListEntry;     // Other threads that wait for this event
} *PWAIT_BLOCK;

#define THREAD_WAIT_BLOCKS 4

typedef enum PROCESSOR_MODE
{
	KernelMode = 0,
	DriverMode = 1,
	UserMode = 3,
	MaximumMode = 4
} *PPROCESSOR_MODE;

struct CONTEXT_FRAME;

// end_ddk

#pragma pack(1)

typedef struct THREAD
{
	// Double-linked cycle list of threads.
	LIST_ENTRY SchedulerListEntry;

	// Unique thread id
	ULONG  UniqueId;

	// Kernel stack pointer
	PVOID  KernelStack;

	// Thread state
	UCHAR  State;

	// Thread is just initialized. Perform special context switch
	UCHAR  JustInitialized;

	// Thread base quantum
	USHORT Quantum;

	// Thread dynamic quantum
	USHORT QuantumDynamic;

	// Wait parameters
	UCHAR  WaitType;
	
	union
	{
		// Suspend extra parameters
		UCHAR  SuspendedOldState;

		// Delay extra parameters
		ULONG  DelayedTickCount;
	};

	// Owning process
	PROCESS* OwningProcess;
	LIST_ENTRY ProcessThreadListEntry;

	// Wait blocks
	UCHAR  NumberOfObjectsWaiting;
	PWAIT_BLOCK WaitBlockUsed;
	WAIT_BLOCK  WaitBlocks[THREAD_WAIT_BLOCKS];
	
	// Wait mode
	PROCESSOR_MODE WaitMode;

	// Context Frame
	CONTEXT_FRAME* ContextFrame;

	// Exit code
	ULONG ExitCode;

	// Exceptions handler list
	EXCEPTION_FRAME *ExceptionList;

	// Current processing exception
	EXCEPTION_ARGUMENTS *CurrentException;

	//BUGBUG: Not added into common.inc!
	BOOLEAN InExGuardedRegion;
	PVOID ExGuardTable;

	// Irp list
	MUTEX IrpListLock;
	LIST_ENTRY IrpList;

	PROCESSOR_MODE PreviousMode;

} *PTHREAD;

// begin_ddk

enum THREAD_STATE
{
	THREAD_STATE_READY,
	THREAD_STATE_RUNNING,
	THREAD_STATE_WAIT,
	THREAD_STATE_TERMINATED
};

enum WAIT_STATE
{
	THREAD_WAIT_NOTWAITING,
	THREAD_WAIT_SUSPENDED,
	THREAD_WAIT_EXECUTIONDELAYED,
	THREAD_WAIT_SCHEDULEROBJECTS_ALL,
	THREAD_WAIT_SCHEDULEROBJECTS_ANY
};

#define THREAD_NORMAL_QUANTUM  3

typedef struct CONTEXT_FRAME
{
	ULONG  Eax;
	ULONG  Ecx;
	ULONG  Edx;
	ULONG  Ebx;
	ULONG  Esp;
	ULONG  Ebp;
	ULONG  Esi;
	ULONG  Edi;
	ULONG  Ds;
	ULONG  Es;
	ULONG  Gs;
	ULONG  Fs;
	ULONG  Eip;
	ULONG  Cs;
	ULONG  Eflags;
} CONTEXT, *PCONTEXT;

// end_ddk

extern THREAD SystemThread;
extern ULONG UniqueThreadIdSeed;
extern LIST_ENTRY PsReadyListHead;
extern LIST_ENTRY PsWaitListHead;

#define THREAD_INITIAL_STACK_SIZE  0x3000

typedef struct OBJECT_HANDLE *POBJECT_HANDLE;

typedef struct OBJECT_TABLE
{
	MUTEX TableLock;
	ULONG CurrentSize;
	POBJECT_HANDLE HandleTable;
} *POBJECT_TABLE;

//
// Process
//

typedef struct PROCESS
{
	// Link to system processes
	LIST_ENTRY ActiveProcessLinks;

	// CR3
	ULONG DirectoryTableBase;

	// Thread list
	LIST_ENTRY ThreadListHead;

	// Number of thread that belong to this process
	ULONG  NumberOfThreads;

	// Object table
	OBJECT_TABLE ObjectTable;
	
	// Working set
	PMMWORKING_SET WorkingSet;

} *PPROCESS;

extern LIST_ENTRY PsActiveProcessHead;

extern PROCESS InitialSystemProcess;

#pragma pack()

// begin_ddk

typedef struct LDR_MODULE
{
	LIST_ENTRY ListEntry;
	UNICODE_STRING ModuleName;
	PVOID Base;
	ULONG Size;
} *PLDR_MODULE;

// end_ddk

extern LOCKED_LIST PsLoadedModuleList;

KESYSAPI
BOOLEAN
KEAPI
PspLockSchedulerDatabase( );

KESYSAPI
VOID
KEAPI
PspUnlockSchedulerDatabase( );

KESYSAPI
VOID
KEAPI
PspDumpThread( );

KESYSAPI
VOID
KEAPI
PspDumpSingleThread(
	IN PTHREAD Thread
	);

// begin_ddk

KENORETURN
KESYSAPI
VOID
KEAPI
PsExitThread(
	ULONG Code
	);

KESYSAPI
PTHREAD
KEAPI
PsGetCurrentThread(
	);

KESYSAPI
PPCB
KEAPI
PsGetCurrentPcb(
	);

// end_ddk

VOID
KEAPI
PsInitSystem( );

VOID
KEAPI
PspInitSystem( );

VOID
KEAPI
PspBaseThreadStartup(
	PVOID StartRoutine,   // ESP+0
	PVOID StartContext    // ESP+4
	);

VOID
KEAPI
PspMakeInitialThread(
	OUT PTHREAD InitialThread
	);

VOID
KEAPI
PspCreateThread(
	OUT PTHREAD Thread,
	IN  PPROCESS OwningProcess,
	IN  PVOID  StartRoutine,
	IN  PVOID  StartContext
	);

// begin_ddk

KESYSAPI
PTHREAD
KEAPI
PsCreateThread(
	IN  PPROCESS OwningProcess,
	IN  PVOID  StartRoutine,
	IN  PVOID  StartContext
	);

KESYSAPI
VOID
KEAPI
PsTerminateThread(
	IN PTHREAD Thread,
	IN ULONG Code
	);

// end_ddk

PTHREAD
KEAPI
FindReadyThread(
	);

/*
KESYSAPI
VOID
KEFASTAPI
SwapContext(
	IN PTHREAD NextThread,
	IN PTHREAD CurrentThread
	);
*/

// begin_ddk

KESYSAPI
VOID
KEAPI
PspSwapThread(
	);

KESYSAPI
VOID
KEAPI
PsSwapThread(
	);

// end_ddk

VOID
KEAPI
PspDumpReadyQueue(
	);

VOID
KEAPI
PspDumpSystemThreads(
	);

VOID
KEAPI
PsQuantumEnd(
	);

VOID
KEAPI
PspSchedulerTimerInterruptDispatcher(
	);

// begin_ddk

KESYSAPI
VOID
KEAPI
PsGetThreadContext(
	IN  PTHREAD Thread,
	OUT PCONTEXT Context
	);

KESYSAPI
VOID
KEAPI
PsSetThreadContext(
	IN PTHREAD Thread,
	IN PCONTEXT Context
	);

KESYSAPI
VOID
KEAPI
PsSuspendThread(
	IN PTHREAD Thread
	);

KESYSAPI
VOID
KEAPI
PsResumeThread(
	IN PTHREAD Thread
	);

KESYSAPI
VOID
KEAPI
PsDelayThreadExecution(
	IN ULONG ClockTickCount
	);

// end_ddk

VOID
KEAPI
PspReadyThread(
	IN PTHREAD Thread
	);

VOID
KEAPI
PspUnwaitThread(
	IN PTHREAD Thread,
	IN USHORT QuantumIncrement
	);

VOID
KEAPI
PspDumpReadyQueue(
	);

VOID
KEAPI
PspProcessWaitList(
	);

extern LOCK SchedulerLock;


VOID
KEAPI
PspCreateProcess(
	OUT PPROCESS Process
	);

// begin_ddk

KESYSAPI
PPROCESS
KEAPI
PsCreateProcess(
	);

KESYSAPI
PPROCESS
KEAPI
PsGetCurrentProcess(
	);

// end_ddk