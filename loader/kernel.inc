;
; ======================
;        ядро
; ======================
;
;struc dpc
;{
;   dpc_proc     dd 0
;   dpc_context  dd 0
;}

dpc_offset_proc equ 0
dpc_offset_ctx	equ 4
dpc_struct_size equ 8

dpc_head	equ 80000200h
number_of_dpcs	db 10
dpc_queue_lock	dw 0

;
; void AcquireMutex( void* Mutex )
;

AcquireMutex:
	enter	0, 0
	push	esi
	push	eax
	push	ebx

	mov	esi, [ebp+8]

	;pushfd
	xor   ebx, ebx
	inc   bl

	; trying to acquire lock
  @@:	  xor  eax, eax

	  lock cmpxchg byte [esi], bl	       ; first byte of spinlock is spin count. second byte - old interrupt state.

	  ; SpinLock spinning for too long.
	  ;inc     edi
	  ;.if     edi, g, 10000
	  ;        ud2
	  ;.endif

	  jnz	   @B

	; spin lock acquired. save old interrupt state
	pushfd
	pop eax
	shr eax, 9
	and eax, 1
	mov  byte [esi+1], al

	; raise irql
	cli
	;popfd

	; leave with IF=0 !!

	pop	ebx
	pop	eax
	pop	esi
	leave
	ret	4

;
; void ReleaseMutex( void* Mutex )
;

ReleaseMutex:
	enter 0, 0
	mov  esi, [ebp+8]

	pushfd

	mov  byte [esi], 0

	xor  eax, eax
	mov  al, byte [esi+1]
	shl  eax, 9
	or   [esp], eax

	popfd

	leave
	ret	4

;
; dpc* InsertQueueDpc( void* DpcFunction, void* Context )
;

InsertQueueDpc:
	enter	0, 0
	mov	edi, [ebp+8]	; DpcFunction
	mov	esi, [ebp+12]	 ; Context

	movzx	ecx, byte [number_of_dpcs]
	mov	eax, dpc_head

	.while	ecx,g,0
	     mov     edx, dword [eax + dpc_offset_proc]

	     ; DPC свободен? ƒа - заполн€ем и возвращаемс€
	     .if     edx,e,0
		  invoke  AcquireMutex, dpc_queue_lock

		  mov	  dword [eax + dpc_offset_proc], edi
		  mov	  dword [eax + dpc_offset_ctx], esi

		  invoke  ReleaseMutex, dpc_queue_lock
		  jmp	  @InsertQueueDpc__end
	     .endif

	     dec     ecx
	     add     eax, dpc_struct_size
	.endw
	xor	eax, eax
  @InsertQueueDpc__end:
	leave
	ret	8

;
; void CallDpc( dpc* dpc )
;

CallDpc:
	enter	0, 0
	mov	eax, [ebp+8] ; dpc
	mov	edi, [eax + dpc_offset_proc] ; DpcFunction
	mov	esi, [eax + dpc_offset_ctx]  ; Context
	.if	edi, ne, 0
	  pushad
	  pushfd
	  invoke  edi, esi
	  popfd
	  popad
	.endif
	leave
	ret	4

;
; void RemoveQueueDpc( dpc* dpc )
;

RemoveQueueDpc:
	enter	0, 0
	mov	esi, [ebp+8] ; dpc
	mov	dword [esi + dpc_offset_proc], 0
	mov	dword [esi + dpc_offset_ctx], 0
	leave
	ret	4

;
; void ProcessDpcQueue( )
;

ProcessDpcQueue:
	enter	0, 0
	movzx	ecx, byte [number_of_dpcs]
	mov	eax, dpc_head

	.while	ecx,g,0
	     mov     edx, dword [eax + dpc_offset_proc]

	     .if     edx,ne,0
		  mov  ebx, eax

		  invoke  AcquireMutex, dpc_queue_lock
		  invoke  CallDpc, ebx
		  invoke  RemoveQueueDpc, ebx
		  invoke  ReleaseMutex, dpc_queue_lock

		  mov	  eax, ebx

	     .endif

	     dec     ecx
	     add     eax, dpc_struct_size
	.endw

	leave
	ret

;
; void ClearDpcQueue( )
;

ClearDpcQueue:
	enter	0, 0

	; Map DPC queue to the kernel virtual address space
	invoke MiMapPhysicalPages, 0x80000000, 0x00000000, 1

	mov	edi, dpc_head
	xor	edx, edx
	movzx	eax, byte [number_of_dpcs]
	mov	ebx, dpc_struct_size
	mul	ebx
	mov	ecx, eax
	xor	eax, eax
	push	es
	push	ds
	pop	es
	cld
	rep	stosb
	pop	es
	leave
	ret

struct LIST_ENTRY
    Flink   dd ?
    Blink   dd ?
ends


