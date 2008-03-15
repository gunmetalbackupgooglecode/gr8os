format MS COFF

include 'inc/common.inc'

public KeAcquireLock as '@KeAcquireLock@4'
public KeReleaseLock as '@KeReleaseLock@4'


public tempbuffer
tempbuffer rb 512


;
; KeAcquireLock
;
;  Acquires specified lock atomically
;
KeAcquireLock:
	xor  edx, edx
	inc  dl
	
  @@:
	xor  eax, eax
	lock cmpxchg byte [ecx], dl
	jnz  @B
	
	pushfd
	pop  eax
	shr  eax, 9
	and  eax, 1
	mov  byte [ecx+1], al
	
	cli
	
	retn
	
	
;
; KeReleaseLock
;
;  Releases specified lock
;
KeReleaseLock:
	pushfd
	mov  byte [ecx], 0
	xor  eax, eax
	mov  al, byte [ecx+1]
	shl  eax, 9
	or   [esp], eax
	popfd
	retn

;	mov  byte [ecx], 0
;	cmp  byte [ecx+1], 1
;	jnz  @F
;	sti
; @@:
;	retn


public KeReleaseIrqState as '_KeReleaseIrqState@4'

; @implemented
;
; KeReleaseIrqState
;
;  esp+4  Old IRQ state
;
KeReleaseIrqState:
	mov  al, [esp+4]
	test al, al
	jnz  KR20
	cli
	jmp  KR10
 KR20:
	sti
 KR10:
	retn 4
	

public KeGetCpuFeatures as '_KeGetCpuFeatures@4'

; @implemented
;
; KeGetCpuFeatures
;
;  esp+4  pointer to the buffer where to store CPU features
;
KeGetCpuFeatures:
	push ebx
	push edi
	
	xor  eax, eax
	cpuid
	
	mov  edi, [esp+12]
	mov  [edi + CPU_FEATURES.ProcessorId], ebx
	mov  [edi + CPU_FEATURES.ProcessorId + 4], edx
	mov  [edi + CPU_FEATURES.ProcessorId + 8], ecx
	mov  byte [edi + CPU_FEATURES.ProcessorId + 12], 0
	mov  [edi + CPU_FEATURES.MaximumEax], eax
	
	xor  eax, eax
	inc  eax
	cpuid
	
	mov  [edi + CPU_FEATURES.Version], eax
	mov  [edi + CPU_FEATURES.EbxAddInfo], ebx
	mov  [edi + CPU_FEATURES.FeatureInfo], ecx
	mov  [edi + CPU_FEATURES.FeatureInfo + 4], edx
	
	mov  eax, 0x80000000
	cpuid
	test eax, 0x80000000
	jnz  KGC10
	mov  byte [edi + CPU_FEATURES.BrandString], 0
	jmp  KGC20
 KGC10:
	mov  eax, 0x80000002
	cpuid
	mov  [edi + CPU_FEATURES.BrandString], eax
	mov  [edi + CPU_FEATURES.BrandString + 4], ebx
	mov  [edi + CPU_FEATURES.BrandString + 8], ecx
	mov  [edi + CPU_FEATURES.BrandString + 12], edx
	mov  eax, 0x80000003
	cpuid
	mov  [edi + CPU_FEATURES.BrandString + 16], eax
	mov  [edi + CPU_FEATURES.BrandString + 20], ebx
	mov  [edi + CPU_FEATURES.BrandString + 24], ecx
	mov  [edi + CPU_FEATURES.BrandString + 28], edx
	mov  eax, 0x80000004
	cpuid
	mov  [edi + CPU_FEATURES.BrandString + 32], eax
	mov  [edi + CPU_FEATURES.BrandString + 36], ebx
	mov  [edi + CPU_FEATURES.BrandString + 40], ecx
	mov  [edi + CPU_FEATURES.BrandString + 44], edx
		
 KGC20:	
	pop  edi
	pop  ebx
	retn 4


public KeQueryTimerTickMult as '_KeQueryTimerTickMult@0'
extrn irq0_handler

; @implemented
;
; KeQueryTimerTickMult
;
;  Query timer ticks multiplier
;
KeQueryTimerTickMult:
	push edi

	; Disable interrupts
	pushfd
	cli
	
	; Turn off timer handler
	mov  di, word [irq0_handler]
	mov  byte [irq0_handler], 0xCF    ; iretd
	
	; Mask all IRQs except IRQ0
	mov  al, 0xFE
	out  0x21, al
	
	; Enable interrupts
	sti
	
	; Wait for the first timer IRQ
	hlt	
	rdtsc
	mov  ecx, eax
	
	; Wait for the second timer IRQ
	hlt
	rdtsc
	sub  eax, ecx     ; calc the difference
	
	cli
	
	; Clear IRQ mask
	push eax
	mov  al, 0
	out  0x21, al
	pop  eax
	
	; Restore old handler
	mov  word [irq0_handler], di
	
	popfd
	
	pop  edi
	retn
	

public KeQueryApicTimerConf as '_KeQueryApicTimerConf@4'

; @implemented
;
; KeQueryApicTimerConf
;
;  Query APIC timer conf
;  esp+4   APIC_TIMER_CONFIG
;
KeQueryApicTimerConf:
	mov  ecx, [esp+4]
	
	pushfd
	cli
	
	mov  eax, [APIC_INITCNT]
	mov  [ecx + APIC_TIMER_CONFIG.InitialCounter], eax
	
	mov  eax, [APIC_CURRCNT]
	mov  [ecx + APIC_TIMER_CONFIG.CurrentCounter], eax
	
	mov  eax, [APIC_DIVCONF]
	mov  [ecx + APIC_TIMER_CONFIG.Divisor], eax
	
	mov  eax, [APIC_LVTTMR]
	mov  [ecx + APIC_TIMER_CONFIG.LvtTimer], eax
	
	popfd
	
	retn 4


public KeSetApicTimerConf as '_KeSetApicTimerConf@4'

; @implemented
;
; KeSetApicTimerConf
;
;  Set APIC timer conf
;  esp+4   APIC_TIMER_CONFIG
;
KeSetApicTimerConf:
	mov  ecx, [esp+4]
	
	pushfd
	cli
	
	mov  edx, [ecx + APIC_TIMER_CONFIG.Flags]
	test edx, TIMER_MODIFY_INITIAL_COUNTER
	jz   @F
	
	mov  eax, [ecx + APIC_TIMER_CONFIG.InitialCounter]
	mov  [APIC_INITCNT], eax
	
@@: test edx, TIMER_MODIFY_DIVISOR
	jz   @F
	
	mov  eax, [ecx + APIC_TIMER_CONFIG.Divisor]
	mov  [APIC_DIVCONF], eax
	
@@: test edx, TIMER_MODIFY_LVT_ENTRY
	jz   @F
	
	mov  eax, [ecx + APIC_TIMER_CONFIG.LvtTimer]
	mov  [APIC_LVTTMR], eax
	
@@: popfd
	
	retn 4


public KiReadApicConfig as '_KiReadApicConfig@4'

; @implemented
;
; KiReadApicConfig
;
;  Reads APIC config registers
;  esp+4  Offset
;
KiReadApicConfig:
	mov  eax, [esp+4]
	add  eax, LAPIC_BASE
	mov  eax, [eax]
	retn 4


public KiWriteApicConfig as '_KiWriteApicConfig@8'

; @implemented
;
; KiWriteApicConfig
;
;  Writes APIC config registers
;  esp+4  Offset
;  esp+8  New value
;
KiWriteApicConfig:
	mov  eax, [esp+4]
	mov  ecx, [esp+8]
	add  eax, LAPIC_BASE
	mov  [eax], ecx
	retn 8


;
; DPCs
;

;BUGBUG: Not implemented fully

public KiDpcListHead  as '_KiDpcListHead'
public KiNumberDpcs   as '_KiNumberDpcs'
public KiDpcQueueLock as '_KiDpcQueueLock'

KiDpcListHead   dd 80000200h
KiNumberDpcs    db 10
KiDpcQueueLock  dw 0


public memcpy as '_memcpy'
;
;KESYSAPI
;VOID
;KEFASTAPI
;memcpy(
;	IN PVOID To,
;	IN PVOID From,
;	IN ULONG Length
;	);
;

memcpy:
	push esi
	push edi
	
	; Stack map
	;
	; ESP -> edi
	; +4  -> esi
	; +8  -> ret
	; +12 -> to
	; +16 -> from
	; +20 -> length

	cld
	mov  edi, [esp+12]
	mov  esi, [esp+16]
	mov  ecx, [esp+20]
	shr  ecx, 2
	rep  movsd
	mov  ecx, [esp+20]
	and  ecx, 11b
	rep  movsb
	
	pop  edi
	pop  esi
	retn

public KeZeroMemory as '@KeZeroMemory@8'
;
;KESYSAPI
;VPOD
;KEFASTAPI
;KeZeroMemory(
;	IN PVOID To,
;	IN ULONG Length
;	);
;
KeZeroMemory:
	push esi
	push edi
	
	cld
	xor  eax, eax
	mov  edi, ecx
	mov  ecx, edx
	shr  ecx, 2
	rep  stosd
	mov  ecx, edx
	and  ecx, 11b
	rep  stosb
	
	pop  edi
	pop  esi
	retn
	

public memset as '_memset'

;
;KESYSAPI
;VOID
;_cdecl
;memset(
;	IN PVOID To,		// esp + 4
;   IN UCHAR Byte,		// esp + 8
;	IN ULONG Length		// esp + 12
;	);
;
memset:
	push edi
	
	cld
	mov  edi, [esp+8]  ; to
	movzx eax, byte [esp+12]  ; byte
	
	; EAX = 000000XX   =>  EAX = 0000XXXX
	mov  ah, al
	
	; EAX = 0000XXXX   =>  EAX = XXXXXXXX
	movzx  ecx, ax
	shl  eax, 16
	or   eax, ecx
	
	mov  ecx, [esp+16] ; length
	shr  ecx, 2
	rep  stosd
	
	mov  ecx, [esp+16] ; length
	and  ecx, 11b
	rep  stosb
	
	pop  edi
	retn
	
	
public memmove as '_memmove'

;
;KESYSAPI
;VOID
;_cdecl
;memmove(
;	IN PVOID To,		// esp + 4
;   IN PVOID From,		// esp + 8
;	IN ULONG Length		// esp + 12
;	);
;
memmove:
	push esi
	push edi
	pushfd
	
	; Stack map
	;
	; ESP -> edi
	; +4  -> esi
	; +8  -> ret
	; +12 -> to
	; +16 -> form
	; +20 -> length
	
	mov  esi, [esp + 12]
	mov  edi, [esp + 16]
	mov  ecx, [esp + 20]

	cmp  esi, edi
	jbe  _1

	cld
	rep  movsb
	jmp  _2

_1:
	std
	add  esi, ecx
	add  edi, ecx
	dec  esi
	dec  edi
	rep  movsb

_2:
	popfd
	pop  edi
	pop  esi
	retn

	

public KiOutChar as '_KiOutChar@12'
;
; KiOutChar
;
;  Prints char on screen
;  esp+4   x
;  esp+8   y
;  esp+12  chr
;
KiOutChar:
	movzx  eax, word [esp+8]
	mov  ebx, 80
	mul  ebx
	movzx  edx, word [esp+4]
	add  eax, edx
	
	shl  eax, 1
	mov  cl, [esp+12]
	mov  byte [gs:eax], cl
	retn 12


public MmInvalidateTlb as '_MmInvalidateTlb@4'

; @implemented
;
; MmInvalidateTlb
;
;  Invalidates translation look-aside buffers for the specified VA
;  esp+4  VirtualAddress to be invalidated
;
MmInvalidateTlb:
	mov  eax, [esp+4]
	invlpg [eax]
	retn 4

