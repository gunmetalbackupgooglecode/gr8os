struct TASK32
  LinkTSS dw ?
	  dw ?
  Esp0	  dd ?
  Ss0	  dw ?
	  dw ?
  Esp1	  dd ?
  Ss1	  dw ?
	  dw ?
  Esp2	  dd ?
  Ss2	  dw ?
	  dw ?
  Cr3	  dd ?
  Eip	  dd ?
  Eflags  dd ?
  Eax	  dd ?
  Ecx	  dd ?
  Edx	  dd ?
  Ebx	  dd ?
  Esp	  dd ?
  Ebp	  dd ?
  Esi	  dd ?
  Edi	  dd ?
  Es	  dw ?
	  dw ?
  Cs	  dw ?
	  dw ?
  Ss	  dw ?
	  dw ?
  Ds	  dw ?
	  dw ?
  Fs	  dw ?
	  dw ?
  Gs	  dw ?
	  dw ?
  Ldtr	  dw ?
	  dw ?
	  dw ?
  Iomap   dw ?
ends

SIZEOF_TSS = 104

SystemTss TASK32
Tss1	  TASK32
Tss2	  TASK32

FillTss:
    mov  [edx + TASK32.Cs], 8h
    mov  [edx + TASK32.Eax], 0
    mov  [edx + TASK32.Eip], eax
    pushfd
    pop  [edx + TASK32.Eflags]
    mov  ax, ds
    mov  [edx + TASK32.Ss], ax
    mov  [edx + TASK32.Ds], ax
    mov  ax, es
    mov  [edx + TASK32.Es], ax
    mov  [edx + TASK32.Ebp], ebp
    mov  [edx + TASK32.Esp], esp
    add  [edx + TASK32.Esp], 4
    mov  [edx + TASK32.Ecx], ecx
    mov  [edx + TASK32.Ebx], ebx
    mov  [edx + TASK32.Esi], esi
    mov  [edx + TASK32.Edi], edi
    mov  [edx + TASK32.Edx], edx
    ret
