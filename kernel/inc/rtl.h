#pragma once

KESYSAPI
INT 
_cdecl 
sprintf(
	char * buf, 
	const char *fmt, 
	...
	);

KESYSAPI
INT
KEAPI 
vsprintf(
	char *buf,
	const char *fmt, 
	va_list args
	);

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

KESYSAPI
INT
KEAPI
wcslen(
	PWSTR wstr
	);


KESYSAPI
VOID
KEAPI
RtlInitUnicodeString(
	OUT PUNICODE_STRING UnicodeString,
	IN PWSTR Buffer
	);

KESYSAPI
VOID
KEAPI
RtlDuplicateUnicodeString(
	IN PUNICODE_STRING SourceString,
	OUT PUNICODE_STRING DestinationString
	);

KESYSAPI
VOID
KEAPI
RtlAllocateUnicodeString(
	OUT PUNICODE_STRING UnicodeString,
	IN ULONG MaximumLength
	);

KESYSAPI
VOID
KEAPI
RtlFreeUnicodeString(
	IN PUNICODE_STRING UnicodeString
	);

#define RTL_NULL_UNICODE_STRING L""