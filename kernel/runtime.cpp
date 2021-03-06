//
// FILE:		runtime.cpp
// CREATED:		28-Feb-2008  by Great
// PART:        General
// ABSTRACT:
//			Run-Time routines to support formatted output.
//			From ReactOS kernel
//

#include "common.h"

/*
 * PROGRAMMERS:     David Welch
 *                  Eric Kohl
 *
 * TODO:
 *   - Verify the implementation of '%Z'.
 */

/*
 *  linux/lib/vsprintf.c
 *
 *  Copyright (C) 1991, 1992  Linus Torvalds
 */

/* vsprintf.c -- Lars Wirzenius & Linus Torvalds. */
/*
 * Wirzenius wrote this portably, Torvalds fucked it up :-)
 */

#define ZEROPAD	1		/* pad with zero */
#define SIGN	2		/* unsigned/signed long */
#define PLUS	4		/* show plus */
#define SPACE	8		/* space if plus */
#define LEFT	16		/* left justified */
#define SPECIAL	32		/* 0x */
#define LARGE	64		/* use 'ABCDEF' instead of 'abcdef' */
#define REMOVEHEX	256		/* use 256 as remve 0x frim BASE 16  */
typedef struct {
    unsigned int mantissal:32;
    unsigned int mantissah:20;
    unsigned int exponent:11;
    unsigned int sign:1;
} double_t;

static
__inline
int
_isinf(double __x)
{
	union
	{
		double*   __x;
		double_t*   x;
	} x;

	x.__x = &__x;
	return ( x.x->exponent == 0x7ff  && ( x.x->mantissah == 0 && x.x->mantissal == 0 ));
}

static
__inline
int
_isnan(double __x)
{
	union
	{
		double*   __x;
		double_t*   x;
	} x;
    	x.__x = &__x;
	return ( x.x->exponent == 0x7ff  && ( x.x->mantissah != 0 || x.x->mantissal != 0 ));
}


static
__inline
int
do_div(long *n, int base)
{
    int a;
    a = ((unsigned long) *n) % (unsigned) base;
    *n = ((unsigned long) *n) / (unsigned) base;
    return a;
}


static int skip_atoi(const char **s)
{
	int i=0;

	while (isdigit(**s))
		i = i*10 + *((*s)++) - '0';
	return i;
}


static char *
number(char * buf, char * end, long num, int base, int size, int precision, int type)
{
	char c,sign,tmp[66];
	const char *digits;
	const char *small_digits = "0123456789abcdefghijklmnopqrstuvwxyz";
	const char *large_digits = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";
	int i;

	digits = (type & LARGE) ? large_digits : small_digits;
	if (type & LEFT)
		type &= ~ZEROPAD;
	if (base < 2 || base > 36)
		return 0;
	c = (type & ZEROPAD) ? '0' : ' ';
	sign = 0;
	if (type & SIGN) {
		if (num < 0) {
			sign = '-';
			num = -num;
			size--;
		} else if (type & PLUS) {
			sign = '+';
			size--;
		} else if (type & SPACE) {
			sign = ' ';
			size--;
		}
	}

	if ((type & SPECIAL) && ((type & REMOVEHEX) == 0)) {
		if (base == 16)
			size -= 2;

	}
	i = 0;
	if ((num == 0) && (precision !=0))
		tmp[i++] = '0';
	else while (num != 0)
		tmp[i++] = digits[do_div(&num,base)];
	if (i > precision)
		precision = i;
	size -= precision;
	if (!(type&(ZEROPAD+LEFT))) {
		while(size-->0) {
			if (buf <= end)
				*buf = ' ';
			++buf;
		}
	}
	if (sign) {
		if (buf <= end)
			*buf = sign;
		++buf;
	}

	if ((type & SPECIAL) && ((type & REMOVEHEX) == 0)) {
		 if (base==16) {
			if (buf <= end)
				*buf = '0';
			++buf;
			if (buf <= end)
				*buf = digits[33];
			++buf;
		}
	}

	if (!(type & LEFT)) {
		while (size-- > 0) {
			if (buf <= end)
				*buf = c;
			++buf;
		}
	}
	while (i < precision--) {
		if (buf <= end)
			*buf = '0';
		++buf;
	}
	while (i-- > 0) {
		if (buf <= end)
			*buf = tmp[i];
		++buf;
	}
	while (size-- > 0) {
		if (buf <= end)
			*buf = ' ';
		++buf;
	}

	return buf;
}

/*
static char *
numberf(char * buf, char * end, double num, int base, int size, int precision, int type)
{
	char c,sign,tmp[66];
	const char *digits;
	const char *small_digits = "0123456789abcdefghijklmnopqrstuvwxyz";
	const char *large_digits = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";
	int i;
	long long x;

	digits = (type & LARGE) ? large_digits : small_digits;
	if (type & LEFT)
		type &= ~ZEROPAD;
	if (base < 2 || base > 36)
		return 0;
	c = (type & ZEROPAD) ? '0' : ' ';
	sign = 0;
	if (type & SIGN) {
		if (num < 0) {
			sign = '-';
			num = -num;
			size--;
		} else if (type & PLUS) {
			sign = '+';
			size--;
		} else if (type & SPACE) {
			sign = ' ';
			size--;
		}
	}
	if (type & SPECIAL)  {
		if (base == 16)
			size -= 2;
		else if (base == 8)
			size--;
	}
	i = 0;
	if (num == 0)
		tmp[i++] = '0';
	else while (num != 0)
    {
        x = (long long)num;
		tmp[i++] = digits[do_div(&x,base)];
		num = (double)x;
    }
	if (i > precision)
		precision = i;
	size -= precision;
	if (!(type&(ZEROPAD+LEFT))) {
		while(size-->0) {
			if (buf <= end)
				*buf = ' ';
			++buf;
		}
	}
	if (sign) {
		if (buf <= end)
			*buf = sign;
		++buf;
	}
	if (type & SPECIAL) {
		if (base==8) {
			if (buf <= end)
				*buf = '0';
			++buf;
		} else if (base==16) {
			if (buf <= end)
				*buf = '0';
			++buf;
			if (buf <= end)
				*buf = digits[33];
			++buf;
		}
	}
	if (!(type & LEFT)) {
		while (size-- > 0) {
			if (buf <= end)
				*buf = c;
			++buf;
		}
	}
	while (i < precision--) {
		if (buf <= end)
			*buf = '0';
		++buf;
	}
	while (i-- > 0) {
		if (buf <= end)
			*buf = tmp[i];
		++buf;
	}
	while (size-- > 0) {
		if (buf <= end)
			*buf = ' ';
		++buf;
	}
	return buf;
}
*/

static char*
string(char* buf, char* end, const char* s, int len, int field_width, int precision, int flags)
{
	int i;
    char c;

    c = (flags & ZEROPAD) ? '0' : ' ';

	if (s == NULL)
	{
		s = "<NULL>";
		len = 6;
	}
	else
	{
		if (len == -1)
		{
			len = 0;
			while ((unsigned int)len < (unsigned int)precision && s[len])
				len++;
		}
		else
		{
			if ((unsigned int)len > (unsigned int)precision)
				len = precision;
		}
	}
	if (!(flags & LEFT))
		while (len < field_width--)
		{
			if (buf <= end)
				*buf = c;
			++buf;
		}
	for (i = 0; i < len; ++i)
	{
		if (buf <= end)
			*buf = *s++;
		++buf;
	}
	while (len < field_width--)
	{
		if (buf <= end)
			*buf = ' ';
		++buf;
	}
	return buf;
}

static char*
stringw(char* buf, char* end, const wchar_t* sw, int len, int field_width, int precision, int flags)
{
	int i;
	char c;

    c = (flags & ZEROPAD) ? '0' : ' ';

	if (sw == NULL)
	{
		sw = L"<NULL>";
		len = 6;
	}
	else
	{
		if (len == -1)
		{
			len = 0;
			while ((unsigned int)len < (unsigned int)precision && sw[len])
				len++;
		}
		else
		{
			if ((unsigned int)len > (unsigned int)precision)
				len = precision;
		}
	}
	if (!(flags & LEFT))
		while (len < field_width--)
		{
			if (buf <= end)
				*buf = c;
			buf++;
		}
	for (i = 0; i < len; ++i)
	{
		if (buf <= end)
			*buf = (unsigned char)(*sw++);
		buf++;
	}
	while (len < field_width--)
	{
		if (buf <= end)
			*buf = ' ';
		buf++;
	}
	return buf;
}

/*
 * @implemented
 */
int __cdecl _vsnprintf(char *buf, size_t cnt, const char *fmt, va_list args)
{
//	int len;
//	unsigned long long num;
//	double _double;
	unsigned long num;

	int base;
	char *str, *end;
	const char *s;
	const wchar_t *sw;

	int flags;		/* flags to number() */

	int field_width;	/* width of output field */
	int precision;		/* min. # of digits for integers; max
				   number of chars for from string */
	int qualifier;		/* 'h', 'l', 'L', 'I' or 'w' for integer fields */

    /* clear the string buffer with zero so we do not need NULL terment it at end */

	str = buf;
	end = buf + cnt - 1;
	if (end < buf - 1) {
		end = ((char *) -1);
		cnt = end - buf + 1;
	}

	for ( ; *fmt ; ++fmt) {
		if (*fmt != '%') {
			if (str <= end)
				*str = *fmt;
			++str;
			continue;
		}

		/* process flags */
		flags = 0;
		repeat:
			++fmt;		/* this also skips first '%' */
			switch (*fmt) {
				case '-': flags |= LEFT; goto repeat;
				case '+': flags |= PLUS; goto repeat;
				case ' ': flags |= SPACE; goto repeat;
				case '#': flags |= SPECIAL; goto repeat;
				case '0': flags |= ZEROPAD; goto repeat;
			}

		/* get field width */
		field_width = -1;
		if (isdigit(*fmt))
			field_width = skip_atoi(&fmt);
		else if (*fmt == '*') {
			++fmt;
			/* it's the next argument */
			field_width = va_arg(args, int);
			if (field_width < 0) {
				field_width = -field_width;
				flags |= LEFT;
			}
		}

		/* get the precision */
		precision = -1;
		if (*fmt == '.') {
			++fmt;
			if (isdigit(*fmt))
				precision = skip_atoi(&fmt);
			else if (*fmt == '*') {
				++fmt;
				/* it's the next argument */
				precision = va_arg(args, int);
			}
			if (precision < 0)
				precision = 0;
		}

		/* get the conversion qualifier */
		qualifier = -1;
		if (*fmt == 'h' || *fmt == 'l' || *fmt == 'L' || *fmt == 'w') {
			qualifier = *fmt;
			++fmt;
		} else if (*fmt == 'I' && *(fmt+1) == '6' && *(fmt+2) == '4') {
			qualifier = *fmt;
			fmt += 3;
		} else if (*fmt == 'I' && *(fmt+1) == '3' && *(fmt+2) == '2') {
			qualifier = 'l';
			fmt += 3;
		} else if (*fmt == 'F' && *(fmt+1) == 'p') {
			fmt += 1;
            flags |= REMOVEHEX;
        }

		/* default base */
		base = 10;

		switch (*fmt) {
		case 'c': /* finished */
             if (qualifier == 'l' || qualifier == 'w') {
	              wchar_t sw1[2];
				/* print unicode string */
                sw1[0] = (wchar_t) va_arg(args, int);
                sw1[1] = 0;
				str = stringw(str, end, (wchar_t *)&sw1, -1, field_width, precision, flags);
			} else {
                char s1[2];
				/* print ascii string */
                s1[0] = ( unsigned char) va_arg(args, int);
                s1[1] = 0;
				str = string(str, end, (char *)&s1, -1,  field_width, precision, flags);
			}
            continue;

		case 'C': /* finished */
			if (!(flags & LEFT))
				while (--field_width > 0) {
					if (str <= end)
						*str = ' ';
					++str;
				}
			if (qualifier == 'h') {
				if (str <= end)
					*str = (unsigned char) va_arg(args, int);
				++str;
			} else {
				if (str <= end)
					*str = (unsigned char)(wchar_t) va_arg(args, int);
				++str;
			}
			while (--field_width > 0) {
				if (str <= end)
					*str = ' ';
				++str;
			}
			continue;

		case 's': /* finished */
			if (qualifier == 'l' || qualifier == 'w') {
				/* print unicode string */
				sw = va_arg(args, wchar_t *);
				str = stringw(str, end, sw, -1, field_width, precision, flags);
			} else {
				/* print ascii string */
				s = va_arg(args, char *);
				str = string(str, end, s, -1,  field_width, precision, flags);
			}
			continue;

		case 'S':
			if (qualifier == 'h') {
				/* print ascii string */
				s = va_arg(args, char *);
				str = string(str, end, s, -1,  field_width, precision, flags);
			} else {
				/* print unicode string */
				sw = va_arg(args, wchar_t *);
				str = stringw(str, end, sw, -1, field_width, precision, flags);
			}
			continue;

		case 'Z':
			{
				PUNICODE_STRING sw = va_arg(args, PUNICODE_STRING);
				str = stringw(str, end, sw->Buffer, sw->Length, field_width, precision, flags);
			}
			continue;

		case 'p':
            if ((flags & LARGE) == 0)
                flags |= LARGE;

			if (field_width == -1) {
				field_width = 2 * sizeof(void *);
				flags |= ZEROPAD;
			}
			str = number(str, end,
				(unsigned long) va_arg(args, void *), 16,
				field_width, precision, flags);
			continue;

		case 'n':
			/* FIXME: What does C99 say about the overflow case here? */
			if (qualifier == 'l') {
				long * ip = va_arg(args, long *);
				*ip = (str - buf);
			} else {
				int * ip = va_arg(args, int *);
				*ip = (str - buf);
			}
			continue;

		/*
		// float number formats - set up the flags and "break" 
        case 'e':
		case 'E':
		case 'f':
		case 'g':
		case 'G':
          _double = (double)va_arg(args, double);
         if ( _isnan(_double) ) {
            s = "Nan";
            len = 3;
            while ( len > 0 ) {
               if (str <= end)
					*str = *s++;
				++str;
               len --;
            }
         } else if ( _isinf(_double) < 0 ) {
            s = "-Inf";
            len = 4;
            while ( len > 0 ) {
              	if (str <= end)
					*str = *s++;
				++str;
               len --;
            }
         } else if ( _isinf(_double) > 0 ) {
            s = "+Inf";
            len = 4;
            while ( len > 0 ) {
               if (str <= end)
					*str = *s++;
				++str;
               len --;
            }
         } else {
            if ( precision == -1 )
               precision = 6;
               	str = numberf(str, end, (int)_double, base, field_width, precision, flags);
         }

          continue;
		*/


		/* integer number formats - set up the flags and "break" */
		case 'o':
			base = 8;
			break;

		case 'b':
			base = 2;
			break;

		case 'X':
			flags |= LARGE;
		case 'x':
			base = 16;
			break;

		case 'd':
		case 'i':
			flags |= SIGN;
		case 'u':
			break;

		default:
			if (*fmt) {
				if (str <= end)
					*str = *fmt;
				++str;
			} else
				--fmt;
			continue;
		}

		/*
		if (qualifier == 'I')
			num = va_arg(args, unsigned long long);
		else*/ if (qualifier == 'l') {
			if (flags & SIGN)
				num = va_arg(args, long);
			else
				num = va_arg(args, unsigned long);
		}
		else if (qualifier == 'h') {
			if (flags & SIGN)
				num = va_arg(args, int);
			else
				num = va_arg(args, unsigned int);
		}
		else {
			if (flags & SIGN)
				num = va_arg(args, int);
			else
				num = va_arg(args, unsigned int);
		}
		str = number(str, end, num, base, field_width, precision, flags);
	}
	if (str <= end)
		*str = '\0';
	else if (cnt > 0)
	{
		/* don't write out a null byte if the buf size is zero */
		//*end = '\0';
	   if ( (size_t)(str-buf) > cnt )
       {
		 *end = '\0';
       }
       else
       {
           end++;
          *end = '\0';
       }

    }
	return str-buf;
}


/*
 * @implemented
 */
KESYSAPI
int _cdecl sprintf(char * buf, const char *fmt, ...)
{
	va_list args;
	int i;

	va_start(args, fmt);
	i=_vsnprintf(buf,MAXLONG,fmt,args);
	va_end(args);
	return i;
}


/*
 * @implemented
 */
int _snprintf(char * buf, size_t cnt, const char *fmt, ...)
{
	va_list args;
	int i;

	va_start(args, fmt);
	i=_vsnprintf(buf,cnt,fmt,args);
	va_end(args);
	return i;
}


/*
 * @implemented
 */
int KEAPI vsprintf(char *buf, const char *fmt, va_list args)
{
	return _vsnprintf(buf,MAXLONG,fmt,args);
}

KESYSAPI
char*
KEAPI
strncpy(
	char* to,
	const char* from, 
	int count
	)
{
	for (int i=0;i<count && (to[i] = from[i]);i++);
	return to;
}

KESYSAPI
char*
KEAPI
strcpy(
	char *s1,
	char *s2
	)
{
	return strncpy (s1, s2, strlen(s2));
}


KESYSAPI
INT
KEAPI
wcslen(
	PWSTR wstr
	)
{
	int length;
	for (length = 0;*wstr;wstr++,length++);
	return length;
}

KESYSAPI
INT
KEAPI
strlen(
	char *str
	)
{
	int length;
	for (length = 0;*str;str++,length++);
	return length;
}

KESYSAPI
char*
KEAPI
strchr(
	char *str,
	char chr
	)
{
	for (char *ch=str; *ch; ch++)
		if (*ch == chr)
			return ch;

	return NULL;
}

KESYSAPI
ULONG
KEAPI
wcstomb(
	char *mbs,
	WCHAR *wcs,
	ULONG count
	)
{
	if (count == -1)
		count = wcslen(wcs);

	ULONG i=0;

	for (; i<count; i++)
	{
		mbs[i] = (char) wcs[i];
	}

	mbs[i] = 0;
	return i;
}

KESYSAPI
ULONG
KEAPI
mbstowcs(
	WCHAR *wcs,
	char *mbs,
	ULONG count
	)
{
	if (count == -1)
		count = strlen(mbs);

	ULONG i=0;

	for (; i<count; i++)
	{
		wcs[i] = (WCHAR) mbs[i];
	}

	wcs[i] = 0;
	return i;
}

KESYSAPI
INT 
KEAPI
strncmp(
	char *s1,
	char *s2,
	ULONG count
	)
{
	for (ULONG i=0; i<count; i++)
	{
		char diff = s1[i] - s2[i];

		if (diff)
			return diff;
	}

	return 0;
}

KESYSAPI
INT 
KEAPI
strcmp(
	char *s1,
	char *s2
	)
{
	int l1 = strlen(s1);
	int l2 = strlen(s2);

	if( l1 != l2 )
		return l2-l1;

	return strncmp (s1, s2, l1);
}


KESYSAPI
INT 
KEAPI
strnicmp(
	char *s1,
	char *s2,
	ULONG count
	)
{
	for (ULONG i=0; i<count; i++)
	{
		char diff = UPCASE(s1[i]) - UPCASE(s2[i]);

		if (diff)
			return diff;
	}

	return 0;
}

KESYSAPI
INT 
KEAPI
stricmp(
	char *s1,
	char *s2
	)
{
	int l1 = strlen(s1);
	int l2 = strlen(s2);

	if( l1 != l2 )
		return l2-l1;

	return strnicmp (s1, s2, l1);
}

KESYSAPI
char* 
KEAPI
strcat(
	char *s1,
	char *s2
	)
{
	return strcpy (s1 + strlen(s1), s2);
}

KESYSAPI
INT
KEAPI
wcscmp(
	PWSTR wstr,
	PWSTR wstr2
	)
{
	int l1 = wcslen(wstr);
	int l2 = wcslen(wstr2);

	if (l1!=l2)
		return -1;

	for (int i=0; i<l1;i++)
	{
		WCHAR diff = wstr[i] - wstr2[i];

		if (diff)
			return diff;
	}

	return 0;
}

KESYSAPI
INT
KEAPI
wcsicmp(
	PWSTR wstr,
	PWSTR wstr2
	)
{
	int l1 = wcslen(wstr);
	int l2 = wcslen(wstr2);

	if (l1!=l2)
		return -1;

	for (int i=0; i<l1;i++)
	{
		WCHAR diff = UPCASEW(wstr[i]) - UPCASEW(wstr2[i]);

		if (diff)
			return diff;
	}

	return 0;
}

KESYSAPI
VOID
KEAPI
wcsncpy(
	PWSTR dst,
	PWSTR src,
	INT count
	)
{
	for (int i=0;i<count && (dst[i] = src[i]);i++);
}

KESYSAPI
VOID
KEAPI
wcscpy(
	PWSTR dst,
	PWSTR src
	)
{
	wcsncpy (dst, src, wcslen(src)+1);
}

KESYSAPI
VOID
KEAPI
wcscat(
	PWSTR dst,
	PWSTR src
	)
{
	wcscpy (dst + wcslen(dst), src);
}

KESYSAPI
VOID
KEAPI
wcssubstr(
	PWSTR SourceString,
	INT StartPosition,
	INT Length,
	PWSTR DestinationBuffer
	)
{
	wcsncpy (DestinationBuffer, SourceString + StartPosition, Length);
	DestinationBuffer[Length]=0;
}

KESYSAPI
PWSTR
KEAPI
wcsrchr(
	 PWSTR SourceString,
	 WCHAR Char
	 )
{
	for (PWSTR end = SourceString + wcslen(SourceString) - 1; end>=SourceString; end--)
	{
		if (*end == Char)
			return end;
	}
	return NULL;
}


KESYSAPI
VOID
KEAPI
RtlInitUnicodeString(
	OUT PUNICODE_STRING UnicodeString,
	IN PWSTR Buffer
	)
{
	UnicodeString->Buffer = Buffer;
	UnicodeString->Length = wcslen(Buffer)*2;
	UnicodeString->MaxLength = UnicodeString->Length + 2;
}

KESYSAPI
VOID
KEAPI
RtlDuplicateUnicodeString(
	IN PUNICODE_STRING SourceString,
	OUT PUNICODE_STRING DestinationString
	)
{
	*DestinationString = *SourceString;
	DestinationString->Buffer = (PWSTR) ExAllocateHeap (FALSE, SourceString->MaxLength);
	memcpy (DestinationString->Buffer, SourceString->Buffer, SourceString->MaximumLength);
}

KESYSAPI
VOID
KEAPI
RtlFreeUnicodeString(
	IN PUNICODE_STRING UnicodeString
	)
{
	ExFreeHeap (UnicodeString->Buffer);
	UnicodeString->Length = UnicodeString->MaximumLength = 0;
}

void DumpMemory( DWORD base, ULONG length, DWORD DisplayBase )
{
#define ptc(x) KiDebugPrint("%c", x)
#define is_print(c) ( (c) >= '!' && (c) <= '~' )
	bool left = true;
	bool newline = true;
	int based_length = length;
	int i;
	int baseoffs;

	if( DisplayBase==-1 ) DisplayBase = base;

	baseoffs = DisplayBase - (DisplayBase&0xFFFFFFF0);
	base -= baseoffs;
	DisplayBase &= 0xFFFFFFF0;

	length += baseoffs;

	if( length % 16 )
		based_length += 16-(length%16);

	for( i=0; i<based_length; i++ )
	{
		if( newline )
		{
			newline = false;
			KiDebugPrint("%08x [", DisplayBase+i);
		}

#define b (*((unsigned char*)base+i))

		if( left )
		{
			if( (unsigned)i < length && i >= baseoffs )
				KiDebugPrint(" %02x", b);
			else
				KiDebugPrint("   ");

			if( (i+1) % 16 == 0 )
			{
				left = false;
				i -= 16;
				KiDebugPrint(" ] ");
				continue;
			}

			if( (i+1) % 8 == 0 )
			{
				KiDebugPrint(" ");
			}

		}
		else
		{
			if( (unsigned)i < length && i >= baseoffs )
				ptc(is_print(b)?b:'.');
			else
				ptc(' ');

			if( (i+1) % 16 == 0 )
			{
				KiDebugPrint("\n");
				newline = true;
				left = true;
				continue;
			}
			if( (i+1) % 8 == 0 )
			{
				KiDebugPrint(" ");
			}
		}
	}
}

/* EOF */
