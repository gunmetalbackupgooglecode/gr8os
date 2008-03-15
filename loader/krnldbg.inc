; ======================
;    Kernel debugger
; ======================


BOCHS_LOGGER_PORT = 0xE9

DbgPrintLn:
       push edx
       push eax

       mov  dx, BOCHS_LOGGER_PORT
       mov  al, 0dh
       out  dx, al
       mov  al, 0ah
       out  dx, al

       pop  eax
       pop  edx
       ret

;
; DbgPrintRaw saves all registers.
;

DbgPrintRaw:
       push esi
       push edx
       push eax
       mov  esi, [esp+16]

       mov  dx, BOCHS_LOGGER_PORT
       in   al, dx
       cmp  dl, al
       jnz  DPR20

  DPR10:
       lodsb
       .if  al, e, 13
	    mov  al, 13
	    out  dx, al
	    mov  al, 10
       .endif
       out  dx, al
       or   al, al
       jnz  DPR10

       mov  al, 0dh
       out  dx, al
       mov  al, 0ah
       out  dx, al

  DPR20:
       pop  eax
       pop  edx
       pop  esi
       ret  4

;
; WARNING! DbgPrint don't save any general registers except EBP, ESP.
;
DbgPrint:
       mov  ebx, [esp]
       add  esp, 4
       invoke sprintf, buffer
       invoke DbgPrintRaw, buffer
       add  esp, 4
       jmp  ebx


