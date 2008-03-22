;
; =============================================
;  Обработчики IRQ защищенного режима
; =============================================
;

use32

;
; IRQ 0 обработчик - системный таймер
;

int8_handler:
    push fs
    push gs
    push es
    push ds
    pushad

    mov  al, 20h
    out  020h, al
    out  0a0h, al

    call ProcessDpcQueue

    popad
    pop  ds
    pop  es
    pop  gs
    pop  fs
    iretd

;
; IRQ 1 обработчик - клавиатура
;

int9_handler:
    push ax
    in	 al, 061h
    or	 al, 80
    out  061h, al
    xor  al, 80
    out  061h, al

    mov  al, 20h
    out  020h, al
    out  0a0h, al
    pop  ax
    iretd

;
; APIC
;

APIC_BASE_MSR = 0x1b

IOAPIC_BASE_P = 0xFEC00000
IOAPIC_BASE   = 0xFFFE0000

IOAPIC_IDX  = IOAPIC_BASE + 0x0000  ; 8
IOAPIC_DATA = IOAPIC_BASE + 0x0010  ; 32
IOAPIC_PIN  = IOAPIC_BASE + 0x0020  ; 8
IOAPIC_EOI  = IOAPIC_BASE + 0x0040  ; 8

IOAPIC_ID      = 00h
IOAPIC_VER     = 01h
IOAPIC_ARBID   = 02h
IOAPIC_BOOTCFG = 03h
IOAPIC_RDRMAP  = 10h

macro IOAPIC_REG reg
{
	mov  dword [IOAPIC_IDX], reg
	mov  eax, [IOAPIC_DATA]
}

InitializeIOApic:
	; Map I/O APIC registers to virtual memory
	invoke MiMapPhysicalPages, IOAPIC_BASE, IOAPIC_BASE_P, 1

	IOAPIC_REG IOAPIC_ID
	mov  edx, eax
	IOAPIC_REG IOAPIC_VER
	ccall DbgPrint, "I/O APIC: ID=%08x, VER=%08x", edx, eax
	retn

redirect_IRQ:
; BX = { BL = Начало для IRQ 0..7, BH = начало для IRQ 8..15 }
; DX = Маска прерываний IRQ ( DL - для IRQ 0..7, DH - IRQ 8..15 )

	; APIC Off
	mov	ecx, APIC_BASE_MSR
	rdmsr
	or	eax, 1000b
	wrmsr

	mov	al,11h
	out	0a0h,al
	out	20h,al

	mov	al,bh
	out	0a1h,al
	mov	al,bl
	out	21h,al

	mov	al,02
	out	0a1h,al
	mov	al,04
	out	21h,al

	mov	al,01
	out	0a1h,al
	out	21h,al

	mov	al,dh
	out	0a1h,al
	mov	al,dl
	out	21h,al

	; APIC On
	rdmsr
	and	eax, not 1000b
	wrmsr

	ret


;
; Local APIC
;

LAPIC_BASE_P   = 0xFEE00000
LAPIC_BASE     = 0xFFFF0000
APIC_LIMT =	   0x0FFF

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

InitializeLApic:
	; Map Local APIC registers to virtual memory
	invoke MiMapPhysicalPages, LAPIC_BASE, LAPIC_BASE_P, 1

	xor  eax, eax
	inc  eax
	cpuid
	test edx, (1 shl 9)   ; 9 bit - APIC on chip
	jnz  @F

	invoke DbgPrintRaw, "No APIC on board"
	int3

	xor  eax, eax
	retn
  @@:
	; APIC on chip. Initialize it
	; APIC registers are mapped to physical memory
	; range FEE00000 - FEE00FFF
	invoke DbgPrintRaw, "APIC present"

	; test apic presence
	mov  ecx, APIC_BASE_MSR
	rdmsr
	ccall DbgPrint, "APIC_BASE_MSR: %08x`%08x", edx, eax


	ccall DbgPrint, "ID: %08x",  dword [APIC_ID]
	ccall DbgPrint, "VER: %08x", dword [APIC_VER]
	ccall DbgPrint, "SPINTV: %08x", dword [APIC_SPINTV]

	mov  eax, [APIC_SPINTV]
	or   eax, (1 shl 8)
	mov  [APIC_SPINTV], eax

	ccall DbgPrint, "SPINTV: %08x", dword [APIC_SPINTV]


;        cli
;        mov  dword [APIC_INITCNT], 0xFFFFFFFF
;        mov  dword [APIC_LVTTMR],  0x20012
;        mov  dword [APIC_DIVCONF], 1
;        sti

	retn







