//
// <ps.h> built by header file parser at 19:50:50  11 Apr 2008
// This is a part of gr8os include files for GR8OS Driver & Extender Development Kit (DEDK)
//

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
	UserMode = 3
} *PPROCESSOR_MODE;

struct CONTEXT_FRAME;


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


typedef struct LDR_MODULE
{
	LIST_ENTRY ListEntry;
	UNICODE_STRING ModuleName;
	PVOID Base;
	ULONG Size;
} *PLDR_MODULE;


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

