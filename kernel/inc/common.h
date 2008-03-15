#ifndef _GR8OS_
#define _GR8OS_

//
// General types
//

typedef long LONG, *PLONG;
typedef int INT, *PINT;
typedef char CHAR, *PCHAR, BOOLEAN, *PBOOLEAN, LOGICAL;
typedef unsigned long DWORD, *PDWORD, ULONG, *PULONG, ULONG_PTR, *PULONG_PTR;
typedef unsigned short WORD, *PWORD, USHORT, *PUSHORT;
typedef unsigned char BYTE, *PBYTE, UCHAR, *PUCHAR;
typedef void VOID, *PVOID, **PPVOID;
typedef long long LONGLONG, *PLONGLONG, LONG64, *PLONG64;
typedef unsigned long long ULONGLONG, *PULONGLONG, ULONG64, *PULONG64;

typedef ULONG STATUS, *PSTATUS;

#define STATUS_TIMEOUT						((STATUS)  2)
#define STATUS_MORE_AVAILABLE				((STATUS)  1)
#define STATUS_SUCCESS						((STATUS)  0)
#define STATUS_UNSUCCESSFUL					((STATUS) -1)
#define STATUS_INSUFFICIENT_RESOURCES		((STATUS) -2)
#define STATUS_ACCESS_DENIED				((STATUS) -3)
#define STATUS_ACCESS_VIOLATION				((STATUS) -4)
#define STATUS_NOT_FOUND					((STATUS) -5)
#define STATUS_INVALID_PARAMETER			((STATUS) -6)
#define STATUS_INTERNAL_FAULT				((STATUS) -7)
#define STATUS_NOT_IMPLEMENTED				((STATUS) -8)
#define STATUS_REPEAT_NEEDED				((STATUS) -9)
#define STATUS_DEVICE_NOT_READY				((STATUS) -10)
#define STATUS_PARTIAL_READ					((STATUS) -11)


#define IN
#define OUT
#define OPTIONAL

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

BOOLEAN
FORCEINLINE
isdigit(
	CHAR ch
	)
{
	if( ch >= '0' && ch <= '9' )
		return TRUE;
	return FALSE;
}

extern "C"
{
KESYSAPI
int _cdecl sprintf(char * buf, const char *fmt, ...);

KESYSAPI
int KEAPI vsprintf(char *buf, const char *fmt, va_list args);
}

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

typedef struct CBUFFER
{
	USHORT Length;
	USHORT MaxLength;
	PVOID Buffer;
} *PCBUFFER;


//
// Configuration
//

#define PS_TRACE_CONTEXT_SWITCH					FALSE
#define PS_TRACE_WAIT_LIST_PROCESSING			FALSE

// Dump each port lock/unlock attempt
#define KD_TRACE_PORT_LOCKING	0

// Print error messages
#define KD_TRACE_PORT_IO_ERRORS	0

// Dump bytes
#define KD_TRACE_LOW_LEVEL_IO	0

// Dump packets
#define TRACE_PACKETS			1


extern "C"
{

// Other include files
#include "init.h"
#include "ke.h"
#include "mm.h"
#include "ps.h"
#include "ex.h"
#include "hal.h"
#include "kd.h"

}


#endif