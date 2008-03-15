	; NOT  'jmp short start'  !!
	jmp near start

Version 		db	"GR8OS1.0"
BPB:
BytesPerSector		dw	512		; Size of a physical sector
SectorsPerCluster	db	1		; Sectors per allocation unit
ReservedSectors 	dw	1		; Number of reserved sectors
Fats			db	2		; Number of fats
DirectoryEntries	dw	224		; Number of directory entries
Sectors 		dw	2880		; No. of sectors - no. of hidden sectors
Media			db	0F0H		; Media byte
FatSectors		dw	9		; Number of fat sectors
SectorsPerTrack 	dw	18		; Sectors per track
Heads			dw	2		; Number of surfaces
HiddenSectors		dd	0		; Number of hidden sectors
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
	cli
	hlt

rb 510-($-$$)
db 0x55, 0xAA

;
; SECTOR 2
;

; File Allocation Table

Fat:

db 0xF0, 0xFF, 0xFF	; start (FF0, FFF)
db 0xF0, 0xFF		; file1 (FF0)

rb 512*9-($-Fat)


;
; FAT2
;

Fat2:

db 0xF0, 0xFF, 0xFF	; start (FF0, FFF)
db 0xF0, 0xFF		; file1 (FF0)

rb 512*9-($-Fat2)


;
; Directory Table
;

DirTable:
db 'TESTFILETXT'   ; FileName
db 0x20 	   ; Attributes (A)
rb 10		   ; Unused
dd 0		   ; Time
dw 2		   ; StartCluster       (Cluster #2)
dd 5		   ; FileSize           (5 bytes)

rb 512*0xE-($-DirTable)


;
; FILE
;

File1:
db 'Hello'






rb 512*2880-($-$$)-1
db 0
