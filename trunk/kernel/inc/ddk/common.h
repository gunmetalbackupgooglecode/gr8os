//
// <common.h> built by header file parser at 09:59:08  17 Apr 2008
// This is a part of gr8os include files for GR8OS Driver & Extender Development Kit (DEDK)
//


#ifndef _GR8OS_
#define _GR8OS_

//
// General types
//

typedef long LONG, *PLONG;
typedef int INT, *PINT;
typedef short SHORT, *PSHORT;
typedef char CHAR, *PCHAR, BOOLEAN, *PBOOLEAN, LOGICAL, *PLOGICAL;
typedef unsigned long DWORD, *PDWORD, ULONG, *PULONG, ULONG_PTR, *PULONG_PTR;
typedef unsigned short WORD, *PWORD, USHORT, *PUSHORT;
typedef wchar_t WCHAR, *PWCHAR, WSTR, *PWSTR;
typedef unsigned char BYTE, *PBYTE, UCHAR, *PUCHAR;
typedef void VOID, *PVOID, **PPVOID;
typedef long long LONGLONG, *PLONGLONG, LONG64, *PLONG64;
typedef unsigned long long ULONGLONG, *PULONGLONG, ULONG64, *PULONG64;

typedef ULONG STATUS, *PSTATUS;

//
// Notice codes
//
#define STATUS_CACHED						((STATUS) 0x00000006)
#define STATUS_ALREADY_FREE					((STATUS) 0x00000005)
#define STATUS_FINISH_PARSING				((STATUS) 0x00000004)
#define STATUS_REPARSE						((STATUS) 0x00000003)
#define STATUS_TIMEOUT						((STATUS) 0x00000002)
#define STATUS_MORE_AVAILABLE				((STATUS) 0x00000001)

// Success code
#define STATUS_SUCCESS						((STATUS) 0x00000000)

// 
// Error codes
//

#define STATUS_UNSUCCESSFUL					((STATUS) 0xF0000001)
#define STATUS_INSUFFICIENT_RESOURCES		((STATUS) 0xF0000002)
#define STATUS_ACCESS_DENIED				((STATUS) 0xF0000003)
#define STATUS_ACCESS_VIOLATION				((STATUS) 0xF0000004)
#define STATUS_NOT_FOUND					((STATUS) 0xF0000005)
#define STATUS_INVALID_PARAMETER			((STATUS) 0xF0000006)
#define STATUS_INTERNAL_FAULT				((STATUS) 0xF0000007)
#define STATUS_NOT_IMPLEMENTED				((STATUS) 0xF0000008)
#define STATUS_REPEAT_NEEDED				((STATUS) 0xF0000009)
#define STATUS_DEVICE_NOT_READY				((STATUS) 0xF000000A)
#define STATUS_PARTIAL_COMPLETION			((STATUS) 0xF000000B)
#define STATUS_IN_USE						((STATUS) 0xF000000C)
#define STATUS_INVALID_HANDLE				((STATUS) 0xF000000D)
#define STATUS_INVALID_FUNCTION				((STATUS) 0xF000000E)
#define STATUS_NOT_SUPPORTED				((STATUS) 0xF000000F)
#define STATUS_DATATYPE_MISALIGNMENT		((STATUS) 0xF0000010)
#define STATUS_BUSY							((STATUS) 0xF0000011)
#define STATUS_INVALID_FILE_FOR_IMAGE		((STATUS) 0xF0000012)
#define STATUS_PRIVILEGE_NOT_HELD			((STATUS) 0xF0000013)

#define STATUS_INVALID_PARAMETER_1			((STATUS) 0xF0000021)
#define STATUS_INVALID_PARAMETER_2			((STATUS) 0xF0000022)
#define STATUS_INVALID_PARAMETER_3			((STATUS) 0xF0000023)
#define STATUS_INVALID_PARAMETER_4			((STATUS) 0xF0000024)
#define STATUS_INVALID_PARAMETER_5			((STATUS) 0xF0000025)
#define STATUS_INVALID_PARAMETER_6			((STATUS) 0xF0000026)
#define STATUS_INVALID_PARAMETER_7			((STATUS) 0xF0000027)
#define STATUS_INVALID_PARAMETER_8			((STATUS) 0xF0000028)

//
// Warning codes
//

#define STATUS_PENDING						((STATUS) 0x80000003)

#define SUCCESS(Status) ((LONG)(Status)>=0)

#pragma warning(disable:4391)

#define IN
#define OUT
#define OPTIONAL
#define UNIMPLEMENTED

#define KEVAR extern "C" extern
#define KECDECL		__cdecl
#define KEAPI		__stdcall
#define KEFASTAPI	__fastcall
#define KESYSAPI	__declspec(dllexport)
#define KENAKED		__declspec(naked)
#define KENORETURN	__declspec(noreturn)
#define FORCEINLINE __inline
#define KE_FORCE_INLINE  1

#define NOTHING

#define TRUE 1
#define FALSE 0
#define NULL (0)

typedef  ULONG* va_list;
#define va_start(va, arg)	va = (va_list)((ULONG)&arg + sizeof(ULONG));
#define va_arg(va, type)	(*(type*)(va++))
#define va_end(va)			va = NULL;

#define MAXLONG ((long)0xFFFFFFFF)

#define LOBYTE(w) ((w)&0xFF)
#define HIBYTE(w) (((w)>>8)&0xFF)

#define INT3 { __asm _emit 0xEB __asm _emit 0xFE }

//
// CONTAINING_RECORD and FIELD_OFFSET macroses
//

#define CONTAINING_RECORD(address, type, field)	((type *)((PCHAR)(address) - (ULONG)(&((type *)0)->field)))
#define FIELD_OFFSET(type, field)				((LONG)(LONG_PTR)&(((type *)0)->field))

#define UNREFERENCED_PARAMETER(P)          (P)
#define UNREFERENCED_VARIABLE(V)           (V)

#define ARGUMENT_PRESENT(X) ( (X) != 0 )

typedef struct LARGE_INTEGER
{
	ULONGLONG QuadPart;
	union
	{
		ULONG HighPart;
		ULONG LowPart;
	};
} *PLARGE_INTEGER;

template <class T>
struct COUNTED_BUFFER
{
	USHORT Length;
	union
	{
		USHORT MaxLength;
		USHORT MaximumLength;	// Backward support for Windows WDM drivers
	};
	T Buffer;
};

typedef struct COUNTED_BUFFER<PVOID> CBUFFER, *PCBUFFER;
typedef struct COUNTED_BUFFER<PWSTR> UNICODE_STRING, *PUNICODE_STRING;
typedef struct COUNTED_BUFFER<PCHAR> ANSI_STRING, *PANSI_STRING;

#define max(a,b) ( (a) > (b) ? (a) : (b) )
#define min(a,b) ( (a) < (b) ? (a) : (b) )

#define STATIC_ASSERT(x)  extern char __dummy[(x)?1:-1];


LARGE_INTEGER __inline RtlMakeLargeInteger(ULONG Low, ULONG High)
{
	LARGE_INTEGER ret = { Low, High };
	return ret;
}

#define bzero(x,y) memset(x,0,y)


extern "C"
{

// Other include files
#include "rtl.h"
#include "init.h"
#include "ke.h"
#include "ex.h"
#include "mm.h"
#include "ps.h"
#include "hal.h"
#include "kd.h"
#include "ob.h"
#include "cc.h"
#include "io.h"

}

#define KdPrint(x) KiDebugPrint x


#endif

