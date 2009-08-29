#ifndef _KERNEL32_H_
#define _KERNEL32_H_

#include "common.h"

typedef struct _OVERLAPPED {
    ULONG_PTR Internal;
    ULONG_PTR InternalHigh;
    union {
        struct {
            DWORD Offset;
            DWORD OffsetHigh;
        };

        PVOID Pointer;
    };

    HANDLE hEvent;
} OVERLAPPED, *LPOVERLAPPED;

#define INVALID_HANDLE_VALUE ((HANDLE)-1)

#endif
