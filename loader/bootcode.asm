include 'macro/struct.inc'

DISPLAY 'Compiling boot sector start-up code',13,10

struct DIR_ENT
	Filename	db 11 dup(?)
	Attribute	db ?
	Reserved	db 10 dup(?)
	Time		dw 2 dup(?)
	StartCluster	dw ?
	FileSize	dd ?
ends

;
; This is the structure used to pass all shared data between the boot sector
; and GRLDR.
;

struct SHARED
	ReadClusters		dd	?		; function pointer
	ReadSectors		dd	?		; function pointer
	SectorBase		dd	?		; starting sector
ends

SectorSize	equ	512		; sector size

BootSeg equ 07c0h
DirSeg	equ 1000h
NtLdrSeg equ 0800h

	; NOT  'jmp short start'  !!
	jmp near start

Version 		db	"GR8OS1.0"
BPB:
BytesPerSector		dw	SectorSize	; Size of a physical sector
SectorsPerCluster	db	8		; Sectors per allocation unit
ReservedSectors 	dw	1		; Number of reserved sectors
Fats			db	2		; Number of fats
DirectoryEntries	dw	512		; Number of directory entries
Sectors 		dw	4*17*305-1	; No. of sectors - no. of hidden sectors
Media			db	0F8H		; Media byte
FatSectors		dw	8		; Number of fat sectors
SectorsPerTrack 	dw	17		; Sectors per track
Heads			dw	4		; Number of surfaces
HiddenSectors		dd	1		; Number of hidden sectors
SectorsLong		dd	0		; Number of sectors iff Sectors = 0

DriveNumber	db	80h		; Physical drive number (0 or 80h)
CurrentHead	db	?		; Unitialized

Signature	db	41		; Signature Byte for bootsector
BootID		dd	0xDEADC0DE		 ; Boot ID field.
Boot_Vol_Label	db	11 dup (?)
Boot_System_ID	db	'FAT     '  ;"FAT     " or "OTHER_FS"

CurrentTrack	equ   BootSectorEnd + 4  ; current track
CurrentSector	equ   BootSectorEnd + 6  ; current sector
SectorCount	equ   BootSectorEnd + 7  ; number of sectors to read
ClusterBase	equ   BootSectorEnd + 8  ; first sector of cluster # 2
Retries 	equ   BootSectorEnd + 10
Arguments	equ   BootSectorEnd + 11 ; structure passed to GRLDR


start:
	xor	ax, ax			 ; Setup the stack to a known good spot
	mov	ss, ax
	mov	sp, 7c00h

	push BootSeg
	pop ds

	MOV	AL, [Fats]	   ;Determine sector root directory starts on
	MUL	[FatSectors]
	ADD	AX, [ReservedSectors]
	PUSH	AX		; AX = Fats*FatSectors + ReservedSectors + HiddenSectors
	XCHG	CX, AX		 ; (CX) = start of DIR
;
; Take into account size of directory (only know number of directory entries)
;
	MOV	AX, sizeof.DIR_ENT	   ; bytes per directory entry
	MUL	[DirectoryEntries]	  ; convert to bytes in directory
	MOV	BX, [BytesPerSector]	   ; add in sector size
	ADD	AX, BX
	DEC	AX			; decrement so that we round up
	DIV	BX			; convert to sector number
	ADD	CX, AX
	MOV	[ClusterBase],CX	  ; save it for later

	push DirSeg
	pop	es

	xor	bx,bx
	pop	word [Arguments + SHARED.SectorBase]
	mov	word [Arguments + SHARED.SectorBase+2],bx

;
; (al) = # of sectors to read
;
	push	cs
	call	DoRead
	jc	BootErr$he

	xor	bx,bx
	mov	cx, [DirectoryEntries]
L10:
	mov	di,bx
	push	cx
	mov	cx,11
	mov	si, LOADERNAME
	repe	cmpsb
	pop	cx
	jz	L10end

	add	bx,sizeof.DIR_ENT
	loop  L10
L10end:

	jcxz	BootErr$bnf

	mov	dx,[es:bx + DIR_ENT.StartCluster] ; (dx) = starting cluster number
	push	dx
	mov	ax,1			; (al) = sectors to read
;
; Now, go read the file
;

	push	NtLdrSeg
	pop	es
	xor	bx,bx			; (es:bx) -> start of GRLDR

;
; (al) = # of sectors to read
; (es:bx)  ->  destination buffer
;

	push	cs
	call	ClusterRead
	jc	BootErr$he

;
; GRLDR requires:
;   BX = Starting Cluster Number of GRLDR
;   DL = INT 13 drive number we booted from
;   DS:SI -> the boot media's BPB
;   DS:DI -> argument structure
;   1000:0000 - entire FAT is loaded
;

	pop	BX			; (bx) = Starting Cluster Number
	mov	si,BPB			; ds:si -> BPB
	mov	di,Arguments		; ds:di -> Arguments

	push	ds
	pop	word [di + SHARED.ReadClusters + 2]
	mov	word [di + SHARED.ReadClusters], ClusterRead
	push	ds
	pop	word [di + SHARED.ReadSectors + 2]
	mov	word [di + SHARED.ReadSectors], DoRead
	MOV	dl, [DriveNumber]	   ; dl = boot drive

	;
	; Check LDR signature
	;

	; ES = 0
	push	es
	push	0
	pop	es
	mov	eax, dword [es:0x8000]
	pop	es

	and	eax, 0xFFFFFF
	cmp	eax, 'LDR'
	jnz	BootErr$ngs


;
; FAR JMP to 0000:8003.
;
;  First three bytes of GRLDR contain signature 'LDR'
;

	db	0EAh			; JMP FAR PTR
	dw	0x8003			; 0000:8003
	dw	0


;
; Print boot error
;

BootErr$ngs:
	MOV	SI,MSG_BAD_GRLDR
	jmp	short BootErr2

BootErr$bnf:
	MOV	SI,MSG_NO_GRLDR
	jmp	short BootErr2

BootErr$he:
	MOV	SI,MSG_READ_ERROR
BootErr2:
	call	BootErrPrint
	MOV	SI,MSG_REBOOT_ERROR
	call	BootErrPrint
	sti
	jmp	$			;Wait forever

BootErrPrint:
	  LODSB 			; Get next character
	  or	al,al
	  jz	BEdone

	  MOV	  AH,14 		  ; Write teletype
	  MOV	  BX,7			  ; Attribute
	  INT	  10H			  ; Print it
	  jmp	BootErrPrint
BEdone:

	ret



ClusterRead:
	push	ax			; (TOS) = # of sectors to read
	dec	dx
	dec	dx			; adjust for reserved clusters 0 and 1
	mov	al, [SectorsPerCluster]
	xor	ah,ah
	mul	dx			; DX:AX = starting sector number
	add	ax,[ClusterBase]	  ; adjust for FATs, root dir, boot sec.
	adc	dx,0
	mov	word [Arguments + SHARED.SectorBase],ax
	mov	word [Arguments + SHARED.SectorBase+2],dx
	pop	ax			; (al) = # of sectors to read


DoRead:
	mov	[SectorCount],AL
DRloop:
	MOV	AX, word [Arguments + SHARED.SectorBase]     ; Starting sector
	MOV	DX, word [Arguments + SHARED.SectorBase+2]     ; Starting sector
	ADD	AX, word [HiddenSectors]    ;adjust for partition's base sector
	ADC	DX,word [HiddenSectors+2]
	DIV	[SectorsPerTrack]
	INC	DL			; sector numbers are 1-based
	MOV	[CurrentSector],DL
	XOR	DX,DX
	DIV	[Heads]
	MOV	[CurrentHead],DL
	MOV	[CurrentTrack],AX
	MOV	AX,[SectorsPerTrack]
	SUB	AL,[CurrentSector]
	INC	AX
	cmp   al,[SectorCount]
	jbe   DoCall
	mov   al,[SectorCount]
	xor   ah,ah
DoCall:
	PUSH	AX
	MOV	AH,2
	MOV	cx,[CurrentTrack]
	SHL	ch,6
	OR	ch,[CurrentSector]
	XCHG	CH,CL
	MOV	DX,WORD [DriveNumber]
	INT	13H
	jnc	DcNoErr
	add	sp,2
	stc
	retf
DcNoErr:
	POP	AX
	SUB	[SectorCount],AL	  ; Are we finished?
	jbe	DRdone
	ADD	word [Arguments + SHARED.SectorBase],AX       ; increment logical sector position
	ADC	word [Arguments + SHARED.SectorBase+2],0
	MUL	[BytesPerSector]	  ; determine next offset for read
	ADD	BX,AX			; (BX)=(BX)+(SI)*(Bytes per sector)

	jmp	DRloop
DRdone:
	mov	[SectorCount],al
	clc
	retf







MSG_NO_GRLDR db 'Cannot find GRLDR',0
MSG_BAD_GRLDR db 'No LDR signature',0
MSG_READ_ERROR db 'Disk I/O error',0
MSG_REBOOT_ERROR db 13,10,'Insert another disk',0


LOADERNAME db 'GRLDR   BIN'

	rb 510-($-$$)
	db	55h,0aah



BootSectorEnd:











