format MS COFF
include 'inc/common.inc'

public KsSystemCall
public SyscallTable as '_SyscallTable'
public SyscallCount as '_SyscallCount'

extrn '_KsClose@4' as KsClose
extrn '_KsCreateFile@24' as KsCreateFile
extrn '_KsCreateEvent@16' as KsCreateEvent
extrn '_KsSetEvent@8' as KsSetEvent


SyscallTable:
	dd KsClose					    ,	1
	dd KsCreateEvent		    ,	4
	dd KsCreateFile			    ,	6
	dd KsSetEvent					  ,	2

SyscallCount dd (($-SyscallTable)/8)

;
; KsSystemCall
;
;  Translates user-mode system call
;  
; Arguments:
;  EAX = syscall number
;  ECX = arg1
;  EDX = arg2
;  EBX = arg3
;  ESI = arg4
;  EDI = arg5
;  arguments 6,7,8,... placed on the stack
;
KsSystemCall:
	KiMakeContextFrame
	mov  [eax + THREAD.CallerFrame], esp

	GET_STACK esp
	; edx = pointer to caller's stack
	
	mov  eax, [esp + CONTEXT.Eax]	; syscall number
	cmp  eax, [SyscallCount]
	jbe  @F
	
	mov  eax, STATUS_INVALID_FUNCTION
	jmp  exit
	
@@: mov  ebx, [SyscallTable + eax*8]			; function
	mov  ecx, [SyscallTable + eax*8 + 4]		; argument count
		
	mov  eax, ecx
	shl  eax, 2		; eax  = ArgumentCount*4
	sub  esp, eax
	
	mov  esi, edx
	mov  edi, esp
	rep  movsd
	
	call ebx
	
	mov  [esp + CONTEXT.Eax], eax
	
exit:
    KiRestoreContextFrame
    iretd
