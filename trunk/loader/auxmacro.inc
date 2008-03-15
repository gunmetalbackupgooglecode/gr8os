include 'macro/struct.inc'

addr equ
ADDR equ
invoke equ
NULL equ 0
FALSE equ 0
TRUE equ 1

macro allow_nesting
{ macro pushd arg
  _%
     if arg eq
     else if  _##arg  eq _invoke arg
      arg
     else
      pushd arg
     end if %_
  macro invoke proc,[arg]
  _% _common_ allow_nesting
     _reverse_ pushd <arg>
     _common_ call [proc]
	      push eax
	      purge pushd
	      purge invoke %_ }

macro pushd first,[more]
{ common local ..continue,..opcode,..address
  if ~ more eq
   call ..continue
   db first,more,0
   ..continue:
  else if _ # first in <_addr first,_ADDR first>
   virtual at 0
    label ..address at first
    mov eax,dword [..address]
    load ..opcode from 0
   end virtual
   if ..opcode = 0A1h
    push first
   else
    lea edx,[..address]
    push edx
   end if
  else if first eqtype ''
   local ..data
   jmp ..continue
   ..data db first,0
   ..continue:
   push ..data
  else
   push first
  end if }

macro ccall proc,[arg]			; directly call CDECL procedure
 { common
    size@ccall = 0
    if ~ arg eq
   reverse
    pushd arg
    size@ccall = size@ccall+4
   common
    end if
    call proc
    if size@ccall
    add esp,size@ccall
    end if }

macro invoke proc,[arg]
{ reverse pushd <arg>
  common call proc }

macro halt
{ cli
  hlt }

macro .if v1,c,v2
{
  __IF equ
  local ..endif
  __ENDIF equ ..endif
  local ..else
  __ELSE equ ..else
  if v2 eq & c eq
   cmp v1,0
   je __ELSE
  else
   cmp v1,v2
   jn#c __ELSE
  end if
}

macro .else
{
  jmp __ENDIF
  __ELSE:
  restore __IF
  __IF equ ,
}

macro .elseif v1,c,v2
{
  local ..else
  jmp __ENDIF
  __ELSE:
  restore __ELSE
  __ELSE equ ..else
  if v2 eq & c eq
   cmp v1,0
   je __ELSE
  else
   cmp v1,v2
   jn#c __ELSE
  end if
}

macro .endif
{
  if __IF eq
   __ELSE:
  end if
  __ENDIF:
  restore __ELSE
  restore __ENDIF
  restore __IF
}

macro .while v1,c,v2
{
  local ..while
  __WHILE equ ..while
  local ..endw
  __ENDW equ ..endw
  __WHILE:
  if v2 eq & c eq
   cmp v1,0
   je __ENDW
  else
   cmp v1,v2
   jn#c __ENDW
  end if
}

macro .endw
{
  jmp __WHILE
  __ENDW:
  restore __ENDW
  restore __WHILE
}

macro .repeat
{
  local ..repeat
  __REPEAT equ ..repeat
  __REPEAT:
}

macro .until v1,c,v2
{
  if v2 eq & c eq
   cmp v1,0
   je __REPEAT
  else
   cmp v1,v2
   jn#c __REPEAT
  end if
  restore __REPEAT
}

jnne equ je
jnna equ ja
jnnb equ jb
jnng equ jg
jnnl equ jl
jnnae equ jae
jnnbe equ jbe
jnnge equ jge
jnnle equ jle
