;
; Enable Paging
;


;
; VM Short Map (see mm.pdf for details)
;
;  00000000 - 7FFFFFFF    UserMode memory
;  80100000 - 801FFFFF    Kernel
;  88000000 - 8FFFFFFF    Kernel heap
;  A8000000 - BFFFFFFF    Drivers
;  C0000000 - CFFFFFFF    PTEs, PPDs
;

;
; PM short map
;
;  00000000 - 000FFFFF - Loader & kernel
;  00180000 - 00FFFFFF - PTEs
;  01000000 - 017FFFFF - PPDs
;
;  24 mb
;

;
; PTE map
;
;  Page Directories (array of PDEs) (size 0x1000)
;
;  1024 Directories:
;
;  1024 Page tables (array of PTEs) for 0 directory (0x1000)
;  1024 Page tables (array of PTEs) for 1 directory (0x1000)
;  1024 Page tables (array of PTEs) for 2 directory (0x1000)
;   ...
;  1024 Page tables (array of PTEs) for 1023 directory (0x1000)
;
;  Total size:  0x1000 /*directories*/ + 1024*0x1000 /*tables*/ = 0x401000
;
;   How to calculate:
;    1) &PDE = KE_PG_PTE_START + Directory*4
;    2) &PTE = KE_PG_PTE_START + 0x1000 /*directories*/ + Directory*0x1000 /*tables*/ + Table*4
;
;            10        10         12
; VA =  [   DIR   |    TBL   |   OFS   ]
;

KE_PG_PDE_START  = 00180000h
KE_PG_PDE_START_VA = 0xC0000000

PAGE_SIZE = 0x1000
PAGE_SHIFT = 12

KE_PG_FULL_SIZE  = 00401000h

KE_PG_PPD_START  = 01000000h


macro GetPdeAddress VA
{
	mov  eax, VA
	shr  eax, 20  ; 22 - 2
	and  eax, not (11b)
	add  eax, KE_PG_PDE_START
}

macro GetPteAddress Base, VA
{
	push ecx

	mov  ecx, VA
	shr  ecx, 22   ; ebx = dir
	shl  ecx, 12   ; ebx *= 0x1000

	mov  eax, VA
	shr  eax, 10   ; 12 - 2
	and  eax, 111111111100b  ; eax = tbl*4

	add  eax, ecx
	add  eax, Base + 0x1000

	pop  ecx
}

macro MiGetPteAddress VA
{
	GetPteAddress KE_PG_PDE_START_VA, VA
}

EnablePaging:
	mov  eax, 12345678h
	mov  [KE_PG_PDE_START], eax
	cmp  [KE_PG_PDE_START], eax
	jnz  NotEnoughPhysicalMemory

	mov  [KE_PG_PPD_START], eax
	cmp  [KE_PG_PPD_START], eax
	jz   EP10

  NotEnoughPhysicalMemory:
	ccall DbgPrint and 0xFFFFFF, "FATAL: not enough physical memory installed on the system"
	ud2

  EP10:
	cld
	; Clear PDEs (1024 PDEs)
	mov  edi, KE_PG_PDE_START
	mov  ecx, 1024
	xor  eax, eax
	rep  stosd

	; Clear PPDs
	;BUGBUG: Implement PPDs
	mov  edi, KE_PG_PPD_START
	mov  ecx, 1024
	xor  eax, eax
	rep  stosd

	; Create directories.
	mov  eax, (KE_PG_PDE_START + PAGE_SIZE) or 111b ; Valid, Write, Owner.
	mov  edi, KE_PG_PDE_START
	mov  ecx, 1024
    @@: stosd
	add  eax, PAGE_SIZE
	loop @B

	; Create 3 tables for loader and 1 page to video buffer
	GetPteAddress KE_PG_PDE_START, 0x80007000
;        mov  ecx, 10               ; Map 10 kernel pages
;        mov  edi, eax
;        mov  eax, 0x7007
;    @@: stosd
;        add  eax, PAGE_SIZE
;        add  edi, 4
;        loop @B

	; Map pages for GRLDR. Kernel will be mapped later
	mov  dword [eax-8],    0x5007 ;  Stack
	mov  dword [eax-4],    0x6007 ;  Stack
	mov  dword [eax],    0x7007 ;  Valid, Write, Owner
	mov  dword [eax+4],  0x8007
	mov  dword [eax+8],  0x9007
	mov  dword [eax+12], 0xA007

;        mov  dword [eax+16], 0xB007
;        mov  dword [eax+20], 0xC007
;        mov  dword [eax+24], 0xD007
;        mov  dword [eax+28], 0xE007

	GetPteAddress KE_PG_PDE_START, 0x800B8000
	mov  dword [eax], 0xB8007 ; 0xB8000 - physical, Valid, Write, Owner

	; Set linear=virtual temporary mapping
	GetPteAddress KE_PG_PDE_START, 0x7000
	mov  dword [eax], 0x7007 ; 0x7000 - physical, Valid, Write, Owner
	mov  dword [eax+4], 0x8007
	mov  dword [eax+8], 0x9007
	mov  dword [eax+12], 0xA007

	; Set self-mapping for PTE/PDE arrays
	GetPteAddress KE_PG_PDE_START, 0xC0000000
	mov  edi, eax
	mov  eax, KE_PG_PDE_START or 111b
	mov  ecx, 1025
    @@: stosd
	add  eax, PAGE_SIZE
	loop @B

	xor  eax, eax
	mov  cr2, eax

	; Load CR3
	mov  eax, KE_PG_PDE_START
	mov  cr3, eax

	mov  eax, cr0
	or   eax, (1 shl 31)
	mov  cr0, eax

	; Paging is already enabled, but due to set linear=physical mapping
	; Linear address equals physical address. Jump to real kernel mapping in the highest 2Gb and
	;  remove temporary mapping.

	jmp KGDT_R0_CODE:PagingFlushTlb

org (0x80008000 + ($-GRLDR_START))

 PagingFlushTlb:

	; Paging enabled, remove temporary mapping
	MiGetPteAddress 0x7000
	mov  ecx, 5
	mov  edi, eax
	xor  eax, eax
	rep  stosd

	; Load new global descriptor table
	lgdt fword [GDTR2]

	mov  ax, KGDT_R0_DATA
	mov  ds, ax
	mov  ss, ax
	mov  es, ax
	mov  ax, KGDT_VIDEO
	mov  gs, ax
	add  esp, 0x80000000

	add  esp, 4

	retn

;
; MiMapPhysicalPages
;
;  Maps specified pages to kernel virtual memory
;
;  ESP+4  Desired VirtualAddress
;  ESP+8  Desired PhysicalAddress
;  ESP+12 Number of pages
;

MiMapPhysicalPages:
	push edi

	mov  edx, [esp+8]	; get PTE address for VA
	MiGetPteAddress edx

	mov  edi, eax
	mov  ecx, [esp+16]	; number of pages
	mov  eax, [esp+12]	; physical address

	pushad
	ccall DbgPrint, "MiMapPhysicalPages: Mapping %d pages from %08x => %08x", ecx, eax, edx
	popad

	or   eax, 111b		; Write, Valid, Owner

	test ecx, ecx
	jnz  @F
	inc  ecx

   @@:	stosd
	add  eax, PAGE_SIZE
	loop @B

	mov  edx, [esp+8]
	invlpg [edx]

	pop  edi
	retn 12


;
; MiGetPhysicalAddress
;
;  ESP+4  Virtual address to be resolved.
;
MiGetPhysicalAddress:
	mov  eax, [esp+4]
	MiGetPteAddress eax
	mov  eax, [eax]
	shr  eax, PAGE_SHIFT
	shl  eax, PAGE_SHIFT
	retn 4




