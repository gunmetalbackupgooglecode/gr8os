format MS COFF
include 'inc/common.inc'

extrn '_SystemThread' as SystemThread:DWORD
extrn '_UniqueThreadIdSeed' as UniqueThreadIdSeed:DWORD
extrn '_PspLockSchedulerDatabase@0' as PspLockSchedulerDatabase
extrn '_PspUnlockSchedulerDatabase@0' as PspUnlockSchedulerDatabase
extrn '_PsExitThread@4' as PsExitThread
extrn '_FindReadyThread@0' as FindReadyThread

public PsInitSystem as '_PsInitSystem@0'

extrn '_PspInitSystem@0' as PspInitSystem
extrn '_PspReadyThread@4' as PspReadyThread
extrn '_PspProcessWaitList@0' as PspProcessWaitList


; @implemented
;
; PsInitSystem
;
;  Initialized processes and threads subsystem of the kernel
;
PsInitSystem:
	mov  ax, KGDT_PCB
	mov  fs, ax
	
	invoke PspMakeInitialThread, SystemThread
	call PspInitSystem
	retn


public PspBaseThreadStartup as '_PspBaseThreadStartup@8'

; @implemented
;
; PspBaseThreadStartup
;
;  This routine starts execution of any thread except initial
;  esp+0  StartRoutine
;  esp+4  StartContext
;
PspBaseThreadStartup:
	pop  eax
	call eax
	
	push 0
	call PsExitThread


extrn '_PspMakeInitialThread@4' as PspMakeInitialThread

;public PspMakeInitialThread as '_PspMakeInitialThread@4'
;
; @implemented, moved to <ps.cpp>
;
; PspMakeInitialThread
;
;  This function makes first, initial thread from the current control flow
;  esp+4  THREAD struct pointer to be filled
;
;PspMakeInitialThread:
;	mov  edx, [esp+4]  ; thread
;	mov  [edx + THREAD.NextThread], edx
;	mov  [edx + THREAD.PrevThread], edx
;	
;	inc  [UniqueThreadIdSeed]
;	mov  eax, [UniqueThreadIdSeed]
;	mov  [edx + THREAD.UniqueId], eax
;	mov  [edx + THREAD.State], THREAD_STATE_RUNNING
;	mov  [edx + THREAD.KernelStack], NULL
;	mov  [edx + THREAD.JustInitialized], FALSE
;	mov  [edx + THREAD.Quantum], THREAD_NORMAL_QUANTUM
;	
;	mov  [fs:PCB.CurrentThread], edx
;	mov  [fs:PCB.QuantumLeft], THREAD_NORMAL_QUANTUM
;	
;	retn 4

public SwapContext

; @implemented
;
; SwapContext
;
;  Swaps context of two threads on the current processor
;   esi = next thread
;   edi = current thread
;
SwapContext:
	; Swap stacks
	mov  [edi + THREAD.KernelStack], esp
	mov  esp, [esi + THREAD.KernelStack]
	
	; Set state for the new thread
	mov  [esi + THREAD.State], THREAD_STATE_RUNNING
	
	; Refresh PCB
	mov  [fs:PCB.CurrentThread], esi
	
	; Swap exception lists
	mov  eax, [fs:PCB.ExceptionList]
	mov  [edi + THREAD.ExceptionList], eax
	mov  eax, [esi + THREAD.ExceptionList]
	mov  [fs:PCB.ExceptionList], eax
	
	; Swap current exceptions
	mov  eax, [fs:PCB.CurrentException]
	mov  [edi + THREAD.CurrentException], eax
	mov  eax, [esi + THREAD.CurrentException]
	mov  [fs:PCB.CurrentException], eax
	
	; Set quantums
	mov  ax, [esi + THREAD.Quantum]
	add  ax, [esi + THREAD.QuantumDynamic]
	mov  [fs:PCB.QuantumLeft], ax
	
	; Zero dynamic quantum
	mov  [esi + THREAD.QuantumDynamic], 0
	
	; Swap processes if need
	mov  eax, [edi + THREAD.OwningProcess]
	mov  ecx, [esi + THREAD.OwningProcess]
	cmp  eax, ecx
	jz   SC20
	mov  eax, [ecx + PROCESS.DirectoryTableBase]
	mov  cr3, eax
 SC20:
		
	; Check if thread is just initialized and didn't have any quantum yet.
	cmp  [esi + THREAD.JustInitialized], TRUE
	jnz  SC10
	
	; Thread is just initialized.
	; We should unlock scheduler DB now (!) and return immediately
	mov  [esi + THREAD.JustInitialized], FALSE
	call PspUnlockSchedulerDatabase
	sti
	
  SC10:
	retn


public PsSwapThread as '_PsSwapThread@0'

; @implemented
;
; PsSwapThread
;
;  Searches next thread and executes it with scheduler DB lock synch
;
PsSwapThread:
	call PspLockSchedulerDatabase
	call PspSwapThread
	call PspUnlockSchedulerDatabase
	retn


public PspSwapThread as '_PspSwapThread@0'

; @implemented
;
; PspSwapThread
;
;  Searches next thread and executes it w/o scheduler DB lock
;
PspSwapThread:

	; If current thread is really still ready, move it to the end of the list.
	mov  eax, [fs:PCB.CurrentThread]
	.if  [eax + THREAD.State], e, THREAD_STATE_READY
		invoke PspReadyThread, eax
	.endif
	
	; Process wait list first.
	call PspProcessWaitList
	
	; Find ready thread
	call FindReadyThread
	
	test eax, eax
	jz   PST10
	
	mov  esi, eax
	mov  edi, [fs:PCB.CurrentThread]
	call SwapContext
	
  PST10:
	retn


public PsQuantumEnd as '_PsQuantumEnd@0'

; @implemented
;
; PsQuantumEnd
;
;  This function leaves current thread (marking it as ready instead of running)
;  and searches next thread to execute. Context is being switched to the next thread
;  and returned only in the future at the next quantum.
;
PsQuantumEnd:
	mov  edx, [fs:PCB.CurrentThread]
	
	mov  [edx + THREAD.State], THREAD_STATE_READY
	call PsSwapThread
	
	retn


public PspSchedulerTimerInterruptDispatcher as '_PspSchedulerTimerInterruptDispatcher@0'
extrn '_PspQuantumEndBreakPoint' as PspQuantumEndBreakPoint:BYTE

; @implemented
;
; PspSchedulerTimerInterruptDispatcher
;
;  Performs context switch if quantum ends. Called directly from IRQ0 (timer) handler
;
PspSchedulerTimerInterruptDispatcher:
	mov  ax, fs
	test ax, ax
	jz   PSTD10
	
	dec  [fs:PCB.QuantumLeft]
	jnz  PSTD10
	
	mov  al, [PspQuantumEndBreakPoint]
	test al, al
	jz  @F
	
	jmp $   ; breakpoint
	
  @@:
	call PsQuantumEnd
	
 PSTD10:
	retn


public PsGetCurrentThread as '_PsGetCurrentThread@0'
public PsGetCurrentPcb as '_PsGetCurrentPcb@0'

; @implemented
;
; PsGetCurrentThread
;
;  Retrieves pointer to the current thread
;
PsGetCurrentThread:
	mov  eax, [fs:PCB.CurrentThread]
	retn


; @implemented
;
; PsGetCurrentPcb
;
;  Retrieves pointer to the current PCB
;
PsGetCurrentPcb:
	mov  eax, [fs:PCB.Self]
	retn
