//
// i8042 device driver for GR8OS
//
// (C) Great, 2008
//

typedef unsigned short wchar_t;

#include "common.h"
#include "keybd.h"

//
// Cyclic buffer & appropriate locks
//

// Locked buffer
char KeybdCyclicBuffer[KBD_BUFFER_SIZE];
LOCK KeybdCyclicBufferLock;

// Buffer r/w positions
int KeybdCurrentPos = -1;


//
// Scan-Code tables
//

char KeybdAsciiCodes[] =
{
	0,0,'1','2','3','4','5','6','7','8','9','0','-','=',0,0,
		'q','w','e','r','t','y','u','i','o','p','[',']',10,0,
		'a','s','d','f','g','h','j','k','l',';','\'', '`',0,
		'\\','z','x','c','v','b','n','m',',','.','/',0,'*',0,
		' ',0, 0,0,0,0,0,0,0,0,0,0, 0,0, '7','8','9','-','4','5',
		'6','+','1','2','3','0','.', 0,0
};

char KeybdAsciiCodesShifted[] =
{
	0,0,'!','@','#','$','%','^','&','*','(',')','_','+',0,0,
		'Q','W','E','R','T','Y','U','I','O','P','{','}',10,0,
		'A','S','D','F','G','H','J','K','L',':','"', '~',0,
		'|','Z','X','C','V','B','N','M','<','>','?',0,'*',0,
		' ',0, 0,0,0,0,0,0,0,0,0,0, 0,0, '7','8','9','-','4','5',
		'6','+','1','2','3','0','.', 0,0
};

char *KeybdScanToAsciiTables[] = { KeybdAsciiCodes, KeybdAsciiCodesShifted };

//
// Global state variables
//  

PKEYBOARD_STATUS_BYTE KeybdStatusByte = (PKEYBOARD_STATUS_BYTE) &KiKeyboardStatusByte;
KEYBOARD_INDICATORS KeybdIndicators;


//
// Convert scan code to ascii code
//

UCHAR KeybdScanCodeToAsciiCode (UCHAR ScanCode)
{
	BOOLEAN Shifted = KeybdStatusByte->Shift;
	if (KeybdIndicators.CapsLock)
	{
		Shifted = !Shifted;
	}

	if (ScanCode < sizeof(KeybdAsciiCodes))
        return KeybdScanToAsciiTables[Shifted][ScanCode];

	return 0;
}


UCHAR read_kbd()
{
	ULONG Timeout;
	UCHAR Stat, Data;

	for (Timeout = 500000; Timeout > 0; Timeout --)
	{
		Stat = KiInPort (0x64);

		if (Stat & 0x01) // Output buffer full?
		{
			Data = KiInPort (0x60);

			if ( (Stat & 0xC0) == 0 )
			{
				return Data;
			}
		}
	}

	KdPrint(("read_kbd: timed out\n"));
	ASSERT (FALSE);

	return -1;
}


void write_kbd (UCHAR Addr, UCHAR Data)
{
	ULONG Timeout;
	ULONG Stat;

	for (Timeout = 500000; Timeout > 0; Timeout --)
	{
		Stat = KiInPort (0x64);

		if ( (Stat & 0x02) == 0)
			break;
	}

	if (Timeout == 0)
	{
		KdPrint(("write_kbd: timed out\n"));
		ASSERT (FALSE);
		return;
	}

	KiOutPort (Addr, Data);
}

VOID wait_kbd ()
{
	__asm
	{
start:
		nop
		in   al, 0x64
		test al, 1		; output buffer full?
		jnz  out_full

		test al, 2		; input buffer full?
		jnz  start
	}

	return;

	__asm
	{
out_full:
		in  al, 0x60
	}

	//ASSERT (FALSE);

	__asm jmp start;
}


void init_kbd()
{
	// TODO
}


//
// Update keyboard indicators
//

VOID KeybdSetIndicators ()
{
	union 
	{
		struct
		{
			UCHAR ScrollLock : 1;
			UCHAR NumLock : 1;
			UCHAR CapsLock : 1;
			UCHAR Unused : 5;
		};

		UCHAR Value;
	} packet;

	packet.Value = 0;
	packet.NumLock = KeybdIndicators.NumLock;
	packet.CapsLock = KeybdIndicators.CapsLock;
	packet.ScrollLock = KeybdIndicators.ScrollLock;

	KiOutPort (0x60, 0xED); // set LEDs
	wait_kbd();

	KiOutPort (0x60, packet.Value);
}


__declspec(naked) void kbd_eoi()
{
	__asm
	{
		; send EOI to kbd
		in  al, 0x61
		or  al, 0x80
		out 0x61, al
		xor al, 0x80
		out 0x61, al
		retn
	}
}



void KeybdIrq1Interrupt()
{
	UCHAR ScanCode;
	UCHAR AsciiCode;
	BOOLEAN UP;

	ScanCode = KiInPort (0x60);

	UP = ScanCode >> 7;
	ScanCode &= 0x7F;

	switch (ScanCode)
	{
	case 1:	// ESC
//		PspDumpSystemThreads();
		break;

	case 28: // ENTER
        if (KeybdStatusByte->Ctrl && KeybdStatusByte->Alt)
		{
			HalRebootMachine();
		}
		break;

	case 42: // Left Shift
		KeybdStatusByte->Shift = 1 && !UP;
		KeybdStatusByte->RightShift = 0;
		break;

	case 54: // Right Shift
		KeybdStatusByte->Shift = 1 && !UP;
		KeybdStatusByte->RightShift = 1 && !UP;
		break;

	case 0x1D: // Ctrl
		KeybdStatusByte->Ctrl = !UP;
		break;

	case 56: // Alt
		KeybdStatusByte->Alt = !UP;
		break;

	case 69: // Num Lock
		KeybdStatusByte->NumLock = !UP;

		KeybdIndicators.NumLock = !KeybdIndicators.NumLock;
		KeybdSetIndicators ();

		break;

	case 58: // Caps Lock
		KeybdStatusByte->CapsLock = !UP;

		KeybdIndicators.CapsLock = !KeybdIndicators.CapsLock;
		KeybdSetIndicators ();
		break;

	case 70: // Scroll Lock
		KeybdStatusByte->ScrollLock = !UP;

		KeybdIndicators.ScrollLock = !KeybdIndicators.ScrollLock;
		KeybdSetIndicators ();
		break;

	case 0xE0:
		ScanCode = KiInPort (0x60); // ignore now

		break;

	case 0xE1:
		ScanCode = KiInPort (0x60); // ignore now

		switch (ScanCode)
		{
		case 0x1D: // Pause/Break
			KiInPort (0x60); // 0x45
			KiInPort (0x60); // 0xE1
			KiInPort (0x60); // 0x9D
			KiInPort (0x60); // 0xC5
			break;
		}

		break;
	}

	//
	// Convert to ascii code
	//

    AsciiCode = KeybdScanCodeToAsciiCode (ScanCode);
    
	//
	// Store into buffer
	//

	if (KeybdCurrentPos == KBD_BUFFER_SIZE-1)
		KeybdCurrentPos = -1;

	KeybdCyclicBuffer [++KeybdCurrentPos] = AsciiCode;


	kbd_eoi();
	__asm sti;  // allow interrupts

	KeSetEvent (&KeybdReadSynchEvent, 0);

#if DBG
	if (!UP)
	{
		char str[2] = " ";
		str[0] = (char)AsciiCode;

		KiDebugPrintRaw(str);
	}

	char buf[40];
	sprintf(buf, "SHT:%d CTL:%d ALT:%d CAPS:%d", 
		KeybdStatusByte->Shift,
		KeybdStatusByte->Ctrl,
		KeybdStatusByte->Alt,
		KeybdIndicators.CapsLock
		);
	KeSetOnScreenStatus (buf);
#endif

}

__declspec(naked) void irq1_handler()
{
	__asm
	{
		cli
		pushad

		call KeybdIrq1Interrupt

		popad

        jmp KiEoiHelper
	}
}

