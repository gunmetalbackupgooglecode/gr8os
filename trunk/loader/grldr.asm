;
; ===============================================================
; *************           GR8OS loader            ***************
; ===============================================================
;
; [c]oded by Great, 2007-2008
;

; макросы fasm
include 'auxmacro.inc'

; Базовый адрес загрузчика, 16-битный код
org 0x8000
use16

GRLDR_START:

db 'LDR'

start:
    jmp init

;
; First GDT (w/o paging)
;


GDTR:	 dw  0xFFFF
	 dd  GDT

	 dw ? ; padding

GDT:	 ; пустой дескриптор
	 dd 0, 0

	 ;
	 ; Формат дескриптора из GDT (в масштабе):
	 ;  [ LIMIT    |      BASE    | PDLSTYPE   GD0ALIMT | BASE ]
	 db  0FFh, 0FFh, 00h, 00h, 00h, 10011010b, 11001111b, 00	; код   (селектор = 8h)
	 db  0FFh, 0FFh, 00h, 00h, 00h, 10010010b, 11001111b, 00	; данные (селектор = 10h)
	 db  0FFh, 0FFh, 00h, 80h, 0Bh, 10010010b, 01000000b, 00	; видеобуфер (селектор = 18h)


buffer: rb 200

KGDT_R0_CODE = 0x08
KGDT_R0_DATA = 0x10
KGDT_VIDEO   = 0x18
KGDT_TSS     = 0x20
KGDT_PCB     = 0x28

;
; GRLDR requires:
;   BX = Starting Cluster Number of GRLDR
;   DL = INT 13 drive number we booted from
;   DS:SI -> the boot media's BPB
;   DS:DI -> argument structure
;   1000:0000 - entire FAT is loaded
;

struct SHARED
	ReadClusters   dd   ?
	ReadSectors    dd   ?
	SectorBase     dd   ?
ends

struct DIR_ENT
	Filename	db 11 dup(?)
	Attribute	db ?
	Reserved	db 10 dup(?)
	Time		dw 2 dup(?)
	StartCluster	dw ?
	FileSize	dd ?
ends

struct BPB
	Version 	db 8 dup (?)
	SectorSize	dw ?
	SectorsPerCluster db ?
	ReservedSectors dw ?
	NumberOfFats	db ?
	DirectoryEntries dw ?
	Sectors 	dw ?
	Media		db ?
	FatSectors	dw ?
	SectorsPerTrack dw ?
	Headsq		dw ?
	HiddenSectors	dd ?
	SectorsLong	dd ?
ends


BootBPB 	equ 0x7C03
ClusterBase	equ 0x7E00 + 8
Arguments	equ 0x7E00 + 11

GrldrStartCluster dw ?
DriveNumber	  db ?
BytesPerCluster   dw ?

macro ALIGN_DOWN x,y
{
    mov  x, y
}

init:
    ALIGN_DOWN ax, 3

    ; очистка экрана
    mov  ax,3
    int  10h

    ; иинициализация RM-сегментов и стека
    mov  ax, cs
    mov  ds, ax
    mov  es, ax

    xor  ax, ax
    mov  ss, ax

    mov  sp, 0x7c00
    mov  bp, sp

    ;
    ; Read the rest of the GRLDR
    ;

    mov  [GrldrStartCluster], bx
    mov  [DriveNumber], dl

;    mov  [Arguments + SHARED.ReadSectors


    ;
    ; Read file allocation table at 0x2000
    ;

    ; (es:bx) -> destination buffer
    xor  ax, ax
    mov  es, ax
    mov  bx, 0x2000

    ; sector #1
    mov  word [Arguments + SHARED.SectorBase],	 1
    mov  word [Arguments + SHARED.SectorBase+2], 0

    ; (al) -> number of sectors to read [entire FAT will be loaded]
    mov  al, byte [BootBPB + BPB.FatSectors]

    ; read FAT
    call ReadSectors

    ; Calculate number of bytes in cluster.
    movzx  ax, byte [BootBPB + BPB.SectorsPerCluster]
    mul  [BootBPB + BPB.SectorSize]
    mov  [BytesPerCluster], ax

    ; Start writing at (start + CLUSTER_SIZE_IN_BYTES)
    mov  bx, GRLDR_START
    add  bx, ax

    ; Parse FAT
    mov  cx, [GrldrStartCluster]
    xor  ax, ax
    mov  es, ax

    ; Read the whole GRLDR
    call ReadClusterChain

    ; Find & load kernel
    jmp LoadKernel


;
; (cx) = Start cluster number
; (es:bx) = Destination buffer
; 0200:0000 - entire fat is loaded
;
ReadClusterChain:
    call GetFatCluster
    cmp  ax, 0xFF8
    jae  end_read
    push ax ; save
    mov  dx, ax        ; (dx) -> starting cluster number
    mov  al, 1	       ; (al) -> number of clusters to read
		       ; (es:bx) -> destination buffer
    push bx ; save

    ; segment border exceeded.
    test bx, bx
    jnz  @F

    ; add es, 0x1000
    push di
    push es
    mov  di, sp
    add  word [di], 0x1000
    pop  es
    pop  di

    @@:

    call ReadClusters
    jc	 HaltProcessor

    pop  bx
    add  bx, [BytesPerCluster]
    pop  cx   ; next cluster
    jmp  ReadClusterChain

  end_read:
    retn
; endp


;
; Stop execution of the current processor
;
HaltProcessor:
    cli
    hlt
; endp


;
; Read sectors from disk
;
ReadSectors:
    push ds
    mov  di, 0x7C0
    mov  ds, di
    call far dword [cs:Arguments + SHARED.ReadSectors]
    pop  ds
    retn
; endp


;
; Read clusters from disk
;
ReadClusters:
    push ds
    mov  di, 0x7c0
    mov  ds, di
    call far dword [cs:Arguments + SHARED.ReadClusters]
    pop  ds
    retn
; endp


;
; CX = Number of cluster
; 02000 = Entire FAT loaded
;
; Return value:  AX = cluster in chain
;
GetFatCluster:
    push ds
    push 0x200
    pop  ds

    ; 0200:0000 - fat
    test cx, 1
    jnz  cluster_odd

    ; cluster number is even
    mov  ax, cx
    mov  cx, 3
    mul  cx
    shr  ax, 1
    mov  si, ax
    ; si = (cx*3)/2

    ; Read FAT
    mov  ax, [ds:si]
    and  ax, 0xFFF
    pop  ds
    retn

  cluster_odd:
    ; cluster number is odd
    mov  ax, cx
    inc  ax
    mov  cx, 3
    mul  cx
    shr  ax, 1
    dec  ax
    dec  ax
    mov  si, ax
    ; si = (cx + 1)*3/2 - 2

    mov  ax, [ds:si]
    shr  ax, 4
    pop  ds
    retn

; endp

;
; (dx) -> ASCIIZ string with file name to look for
; 2000:0000 -> entire directory entry loaded
;
; Returns:   AX = pointer to DIR_ENT record of this file or 0 if no file was found
;
FindFile:
    xor  bx, bx
    mov  cx, [BootBPB + BPB.DirectoryEntries]
    push es
    push 0x1000
    pop  es
@@:
    mov  di, bx
    push cx
    mov  cx, 11
    mov  si, dx
    repe cmpsb
    pop  cx
    jz	 ffend

    add  bx,sizeof.DIR_ENT
    loop @B

    ; not found
    pop  es
    xor  ax, ax
    retn

ffend:
    ; found
    ;pop  es
    add  sp, 2
    mov  ax, bx
    retn
;endp



BootErrPrint:
    LODSB			  ; Get next character
    or	  al,al
    jz	  BEdone

    MOV     AH,14		    ; Write teletype
    MOV     BX,7		    ; Attribute
    INT     10H 		    ; Print it
    jmp   BootErrPrint
BEdone:
    ret
;endp

kernel_not_found:
    mov  si, MSG_NO_KERNEL
    call BootErrPrint
    call HaltProcessor


KERNELNAME     db 'KERNEL  EXE'
MSG_NO_KERNEL  db 'No kernel file found on the disk',0

KernelSize     dd ?

KernelStart equ 0xB000

LoadKernel:

    ;Find kernel file in the root directory
    mov  dx, KERNELNAME
    call FindFile

    test ax,ax
    jz	 kernel_not_found

    mov  di, ax

    mov  cx, [es:di + DIR_ENT.StartCluster]
    mov  dx, word [es:di + DIR_ENT.FileSize]
    mov  word [KernelSize], dx
    mov  dx, word [es:di + DIR_ENT.FileSize + 2]
    mov  word [KernelSize + 2], dx

    push cx    ; save

    ; Read first cluster of the file
    push 0	   ; (es:bx) -> destination buffer
    pop  es

    mov  dx, cx        ; (dx) -> starting cluster number
    mov  al, 1	       ; (al) -> number of clusters to read
    mov  bx, 0xB000
    call ReadClusters
    jc	 HaltProcessor

    pop  cx   ; restore

    ; Read the rest of the file.
    mov  bx, 0xB200
    call ReadClusterChain

    ; kernel loaded.


    ; открываем адресную линию A20
    in	 al, 92h
    or	 al, 2
    out  92h, al

    ; запрет всех прерываний
    cli
    in	 al, 70h
    or	 al, 80h
    out  70h, al  ; запрет NMI

;    mov  ax, 0x2000
;    mov  ds, ax
;    xor  ax, ax
;    mov  es, ax
;    mov  si, GDTR-LDR_BASE
;    xor  di, di
;    mov  cx, buffer-GDTR
;    cld
;    rep  movsb

    ; загрузка GDTR
    lgdt fword [GDTR]

    ; переключение в PM
    mov  eax, cr0
    or	 al, 1
    mov  cr0, eax

    ; загружаем новый селектор в CS
;   jmp  KGDT_R0_CODE:(LDR_BASE + PROTECTED_ENTRY)
    db 0x66
    db 0xEA
    dd (PROTECTED_ENTRY)
    dw KGDT_R0_CODE


; =============================
;    Код защищенного режима
; =============================
use32
;org $+LDR_BASE

PROTECTED_ENTRY:
    ; мы в PM, инициализируем селекторы 32-битных сегментов
    mov  ax, KGDT_R0_DATA  ; DATA
    mov  ds, ax
    mov  ss, ax
    mov  es, ax
    mov  ax, KGDT_VIDEO  ; VIDEO
    mov  gs, ax
    xor  ax, ax
    mov  fs, ax

    ;
    ; Calculate memory size
    ;

    mov  eax, 12345678h
    mov  esi, 00200000h    ; Start from 2 megs
    mov  ecx, 2 	     ; Initial: 2 megs

 @@:
    mov  ebx, [esi]
      mov  [esi], eax
      cmp  [esi], eax
      jnz  stop
    mov  [esi], ebx
    add  esi, 00100000h
    inc  ecx
    jmp  @B

 stop:
    shl  ecx, 8    ; in pages
    mov  [PhysicalMemoryPages and 0xFFFFFF], ecx

    ;
    ; Enable paging
    ;

    invoke EnablePaging, ContinueLoading

include 'paging.inc'

;
; Second GDT for paging
;

GDTR2:	  dw  (LoaderBlock-GDT2)-1
GDT2:	  dd  GDT2
	 dw  0
	 dw  0

	 ;  [ LIMIT    |      BASE    | PDLSTYPE   GD0ALIMT | BASE ]
	 db  0FFh, 0FFh, 00h, 00h, 00h, 10011010b, 11001111b, 00h	 ; код   (селектор = 8h)
	 db  0FFh, 0FFh, 00h, 00h, 00h, 10010010b, 11001111b, 00h	 ; данные (селектор = 10h)
	 db  0FFh, 0FFh, 00h, 80h, 0Bh, 10010010b, 01000000b, 80h	 ; видеобуфер (селектор = 18h)

	 ; TSSs
	 dw SIZEOF_TSS-1, (SystemTss and 0xFFFFFF)
	 ;  [ LIMIT    |      BASE    | PDLSTYPE   GD0ALIMT | BASE ]
	 db			   00h, 10001001b, 00000000b, 80h	 ; System TSS (sel = 20h)

	 dw SIZEOF_PCB-1
	 dw Pcb0 and 0xFFFFFF
	 db			   00h, 10010010b, 11000000b, 80h	 ; Processor Control Block (sel = 28h)

	 dd 0, 0,0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
	 dd 0, 0,0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
	 dd 0, 0,0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
	 dd 0, 0,0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
	 dd 0, 0,0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
	 dd 0, 0,0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
	 dd 0, 0,0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
	 dd 0, 0,0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
	 dd 0, 0,0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
	 dd 0, 0,0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
	 dd 0, 0,0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
	 dd 0, 0,0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
	 dd 0, 0,0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
	 dd 0, 0,0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
	 dd 0, 0,0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
	 dd 0, 0,0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
	 dd 0, 0,0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
	 dd 0, 0,0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
	 dd 0, 0,0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
	 dd 0, 0,0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
	 dd 0, 0,0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
	 dd 0, 0,0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0

LoaderBlock:
PhysicalMemoryPages dd 0

ContinueLoading:

buffer equ (buffer or 0x80000000)

    mov  [Pcb0 + PCB.Self], Pcb0

    ; Load IDTR
    lidt fword [IDTR]

    call  DbgPrintLn
    ccall DbgPrint, "LDR: Protected mode initialized successfully."
    call  DbgPrintLn

    mov  eax, [PhysicalMemoryPages]
    mov  ebx, eax
    shr  ebx, 8

    ccall DbgPrint, "LDR: Booting with %d pages of memory (%d megs) on board", eax, ebx
    call  DbgPrintLn

    mov  dx, 0FFFFh
    mov  bx, 2820h
    call redirect_IRQ

    call ClearDpcQueue
    invoke InsertQueueDpc, my_dpc, msg

    call  DbgPrintLn
    ccall DbgPrint, "LDR: Initializing TSSs.."

    ; Load initial task1 context
    mov  edx, SystemTss
    xor  eax, eax
    call FillTss

    mov  ax, KGDT_TSS
    ltr  ax

    ;
    ; Init PICs
    ;

    call InitializeLApic
    call InitializeIOApic

    ; Enable interrupts.
    in	 al, 70h
    and  al, 7Fh
    out  70h, al
    ;sti

    ; Initialize memory management
    ;call MmInitSystem


;TIMER_FREQ = 1193180
;
;    ; Посчитать делитель
;    xor  edx, edx
;    mov  eax, TIMER_FREQ
;    mov  ebx, 200         ; 1 KHz
;    div  ebx
;    mov  ecx, eax
;
;    ; Запретить счет второго канала (GATE2 = 0)
;    mov  dx, 61h
;    in   al, dx
;    and  al, 11111100b
;    out  dx, al
;
;    ; Сконфигурировать второй канал:
;    mov  al, 10110100b  ;канал=2, режим=LSB/MSB, счетчик=одновибратор, счет=двоичный счет
;    out  43h, al
;    jmp $+2
;
;    mov  ax, cx    ; divisor
;    out  42h, al   ; LSB
;    jmp $+2
;    mov  al, ah    ; MSB
;    out  42h, al
;    jmp $+2
;
;    ; Разрешить счет второго канала (GATE2 = 0)
;    mov  dx, 61h
;    in   al, dx
;    or   al, 1
;    out  dx, al
;
;lp:
;    mov  ecx, 1
;
;    ; ждем счета
;@@: in   al, dx
;    test al, 100000b    ; 5й бит системного порта: T2O (окончание счета второго канала)
;    jnz  @B
;
;    loop lp


    ; Calculate number of pages need for kernel.
    mov  eax, [0x80000000 + KernelSize]
    add  eax, 0x1000
    shr  eax, 12

    ; Map kernel
    invoke MiMapPhysicalPages, 0x80100000, KernelStart, eax

    mov  eax, 0x80100000
    add  eax, [eax + IMAGE_DOS_HEADER.e_lfanew]
    add  eax, 4 + sizeof.IMAGE_FILE_HEADER
    mov  eax, [eax + IMAGE_OPTIONAL_HEADER.AddressOfEntryPoint]
    add  eax, 0x80100000

    push LoaderBlock
    call eax


my_dpc:
    xor  ax,ax
    mov  esi, dword [esp+4]
    int  30h
    ret 4

    msg  db '[ ]  Starting GR8OS ...',0

; Memory Manager
include 'mm.inc'

; Обработчики IRQ
include 'irq.inc'

; Обработчики программных прерываний
include 'interrupts.inc'

; Обработчики исключений
include 'except.inc'

; Переключение в реальный режим
;include 'realmode.inc'

; Run Time
buf rb 32

;buffer rb 512
include 'runtime.inc'

; Отладчик
include 'krnldbg.inc'

; Ядро
include 'kernel.inc'

; TSS
include 'tss.inc'

; Tasks
include 'tasks.inc'


;===========================
;        Данные
;===========================

; Таблица преобразования печатаемых скан-кодов в ASCII
;ascii    db 0,0,'1234567890-=',0,0,'qwertyuiop[]',0,0,'asdfghjkl;',"'`",0,'\zxcvbnm,./',0,'*',0,' ',0, 0,0,0,0,0,0,0,0,0,0, 0,0, '789-456+1230.', 0,0
;asc_shft db 0,0,'!@#$%^&*()_+',0,0,'QWERTYUIOP{}',0,0,'ASDFGHJKL:"~',0,'|ZXCVBNM<>?',0,'*',0,' ',0, 0,0,0,0,0,0,0,0,0,0, 0,0, '789-456+1230.', 0,0

; байт состояния клавиатуры.
; битовая маска (;;; отмечено нереализованное):
;  бит 0  -  Shift
;  бит 1  -  какой Shift (0 - левый, 1 - правый)
;  бит 2  -  Ctrl
;  бит 3  -  Alt
;  бит 4  -  Num Lock
;  бит 5  -  Caps Lock
;  бит 6  -  Scroll Lock
;  бит 7 зарезервирован
kbrd_status_byte db 0

; Сообщение
;string   db '  Switched to ProtectedMode. Press <Esc> to clear display', 0

; Позиция курсора
cursor	 dd 0

; Interrupt Descriptor Table
IDT:
	 dw exDE_handler and 0xFFFFFF,08h, 1000111000000000b, 8000h ; 0
	 dw exDB_handler and 0xFFFFFF,08h, 1000111000000000b, 8000h ; 1
	 dd 0,0 ; 2
	 dw exBP_handler and 0xFFFFFF,08h, 1000111000000000b, 8000h ; 3
	 dw exOF_handler and 0xFFFFFF,08h, 1000111000000000b, 8000h ; 4
	 dw exBR_handler and 0xFFFFFF,08h, 1000111000000000b, 8000h ; 5
	 dw exUD_handler and 0xFFFFFF,08h, 1000111000000000b, 8000h ; 6
	 dw exNM_handler and 0xFFFFFF,08h, 1000111000000000b, 8000h ; 7
	 dw exDF_handler and 0xFFFFFF,08h, 1000111000000000b, 8000h ; 8
	 dd 0,0 ; 9
	 dw exTS_handler and 0xFFFFFF,08h, 1000111000000000b, 8000h ; 10
	 dw exNP_handler and 0xFFFFFF,08h, 1000111000000000b, 8000h ; 11
	 dw exSS_handler and 0xFFFFFF,08h, 1000111000000000b, 8000h ; 12
	 dw exGP_handler and 0xFFFFFF,08h, 1000111000000000b, 8000h ; 13  #GP
	 dw exPF_handler and 0xFFFFFF,08h, 1000111000000000b, 8000h ; 14
	 dd 0,0 ; 15
	 dw exMF_handler and 0xFFFFFF,08h, 1000111000000000b, 8000h ; 16
	 dw exAC_handler and 0xFFFFFF,08h, 1000111000000000b, 8000h ; 17
	 dw exMC_handler and 0xFFFFFF,08h, 1000111000000000b, 8000h ; 18
	 dw exXF_handler and 0xFFFFFF,08h, 1000111000000000b, 8000h ; 19
	 dd 0,0 ; 20
	 dd 0,0 ; 21
	 dd 0,0 ; 22
	 dd 0,0 ; 23
	 dd 0,0 ; 24
	 dd 0,0 ; 25
	 dd 0,0 ; 26
	 dd 0,0 ; 27
	 dd 0,0 ; 28
	 dd 0,0 ; 29
	 dd 0,0 ; 30
	 dd 0,0 ; 31
	 dw int8_handler and 0xFFFFFF,08h, 1000111000000000b, 8000h ; 32  IRQ 0 - системный таймер
	 dw int9_handler and 0xFFFFFF,08h, 1000111000000000b, 8000h ; 33  IRQ 1 - клавиатура
	 ;   2    3    4    5    6    7
	 dd 0,0, 0,0, 0,0, 0,0, 0,0, 0,0
	 ;   8,   9,  10,  11,  12,  13   14   15
	 dd 0,0, 0,0, 0,0, 0,0, 0,0, 0,0, 0,0, 0,0

	 dw syscall_handler and 0xFFFFFF,08h, 1000111000000000b, 8000h	 ; 30h (48)  System Service

;         db  00h, 00h,  48h, 00h, 00h,  10000101b, 01000000b, 00        ; Task Gate for TSS2 48h (sel = 50h)

	 dd 0, 0, 0, 0, 0, 0,0, 0, 0,0, 0, 0,0, 0, 0,0, 0, 0,0, 0, 0,0, 0, 0,0, 0, 0,0, 0, 0,0, 0, 0,0, 0, 0,0, 0, 0,0, 0, 0,0, 0, 0,0, 0, 0,0, 0, 0


    IDT_size	 equ $-IDT
    IDTR	 dw IDT_size-1
		 dd IDT

;
; -=[ EOF ]=-
;
