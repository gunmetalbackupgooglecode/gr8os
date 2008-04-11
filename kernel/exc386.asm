format MS COFF

include 'inc/common.inc'

struct TSS32
  LinkTSS dw ?
          dw ?
  Esp0    dd ?
  Ss0     dw ?
          dw ?
  Esp1    dd ?
  Ss1     dw ?
          dw ?
  Esp2    dd ?
  Ss2     dw ?
          dw ?
  Cr3     dd ?
  Eip     dd ?
  Eflags  dd ?
  Eax     dd ?
  Ecx     dd ?
  Edx     dd ?
  Ebx     dd ?
  Esp     dd ?
  Ebp     dd ?
  Esi     dd ?
  Edi     dd ?
  Es      dw ?
          dw ?
  Cs      dw ?
          dw ?
  Ss      dw ?
          dw ?
  Ds      dw ?
          dw ?
  Fs      dw ?
          dw ?
  Gs      dw ?
          dw ?
  Ldtr    dw ?
          dw ?
          dw ?
  Iomap   dw ?
ends          

 
public SpecialDFTss as '_SpecialDFTss'
SpecialDFTss:
  LinkTSS  dw 0
           dw 0
  Esp0     dd 0x80007c00
  Ss0      dw KGDT_R0_DATA
           dw 0
  Esp1     dd 0
  Ss1      dw 0
           dw 0
  Esp2     dd 0
  Ss2      dw 0
           dw 0
  PDBR     dd KE_PG_PDE_START
  Eip      dd exDF_handler
  Eflags   dd 2
  _Eax     dd 0
  _Ecx     dd 0
  _Edx     dd 0
  _Ebx     dd 0
  _Esp     dd 0x80007c00
  _Ebp     dd 0x80007c00
  _Esi     dd 0
  _Edi     dd 0
  _Es      dw KGDT_R0_DATA
           dw 0
  _Cs      dw KGDT_R0_CODE
           dw 0
  _Ss      dw KGDT_R0_DATA
           dw 0
  _Ds      dw KGDT_R0_DATA
           dw 0
  _Fs      dw KGDT_PCB
           dw 0
  _Gs      dw KGDT_VIDEO
           dw 0
  Ldtr     dw 0
           dw 0
           dw 0
  Iomap    dw 0
           
           
public SpecialSFTss as '_SpecialSFTss'
SpecialSFTss:
  xLinkTSS  dw 0
           dw 0
  xEsp0     dd 0x802EF000-4
  xSs0      dw KGDT_R0_DATA
           dw 0
  xEsp1     dd 0
  xSs1      dw 0
           dw 0
  xEsp2     dd 0
  xSs2      dw 0
           dw 0
  xPDBR     dd KE_PG_PDE_START
  xEip      dd exSS_handler
  xEflags   dd 2
  x_Eax     dd 0
  x_Ecx     dd 0
  x_Edx     dd 0
  x_Ebx     dd 0
  x_Esp     dd 0x802EF000-4
  x_Ebp     dd 0x802EF000-4
  x_Esi     dd 0
  x_Edi     dd 0
  x_Es      dw KGDT_R0_DATA
           dw 0
  x_Cs      dw KGDT_R0_CODE
           dw 0
  x_Ss      dw KGDT_R0_DATA
           dw 0
  x_Ds      dw KGDT_R0_DATA
           dw 0
  x_Fs      dw KGDT_PCB
           dw 0
  x_Gs      dw KGDT_VIDEO
           dw 0
  xLdtr     dw 0
           dw 0
           dw 0
  xIomap    dw 0

public exDE_handler
public exDB_handler
public exBP_handler
public exOF_handler
public exBR_handler
public exUD_handler
public exNM_handler
public exDF_handler
public exTS_handler
public exNP_handler
public exSS_handler
public exGP_handler
public exPF_handler
public exMF_handler
public exAC_handler
public exMC_handler
public exXF_handler
extrn cursor
extrn '_sprintf' as sprintf
extrn tempbuffer
extrn '_KiDebugPrintRaw@4' as DbgPrintRaw
extrn '_KeDispatchException@8' as KeDispatchException

exDE_handler:
	sti
	
    KiMakeContextFrame
    
    mov eax, esp	; PCONTEXT_FRAME
    
    push 0							; parameters[3]
    push 0							; parameters[2]
    push 0							; parameters[1]
    push 0							; parameters[0]
    push 0							; number parameters
    push [eax + CONTEXT.Eip]		; exception address
    push 0							; exception arguments
    push 0							; flags
    push EXCEPTION_DIVISION_BY_ZERO ; exception code
    
    mov  ecx, esp	; PEXCEPTION_ARGUMENTS
    
    push eax
    push ecx
    call KeDispatchException	; KeDispatchException (ExceptionArguments, ContextFrame)
    
    add esp, 36
    
    KiRestoreContextFrame
    iretd
    

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
    push -1
    push 0
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
    mov  [esp], ebp
    mov  ebp, esp
   
	push fs
    push gs
    push es
    push ds
    pushad
       
    mov  eax, cr2     ; replace error code with CR2 value
    mov  ecx, [ebp+4]

extrn '_MmAccessFault@8' as MmAccessFault

    invoke MmAccessFault, eax, ecx
    
    popad
    pop  ds
    pop  es
    pop  gs
    pop  fs
    
    pop  ebp
    
    iretd

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
    push fs       ; 24 bytes
    pushad        ; 32 bytes
    push dword [faulting_message]
    
    push dword [faulting_message]
    call DbgPrintRaw
    
;    jmp $

    invoke sprintf, tempbuffer, ctx   ; 8 bytes
    add  esp, 72

    invoke DbgPrintRaw, tempbuffer
    
    mov  esi, tempbuffer
    mov  ax, 2
    mov  bl, 75
    int  30h

    halt

    iretd

ctx db 13, \
	   '   *** FATAL EXCEPTION: %s ***    ', 13, \
       '    Context dump:    ', 13, \
       '    EDI = %08x     ESI = %08x    ', 10, \
       '    EBP = %08x     ESP = %08x    ', 10, \
       '    EBX = %08x     EDX = %08x    ', 10, \
       '    ECX = %08x     EAX = %08x    ', 10, \
       '     FS =     %04x      GS =     %04x    ', 10, \
       '     ES =     %04x      DS =     %04x    ', 10, \
       '     SS =     %04x      CS =     %04x    ', 10, \
       '    Faulting EIP: %08x    ', 10, \
       '    Exception error code: %08x    ', 10, \
       0

_derr db 'DIVIDE ERROR', 0              ; 0
_dbg  db 'DEBUGGING', 0                 ; 1
_bpt  db 'BREAKPOINT', 0                ; 3
_ovf  db 'OVERFLOW', 0                  ; 4
_brg  db 'BOUND RANGES EXCEEDED', 0     ; 5
_ud   db 'UNDEFINED OPERATION CODE', 0  ; 6
_nmc  db 'NO MATH COPROCESSOR', 0       ; 7
_dbf  db 'DOUBLE FAULT', 0              ; 8
_tss  db 'INVALID TSS', 0               ; A
_snp  db 'SEGMENT NOT PRESENT', 0       ; B
_sft  db 'STACK FAULT', 0               ; C
_gpf  db 'GENERAL PROTECTION FAULT', 0  ; D
_pft  db 'PAGE FAULT', 0                ; E
_mfp  db 'MATH FLOATING POINT ERROR', 0 ; 10
_ach  db 'ALIGNMENT CHECK ERROR', 0     ; 11
_mch  db 'MACHINE CHECK ERROR', 0       ; 12
_fpx  db 'FLOATING POINT SIMD EXCEPTION', 0 ; 13


faulting_message dd 0

