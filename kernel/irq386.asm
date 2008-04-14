format MS COFF

include 'inc/common.inc'

extrn '_PspSchedulerTimerInterruptDispatcher@0' as PspSchedulerTimerInterruptDispatcher
extrn '_KiProcessDpcQueue@0' as KiProcessDpcQueue

public irq0_handler
public irq1_handler
public KiEoiHelper

;
; IRQ 0
;

arrows db '-\|/',0
load_counter db 0

irq0_handler:
	KiMakeContextFrame

	inc  [load_counter]
	and  [load_counter], 11b
	movzx ecx, [load_counter]
	mov  al, [arrows+ecx]
	mov  byte [gs:158], al
	
    mov  al, 20h
    out  020h, al
    out  0a0h, al

;	mov  byte [0xFFFF00B0], 0

    call KiProcessDpcQueue

    call PspSchedulerTimerInterruptDispatcher

    KiRestoreContextFrame
    iretd

;
; IRQ 1
;

extrn '_PspDumpSystemThreads@0' as PspDumpSystemThreads

irq1_handler:
    push ax
    push edi
    xor  ax, ax

    ; scan-code
    in	 al, 060h

	.if al,e,1    ; ESC
	
	  call PspDumpSystemThreads
	  jmp Ack

    .elseif al,e,28  ; ENTER

      mov  bl, [kbrd_status_byte]
      and  bl, 00001100b	    ; Ctrl+Alt?
      .if  bl,e,00001100b	    ; Ctrl-Alt-Enter pressed, reboot
	mov  al, 0feh
	out  64h, al		    ; #RESET
      .else
	mov  ax, 1
	int  30h
      .endif
      jmp  Ack

    .elseif al,e,42  ; Left Shift

      and  [kbrd_status_byte], 11111100b
      or   [kbrd_status_byte], 00000001b
      jmp  Ack

    .elseif al,e,54  ; Right Shift

      and  [kbrd_status_byte], 11111100b
      or   [kbrd_status_byte], 00000011b
      jmp  Ack

    .elseif al,e,80h + 42  ; Left Shift (UP)

      and  [kbrd_status_byte], 11111100b
      jmp  Ack

    .elseif al,e,80h + 54  ; Right Shift (UP)

      and  [kbrd_status_byte], 11111100b
      jmp  Ack

    .elseif al,e,29  ; Ctrl

      or   [kbrd_status_byte], 00000100b
      jmp  Ack

    .elseif al,e,56  ; Alt

      or   [kbrd_status_byte], 00001000b
      jmp  Ack

    .elseif al,e,80h + 29  ; Ctrl (UP)

      and  [kbrd_status_byte], 11111011b
      jmp  Ack

    .elseif al,e,80h + 56  ; Alt (UP)

      and  [kbrd_status_byte], 11110111b
      jmp  Ack

    .elseif al,e,69  ; Num Lock

      or   [kbrd_status_byte], 00010000b
      jmp  Ack

    .elseif al,e,58  ; Caps Lock

      or   [kbrd_status_byte], 00100000b
      jmp  Ack

    .elseif al,e,70  ; Scroll Lock

      or   [kbrd_status_byte], 01000000b
      jmp  Ack

    .elseif al,e,80h + 69  ; Num Lock (UP)

      and  [kbrd_status_byte], 11101111b
      jmp  Ack

    .elseif al,e,80h + 58  ; Caps Lock (UP)

      and  [kbrd_status_byte], 11011111b
      jmp  Ack

    .elseif al,e,80h + 70  ; Scroll Lock (UP)

      and  [kbrd_status_byte], 10111111b
      jmp  Ack

    .elseif al,e,14  ; Backspace

      dec  dword [cursor]
      mov  edi, dword [cursor]
      mov  byte [gs:edi*2], 0
      jmp  Ack

    .endif

    ; only UP, not DOWNs
    mov  ah, al
    and  ah, 80h
    jnz clear_request
    and  al, 7Fh

    ;call kd_irq_kbd

    push edi

    ; Shift pressed? Load tables
    mov  bl, [kbrd_status_byte]
    and  bl, 1
    .if bl,e,0
      mov  edi, ascii
    .else
      mov  edi, asc_shft
    .endif

    ; Convert scancode to ASCII code
    add  di, ax
    mov  al, [edi]

    pop  edi

    ; Output characters
    .if  al, ne, 0
      mov  edi, dword [cursor]
      shl  edi, 1
      mov  byte [gs:edi], al
      inc  dword [cursor]
    .endif

    .if dword [cursor],g,2000
      mov  dword [cursor], 0
    .endif

    ; Send acknowledgement to the keyboard
    ; (set and clear 7 bit in 061h port)
   Ack:
    in	 al, 061h
    or	 al, 80
    out  061h, al
    xor  al, 80
    out  061h, al

clear_request:
    pop  edi
    pop  ax
    jmp  KiEoiHelper
    
;
; FD IRQ
;

public FdpIrqState as '_FdpIrqState'
FdpIrqState db 0

FdIrq_handler:
	jmp  KiEoiHelper
	push edx
	push eax

	or   [FdpIrqState], 1
	
	mov  edx, 0x3F4
	in   al, dx
	test al, 0xC0
	jnz   fd_normal
	
	; read irq state
	mov  al, 8
	mov  edx, 0x3f5
	out  dx, al
	
	mov  edx, 0x3f4
  @@:
	in   al, dx
	test al, 0xC0
	jz   @B
	
  @@:
	mov  edx, 0x3f5
	in   al, dx
	mov  edx, 0x3f4
	in   al, dx
	test al, 0xC0
	jnz  @B
		
  fd_normal:
	mov  edx, 0x3F7
	in   al, dx
	test al, 0x80
	jz   @F
	; disk change bit set
	or   [FdpIrqState], 2
  @@:		
	inc  byte [gs:4]
	
	pop  eax
	pop  edx
	mov  byte [APIC_EOI], 0
	jmp  KiEoiHelper

;
; EOI helper
;

public KiEoiHelper as '_KiEoiHelper@0'

KiEoiHelper:
    push ax
    mov  al, 20h
    out  020h, al
    out  0a0h, al
    pop  ax
    iretd


ascii	 db 0,0,'1234567890-=',0,0,'qwertyuiop[]',0,0,'asdfghjkl;',"'`",0,'\zxcvbnm,./',0,'*',0,' ',0, 0,0,0,0,0,0,0,0,0,0, 0,0, '789-456+1230.', 0,0
asc_shft db 0,0,'!@#$%^&*()_+',0,0,'QWERTYUIOP{}',0,0,'ASDFGHJKL:"~',0,'|ZXCVBNM<>?',0,'*',0,' ',0, 0,0,0,0,0,0,0,0,0,0, 0,0, '789-456+1230.', 0,0

kbrd_status_byte db 0


public KiInitializeIdt as '_KiInitializeIdt@0'

IDTR:
IDTR_limit dw 0
IDTR_base  dd 0


; @implemented
;
; KiWriteVector
;
;  Writes IDT vector
;  esp+4  idt number
;  esp+8  handler
;
KiWriteVector:
	push ecx
	
	mov  ecx, [esp+8]
	
	mov  ebx, [esp+12]
	mov  eax, ebx
	and  eax, 0xFFFF
	mov  word [esi + ecx*8], ax
	mov  eax, ebx
	and  eax, 0xFFFF0000
	shr  eax, 16
	mov  word [esi + ecx*8 + 6], ax 
	
	pop  ecx
	retn 8
	
public KiConnectInterrupt as '_KiConnectInterrupt@8'

;  Writes IDT vector
;  esp+4  vector
;  esp+8  handler
KiConnectInterrupt:
	mov  eax, [esp + 4]
	add  eax, KE_IDT_IRQ_BASE
	
	mov  ecx, [esp + 8]
	
	push esi
	
	sidt fword [IDTR]
	mov  esi, [IDTR_base]

	push ecx
	push eax
	call KiWriteNewVector
	
	pop  esi
	retn 8
	

; @implemented
;
; KiWriteNewVector
;
;  Writes IDT vector
;  esp+4  idt number
;  esp+8  handler
;
KiWriteNewVector:
	push ecx
	
	mov  ecx, [esp+8]
	mov  word [esi + ecx*8], 0  ; offset
	mov  word [esi + ecx*8 + 2], 8  ; KE_GDT_CODE
	mov  word [esi + ecx*8 + 4], 1000111000000000b
	mov  word [esi + ecx*8 + 6], 0  ; offset
		
	pop  ecx
	
	jmp  KiWriteVector
	
extrn exDE_handler
extrn exDB_handler
extrn exBP_handler
extrn exOF_handler
extrn exBR_handler
extrn exUD_handler
extrn exNM_handler
extrn exDF_handler
extrn exTS_handler
extrn exNP_handler
extrn exSS_handler
extrn exGP_handler
extrn exPF_handler
extrn exMF_handler
extrn exAC_handler
extrn exMC_handler
extrn exXF_handler


; @implemented
;
; KiInitializeIdt
;
;  This function replaces all IDT vectors to new kernel handlers
;
KiInitializeIdt:
	push esi
	push ebx
	
	sidt fword [IDTR]
	mov  esi, [IDTR_base]
	
	pushfd
	cli
	
	invoke KiWriteVector, 0, exDE_handler
	invoke KiWriteVector, 1, exDB_handler
	invoke KiWriteVector, 3, exBP_handler
	invoke KiWriteVector, 4, exOF_handler
	invoke KiWriteVector, 5, exBR_handler
	invoke KiWriteVector, 6, exUD_handler
	invoke KiWriteVector, 7, exNM_handler
	invoke KiWriteVector, 8, exDF_handler
	invoke KiWriteVector, 10, exTS_handler
	invoke KiWriteVector, 11, exNP_handler
	invoke KiWriteVector, 12, exSS_handler
	invoke KiWriteVector, 13, exGP_handler
	invoke KiWriteVector, 14, exPF_handler
	invoke KiWriteVector, 16, exMF_handler
	invoke KiWriteVector, 17, exAC_handler
	invoke KiWriteVector, 18, exMC_handler
	invoke KiWriteVector, 19, exXF_handler
	invoke KiWriteVector, 0x20, irq0_handler
	invoke KiWriteVector, 0x21, irq1_handler
	invoke KiWriteNewVector, 0x26, FdIrq_handler	; IRQ6 - FD
	invoke KiWriteVector, 0x30, KiSystemCall
	
	popfd
	
	pop  ebx
	pop  esi
	retn

public KiSystemCall as '_KiSystemCall@0'

; @implemented
;
; KiSystemCall
;
;  Translates system call
;
KiSystemCall:
    pushad

    cmp  ax, 0
    jz   __syscall_print_string_regular
    cmp  ax, 1
    jz   __syscall_goto_nextline
    cmp  ax, 2
    jz   __syscall_print_bugcheck
    jmp  __syscall_invalid_syscall
    
    
__syscall_print_string_regular:
	mov  bl, 0
	jmp __syscall_print_string
    
__syscall_print_bugcheck:
	mov  dword [cursor], 0
	
	push es
	push gs
	pop  es
	
	push edi
	push ecx
	push eax
	
	xor  eax, eax
	mov  ah, bl
	xor  edi, edi
	mov  ecx, 80*25
	cld
	rep  stosw
	
	pop  eax
	pop  ecx
	pop  edi
	pop  es
	
__syscall_print_string:
    lodsb
    cmp  al, 13
    jz _nextline
    cmp  al, 10
    jz _nextline
    
    _regularchar:
      mov  edi, dword [cursor]
      mov  [gs:edi*2], al
      mov  [gs:edi*2+1], bl
      inc  dword [cursor]
      jmp _sps_quit
      
    _nextline:
      mov  ax, 1
      int  30h

	_sps_quit:

    .if dword [cursor],g,2000
      mov  dword [cursor], 0
    .endif

    test al, al
    jnz  __syscall_print_string
    jmp  __syscall_end


__syscall_goto_nextline:
    xor  edx, edx
    mov  eax, dword [cursor]
    mov  ebx, 80
    div  ebx
    mov  ebx, 80
    sub  ebx, edx
    add  dword [cursor], ebx

    .if dword [cursor],g,2000
      mov  dword [cursor], 0
    .endif

    jmp  __syscall_end


__syscall_invalid_syscall:
    halt
    ; stc
__syscall_end:
    popad
    iretd


public cursor
cursor dd 0
