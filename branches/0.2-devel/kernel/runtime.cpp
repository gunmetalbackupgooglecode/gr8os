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
int CRTAPI _snprintf(char * buf, size_t cnt, const char *fmt, ...)
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
int CRTAPI vsprintf(char *buf, const char *fmt, va_list args)
{
	return _vsnprintf(buf,MAXLONG,fmt,args);
}

KESYSAPI
int
CRTAPI
printf(
	const char *fmt,
	...
	)
{
	char buffer[1024];
	va_list va;
	int l;

	va_start (va, fmt);
	l = _vsnprintf (buffer, sizeof(buffer), fmt, va);
	va_end (va);

	KiDebugPrintRaw (buffer);
	return l;
}

__declspec(noreturn)
KESYSAPI
void
CRTAPI
panic(
	const char *fmt,
	...
	)
{
	char buffer[1024];
	va_list va;
	int l;

	va_start (va, fmt);
	l = _vsnprintf (buffer, sizeof(buffer), fmt, va);
	va_end (va);

	KiDebugPrintRaw (buffer);
	__asm cli
	__asm hlt
	for(;;);
}

KESYSAPI
int
CRTAPI
fprintf(
	void *v,
	const char *fmt,
	...
	)
{
	char buffer[1024];
	va_list va;
	int l;

	va_start (va, fmt);
	l = _vsnprintf (buffer, sizeof(buffer), fmt, va);
	va_end (va);

	KiDebugPrintRaw (buffer);
	return l;
}

KESYSAPI
char*
CRTAPI
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
CRTAPI
strcpy(
	char *s1,
	const char *s2
	)
{
	return strncpy (s1, s2, strlen(s2));
}


KESYSAPI
INT
CRTAPI
wcslen(
	PCWSTR wstr
	)
{
	int length;
	for (length = 0;*wstr;wstr++,length++);
	return length;
}

KESYSAPI
INT
CRTAPI
strlen(
	const char *str
	)
{
	int length;
	for (length = 0;*str;str++,length++);
	return length;
}

KESYSAPI
char*
CRTAPI
strchr(
	const char *str,
	char chr
	)
{
	for (const char *ch=str; *ch; ch++)
		if (*ch == chr)
			return (char*) ch;

	return NULL;
}

KESYSAPI
char*
CRTAPI
strrchr(
	const char *str,
	char chr
	)
{
	int len = strlen(str);
	for (const char *ch=(str+len-1); ch!=str; ch--)
		if (*ch == chr)
			return (char*) ch;

	return NULL;
}

KESYSAPI
ULONG
CRTAPI
wcstomb(
	char *mbs,
	const WCHAR *wcs,
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
CRTAPI
mbstowcs(
	WCHAR *wcs,
	const char *mbs,
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
CRTAPI
strncmp(
	const char *s1,
	const char *s2,
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
CRTAPI
strcmp(
	const char *s1,
	const char *s2
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
CRTAPI
strnicmp(
	const char *s1,
	const char *s2,
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
CRTAPI
stricmp(
	const char *s1,
	const char *s2
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
CRTAPI
strcat(
	char *s1,
	const char *s2
	)
{
	return strcpy (s1 + strlen(s1), s2);
}

KESYSAPI
char*
CRTAPI
strstr(
	const char *s1,
	const char *s2
	)
{
	int l2 = strlen(s2);
	for (const char *sp = s1; *sp; sp++)
	{
		if (*sp == *s2 &&
				!memcmp (&sp[1], &s2[1], l2-1))
		{
			return (char*) sp;
		}
	}
	return NULL;
}

KESYSAPI
PWSTR
CRTAPI
wcsstr(
	PCWSTR s1,
	PCWSTR s2
	)
{
	int l2 = wcslen(s2);
	for (PCWSTR sp = s1; *sp; sp++)
	{
		if (*sp == *s2 &&
				!memcmp (&sp[1], &s2[1], (l2-1)*2))
		{
			return (PWSTR) sp;
		}
	}
	return NULL;
}

KESYSAPI
INT
CRTAPI
wcscmp(
	PCWSTR wstr,
	PCWSTR wstr2
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
CRTAPI
wcsicmp(
	PCWSTR wstr,
	PCWSTR wstr2
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
CRTAPI
wcsncpy(
	PWSTR dst,
	PCWSTR src,
	INT count
	)
{
	for (int i=0;i<count && (dst[i] = src[i]);i++);
}

KESYSAPI
VOID
CRTAPI
wcscpy(
	PWSTR dst,
	PCWSTR src
	)
{
	wcsncpy (dst, src, wcslen(src)+1);
}

KESYSAPI
VOID
CRTAPI
wcscat(
	PWSTR dst,
	PCWSTR src
	)
{
	wcscpy (dst + wcslen(dst), src);
}

KESYSAPI
VOID
KEAPI
wcssubstr(
	PCWSTR SourceString,
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
CRTAPI
wcsrchr(
	 PCWSTR SourceString,
	 WCHAR Char
	 )
{
	for (PCWSTR end = SourceString + wcslen(SourceString) - 1; end>=SourceString; end--)
	{
		if (*end == Char)
			return (PWSTR) end;
	}
	return NULL;
}


KESYSAPI
VOID
KEAPI
RtlInitUnicodeString(
	OUT PUNICODE_STRING UnicodeString,
	IN PCWSTR Buffer
	)
{
	UnicodeString->Buffer = (PWSTR) Buffer;
	UnicodeString->Length = wcslen(Buffer)*2;
	UnicodeString->MaxLength = UnicodeString->Length + 2;
}

KESYSAPI
VOID
KEAPI
RtlInitString(
	OUT PSTRING String,
	IN PCSTR Buffer
	)
{
	String->Buffer = (PSTR) Buffer;
	String->Length = strlen(Buffer);
	String->MaxLength = String->Length + 2;
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

/*
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
#define STATUS_NO_MORE_ENTRIES				((STATUS) 0xF0000017)
#define STATUS_FILE_SYSTEM_NOT_RECOGNIZED	((STATUS) 0xF0000018)

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
*/

#define STATUS_ERROR						((STATUS) 0xF0000000)
#define STATUS_WARNING						((STATUS) 0x80000000)
#define STATUS_NOTICE						((STATUS) 0x00000000)
#define STATUS_TYPE							0xF0000000

#define STATUS_TABLE_END {0,0,0}

STATUS_DESCRIPTION
RtlpNoticeCodes[] = {
	{ STATUS_PENDING, "STATUS_PENDING", "Request has been delayed" },
	{ STATUS_CACHED, "STATUS_CACHED", "Caching was performed to satisfy the request" },
	{ STATUS_ALREADY_FREE, "STATUS_ALREADY_FREE", "A freeing attempt was performed on already free DMA channel" },
	{ STATUS_FINISH_PARSING, "STATUS_FINISH_PARSING", "Object parse routine returns this status to stop parsing object path" },
	{ STATUS_REPARSE, "STATUS_REPARSE", "Symlink parse routine returns this status to reparse object path" },
	{ STATUS_TIMEOUT, "STATUS_TIMEOUT", "Request timed out" },
	{ STATUS_MORE_AVAILABLE, "STATUS_MORE_AVAILABLE", "More information available, request was partly satisfied" },
	{ STATUS_SUCCESS, "STATUS_SUCCESS", "Request succeeded" },
	STATUS_TABLE_END
};

STATUS_DESCRIPTION
RtlpWarningCodes[] = {
	{ STATUS_END_OF_FILE, "STATUS_END_OF_FILE", "The end of file reached while reading it" },
	STATUS_TABLE_END
};

STATUS_DESCRIPTION
RtlpErrorCodes[] = {
	{ STATUS_UNSUCCESSFUL, "STATUS_UNSUCCESSFUL", "Request failed" },
	{ STATUS_INSUFFICIENT_RESOURCES, "STATUS_INSUFFICIENT_RESOURCES", "Not enough resources to complete the request" },
	{ STATUS_ACCESS_DENIED, "STATUS_ACCESS_DENIED", "Not enough rights to complete the request, access was denied" },
	{ STATUS_ACCESS_VIOLATION, "STATUS_ACCESS_VIOLATION", "A memory access violation occurred while processing the request" },
	{ STATUS_NOT_FOUND, "STATUS_NOT_FOUND", "An object was not found" },
	{ STATUS_INVALID_PARAMETER, "STATUS_INVALID_PARAMETER", "Request does not support such a parameter" },
	{ STATUS_INTERNAL_FAULT, "STATUS_INTERNAL_FAULT", "Internal fault occurred while processing the request" },
	{ STATUS_NOT_IMPLEMENTED, "STATUS_NOT_IMPLEMENTED", "The requested operation is not implemented yet" },
	{ STATUS_REPEAT_NEEDED, "STATUS_REPEAT_NEEDED", "The operation should be repeated to success" },
	{ STATUS_DEVICE_NOT_READY, "STATUS_DEVICE_NOT_READY", "Device is not ready to complete the request" },
	{ STATUS_PARTIAL_COMPLETION, "STATUS_PARTIAL_COMPLETION", "Only the part of request completed successfully" },
	{ STATUS_IN_USE, "STATUS_IN_USE", "The requested object is in use and caller should wait until it becomes released" },
	{ STATUS_INVALID_HANDLE, "STATUS_INVALID_HANDLE", "Invalid handle passed" },
	{ STATUS_INVALID_FUNCTION, "STATUS_INVALID_FUNCTION", "Not valid operation is being performed" },
	{ STATUS_NOT_SUPPORTED, "STATUS_NOT_SUPPORTED", "Requested operation is not supported" },
	{ STATUS_DATATYPE_MISALIGNMENT, "STATUS_DATATYPE_MISALIGNMENT", "Data alignment should be met to complete the request" },
	{ STATUS_BUSY, "STATUS_BUSY", "The requested DMA channel is busy now" },
	{ STATUS_INVALID_FILE_FOR_IMAGE, "STATUS_INVALID_FILE_FOR_IMAGE", "File is not valid to be interpreted as image" },
	{ STATUS_PRIVILEGE_NOT_HELD, "STATUS_PRIVILEGE_NOT_HELD", "Not enough privileges to complete the request" },
	{ STATUS_DELETE_PENDING, "STATUS_DELETE_PENDING", "Object is going to be deleted and cannot be used" },
	{ STATUS_IN_PAGE_ERROR, "STATUS_IN_PAGE_ERROR", "Access fault resolving failed because of internal in-page error" },
	{ STATUS_NO_MEDIA_IN_DEVICE, "STATUS_NO_MEDIA_IN_DEVICE", "No media in device" },
	{ STATUS_NO_MORE_ENTRIES, "STATUS_NO_MORE_ENTRIES", "No more entries to continue the enumeration" },
	{ STATUS_FILE_SYSTEM_NOT_RECOGNIZED, "STATUS_FILE_SYSTEM_NOT_RECOGNIZED", "File system is not recognized on the media" },
	STATUS_TABLE_END
};


KESYSAPI
PSTATUS_DESCRIPTION
KEAPI
RtlLookupStatusString (
	STATUS Status
	)
/**
	Internal routine to lookup status description by value
*/
{
	STATUS_DESCRIPTION *ptr = NULL;
	switch (Status & STATUS_TYPE)
	{
	case STATUS_NOTICE:  ptr = RtlpNoticeCodes;  break;
	case STATUS_WARNING: ptr = RtlpWarningCodes; break;
	case STATUS_ERROR:   ptr = RtlpErrorCodes;   break;
	default: ASSERT (FALSE);
	}

	while (ptr->String)
	{
		if (ptr->Status == Status)
			return ptr;
		ptr ++;
	}

	return NULL;
}

extern "C" {
	struct F
	{
		int dummy;
	};
	
	F _iob[4];
}

KESYSAPI
void*
CRTAPI
memmem(
	const void* p1,
	size_t l1,
	const void *p2,
	size_t l2
	)
{
	const char *begin;
	const char *const last_possible = (const char*) p1 + l1 - l2;
	if (l2 == 0)
		return (void*)p1;

	if (l1 < l2)
		return NULL;

	for (begin = (const char*) p1; begin <= last_possible; ++begin)
	{
		if (begin[0] == ((const char*) p2)[0] &&
			!memcmp((const void*) &begin[1],
			(const void*) ((const char*) p2 + 1),
			l2 - 1))
		{
			return (void*) begin;
		}
	}

	return NULL;
}
