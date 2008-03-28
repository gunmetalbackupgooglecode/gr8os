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

			return STATUS_PARTIAL_COMPLETION;
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
				return STATUS_PARTIAL_COMPLETION;
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


//#define KiDebugPrint

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
	Timer.InitialCounter = -20;

	HalSetApicTimerConf (&Timer);
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

	ULONG s1 = HalQueryTimerCounter(2);

	if (KiInPort(BOCHS_LOGGER_PORT) != BOCHS_LOGGER_PORT)
	{
		//HalBusClockFrequency = HalQueryBusClockFreq();
	}

	KiDebugPrint("HAL: BusFreq=%08x (%d), s1=%08x\n", HalBusClockFrequency, HalBusClockFrequency, s1);

	//
	// Re-Configure channel 2
	//

	HalConfigureTimer( 2, 200 );

	ExInitializeMutex (&HalpLowMegDbLock);
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

	if (InterlockedExchange (&HalpDmaChannelBusy[Channel], 0) == 0)
		return STATUS_ALREADY_FREE;

	return STATUS_SUCCESS;
}


//
//  0  -  free
//  1  -  allocated
//  2  -  reserved
//
ULONG HalpLowMegBusyForDma[_1MegPages] = {
	2, 2, 2, 0, 0, 0, 0, 2,   2, 2, 2, 2, 2, 2, 2, 2,	// Reserve first two pages for BIOS area and pages for kernel (7000-F000)
	0, 0, 0, 0, 0, 0, 0, 0,   0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,   0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,   0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,   0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,   0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,   0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,   0, 0, 0, 0, 0, 0, 0, 0,

	0, 0, 0, 0, 0, 0, 0, 0,   0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,   0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,   0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,   0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,   0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,   0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,   0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,   0, 0, 0, 0, 0, 0, 0, 0,
};

MUTEX HalpLowMegDbLock;


KESYSAPI
UCHAR
KEAPI
HalAllocatePhysicalLowMegPages(
	UCHAR PageCount
	)
/*++
	Allocates physical pages in the first megabyte of the RAM
--*/
{
	ExAcquireMutex (&HalpLowMegDbLock);

	for (UCHAR i=0; i<_1MegPages; i++)
	{
		if (HalpLowMegBusyForDma[i] == 0)
		{
			BOOLEAN Busy = 0;

			for (UCHAR j=i; j<i+PageCount; j++)
			{
				Busy |= HalpLowMegBusyForDma[j];
				if (Busy)
				{
					break;
				}
			}
			
			if (!Busy)
			{
				for (UCHAR j=i; j<i+PageCount; j++)
				{
					HalpLowMegBusyForDma[j] = 1;
				}

				ExReleaseMutex (&HalpLowMegDbLock);
				return i;
			}

			i += PageCount-1;
		}
	}

	ExReleaseMutex (&HalpLowMegDbLock);
	return 0;
}


KESYSAPI
VOID
KEAPI
HalFreePhysicalLowMegPages(
	UCHAR StartPage,
	UCHAR PageCount
	)
/*++
	Free pages to the first megabyte of the physical RAM.
	This function bugchecks on failure.
--*/
{
	// Check region
	if ((StartPage + PageCount) >= _1MegPages)
	{
		KeBugCheck (HAL_FREEING_INVALID_PAGES,
					StartPage,
					PageCount,
					__LINE__,
					0
					);
	}

	ExAcquireMutex (&HalpLowMegDbLock);

	for (UCHAR i=StartPage; i<StartPage+PageCount; i++)
	{
		if (HalpLowMegBusyForDma[i] == 2)
		{
			KeBugCheck (HAL_FREEING_RESERVED_PAGES,
						i,
						StartPage,
						PageCount,
						__LINE__
						);
		}
		else if (HalpLowMegBusyForDma[i] == 0)
		{
			KeBugCheck (HAL_FREEING_ALREADY_FREE_PAGES,
						i,
						StartPage,
						PageCount,
						__LINE__
						);
		}
		else
		{
			// Allocated. Free it

			HalpLowMegBusyForDma[i] = 0;
		}
	}

	ExReleaseMutex (&HalpLowMegDbLock);
}

LONG HalpDmaControllerBusy = 0;


USHORT HalpDmaAddr[MAX_DMA_CHANNELS] = {
	DMA_ADDR_0,
	DMA_ADDR_1,
	DMA_ADDR_2,
	DMA_ADDR_3,
	DMA_ADDR_4,
	DMA_ADDR_5,
	DMA_ADDR_6,
	DMA_ADDR_7
};

USHORT HalpDmaCnt[MAX_DMA_CHANNELS] = {
	DMA_CNT_0,
	DMA_CNT_1,
	DMA_CNT_2,
	DMA_CNT_3,
	DMA_CNT_4,
	DMA_CNT_5,
	DMA_CNT_6,
	DMA_CNT_7
};

USHORT HalpDmaPage[MAX_DMA_CHANNELS] = {
	DMA_PAGE_0,
	DMA_PAGE_1,
	DMA_PAGE_2,
	DMA_PAGE_3,
	0,
	DMA_PAGE_5,
	DMA_PAGE_6,
	DMA_PAGE_7
};

KESYSAPI
VOID
KEAPI
HalEnableDma(
	UCHAR Channel
	)
/*++
	Enables the specified DMA channel
--*/
{
	if (Channel <= 3)
	{
		KiOutPort (DMA1_MASK_REG, Channel);
	}
	else
	{
		KiOutPort (DMA2_MASK_REG, Channel & 3);
	}
}

KESYSAPI
VOID
KEAPI
HalDisableDma(
	UCHAR Channel
	)
/*++
	Disables the specified DMA channel
--*/
{
	if (Channel <= 3)
	{
		KiOutPort (DMA1_MASK_REG, Channel | 4);
	}
	else
	{
		KiOutPort (DMA2_MASK_REG, (Channel & 3) | 4);
	}
}

KESYSAPI
VOID
KEAPI
HalClearDmaFf(
	UCHAR Channel
	)
/*++
	Clear the DMA pointer flip-flop
--*/
{
	if (Channel <= 3)
	{
		KiOutPort (DMA1_CLEAR_FF_REG, 0);
	}
	else
	{
		KiOutPort (DMA2_CLEAR_FF_REG, 0);
	}
}


KESYSAPI
VOID
KEAPI
HalSetDmaMode(
	UCHAR Channel,
	UCHAR Mode
	)
/*++
	Set DMA mode
--*/
{
	if (Channel <= 3)
	{
		KiOutPort (DMA1_MODE_REG, Mode | Channel);
	}
	else
	{
		KiOutPort (DMA2_MODE_REG, Mode | (Channel & 3));
	}
}

KESYSAPI
STATUS
KEAPI
HalInitializeDmaRequest(
	UCHAR DmaCommand,
	UCHAR Channel,
	PVOID Buffer,
	ULONG Size,
	PDMA_REQUEST *pDmaRequest
	)
/*++
	Prepare DMA controller for reading/writing (DmaCommand)
	 on the specified _Channel_ to/from the specified _Buffer_

Return Value:
	PDMA_REQUEST pointer, that should be treated as opaque.
	It should be passed to HalCompleteDmaRequest() further.
--*/
{
	STATUS Status;

	if (InterlockedExchange (&HalpDmaControllerBusy, 1) == 1)
	{
		KdPrint(("HAL: DMA busy\n"));
		return STATUS_BUSY;
	}
	
	// Reserve the channel
	Status = HalRequestDma (Channel);
	if (!SUCCESS(Status))
	{
		KdPrint(("HAL: Dma channel not free\n"));
		return STATUS_INSUFFICIENT_RESOURCES;
	}

	// Allocate buffer for DMA_REQUEST structure
	PDMA_REQUEST DmaReq = (PDMA_REQUEST) ExAllocateHeap (FALSE, sizeof(DMA_REQUEST));
	if (!DmaReq)
	{
		KdPrint(("HAL: Not enough resources to allocate from heap\n"));
		HalFreeDma (Channel);
		return STATUS_INSUFFICIENT_RESOURCES;
	}

	// Initialize struct
	DmaReq->Channel = Channel;
	DmaReq->Buffer = Buffer;

	if (DmaCommand == DMA_MODE_READ)
	{
		DmaReq->ReadOperation = TRUE;
	}
	else
	{
		DmaReq->ReadOperation = FALSE;
	}

	//Size = ALIGN_UP(Size,PAGE_SIZE);
	DmaReq->BufferSize = Size;
	DmaReq->PageCount =  (UCHAR)(ALIGN_UP(Size,PAGE_SIZE) >> PAGE_SHIFT);

	// Allocate physical pages in the low megabyte
	DmaReq->PageUsed = HalAllocatePhysicalLowMegPages (DmaReq->PageCount);

	// ! Notice: buffer (PageUsed) must not contain a 64k byte boundary crossing, or data will be corrupted/lost !

	if (!DmaReq->PageUsed)
	{
		KdPrint(("HAL: Cannot allocate physical pages from low meg\n"));
		ExFreeHeap (DmaReq);
		HalFreeDma (Channel);
		return STATUS_INSUFFICIENT_RESOURCES;
	}

	DmaReq->MappedPhysical = MmMapPhysicalPages (DmaReq->PageUsed << PAGE_SHIFT, DmaReq->PageCount);

	if (!DmaReq->ReadOperation)
	{
		//
		// Copy buffer to the physical buffer
		//

		memcpy (DmaReq->MappedPhysical, DmaReq->Buffer, DmaReq->BufferSize);
	}

	//
	// Initialize DMA controller
	//

	ULONG f_addr = DmaReq->PageUsed << PAGE_SHIFT;
	USHORT seg, ofs;

	seg = (f_addr >> 16) & 0xFF;
	ofs = f_addr & 0xFFFF;

	_disable();
	HalDisableDma (Channel);
	HalClearDmaFf (Channel);
	KiOutPort (HalpDmaAddr[Channel], LOBYTE(ofs));
	KiOutPort (HalpDmaAddr[Channel], HIBYTE(ofs));
	HalClearDmaFf (Channel);
	KiOutPort (HalpDmaCnt[Channel], LOBYTE(Size));
	KiOutPort (HalpDmaCnt[Channel], HIBYTE(Size));
	HalSetDmaMode (Channel, DmaCommand);
	KiOutPort (HalpDmaPage[Channel], (UCHAR)seg);
	HalEnableDma (Channel);
	_enable();

	*pDmaRequest = DmaReq;
	return STATUS_SUCCESS;
}


KESYSAPI
VOID
KEAPI
HalCompleteDmaRequest(
	PDMA_REQUEST DmaReq
	)
/*++
	Completes DMA request initiated by HalInitializeDmaRequest
--*/
{
	if (DmaReq->ReadOperation)
	{
		memcpy (DmaReq->Buffer, DmaReq->MappedPhysical, DmaReq->BufferSize);
	}

	MmUnmapPhysicalPages (DmaReq->MappedPhysical, DmaReq->PageCount);
	HalFreePhysicalLowMegPages (DmaReq->PageUsed, DmaReq->PageCount);
	HalFreeDma (DmaReq->Channel);
	ExFreeHeap (DmaReq);
	
	InterlockedExchange (&HalpDmaControllerBusy, 0);
}

KESYSAPI
UCHAR
KEAPI
HalCmosRead(
	UCHAR Offset
	)
/*++
	Read cmos memory
--*/
{
	KiOutPort (CMOS_SELECTOR, Offset);
	return KiInPort (CMOS_DATA);
}