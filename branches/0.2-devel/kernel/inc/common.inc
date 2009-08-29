;
; Common include file for the ASM source code of the kernel
;

include 'auxmacro.inc'

struct LIST_ENTRY
	Flink		dd ?
	Blink		dd ?
ends

struct LOCK
	Count		db ?
	OldIrqState	db ?
ends

ExceptionContinueExecution = 0
ExceptionContinueSearch = 2

struct EXCEPTION_ARGUMENTS
	ExceptionCode	dd ?
	NumberParameters dd ?
	Parameters		rd 4
ends

struct EXCEPTION_RECORD
	Next			dd ?
	Handler			dd ?
ends

struct PCB
	ExceptionList	dd ?
	CurrentException dd ?
	CurrentThread	dd ?
	QuantumLeft		dw ?
	Self			dd ?
ends

EXCEPTION_ACCESS_VIOLATION	= 0xC0000005
EXCEPTION_BREAKPOINT		= 0x80000003
EXCEPTION_SINGLE_STEP		= 0x80000004
EXCEPTION_DIVISION_BY_ZERO	= 0xC0000094

macro KiMakeContextFrame
{
common
	push fs
    push gs
    push es
    push ds
    pushad
    
    mov  eax, [fs:PCB.CurrentThread]
    mov  [eax + THREAD.ContextFrame], esp
}

macro KiRestoreContextFrame
{
common
	popad
	pop  ds
	pop  es
	pop  gs
	pop  fs
}

KGDT_R0_CODE = 0x08
KGDT_R0_DATA = 0x10
KGDT_VIDEO   = 0x18
KGDT_TSS     = 0x20
KGDT_PCB     = 0x28

struct WAIT_BLOCK
	BackLink		dd ?
	WaitListEntry	LIST_ENTRY
ends

struct SCHEDULER_HEADER
	ObjectType			dd ?
	SignaledState		dd ?
	WaitListHead		LIST_ENTRY
ends

struct THREAD
    NextThread        dd ?
    PrevThread        dd ?

    UniqueId          dd ?
    KernelStack       dd ?
    State             db ?
    JustInitialized   db ?
    Quantum           dw ?          ; Thread quantum length
    QuantumDynamic    dw ?          ; Dynamic quantum.

    WaitType          db ?

    ;SuspendedOldState db ?
    DelayedTickCounts dd ?
    
    OwningProcess	  dd ?
    ProcessThreadListEntry  LIST_ENTRY
    
    NumberOfObjectsWaiting db ?
    WaitBlockUsed	  dd ?
    WaitBlock0		  WAIT_BLOCK
    WaitBlock1		  WAIT_BLOCK
    WaitBlock2		  WAIT_BLOCK
    WaitBlock3		  WAIT_BLOCK
    
    WaitMode		  db ?
    
    ContextFrame	  dd ?
    ExitCode		  dd ?
    ExceptionList	  dd ?
    CurrentException  dd ?
    
    IrpListLock		  SCHEDULER_HEADER
    IrpList			  LIST_ENTRY
ends

struct PROCESS
	ActiveProcessLinks	LIST_ENTRY
	DirectoryTableBase	dd ?
	ThreadListHead		LIST_ENTRY
	NumberOfThreads		dd ?
	ObjectTableLock		SCHEDULER_HEADER
	ObjectTable			dd ?
ends

THREAD_STATE_READY = 0
THREAD_STATE_RUNNING = 1
THREAD_STATE_WAIT = 2
THREAD_STATE_TERMINATED = 3

THREAD_WAIT_NOTWAITING = 0
THREAD_WAIT_SUSPENDED = 1
THREAD_WAIT_EXECUTIONDELAYED = 2
THREAD_WAIT_SINGLEOBJECT = 3

KernelMode = 0
DriverMode = 1
UserMode   = 3

THREAD_NORMAL_QUANTUM = 3

struct CONTEXT
    Eax  dd ?
    Ecx  dd ?
    Edx  dd ?
    Ebx  dd ?
    Esp  dd ?
    Ebp  dd ?
    Esi  dd ?
    Edi  dd ?
    Ds   dd ?
    Es   dd ?
    Gs   dd ?
    Fs   dd ?
    Eip  dd ?
    Cs   dd ?
    Eflags dd ?
ends

KE_PG_PDE_START  = 00180000h

struct CPU_FEATURES
	ProcessorId		rd 4
	MaximumEax		dd ?
	Version			dd ?
	EbxAddInfo		dd ?
	FeatureInfo		rd 2
	BrandString		rd 12
ends

LAPIC_BASE_P   = 0xFEE00000
LAPIC_BASE     = 0xFFFF0000
APIC_LIMT =        0x0FFF

APIC_ID     = LAPIC_BASE + 0x0020
APIC_VER    = LAPIC_BASE + 0x0030  ; r/o
APIC_TPR    = LAPIC_BASE + 0x0080
APIC_APR    = LAPIC_BASE + 0x0090  ; r/o
APIC_PPR    = LAPIC_BASE + 0x00A0  ; r/o
APIC_EOI    = LAPIC_BASE + 0x00B0  ; w/o
APIC_LOCDST = LAPIC_BASE + 0x00D0
APIC_DSTFMT = LAPIC_BASE + 0x00E0  ; r/o+r/w
APIC_SPINTV = LAPIC_BASE + 0x00F0  ; r/o+r/w
APIC_ISR    = LAPIC_BASE + 0x0100  ; r/o
APIC_TRGMOD = LAPIC_BASE + 0x0180  ; r/o
APIC_INTREQ = LAPIC_BASE + 0x0200  ; r/o
APIC_ERRSTS = LAPIC_BASE + 0x0280  ; r/o
APIC_INTCMD = LAPIC_BASE + 0x0300
APIC_LVTTMR = LAPIC_BASE + 0x0320
APIC_LVTTHR = LAPIC_BASE + 0x0330
APIC_LVTPRF = LAPIC_BASE + 0x0340
APIC_LVTLI0 = LAPIC_BASE + 0x0350
APIC_LVTLI1 = LAPIC_BASE + 0x0360
APIC_LVTERR = LAPIC_BASE + 0x0370
APIC_INITCNT= LAPIC_BASE + 0x0380
APIC_CURRCNT= LAPIC_BASE + 0x0390  ; R/O
APIC_DIVCONF= LAPIC_BASE + 0x03E0

struct APIC_TIMER_CONFIG
	Flags				dd ?
	InitialCounter		dd ?
	CurrentCounter		dd ?
	Divisor				dd ?
	LvtTimer			dd ?
ends

TIMER_MODIFY_INITIAL_COUNTER	= 0x01
TIMER_MODIFY_DIVISOR			= 0x02
TIMER_MODIFY_LVT_ENTRY			= 0x04

PIT_FREQ = 1193180


KE_IDT_IRQ_BASE   =  0x20