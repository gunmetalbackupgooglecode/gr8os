//
// FILE:		hal.cpp
// CREATED:		28-Feb-2008  by Great
// PART:        HAL
// ABSTRACT:
//			Functions to work with hardware
//

#include "common.h"



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
