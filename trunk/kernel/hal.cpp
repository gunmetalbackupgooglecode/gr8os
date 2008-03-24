//
// FILE:		hal.cpp
// CREATED:		28-Feb-2008  by Great
// PART:        HAL
// ABSTRACT:
//			Functions to work with hardware
//

#include "common.h"

//
// COM Port
//

USHORT ComPorts[2] = {
	PORT_COM1_BASE,
	PORT_COM2_BASE
};

BOOLEAN
KEAPI
HalpCheckComPortConnected(
	IN UCHAR ComPortNumber
	)
{
	UCHAR Read;

	for( int i=0; i<8; i++ )
	{
		Read = KiInPort ( ComPorts[ComPortNumber] );
		if( Read != 0xFF )
			return TRUE;
	}

	return FALSE;
}


BOOLEAN
KEAPI
HalpInitializeComPort(
	IN UCHAR Port,
	IN ULONG Frequency
	)
{
	if( !HalpCheckComPortConnected( Port  ) )
	{
		//
		// Port is not connected.
		//

		return FALSE;
	}

	USHORT Multiplier = (USHORT)(COMPORT_BASEFREQ / (16 * Frequency));

	UCHAR Lcr = (1<<7);
	WRITE_COM(Port, 3, Lcr);

	WRITE_COM(Port, 0, Multiplier & 0xFF);
	WRITE_COM(Port, 0, Multiplier >> 8);
	
	         // READY   BR FixParity     ParityCheck     Stop    Size
	Lcr = 7; //   0     0   0               00            1       11
	WRITE_COM(Port, 3, Lcr);

	WRITE_COM(Port, 1, 0);

	return TRUE;
}

KESYSAPI
STATUS
KEAPI
HalWriteComPort(
	IN UCHAR ComPortNumber,
	IN PVOID Data,
	IN ULONG DataSize
	)
/*++
	Send data through COM port
--*/
{
	UCHAR State;
	UCHAR Iterations = 0;

	//
	// Wait for writer
	//

	for( ;; Iterations++ )
	{
		State = GET_COMPORT_STATE (ComPortNumber) & COMPORT_WRITER;

		if( State & COMPORT_READY_TO_WRITE )
		{
			break;
		}

		KeStallExecution( 10 ); // 86mcs need COM port to send one byte at maximum speed.

		if( Iterations > 100 )
		{
			//
			// While 1 second COM port writer did not complete his operation.
			// Obviously some error occurred.
			//

			return STATUS_DEVICE_NOT_READY;
		}
	}
	
	//
	// COM port ready. Write
	//

    for( UCHAR *datapos = (UCHAR*)Data; datapos < (UCHAR*)Data+DataSize; datapos ++ )
	{
		while( !(GET_COMPORT_STATE(ComPortNumber) & COMPORT_READY_TO_WRITE) )
		{
			KeStallExecution( 10 ); // 86mcs need COM port to send one byte at maximum speed.
		}

		WRITE_COM( ComPortNumber, 0, *datapos );
	}

	return STATUS_SUCCESS;
}

KESYSAPI
STATUS
KEAPI
HalReadComPort(
	IN UCHAR ComPortNumber,
	OUT PVOID Data,
	IN OUT PULONG DataSize
	)
/*++
	Read data from COM port
--*/
{
	UCHAR State;
	UCHAR Iterations = 0;

	//
	// Wait for ready
	//

	for( ;; Iterations++ )
	{
		State = GET_COMPORT_STATE (ComPortNumber) & COMPORT_READER;

		if( State & COMPORT_DATAREADY )
		{
			break;
		}

		KeStallExecution( 10 ); // 86mcs need COM port to send one byte at maximum speed.

		if( Iterations > 400 )
		{
			//
			// While 1 second COM port reader did not complete his operation.
			// Obviously some error occurred.
			//

			return STATUS_DEVICE_NOT_READY;
		}
	}
	
	Iterations = 0;

	//
	// COM port ready. Read
	//

    for( UCHAR *datapos = (UCHAR*)Data; datapos < (UCHAR*)Data+*DataSize; datapos ++ )
	{
		*datapos = READ_COM( ComPortNumber, 0 );

		if( State & COMPORT_READER_ERROR )
		{
//			KdPrint(("\nReader error: overflow=%d, parity=%d, synch=%d\n",
//				(State & COMPORT_OVERFLOW_READERR) != 0,
//				(State & COMPORT_PARITY_READERR) != 0,
//				(State & COMPORT_SYNCH_READERR) != 0 ));

			*DataSize = (ULONG)datapos - (ULONG)Data;

			return STATUS_PARTIAL_READ;
		}

//		KiDebugPrint ("HalReadComPort: got byte %02x\n", *datapos);
		
		if( datapos == (UCHAR*)Data+*DataSize-1 )
			break;

		while( !( (State=GET_COMPORT_STATE(ComPortNumber)) & COMPORT_DATAREADY) )
		{
			KeStallExecution( 10 ); // 86mcs need COM port to send one byte at maximum speed.
			Iterations ++;

			if (Iterations == 400)
			{
				*DataSize = (ULONG)datapos - (ULONG)Data;
				return STATUS_PARTIAL_READ;
			}
		}

		Iterations = 0;
	}

	// Does not modify DataSize - all reada has been read successfully

	return STATUS_SUCCESS;
}

KESYSAPI
UCHAR
KEAPI
KdPortGetByte(
	UCHAR* Byte
	)
{
	UCHAR t;

	KeStallExecution(2000);
	if (GET_COMPORT_STATE(KdpComPortPreferred) & COMPORT_DATAREADY)
	{
		t = READ_COM (KdpComPortPreferred, 0);

		if (ARGUMENT_PRESENT(Byte))
		{
			*Byte = t;
		}

#if KD_TRACE_LOW_LEVEL_IO
		KiDebugPrint("KdPortGetByte: %02x\n", *Byte);
#endif

		return CP_GET_SUCCESS;
	}

	if (GET_COMPORT_STATE(KdpComPortPreferred) & COMPORT_READER_ERROR)
	{
		return CP_GET_ERROR;
	}

	return CP_GET_NODATA;
}

KESYSAPI
VOID
KEAPI
KdPortPutByte(
	UCHAR Byte
	)
{
	while( !(GET_COMPORT_STATE(KdpComPortPreferred) & COMPORT_READY_TO_WRITE) )
	{
		KeStallExecution( 20 );
	}

#if KD_TRACE_LOW_LEVEL_IO
	KiDebugPrint("KdPortPutByte: %02x\n", Byte);
#endif
		
	WRITE_COM( KdpComPortPreferred, 0, Byte );
}


//
// Programmable Interval Timer (PIT) Intel 8253
//

KESYSAPI
VOID
KEAPI
HalConfigureTimer(
	UCHAR Timer,
	ULONG Freq
	)
/*++
	Congure one of three counters of i8254 timer
--*/
{
	TIMER_CONTROL Tmr;
	Tmr.CounterSelector = Timer;
	Tmr.CountMode = 0;
	Tmr.CounterMode = MeandrGenerator;
	Tmr.RequestMode = LSBMSB;

	ULONG Div32 = ( (ULONG)TIMER_FREQ / (ULONG)Freq );
	USHORT Divisor = (USHORT) Div32;

	if( Div32 >= 0x10000 )
		Divisor = 0;

	KiDebugPrint ("KE: Timer configured for: channel=%d, freq=%d, divisor=0x%04x\n", Timer, Freq, Divisor);


	KiOutPort (0x43, Tmr.RawValue);
	KeStallExecution(1);

	KiOutPort (0x40 + Timer, Divisor & 0xFF);
	KeStallExecution(1);

	KiOutPort (0x40 + Timer, Divisor >> 8);
	KeStallExecution(1);
}

KESYSAPI
VOID
KEAPI
HalReadConfigTimer(
	UCHAR Timer,
	ULONG *Freq
	)
/*++
	Congure one of three counters of i8254 timer
--*/
{
	UCHAR c1 = KiInPort (0x40 + Timer);
	KeStallExecution(1);
	UCHAR c2 = KiInPort (0x40 + Timer);

	ULONG Divisor = (c2 << 8) | c1;
	
	*Freq = (ULONG)TIMER_FREQ / Divisor;
}

KESYSAPI
USHORT
KEAPI
HalQueryTimerCounter(
	UCHAR Timer
	)
/*++
	Query counter channel current value
--*/
{
	UCHAR lsb, msb;

	lsb = KiInPort (Timer + 0x40);
	__asm nop
	__asm nop

	msb = KiInPort (Timer + 0x40);
	__asm nop
	__asm nop

	return (msb << 8) | lsb;
}


KESYSAPI
SYSTEM_PORT
KEAPI
HalReadSystemPort(
	)
/*++
	Reads system port
--*/
{
	volatile UCHAR Val = KiInPort( SYSTEM_PORT_NUMBER );
	return *(SYSTEM_PORT*)&Val;
}

ULONG HalBusClockFrequency;


#define KiDebugPrint

VOID
KEAPI
HalInitSystem(
	)
/*++
	Initialize HAL
--*/
{
	//
	// Configure APIC timer
	//

	APIC_TIMER_CONFIG Timer = {0};

	Timer.Divisor = 0xA;
	Timer.Flags = TIMER_MODIFY_DIVISOR | TIMER_MODIFY_LVT_ENTRY | TIMER_MODIFY_INITIAL_COUNTER;
	Timer.LvtTimer.Masked = FALSE;
	Timer.LvtTimer.Vector = 6;
	Timer.LvtTimer.TimerMode = Periodic;
	Timer.InitialCounter = -1;

	HalSetApicTimerConf (&Timer);
	__asm nop;
	HalQueryApicTimerConf (&Timer);

	KiDebugPrint ("HAL: APIC Timer:\n  Initial = %08x, Current = %08x, Divisor = %08x, LVT = %08x\n", 
		Timer.InitialCounter,
		Timer.CurrentCounter,
		Timer.Divisor,
		Timer.LvtTimer.RawValue
		);
	
	//
	// Query bus clock frequency
	//

	KiDebugPrint("HAL: Quering bus clock freq..\n");

	HalBusClockFrequency = HalQueryBusClockFreq();

	KiDebugPrint("HAL: BusFreq=%08x (%d)\n", HalBusClockFrequency, HalBusClockFrequency);

	//
	// Re-Configure channel 2
	//

	HalConfigureTimer( 2, 200 );
}

#define MAX_DMA_CHANNELS  8

LONG HalpDmaChannelBusy[MAX_DMA_CHANNELS] = { 1, 0, 0, 0, 1, 0, 0, 0 };


KESYSAPI
STATUS
KEAPI
HalRequestDma(
	UCHAR Channel
	)
/*++
	This routine requests DMA channel for the device driver.
	Channel should be freed with HalFreeDma() later.
--*/
{
	if (Channel >= MAX_DMA_CHANNELS)
		return STATUS_INVALID_PARAMETER;

	if (InterlockedExchange (&HalpDmaChannelBusy[Channel], 1) != 0)
		return STATUS_BUSY;

	return STATUS_SUCCESS;
}


KESYSAPI
STATUS
KEAPI
HalFreeDma(
	UCHAR Channel
	)
/*++
	This routine frees DMA channel allocated by HalRequestDma()
--*/
{
	if (Channel >= MAX_DMA_CHANNELS)
		return STATUS_INVALID_PARAMETER;

	if (InterlockedExchange (&HalpDmaChannelBusy[Channel], 1) == 0)
		return STATUS_ALREADY_FREE;

	return STATUS_SUCCESS;
}
