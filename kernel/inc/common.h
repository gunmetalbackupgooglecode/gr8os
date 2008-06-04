// begin_ddk

#ifndef _GR8OS_
#define _GR8OS_


#if DBG
#define OS_DEBUG_VERSION "-debug"
#else
#define OS_DEBUG_VERSION
#endif

#define OS_VERSION "GR8OS Ver 0.1" OS_DEBUG_VERSION

//
// General types
//

typedef long LONG, *PLONG;
typedef int INT, *PINT;
typedef short SHORT, *PSHORT;
typedef char CHAR, *PCHAR, *PSTR, BOOLEAN, *PBOOLEAN, LOGICAL, *PLOGICAL;
typedef unsigned long DWORD, *PDWORD, ULONG, *PULONG, ULONG_PTR, *PULONG_PTR;
typedef unsigned short WORD, *PWORD, USHORT, *PUSHORT
#ifdef _DEFINE_WCHART_
, wchar_t
#endif
;
typedef wchar_t WCHAR, *PWCHAR, WSTR, *PWSTR;

typedef const char *PCSTR;
typedef const wchar_t *PCWSTR;

typedef unsigned char BYTE, *PBYTE, UCHAR, *PUCHAR;
typedef void VOID, *PVOID, **PPVOID;
typedef long long LONGLONG, *PLONGLONG, LONG64, *PLONG64;
typedef unsigned long long ULONGLONG, *PULONGLONG, ULONG64, *PULONG64;

typedef ULONG STATUS, *PSTATUS;

//
// Notice codes
//
#define STATUS_PENDING						((STATUS) 0x00000007)
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
#define STATUS_DELETE_PENDING				((STATUS) 0xF0000014)
#define STATUS_IN_PAGE_ERROR				((STATUS) 0xF0000015)
#define STATUS_NO_MEDIA_IN_DEVICE			((STATUS) 0xF0000016)

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

#define STATUS_END_OF_FILE					((STATUS) 0x80000001)

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

// end_ddk

#define KGDT_R0_CODE 0x08
#define KGDT_R0_DATA 0x10
#define KGDT_VIDEO   0x18
#define KGDT_TSS     0x20
#define KGDT_PCB     0x28

/*
#pragma pack(2)
template <class T>
struct FAR_POINTER
{
	ULONG Segment;
	T* Offset;
};

template <class T>
FAR_POINTER<T> __inline RtlMakeFarPointer(USHORT Segment, T* Offset)
{
	FAR_POINTER<T> f = {Segment, Offset};
	return f;
}
#pragma pack()
*/

// begin_ddk

LARGE_INTEGER __inline RtlMakeLargeInteger(ULONG Low, ULONG High)
{
	LARGE_INTEGER ret = { Low, High };
	return ret;
}

#define bzero(x,y) memset(x,0,y)

// end_ddk

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
#define TRACE_PACKETS			0

// Dump head allocation/freeing atttempts
#define EX_TRACE_HEAP			0

// Trace object ref/deref
#define OB_TRACE_REF_DEREF		0

// Trace file caching
#define CC_TRACE_CACHING		0

// Trace operations with mutexes
#define EX_TRACE_MUTEXES		0

// Trace operations with memory descriptors
#define MM_TRACE_MMDS			0

// Trace exception unwind
#define KI_TRACE_EXCEPTION_UNWIND	0

// Trace PE loading
#define LDR_TRACE_MODULE_LOADING	0

// Trace FS FAT reading
#define FSFAT_TRACE_READING			0

// Hang on bugcheck
#define KE_HANG_ON_BUGCHECK			1

// Disable cache rebuilding
#define CC_DISABLE_CACHE_REBUILDING	1

// Extended info on quiet bugcheck
#define KE_QUIET_BUGCHECK_EXTENDED	0


// begin_ddk

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

// end_ddk

// Built-in driver includes
#include "builtin/floppy/floppy.h"
#include "builtin/fsfat/fsfat.h"

// begin_ddk
}

#define KdPrint(x) KiDebugPrint x


#endif

// end_ddk
