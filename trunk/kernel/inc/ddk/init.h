//
// <init.h> built by header file parser at 20:46:52  08 Apr 2008
// This is a part of gr8os include files for GR8OS Driver & Extender Development Kit (DEDK)
//


#pragma once

//
// Interrupt descriptor table
//

#pragma pack(1)


typedef struct _IDT_ENTRY
{
    USHORT OffsetLow;
    USHORT Selector;
	struct {
		UCHAR ReservedByte;
		UCHAR Type : 4;
		UCHAR UnusedBits2 : 1;
		UCHAR DPL : 2;
		UCHAR Present : 1;
	} ThirdWord;
    USHORT OffsetHigh;
} IDT_ENTRY, *PIDT_ENTRY;

typedef struct GATE_ENTRY
{
    USHORT Unused1;
    USHORT Selector;
	struct {
		UCHAR ReservedByte;
		UCHAR Type : 4;
		UCHAR UnusedBits2 : 1;
		UCHAR DPL : 2;
		UCHAR Present : 1;
	} ThirdWord;
    USHORT Unused2;
} *PGATE_ENTRY;


//
// IDTR
//

#pragma pack(2)
struct IDTR
{
    USHORT		Limit;
    PIDT_ENTRY	Table;
};
#pragma pack()

typedef struct _SEG_DESCRIPTOR {
    USHORT    LimitLow;
    USHORT    BaseLow;
    union {
        struct {
            BYTE    BaseMid;
            BYTE    Flags1;
            BYTE    Flags2;
            BYTE    BaseHi;
        } Bytes;
        struct {
            DWORD   BaseMid : 8;
            DWORD   Type : 5;
            DWORD   Dpl : 2;
            DWORD   Pres : 1;
            DWORD   LimitHi : 4;
            DWORD   Sys : 1;
            DWORD   Reserved_0 : 1;
            DWORD   Default_Big : 1;
            DWORD   Granularity : 1;
            DWORD   BaseHi : 8;
        } Bits;
    } HighWord;
} SEG_DESCRIPTOR, *PSEG_DESCRIPTOR;

#define SEGMENT_TSS32_FREE	9
#define SEGMENT_TSS32_OCC	11
#define SEGMENT_TASK_GATE	5
#define SEGMENT_INT32_GATE	14

#define EXC_DIVIDE_ERROR	0
#define EXC_DEBUG			1
#define EXC_NMI				2
#define EXC_BREAKPOINT		3
#define EXC_OVERFLOW		4
#define EXC_BOUNDS_ERROR	5
#define EXC_UNDEFINED_CODE	6
#define EXC_NO_COPROCESSOR	7
#define EXC_DOUBLE_FAULT	8
#define EXC_INVALID_TSS		10
#define EXC_NO_SEGMENT		11
#define EXC_STACK_FAULT		12
#define EXC_GENERAL_PROT	13
#define EXC_PAGE_FAULT		14
#define EXC_MATH_FP_ERR		16
#define EXC_ALIGNMENT_CHECK	17
#define EXC_MACHINE_CHECK	18
#define EXC_FPU_SIMD		19

#pragma pack(2)
struct GDTR
{
    USHORT			Limit;
    PSEG_DESCRIPTOR	Table;
};
#pragma pack()

extern SEG_DESCRIPTOR GlobalDescriptorTable[];
extern GDTR Gdtr;

typedef struct TSS32
{
	ULONG LinkTSS;
	ULONG Esp0;
	ULONG Ss0;
	ULONG Esp1;
	ULONG Ss1;
	ULONG Esp2;
	ULONG Ss2;
	ULONG Cr3;
	ULONG Eip;
	ULONG Eflags;
	ULONG Eax;
	ULONG Ecx;
	ULONG Edx;
	ULONG Ebx;
	ULONG Esp;
	ULONG Ebp;
	ULONG Esi;
	ULONG Edi;
	ULONG Es;
	ULONG Cs;
	ULONG Ss;
	ULONG Ds;
	ULONG Fs;
	ULONG Gs;
	ULONG Ldtr;
	USHORT Reserved;
	USHORT Iomap;
} *PTSS32;

#pragma pack()

