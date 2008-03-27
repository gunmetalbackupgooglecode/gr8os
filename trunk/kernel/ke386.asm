format MS COFF

include 'inc/common.inc'

public KeAcquireLock as '@KeAcquireLock@4'
public KeReleaseLock as '@KeReleaseLock@4'


public tempbuffer
tempbuffer rb 512


public InterlockedExchange as '@InterlockedExchange@8'
;
;KESYSAPI
;LONG
;KEFASTAPI
;InterlockedExchange(
;	PLONG Variable,			// @ECX
;	LONG NewValue			// @EDX
;	);
;
InterlockedExchange:
	mov  eax, edx
	lock xchg [ecx], eax
	retn


public InterlockedCompareExchange as '@InterlockedCompareExchange@12'
;
;KESYSAPI
;LONG
;KEFASTAPI
;InterlockedCompareExchange(
;	PLONG Variable,			// @ECX
;	LONG Exchange,			// @EDX
;	LONG Comperand
;	);
;
InterlockedCompareExchange:
	mov  eax, [esp+4]
	lock cmpxchg dword [ecx], edx
	retn 4


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


public HalQueryTimerTickMult as '_HalQueryTimerTickMult@0'
extrn irq0_handler

; @implemented
;
; HalQueryTimerTickMult
;
;  Query timer ticks multiplier
;
HalQueryTimerTickMult:
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
	

public HalQueryApicTimerConf as '_HalQueryApicTimerConf@4'

; @implemented
;
; HalQueryApicTimerConf
;
;  Query APIC timer conf
;  esp+4   APIC_TIMER_CONFIG
;
HalQueryApicTimerConf:
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


public HalSetApicTimerConf as '_HalSetApicTimerConf@4'

; @implemented
;
; HalSetApicTimerConf
;
;  Set APIC timer conf
;  esp+4   APIC_TIMER_CONFIG
;
HalSetApicTimerConf:
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


public HalpReadApicConfig as '_HalpReadApicConfig@4'

; @implemented
;
; HalpReadApicConfig
;
;  Reads APIC config registers
;  esp+4  Offset
;
HalpReadApicConfig:
	mov  eax, [esp+4]
	add  eax, LAPIC_BASE
	mov  eax, [eax]
	retn 4


public HalpWriteApicConfig as '_HalpWriteApicConfig@8'

; @implemented
;
; HalpWriteApicConfig
;
;  Writes APIC config registers
;  esp+4  Offset
;  esp+8  New value
;
HalpWriteApicConfig:
	mov  eax, [esp+4]
	mov  ecx, [esp+8]
	add  eax, LAPIC_BASE
	mov  [eax], ecx
	retn 8


public HalQueryBusClockFreq as '_HalQueryBusClockFreq@0'

; @implemented
;
; HalQueryBusClockFreq
;
;  Determines bus clock frequency in ticks per second.
;
HalQueryBusClockFreq:
	pushfd
	cli

	; Set divisor to 128 and some initial counter
	mov  dword [APIC_DIVCONF], 1010b
	mov  dword [APIC_INITCNT], 0x10000

	; Stop channel 2
	mov  edx, 0x61
	in   al, dx
	and  al, 11111100b
	out  dx, al
	jmp  $+2

	; Channel 2, LSBMSB, one-shot
	mov  al, 10111000b
	out  43h, al
	jmp  $+2
	
	; Set divisor
	mov  eax, 1024
	out  42h, al
	jmp  $+2
	mov  al, ah
	out  42h, al
	jmp  $+2
	
	mov  ebx, 0x10000
	
	; Start channel 2
	in   al, dx
	or   al, 1
	out  dx, al
	
	; Set initial counter
	mov  dword [APIC_INITCNT], ebx
	
	; Wait for 5 bit in system port
	
@@:	in   al, dx
	test al, 100000b
;	inc  byte [gs:0]
	jnz  @B
	
	sub  ebx, [APIC_CURRCNT]
	xor  edx, edx
	mov  eax, PIT_FREQ
	mul  ebx
	shr  eax, 3
	
	; Set divisor to 1
	mov  dword [APIC_DIVCONF], 1011b
	
	popfd
	retn
	
	


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
	
	mov  edi, [esp + 12]
	mov  esi, [esp + 16]
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

public memmove_far as '_memmove_far'
;
; memmove_far
;
;  Performs far memory moving
;
;   ESP+4   dst segment
;   ESP+8   dst offset
;   ESP+12  src segment
;   ESP+16  src offset
;   ESP+20  Length
;
memmove_far:
	push esi
	push edi
	pushfd
	push es
	push ds
	
	mov  ax, [esp+24]
	mov  es, ax
	
	mov  ax, [esp+32]
	mov  ds, ax
	
	; Stack map
	;
	; ESP -> ds
	; +4  -> es
	; +8  -> efl
	; +12 -> edi
	; +16 -> esi
	; +20 -> ret
	; +24 -> toseg
	; +28 -> to
	; +32 -> fromseg
	; +36 -> from
	; +40 -> length
	
	mov  edi, [esp + 28]
	mov  esi, [esp + 36]
	mov  ecx, [esp + 40]

	cmp  esi, edi
	jbe  _3

	cld
	rep  movsb
	jmp  _4

_3:
	std
	add  esi, ecx
	add  edi, ecx
	dec  esi
	dec  edi
	rep  movsb

_4:
	pop  ds
	pop  es
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

