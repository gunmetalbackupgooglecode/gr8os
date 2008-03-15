format MS COFF

include 'inc/common.inc'

public sprintf as '_sprintf'
public vsprintf as '_vsprintf@12'
public itoa as '_itoa@16'

buf rb 256

;
; void sprintf( buffer, format, ... );
;

sprintf:
	lea  edx, [esp+12]
	ccall vsprintf, dword [esp+4], dword [esp+8], edx
	retn
	

;
; void vsprintf( buffer, format, va_list );
;

vsprintf:
   push ebp
   mov  ebp, esp

   ; ES = DS
   push es
   push ds
   pop  es

   push esi
   push edi
   push ebx

   mov  edi, [ebp+8]     ; buffer
   mov  esi, [ebp+12]    ; format string
   mov  ebp, [ebp+16]    ; pointer to args
   cld

 __l:
   lodsb      
   push ax
   cmp  al, '%'
   jnz  __mov
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
     push  esi           ; save
     push  edi
     push  edx           ; mask
     push  dword 10      ; radix
     push  dword [ebp]   ; value
     push  buf           ; buffer
     call   itoa    ; itoa()
     pop   edi
     mov  esi, buf       ; copy to main buffer
       __move:
       cmp  byte [esi], 0
       jz __end_move
       movsb
       jmp __move
       __end_move:
       dec  edi
     add  ebp, 4         ; go to next argument
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
   mov  [edi], al
 _next_iteration:
   inc  edi
   pop  ax
   test al, al
   jnz  __l

   pop  ebx
   pop  edi
   pop  esi

   pop  es
   pop  ebp
   ret  12

;
; void itoa( buffer, value, radix, 0x00XX00YY );  // XX - letter to fill, YY - number of fills
;

itoa:
   mov  ebx, [esp+12]  ; radix
   mov  eax, [esp+8]   ; value
   mov  edi, [esp+4]   ; buffer
   xor  ecx, ecx       

  _div:
   xor  edx, edx  

   div  ebx            ; EDX:EAX / EBX

   mov  dl, byte [edx+small_table]  
   mov  byte [edi], dl

   inc  edi          
   inc  ecx          

   test eax, eax     
   jz   _q           

   jmp _div
  _q:

   mov  ax, [esp+16]
   movzx eax, ax
   .if  eax,ne,0
     .while ecx,l,eax
       mov  bl, [esp+18]
       mov  byte [edi], bl
       inc  edi
       inc  ecx
     .endw
   .endif

   mov  byte [edi], 0

   mov  edi, [esp+4] 
   mov  edx, ecx     
  _r:                
   mov  al, byte [edi+ecx-1] 
   mov  esi, edx
   sub  esi, ecx
   mov  ah, byte [edi+esi]   

   cmp  ecx, esi             
   jna _qq

   mov  byte [edi+ecx-1], ah 
   mov  byte [edi+esi], al

   loop _r
  _qq:

   ret  16
small_table db '0123456789abcdefghijklmnopqrstuvwxyz'
large_table db '0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ'

