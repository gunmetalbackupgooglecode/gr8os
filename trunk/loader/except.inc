;
; =============================================
;   Обработчики исключений защищенного режима
; =============================================
;

use32

exDE_handler:
    push dword 0
    mov  dword [faulting_message], _derr
    jmp  general_exception

exDB_handler:
    push dword 0
    mov  dword [faulting_message], _dbg
    jmp  general_exception

exBP_handler:
    cli
    jmp $
    sti
    iretd

exOF_handler:
    push dword 0
    mov  dword [faulting_message], _ovf
    jmp  general_exception

exBR_handler:
    push dword 0
    mov  dword [faulting_message], _brg
    jmp  general_exception

exUD_handler:
    push dword 0
    mov  dword [faulting_message], _ud
    jmp  general_exception

exNM_handler:
    push dword 0
    mov  dword [faulting_message], _nmc
    jmp  general_exception

exDF_handler:
    ;pop  eax
    mov  dword [faulting_message], _dbf
    jmp  general_exception

exTS_handler:
    ;pop  eax
    mov  dword [faulting_message], _tss
    jmp  general_exception

exNP_handler:
    ;pop  eax
    mov  dword [faulting_message], _snp
    jmp  general_exception

exSS_handler:
    ;pop  eax
    mov  dword [faulting_message], _sft
    jmp  general_exception

exGP_handler:
    ;pop  eax     ; Error Code
    mov  dword [faulting_message], _gpf
    jmp  general_exception

exPF_handler:
    pop  eax	 ; Error Code
    mov  eax, cr2     ; replace error code with CR2 value
    push eax
    mov  dword [faulting_message], _pft
    jmp  general_exception

exMF_handler:
    push dword 0
    mov  dword [faulting_message], _nmc
    jmp  general_exception

exAC_handler:
    ;pop  eax     ; Error Code
    mov  dword [faulting_message], _ach
    jmp  general_exception

exMC_handler:
    push dword 0
    mov  dword [faulting_message], _mch
    jmp  general_exception

exXF_handler:
    push dword 0
    mov  dword [faulting_message], _fpx
    jmp  general_exception

general_exception:
    mov  dword [cursor], 240

    push dword [esp+4]
    push cs
    push ss
    push ds
    push es
    push gs
    push fs	  ; 24 bytes
    pushad	  ; 32 bytes
    push dword [faulting_message]

    invoke sprintf, buffer, ctx   ; 8 bytes
    add  esp, 72

    invoke DbgPrintRaw, buffer

    cli
    hlt

    mov  esi, buffer
    xor  ax, ax
    int  30h

    halt

    iretd

ctx db '   *** FATAL EXCEPTION: %s ***    ', 13, \
       '    Context dump:    ', 13, \
       '    EDI = %08x     ESI = %08x    ', 13, \
       '    EBP = %08x     ESP = %08x    ', 13, \
       '    EBX = %08x     EDX = %08x    ', 13, \
       '    ECX = %08x     EAX = %08x    ', 13, \
       '     FS =     %04x      GS =     %04x    ', 13, \
       '     ES =     %04x      DS =     %04x    ', 13, \
       '     SS =     %04x      CS =     %04x    ', 13, \
       '    Faulting EIP: %08x    ', 13, \
       '    Exception error code: %08x    ', 13, \
       0

_derr db 'DIVIDE ERROR', 0		; 0
_dbg  db 'DEBUGGING', 0 		; 1
_bpt  db 'BREAKPOINT', 0		; 3
_ovf  db 'OVERFLOW', 0			; 4
_brg  db 'BOUND RANGES EXCEEDED', 0	; 5
_ud   db 'UNDEFINED OPERATION CODE', 0	; 6
_nmc  db 'NO MATH COPROCESSOR', 0	; 7
_dbf  db 'DOUBLE FAULT', 0		; 8
_tss  db 'INVALID TSS', 0		; A
_snp  db 'SEGMENT NOT PRESENT', 0	; B
_sft  db 'STACK FAULT', 0		; C
_gpf  db 'GENERAL PROTECTION FAULT', 0	; D
_pft  db 'PAGE FAULT', 0		; E
_mfp  db 'MATH FLOATING POINT ERROR', 0 ; 10
_ach  db 'ALIGNMENT CHECK ERROR', 0	; 11
_mch  db 'MACHINE CHECK ERROR', 0	; 12
_fpx  db 'FLOATING POINT SIMD EXCEPTION', 0 ; 13


faulting_message dd 0




