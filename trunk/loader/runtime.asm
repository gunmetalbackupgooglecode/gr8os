;
; =============================================
;     bootstrap loader run-time library
; =============================================
;
; Общие соглашения вызова:
;  - тип вызова: stdcall для функций с постоянным числом аргументов и cdecl
;    для функций с переменным числом аргументов
;  - Не гарантируется сохранение никаких регистров, кроме сегментных и ESP.
;

;
; void sprintf( buffer, format, ... );
;

sprintf:
   ; фрейм стека
   push ebp
   mov	ebp, esp

   ; ES = DS
   push es
   push ds
   pop	es

   push esi
   push edi
   push ebx

   ; достаем параметры из стека
   mov	edi, [ebp+8]	 ; buffer
   mov	esi, [ebp+12]	 ; format string
   lea	ebp, [ebp+16]	 ; pointer to args
   cld

   ; цикл обработки символов
 __l:
   lodsb	     ; загружаем новый байт
   push ax
   cmp	al, '%'      ; символ - '%' ?
   jnz	__mov
     xor  edx, edx   ; zero mask for itoa
     ; percent
    _load_percent:
     lodsb

     cmp  al, 'd'
     jz   _decimal
     cmp  al, 's'
     jz   _string
     cmp  al, 'c'
     jz   _char
     cmp  al, 'x'
     jz   _hex
     cmp  al, '%'
     jz   _percent
     cmp  al, '0'
     jz   _zero
     mov  byte [edi], '%'
     inc  edi
     mov  byte [edi], al
     jmp  _skip_case

     ; width specifiers
     ; например, %08x
     ; достаем ширину и кладем в EDX сразу маску для itoa
    _zero:
     xor  eax, eax
     lodsb ; get number to fill
     sub  al, '0'
     mov  edx, 0x00300000
     or   edx, eax
     jmp  _load_percent

     ; '%%'
    _percent:
     mov  byte [edi], '%'
     jmp  _skip_case

     ; '%c' specifier - char
    _char:
     mov  eax, [ebp]
     mov  byte [edi], al
     add  ebp, 4
     jmp  _skip_case

     ; '%d' specifier - format decimal
    _decimal:
     push  esi		 ; save
     push  edi
     push  edx		 ; mask
     push  dword 10	 ; radix
     push  dword [ebp]	 ; value
     push  buf		 ; buffer
     call   itoa    ; itoa()
     pop   edi
     mov  esi, buf	 ; copy to main buffer
       __move:
       cmp  byte [esi], 0
       jz __end_move
       movsb
       jmp __move
       __end_move:
       dec  edi
     add  ebp, 4	 ; go to next argument
     pop  esi
     jmp  _skip_case

     ; '%x' specifier - format hexadecimal
    _hex:
     push  esi
     push  edi
     push  edx
     push  dword 16
     push  dword [ebp]
     push  buf
     call   itoa
     pop   edi
     mov  esi, buf
       __move3:
       cmp  byte [esi], 0
       jz __end_move3
       movsb
       jmp __move3
       __end_move3:
       dec  edi
     add  ebp, 4
     pop  esi
     jmp  _skip_case

     ; '%s' specifier - move string
    _string:
     push  esi
     mov   esi, [ebp]
       __move2:
       cmp  byte [esi], 0
       jz __end_move2
       movsb
       jmp __move2
       __end_move2:
       dec   edi
     add  ebp, 4
     pop  esi
     jmp  _skip_case

     _skip_case:
     jmp  _next_iteration

__mov:
   mov	[edi], al
 _next_iteration:
   inc	edi
   pop	ax
   test al, al
   jnz	__l

   pop	ebx
   pop	edi
   pop	esi

   pop	es
   pop	ebp
   ret

;
; void itoa( buffer, value, radix, 0x00XX00YY );  // XX - letter to fill, YY - number of fills
;

itoa:
   mov	ebx, [esp+12]  ; radix
   mov	eax, [esp+8]   ; value
   mov	edi, [esp+4]   ; buffer
   xor	ecx, ecx       ; счетчик

   ; последовательное деление
  _div:
   xor	edx, edx       ; старшая часть делимого, всегда 0

   div	ebx	       ; EDX:EAX / EBX

   mov	dl, byte [edx+small_table]  ; лукап символа по таблице, остаток - индекс
   mov	byte [edi], dl

   inc	edi	       ; инкремент указателя
   inc	ecx	       ; инкремент счетчика

   test eax, eax       ; частное = 0 ?
   jz	_q	       ; да, закончили деление

   jmp _div
  _q:

   mov	ax, [esp+16]
   movzx eax, ax
   .if	eax,ne,0
     .while ecx,l,eax
       mov  bl, [esp+18]
       mov  byte [edi], bl
       inc  edi
       inc  ecx
     .endw
   .endif

   mov	byte [edi], 0

   ; переворот строки
   mov	edi, [esp+4]   ; переходим снова в начало буфера
   mov	edx, ecx       ; копируем число символов
  _r:		       ; цикл реверса буфера
   mov	al, byte [edi+ecx-1]	 ; загружаем левый байт
   mov	esi, edx
   sub	esi, ecx
   mov	ah, byte [edi+esi]	 ; загружаем правый байт

   cmp	ecx, esi		 ; хватит?
   jna _qq

   mov	byte [edi+ecx-1], ah	 ; меняем местами
   mov	byte [edi+esi], al

   loop _r
  _qq:

   ret	16
small_table db '0123456789abcdefghijklmnopqrstuvwxyz'
large_table db '0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ'

