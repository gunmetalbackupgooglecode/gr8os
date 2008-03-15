;
; ==================================
;    Организация многозадачности
; ==================================
;


;
; Processor control block
;

struct PCB

    ExceptionList    dd ?
    CurrentException dd ?
    CurrentThread    dd ?
    QuantumLeft      dw ?
    Self	     dd ?
ends

SIZEOF_PCB = sizeof.PCB

; Processor control block for the first processor.
Pcb0   PCB

