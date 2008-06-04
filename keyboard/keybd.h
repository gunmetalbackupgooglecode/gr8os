#pragma once

typedef struct KEYBOARD_STATUS_BYTE
{
	UCHAR Shift : 1;
	UCHAR RightShift : 1;
	UCHAR Ctrl : 1;
	UCHAR Alt : 1;
	UCHAR NumLock : 1;
	UCHAR CapsLock : 1;
	UCHAR ScrollLock : 1;
	UCHAR Reserved : 1;
} *PKEYBOARD_STATUS_BYTE;

typedef struct KEYBOARD_INDICATORS
{
	UCHAR NumLock : 1;
	UCHAR CapsLock : 1;
	UCHAR ScrollLock : 1;
	UCHAR Reserved : 5;
} *PKEYBOARD_INDICATORS;

#define IOCTL_KEYBD_SET_LED_INDICATORS  0x10000001

#define KBD_BUFFER_SIZE 256
extern char KeybdCyclicBuffer[KBD_BUFFER_SIZE];
extern LOCK KeybdCyclicBufferLock;

extern int KeybdCurrentPos;

extern PKEYBOARD_STATUS_BYTE KeybdStatusByte;
extern KEYBOARD_INDICATORS KeybdIndicators;

VOID KeybdSetIndicators ();
void init_kbd();
void irq1_handler();

extern EVENT KeybdReadSynchEvent;