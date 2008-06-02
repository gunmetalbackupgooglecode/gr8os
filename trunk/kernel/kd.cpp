//
// FILE:		kd.cpp
// CREATED:		28-Feb-2008  by Great
// PART:        KD
// ABSTRACT:
//			Built-in kernel debugger
//

#include "common.h"

char KiDebugPrintBuffer[1024];
//char *KiDebugPrintBuffer = 0;
LOCK DebugPrintLock;



ULONG KiScreenX = 0;
ULONG KiScreenY = 0;

ULONG KiXResolution = 80;
ULONG KiYResolution = 24;

BOOLEAN KiFirstPrint = TRUE;
BOOLEAN KiUseBochsLogging  = FALSE;

VOID
KEAPI
BochsPrint(
	PCHAR String
	)
{
	for( PCHAR sp=String; *sp; sp++ )
	{
		if( *sp == '\n' )
			KiOutPort( BOCHS_LOGGER_PORT, '\r' );
		KiOutPort( BOCHS_LOGGER_PORT, *sp );
	}
}

VOID
KEAPI
BochsPrintF(
	PCHAR FormatString,
	...
	)
{
	va_list va;

	va_start( va, FormatString );
	vsprintf ( KiDebugPrintBuffer, FormatString, va );
	BochsPrint (KiDebugPrintBuffer);
}

VOID
KEAPI
KiPutChar(
	CHAR ch
	)
{
	if (KiUseBochsLogging)
	{
		KiOutPort( BOCHS_LOGGER_PORT, ch );
//		return;
	}

	if (ch == '\n')
	{
		KiScreenY++;
	}
	else if (ch == '\r')
	{
		KiScreenX = 0;
	}
	else
	{
		KiWriteChar (KiScreenY*KiXResolution + KiScreenX, ch);
		KiScreenX ++;
	}

	if (KiScreenX == KiXResolution)
	{
		// End of line.

		KiScreenY ++;
		KiScreenX = 0;

//		for (int i=0; i<KiXResolution; i++)
//		{
//			KiWriteChar ( KiXResolution*KiScreenY + i, ' ');
//		}

	}

	if (KiScreenY == KiYResolution)
	{
		// Scroll

		for (int i=0; i<80; i++)
			KiWriteCharAttribute (i, 7);

		/*
		memmove_far (
			KGDT_VIDEO, 0, 
			KGDT_VIDEO, KiXResolution*2, 
			KiXResolution*(KiYResolution-1)*2
			);
		*/

		for( ULONG i=0; i<KiXResolution*(KiYResolution-2); i++ )
		{
			KiWriteChar(i, KiReadChar (i + KiXResolution*2));
		}

		for (ULONG i=0; i<KiXResolution*2; i++)
		{
			KiWriteChar ( KiXResolution*(KiYResolution-2) + i, ' ');
		}

		KiScreenY -= 2;

//		BochsPrintF("KiPutChar: line scrolled, X=%d, Y=%d, XRes=%d, YRes=%d\n",
//			KiScreenX,
//			KiScreenY,
//			KiXResolution,
//			KiYResolution
//			);
	}

}

KESYSAPI
VOID
KEAPI
KiDebugPrintRaw(
	PCHAR String
	)
/*++
	Out debug string
--*/
{
	if (KiFirstPrint)
	{
		if (KiInPort (BOCHS_LOGGER_PORT) == BOCHS_LOGGER_PORT)
		{
			KiUseBochsLogging = TRUE;
		}
		KiFirstPrint = FALSE;
	}

	for( PCHAR sp=String; *sp; sp++ )
	{
		if( *sp == '\n' )
			KiPutChar ('\r');
		KiPutChar (*sp);
	}
}

KESYSAPI
VOID
KEAPI
KiDebugPrint(
	PCHAR FormatString,
	...
	)
/*++
	Print formatted debug string
--*/
{
//	if( KiDebugPrintBuffer == NULL )
//		KiDebugPrintBuffer = (char*) ExAllocateHeap( FALSE, 1024 );

	BOOLEAN LockAcquired = FALSE;

	if( DebugPrintLock.Count == 0 )
	{
		LockAcquired = TRUE;
		KeAcquireLock( &DebugPrintLock );
	}

	va_list va;

	va_start( va, FormatString );
	vsprintf ( KiDebugPrintBuffer, FormatString, va );
	KiDebugPrintRaw (KiDebugPrintBuffer);

	if( LockAcquired )
	{
		KeReleaseLock (&DebugPrintLock);
	}
}

//
// KeSetOnScreenStatus
//

VOID
KEAPI
KiInitializeOnScreenStatusLine(
	)
{
	for (int i=0; i<80; i++)
	{
		KiWriteChar (KiXResolution*24 + i, ' ');

		KiWriteCharAttribute (KiXResolution*24 + i, 0x8B);
	}
}

UCHAR ProgressBar = 0;

KESYSAPI
VOID
KEAPI
KeSetOnScreenStatus(
	PSTR Status
	)
/*++
	Displays status string
--*/
{
	char buffer[81];
	int i;

	if (ProgressBar < 10)
	{
		sprintf (buffer, " [  __________   " OS_VERSION "  |  %s", Status);
	}
	else
	{
		sprintf (buffer, " [   " OS_VERSION "   |  %s", Status);
	}

	//sprintf (buffer, "0123456789abcdef0123456789   |   Status: %s", Status);

	ASSERT (strlen(buffer) <= 80);

	for (i=0; i<strlen(buffer); i++)
	{
		KiWriteChar (KiXResolution*24 + i, buffer[i]);
	}

	for ( ; i<78; i++)
	{
		KiWriteChar (KiXResolution*24 + i, ' ');
	}

	KiWriteChar (KiXResolution*24 + i, ']');
	KiWriteChar (KiXResolution*24 + i + 1, ' ');
}

VOID
KEAPI
KiMoveLoadingProgressBar(
	UCHAR Percent
	)
{
	for (int i=4; i<4+Percent; i++)
	{
		KiWriteChar (KiXResolution*24 + i, (char)219);
	}

	ProgressBar = Percent;
}



LOCK KdDebugLock ;
BOOLEAN KdDebuggerEnabled = 0;
BOOLEAN KdControlCPending = 0, KdControlCPressed = 0;
UCHAR KdpComPortPreferred = 0;

VOID
KEAPI
KdInitSystem(
	)
/*++
	This function performs initialization of built-in kernel debugger
--*/
{
	KdDebuggerEnabled = 1;  //BUGBUG
	KdpComPortPreferred = 0; // COM1

	KdDebugLock.Count = 0;

	if (!HalpInitializeComPort (KdpComPortPreferred, 115200))
	{
		KdDebuggerEnabled = FALSE;

		KiDebugPrintRaw ("KD: COM port did not pass initialization successfully.\n");

		KeBugCheck (KD_INITIALIZATION_FAILED, KD_COM_PORT_FAILED_INITIALIZATION, 0, 0, 0);
	}
}


VOID
KEAPI
KdLockPort(
    PBOOLEAN OldIrqState
	)
/*++
	Lock debug port
--*/
{
#if KD_TRACE_PORT_LOCKING
	KiDebugPrintRaw("KD Port locking attempt\n");
#endif
	*OldIrqState = KeAcquireLock (&KdDebugLock);
}


VOID
KEAPI
KdUnlockPort(
	BOOLEAN OldIrqState
	)
/*++
	Unlock debug port
--*/
{
#if KD_TRACE_PORT_LOCKING
	KiDebugPrintRaw("KD Port unlocked attempt\n");
#endif
	KeReleaseLock (&KdDebugLock);
	KeReleaseIrqState (OldIrqState);
}



KESYSAPI
BOOLEAN
KEAPI
KdPollBreakIn(
	)
/*++
	This function checks if break-in sequence is pending in COM port buffer.
	If so, return TRUE and caller must execute int3 after this, else return FALSE.
--*/
{
	BOOLEAN BreakIn = FALSE;
	BOOLEAN OldIrqState;

	KdLockPort (&OldIrqState);

	if (KdControlCPending)
	{
		KdControlCPressed = TRUE;
		KdControlCPending = FALSE;
		BreakIn = TRUE;
	}
	else
	{
		if( STATUS_SUCCESS == KdReceivePacket (DbgKdBreakInPacket, NULL, NULL, NULL) )
		{
			KdControlCPressed = TRUE;
			BreakIn = TRUE;
		}
	}

	KdUnlockPort (OldIrqState);

	return BreakIn;
}

KESYSAPI
ULONG
KEAPI
KdCheckSumPacket(
	IN PBYTE Buffer,
	IN ULONG Length
	)
/*++
	Calculate checksum of the packet
--*/
{
  ULONG Checksum;
  ULONG BytesLeft;
  BYTE *TempPointer;

  BytesLeft = Length;
  Checksum = 0;
  if ( Length )
  {
    TempPointer = Buffer;
    do
    {
      Checksum += *(TempPointer++);
      --BytesLeft;
    }
    while ( BytesLeft );
  }

  return Checksum;
}


UCHAR
KEAPI
KdpReceiveString(
	PCBUFFER Buffer,
	ULONG *ActualReadBytes
	)
/*++
	This function receives string from com port
--*/
{
	UCHAR Status = 0;

	for (int i=0; i<Buffer->Length; i++)
	{
		if (ARGUMENT_PRESENT(Buffer->Buffer))
		{
			Status = KdPortGetByte ( (UCHAR*)Buffer->Buffer + i );
		}
		else
		{
			Status = KdPortGetByte ( NULL );
		}

		if (Status == CP_GET_ERROR)
		{
			i--;
			continue;
		}
		if (Status == CP_GET_NODATA)
		{
			*ActualReadBytes = i;
			break;
		}
	}

	return Status;
}


VOID
KEAPI
KdpSendString( 
	PCBUFFER Buffer
	)
{
	UCHAR *Buff = (UCHAR*)Buffer->Buffer;

	for (int i=0; i<Buffer->Length; i++, Buff++)
	{
		KdPortPutByte (*Buff);
	}
}


VOID
KEAPI
KdpSendControlPacket (
    IN USHORT PacketType,
    IN ULONG PacketId OPTIONAL
    )
{
	KD_PACKET Packet;

	Packet.PacketLeader = CONTROL_PACKET_LEADER;
	Packet.PacketId = 0;
	if (PacketId)
		Packet.PacketId = PacketId;
	Packet.ByteCount = 0;
	Packet.Checksum = 0;
	Packet.PacketType = PacketType;

#if TRACE_PACKETS
	KiDebugPrint ("KdpSendControlPacket: Sent:\n");
	DumpMemory ((ULONG)&Packet, sizeof(Packet), 0);
#endif

	KdpSendStringR (&Packet, sizeof(KD_PACKET));
}

ULONG KdpNextPacketIdToSend = INITIAL_PACKET_ID | SYNC_PACKET_ID;
ULONG KdpPacketIdExpected = INITIAL_PACKET_ID;


UCHAR
KEAPI
KdpReceivePacketLeader (
    IN ULONG PacketType,
    OUT PULONG PacketLeader
    )
{
	UCHAR Code;
	UCHAR Byte, PrevByte;
	UCHAR BreakinDetected = FALSE;
	UCHAR Index = 0;

	do
	{
		Code = KdPortGetByte (&Byte);
		if (Code == CP_GET_NODATA)
		{
			if (BreakinDetected)
			{
				KdControlCPending = TRUE;
				return KDP_PACKET_RESEND;
			}
			return KDP_PACKET_TIMEOUT;
		}
		else if (Code == CP_GET_ERROR)
		{
			Index = 0;
			continue;
		}
		
		if (Byte == PACKET_LEADER_BYTE || Byte == CONTROL_PACKET_LEADER_BYTE)
		{
			if (Index == 0)
			{
				PrevByte = Byte;
				Index ++;
			}
			else if (Byte == PrevByte)
			{
				Index ++;
			}
			else
			{
				PrevByte = Byte;
				Index = 1;
			}
		}
		else
		{
			if (Byte == BREAKIN_PACKET_BYTE)
			{
				BreakinDetected = TRUE;
			}
			else
			{
				BreakinDetected = FALSE;
			}
			Index = 0;
		}

		if (BreakinDetected)
		{
			KdControlCPending = TRUE;
			return KDP_PACKET_RESEND;
		}
	}
	while (Index < 4);

	if (BreakinDetected)
	{
		KdControlCPending = TRUE;
	}

	if (Byte == PACKET_LEADER_BYTE)
	{
		*PacketLeader = PACKET_LEADER;
	}
	else
	{
		*PacketLeader = CONTROL_PACKET_LEADER;
	}

	return KDP_PACKET_RECEIVED;
}


KESYSAPI
STATUS
KEAPI
KdReceivePacketWithType(
	IN USHORT* PacketType,
	OUT PCBUFFER MessageHeader,
	OUT PCBUFFER MessageData,
	OUT PULONG DataLength OPTIONAL
	)
{
	UCHAR Byte;
	UCHAR Code;
	KD_PACKET Packet;
	ULONG Checksum;
	CBUFFER FictiveHeader, FictiveHeader2;
	ULONG FictiveLength;

	if (!ARGUMENT_PRESENT(MessageHeader))
	{
		MessageHeader = &FictiveHeader;
		FictiveHeader.Buffer = NULL;
		FictiveHeader.MaxLength = 0;
		FictiveHeader.Length = 0;
	}

	if (!ARGUMENT_PRESENT(MessageData))
	{
		MessageData = &FictiveHeader2;
		FictiveHeader2.Buffer = NULL;
		FictiveHeader2.MaxLength = 0;
		FictiveHeader2.Length = 0;
	}

	if (!ARGUMENT_PRESENT(DataLength))
	{
		DataLength = &FictiveLength;
	}

	do
	{
		Code = KdpReceivePacketLeader (*PacketType, &Packet.PacketLeader);

		if (Code == KDP_PACKET_RESEND)
		{
			if (*PacketType == PACKET_TYPE_KD_POLL_BREAKIN)
			{
				return KDP_PACKET_RECEIVED;
			}
			return Code;
		}


		KdpReceiveStringR (&Code, &Packet.PacketType, sizeof(Packet.PacketType));
		if (Code == CP_GET_NODATA)
			return KDP_PACKET_TIMEOUT;
		else if (Code == CP_GET_ERROR)
		{
			if (Packet.PacketLeader != CONTROL_PACKET_LEADER)
			{
				KdpSendControlPacket (PACKET_TYPE_KD_RESEND, 0);
			}
			continue;
		}

		if (Packet.PacketLeader == CONTROL_PACKET_LEADER &&
			Packet.PacketType == PACKET_TYPE_KD_RESEND)
		{
			return KDP_PACKET_RESEND;
		}

		KdpReceiveStringR (&Code, &Packet.ByteCount, sizeof(Packet.PacketType));
		if (Code == CP_GET_NODATA)
			return KDP_PACKET_TIMEOUT;
		else if (Code == CP_GET_ERROR)
		{
			if (Packet.PacketLeader != CONTROL_PACKET_LEADER)
			{
				KdpSendControlPacket (PACKET_TYPE_KD_RESEND, 0);
			}
			continue;
		}

		KdpReceiveStringR (&Code, &Packet.PacketId, sizeof(Packet.PacketId));
		if (Code == CP_GET_NODATA)
			return KDP_PACKET_TIMEOUT;
		else if (Code == CP_GET_ERROR)
		{
			if (Packet.PacketLeader != CONTROL_PACKET_LEADER)
			{
				KdpSendControlPacket (PACKET_TYPE_KD_RESEND, 0);
			}
			continue;
		}

		KdpReceiveStringR (&Code, &Packet.Checksum, sizeof(Packet.Checksum));
		if (Code == CP_GET_NODATA)
			return KDP_PACKET_TIMEOUT;
		else if (Code == CP_GET_ERROR)
		{
			if (Packet.PacketLeader != CONTROL_PACKET_LEADER)
			{
				KdpSendControlPacket (PACKET_TYPE_KD_RESEND, 0);
			}
			continue;
		}

#if TRACE_PACKETS
		KiDebugPrint ("KdReceivePacket: Got:\n");
		DumpMemory ((ULONG)&Packet, sizeof(Packet), 0);
#endif
		
		//
		// Whole packet header received. Analyze it
		//

		if (Packet.PacketLeader == CONTROL_PACKET_LEADER)
		{
			//
			// This is a control packet
			//

			if (Packet.PacketType == PACKET_TYPE_KD_ACKNOWLEDGE)
			{
				if (Packet.PacketId != (KdpNextPacketIdToSend & ~SYNC_PACKET_ID))
					continue;
				else if(*PacketType == PACKET_TYPE_KD_ACKNOWLEDGE)
				{
					KdpNextPacketIdToSend ^= 1;
					return KDP_PACKET_RECEIVED;
				}
				else continue;
			}
			else if (Packet.PacketType == PACKET_TYPE_KD_RESET)
			{
				KiDebugPrint (" ~ KdReceivePacket: RESET packet - general reset\n");
				KdpNextPacketIdToSend = INITIAL_PACKET_ID;
				KdpPacketIdExpected = INITIAL_PACKET_ID;
				KdpSendControlPacket (PACKET_TYPE_KD_RESET, 0);
				return KDP_PACKET_RECEIVED;
			}
			else if (Packet.PacketType == PACKET_TYPE_KD_RESEND)
			{
				KiDebugPrint (" * * * KdReceivePacket: Got RESEND packet\n");
				return KDP_PACKET_RESEND;
			}
			else
			{
				continue;
			}
		}

		//
		// Data packet
		//

		else if (*PacketType == PACKET_TYPE_KD_ACKNOWLEDGE)
		{
			if (Packet.PacketId == KdpPacketIdExpected)
			{
				KiDebugPrint (" * * * KdReceivePacket: Got data packet (expecting ACK) with packet id expected to normal data packet\n");

				KdpSendControlPacket (PACKET_TYPE_KD_RESEND, 0);
				KdpNextPacketIdToSend ^= 1;
				KdpPacketIdExpected ^= 1;
				return KDP_PACKET_RECEIVED;
			}
			
			KdpSendControlPacket (PACKET_TYPE_KD_ACKNOWLEDGE, Packet.PacketId);
			continue;
		}
		
		if (Packet.ByteCount > PACKET_MAX_SIZE ||
			Packet.ByteCount < MessageHeader->MaxLength)
		{
			KiDebugPrint (" * * * KdReceivePacket: Incorrect byte count in packet [%08x]\n", Packet.ByteCount);
			KdpSendControlPacket (PACKET_TYPE_KD_RESEND, 0);
			continue;
		}
		
		*DataLength = Packet.ByteCount - MessageHeader->MaxLength;

		//
		// Read header
		//

		KdpReceiveStringR (&Code, MessageHeader->Buffer, MessageHeader->MaxLength);
		if (Code != CP_GET_SUCCESS)
		{
			KiDebugPrint (" * * * KdReceivePacket: Header reading failed, code %04x\n", Code);
			KdpSendControlPacket (PACKET_TYPE_KD_RESEND, 0);
			continue;
		}

		MessageHeader->Length = MessageHeader->MaxLength;

		//
		// Read data
		//

//		if (*DataLength)
//		{
//			__asm int 3;
//		}

		KdpReceiveStringR (&Code, MessageData->Buffer, *DataLength);
		if (Code != CP_GET_SUCCESS)
		{
			KiDebugPrint (" * * * KdReceivePacket: Data reading failed, code %04x\n", Code);
			KdpSendControlPacket (PACKET_TYPE_KD_RESEND, 0);
			continue;
		}

		//
		// Read trailing byte
		//

		Code = KdPortGetByte (&Byte);
		if (Code != CP_GET_SUCCESS || Byte != PACKET_TRAILING_BYTE)
		{
			KiDebugPrint (" * * * KdReceivePacket: Trailing byte expected in [%02x], code %04x\n", Byte, Code);
			KdpSendControlPacket (PACKET_TYPE_KD_RESEND, 0);
			continue;
		}

		// Check packet type

		if (*PacketType != Packet.PacketType && *PacketType != 0)
		{
			KiDebugPrint (" * * * KdReceivePacket: Unexpected packet type [type=%04x, expected %04x]\n", Packet.PacketType, *PacketType);
			KdpSendControlPacket (PACKET_TYPE_KD_ACKNOWLEDGE, Packet.PacketId);
			continue;
		}
		else if (*PacketType == 0)
		{
			*PacketType = Packet.PacketType;
		}

		// Check packet id

		if (Packet.PacketId == INITIAL_PACKET_ID ||
			Packet.PacketId == (INITIAL_PACKET_ID ^ 1))
		{
			if (Packet.PacketId != KdpPacketIdExpected)
			{
				KiDebugPrint (" * * * KdReceivePacket: Unexpected packet ID [ID=%08x, expected %08x]\n", Packet.PacketId, KdpPacketIdExpected);		
				KdpSendControlPacket (PACKET_TYPE_KD_ACKNOWLEDGE, Packet.PacketId);
				continue;
			}
		}
		else
		{
			KiDebugPrint (" * * * KdReceivePacket: PacketId not valid for packet [ID=%08x, expected %08x or %08x]\n", 
				Packet.PacketId,
				INITIAL_PACKET_ID,
				INITIAL_PACKET_ID ^ 1);

			KdpSendControlPacket (PACKET_TYPE_KD_RESEND, 0);
			continue;
		}

		// Check checksum

		Checksum = KdCheckSumPacket ((PUCHAR)MessageHeader->Buffer, MessageHeader->Length);
		Checksum += KdCheckSumPacket ((PUCHAR)MessageData->Buffer, *DataLength);
		if (Checksum != Packet.Checksum)
		{
			KiDebugPrint (" * * * KdReceivePacket: Checksum not valid for packet [cs=%08x, expected %08x]\n", Packet.Checksum, Checksum);
			KdpSendControlPacket (PACKET_TYPE_KD_RESEND, 0);
			continue;
		}
// hekked by scrat
#if TRACE_PACKETS
		KiDebugPrint ("KdReceivePacket: Got packet of type %04x\n", *PacketType);
		DumpMemory ((ULONG)MessageHeader->Buffer, MessageHeader->MaxLength, 0);
		if (*DataLength)
		{
			KiDebugPrint ("KdReceivePacket: Data for this packet\n");
			DumpMemory ((ULONG)MessageData->Buffer, *DataLength, 0);
		}
#endif

		// Send ACK
		KdpSendControlPacket (PACKET_TYPE_KD_ACKNOWLEDGE, Packet.PacketId);

		KdpPacketIdExpected ^= 1;

		break;
	}
	while (TRUE);

	return KDP_PACKET_RECEIVED;
}

KESYSAPI
STATUS
KEAPI
KdReceivePacket(
	IN USHORT PacketType,
	OUT PCBUFFER MessageHeader,
	OUT PCBUFFER MessageData,
	OUT PULONG DataLength OPTIONAL
	)
{
	return KdReceivePacketWithType (&PacketType, MessageHeader, MessageData, DataLength);
}

KESYSAPI
VOID
KEAPI
KdSendPacket(
	IN UCHAR PacketType,
	IN PCBUFFER MessageHeader,
	IN PCBUFFER MessageData
	)
{
	KD_PACKET Packet;
	ULONG Code;
	ULONG DataLen = 0;
	ULONG RetriesLeft = 5;
	Packet.Checksum = 0;

	if (MessageData)
	{
		DataLen = MessageData->Length;
		Packet.Checksum = KdCheckSumPacket ((PUCHAR)MessageData->Buffer, MessageData->Length);
	}

	Packet.Checksum += KdCheckSumPacket ((PUCHAR)MessageHeader->Buffer, MessageHeader->Length);
	Packet.PacketLeader = PACKET_LEADER;
	Packet.ByteCount = (USHORT)(MessageHeader->Length + DataLen);
	Packet.PacketType = PacketType;

	do
	{
		if (RetriesLeft == 0)
		{
			if (PacketType == PACKET_TYPE_KD_DEBUG_IO)
			{
				if (((PDBGKD_DEBUG_IO)MessageHeader->Buffer)->ApiNumber == DbgKdPrintStringApi)
				{
					KdpNextPacketIdToSend = INITIAL_PACKET_ID | SYNC_PACKET_ID;
					KdpPacketIdExpected = INITIAL_PACKET_ID;
					return;
				}
			}
			else if (PacketType == PACKET_TYPE_KD_STATE_CHANGE64)
			{
				if (((PDBGKD_ANY_WAIT_STATE_CHANGE)MessageHeader->Buffer)->NewState == DbgKdLoadSymbolsStateChange)
				{
					KdpNextPacketIdToSend = INITIAL_PACKET_ID | SYNC_PACKET_ID;
					KdpPacketIdExpected = INITIAL_PACKET_ID;
					return;
				}
			}
		}

		// Send packet hdr

		Packet.PacketId = KdpNextPacketIdToSend;
		KdpSendStringR (&Packet, sizeof(KD_PACKET));
	
#if TRACE_PACKETS
		KiDebugPrint ("KdSendPacket: Sent:\n");
		DumpMemory ((ULONG)&Packet, sizeof(Packet), 0);
#endif

		// Send functional hdr

		KdpSendString (MessageHeader);

		// Send data

		if (DataLen)
		{
			KdpSendString (MessageData);
		}

		// Send trailing byte
		KdPortPutByte (PACKET_TRAILING_BYTE);

#if TRACE_PACKETS
		KiDebugPrint ("KdSendPacket: Sent packet of type %04x\n", PacketType);
		DumpMemory ((ULONG)MessageHeader->Buffer, MessageHeader->Length, 0);
		if (DataLen)
		{
			KiDebugPrint ("KdSendPacket: Data for this packet\n");
			DumpMemory ((ULONG)MessageData->Buffer, DataLen, 0);
		}
#endif

		// Wait for ACK
		Code = KdReceivePacket (PACKET_TYPE_KD_ACKNOWLEDGE, 0, 0, 0);

		if (Code == KDP_PACKET_TIMEOUT)
		{
			RetriesLeft--;
		}
	}
	while (Code != KDP_PACKET_RECEIVED);

	// Reset sync bit
	KdpNextPacketIdToSend &= ~SYNC_PACKET_ID;
}

namespace NT
{
	ULONG MmSystemRangeStart = 0x80000000;
	UCHAR RtlpBreakPointWithStatusInstruction = 0xCC;
	ULONG PsLoadedModuleList[] = { 0, 0 };
	ULONG MmHighestUserAddress = 0x7FFFFFFF;
	ULONG MmUserProbeAddress =  0x7FFFE000;

	#define KERNEL_BASE 0x80100000

	#define MAKESYSPTR(x) ((ULONG)(x) )
	#define MAKESYSPTR64(x) ((ULONG64) ((ULONG)(x) ))
	#define MAKESIGNED64(x) ((ULONG64)(LONG64)(LONG)(x))

	extern LIST_ENTRY KdpDebuggerDataListHead;

	KPROCESSOR_STATE ProcessorControlSpace = 
		{
			{0},
			{
				0x8000003b,
				0,
				0x0f1c0580,
				0x000006f9,
				0,
				0,
				0,
				0,
				0,
				0,
				{0},
				{0},
				0, // tr
				0, // ldtr
				0
			}
		};

	typedef struct _KDDEBUGGER_DATA {
		LIST_ENTRY List;
		ULONG64  Unused;
		ULONG    ValidBlock; // 'GBDK'
		ULONG    Size; // 0x290
		ULONG64  KernelBase;
		ULONG64  RtlpBreakWithStatusInstruction;
		ULONG64	 SavedContext;
		USHORT	 ThCallbackStack;
		USHORT	 NextCallback;
		USHORT	 FramePointer;
		USHORT	 PaeEnabled;
		ULONG64  KiCallUserMode;
		ULONG64  KeUserCallbackDispatcher;
		ULONG64  PsLoadedModuleList;
		ULONG64  PsActiveProcessHead;
		ULONG64  PspCidTable;
		ULONG64  ExpSystemResourcesList;
		ULONG64  ExpPagedPoolDescriptor;
		ULONG64  ExpNumberOfPagedPools;
		ULONG64  KeTimeIncrement;
		ULONG64  KeBugCheckCallbackListHead;
		ULONG64  KiBugCheckData;
		ULONG64  IopErrorLogListHead;
		ULONG64  ObpRootDirectoryObject;
		ULONG64  ObpTypeObjectType;
		ULONG64  MmSystemCacheStart;
		ULONG64  MmSystemCacheEnd;
		ULONG64  MmSystemCacheWs;
		ULONG64  MmPfnDatabase;
		ULONG64  MmSystemPtesStart;
		ULONG64  MmSystemPtesEnd;
		ULONG64  MmSubsectionBase;
		ULONG64  MmNumberOfPagingFiles;
		ULONG64  MmLowestPhysicalPage;
		ULONG64  MmHighestPhysicalPage;
		ULONG64  MmNumberOfPhysicalPages;
		ULONG64  MmMaximumNonPagedPoolInBytes;
		ULONG64  MmNonPagedSystemStart;
		ULONG64  MmNonPagedPoolStart;
		ULONG64  MmNonPagedPoolEnd;
		ULONG64  MmPagedPoolStart;
		ULONG64  MmPagedPoolEnd;
		ULONG64  MmPagedPoolInfo;
		ULONG64  Unused2;
		ULONG64  MmSizeOfPagedPoolInBytes;
		ULONG64  MmTotalCommitLimit;
		ULONG64  MmTotalCommittedPages;
		ULONG64  MmSharedCommit;
		ULONG64  MmDriverCommit;
		ULONG64  MmProcessCommit;
		ULONG64  MmPagedPoolCommit;
		ULONG64  Unused3;
		ULONG64  MmZeroedPageListHead;
		ULONG64  MmFreePageListHead;
		ULONG64  MmStandbyPageListHead;
		ULONG64  MmModifiedPageListHead;
		ULONG64  MmModifiedNoWritePageListHead;
		ULONG64  MmAvailablePages;
		ULONG64  MmResidentAvailablePages;
		ULONG64  PoolTrackTable;
		ULONG64  NonPagedPoolDescriptor;
		ULONG64  MmHighestUserAddress;
		ULONG64  MmSystemRangeStart;
		ULONG64  MmUserProbeAddress;
		ULONG64  KdPrintCircularBuffer;
		ULONG64  KdPrintWritePointer;
		ULONG64  KdPrintWritePointer2;
		ULONG64  KdPrintRolloverCount;
		ULONG64  MmLoadedUserImageList;
		ULONG64  NtBuildLab;
		ULONG64  Unused4;
		ULONG64  KiProcessorBlock;
		ULONG64  MmUnloadedDrivers;
		ULONG64  MmLastUnloadedDriver;
		ULONG64  MmTriageActionTaken;
		ULONG64  MmSpecialPoolTag;
		ULONG64  KernelVerifier;
		ULONG64  MmVerifierData;
		ULONG64  MmAllocateNonPagedPool;
		ULONG64  MmPeakCommitment;
		ULONG64  MmTotalCommitLimitMaximum;
		ULONG64  CmNtCSDVersion;
		ULONG64  MmPhysicalMemoryBlock;
		ULONG64  MmSessionBase;
		ULONG64  MmSessionSize;
		ULONG64  Unused5;

	} KDDEBUGGER_DATA, *PKDDEBUGGER_DATA;


	KDDEBUGGER_DATA DataBlock = {
		{
			(LIST_ENTRY*) MAKESYSPTR (&KdpDebuggerDataListHead),
			(LIST_ENTRY*) MAKESYSPTR (&KdpDebuggerDataListHead)
		},
		0,
		'GBDK',
		sizeof (KDDEBUGGER_DATA),
		MAKESYSPTR64 (KERNEL_BASE),
		MAKESYSPTR64 (&RtlpBreakPointWithStatusInstruction),
		0, // SavedContext
		0x012c, // CallbackStack
		0x0008, // NextCallback
		0x0018, // FramePointer
		0, // PaeEnabled
		0, // KiCallUserMode
		0, // KeUserCallbackDispatcher
		MAKESYSPTR64 (&PsLoadedModuleList),
		NULL,   // MAKESYSPTR (&PsActiveProcessHead),
		NULL,
		NULL,
		NULL,
		NULL,
		NULL,
		NULL,
		NULL,
		NULL,
		NULL,
		NULL,
		NULL,
		NULL,
		NULL,
		NULL, //MAKESYSPTR (&MmPfnDatabase),
		NULL,
		NULL,
		NULL,
		NULL,
		NULL,
		NULL,
		NULL,
		NULL,
		NULL,
		NULL,
		NULL,
		NULL,
		NULL,
		NULL,
		NULL,
		NULL,
		NULL,
		NULL,
		NULL,
		NULL,
		NULL,
		NULL,
		NULL,
		NULL,
		NULL,
		NULL,
		NULL,
		NULL,
		NULL,
		NULL,
		NULL,
		NULL,
		MAKESYSPTR64 (&MmHighestUserAddress),
		MAKESYSPTR64 (&MmSystemRangeStart),
		MAKESYSPTR64 (&MmUserProbeAddress),
		NULL,
		NULL,
		NULL,
		NULL,
		NULL,
		NULL,
		NULL,
		NULL,
		NULL,
		NULL,
		NULL,
		NULL,
		NULL,
		NULL,
		NULL,
		NULL,
		NULL,
		NULL,
		NULL,
		NULL,
		NULL,
		NULL
	};

	LIST_ENTRY KdpDebuggerDataListHead = {
		(LIST_ENTRY*) MAKESYSPTR (&DataBlock),
		(LIST_ENTRY*) MAKESYSPTR (&DataBlock)
		};

	#define KE_CODE_SEGMENT 0x08
	#define KE_STCK_SEGMENT 0x10
	#define KE_DATA_SEGMENT 0x23
	#define KE_KPCR_SEGMENT 0x30

	#define SIZE_OF_80387_REGISTERS      80
	#define CONTEXT_i386    0x00010000    // this assumes that i386 and
	#define CONTEXT_i486    0x00010000    // i486 have identical context records
	#define CONTEXT_CONTROL         (CONTEXT_i386 | 0x00000001L) // SS:SP, CS:IP, FLAGS, BP
	#define CONTEXT_INTEGER         (CONTEXT_i386 | 0x00000002L) // AX, BX, CX, DX, SI, DI
	#define CONTEXT_SEGMENTS        (CONTEXT_i386 | 0x00000004L) // DS, ES, FS, GS
	#define CONTEXT_FLOATING_POINT  (CONTEXT_i386 | 0x00000008L) // 387 state
	#define CONTEXT_DEBUG_REGISTERS (CONTEXT_i386 | 0x00000010L) // DB 0-3,6,7
	#define CONTEXT_EXTENDED_REGISTERS  (CONTEXT_i386 | 0x00000020L) // cpu specific extensions
	#define CONTEXT_FULL (CONTEXT_CONTROL | CONTEXT_INTEGER |\
						  CONTEXT_SEGMENTS)
	#define MAXIMUM_SUPPORTED_EXTENSION     512

	typedef struct _FLOATING_SAVE_AREA {
		DWORD   ControlWord;
		DWORD   StatusWord;
		DWORD   TagWord;
		DWORD   ErrorOffset;
		DWORD   ErrorSelector;
		DWORD   DataOffset;
		DWORD   DataSelector;
		BYTE    RegisterArea[SIZE_OF_80387_REGISTERS];
		DWORD   Cr0NpxState;
	} FLOATING_SAVE_AREA;

	typedef FLOATING_SAVE_AREA *PFLOATING_SAVE_AREA;
	typedef struct _NTCONTEXT {
		DWORD ContextFlags;
		DWORD   Dr0;
		DWORD   Dr1;
		DWORD   Dr2;
		DWORD   Dr3;
		DWORD   Dr6;
		DWORD   Dr7;
		FLOATING_SAVE_AREA FloatSave;
		DWORD   SegGs;
		DWORD   SegFs;
		DWORD   SegEs;
		DWORD   SegDs;
		DWORD   Edi;
		DWORD   Esi;
		DWORD   Ebx;
		DWORD   Edx;
		DWORD   Ecx;
		DWORD   Eax;
		DWORD   Ebp;
		DWORD   Eip;
		DWORD   SegCs;              // MUST BE SANITIZED
		DWORD   EFlags;             // MUST BE SANITIZED
		DWORD   Esp;
		DWORD   SegSs;
		BYTE    ExtendedRegisters[MAXIMUM_SUPPORTED_EXTENSION];

	} NTCONTEXT, *PNTCONTEXT;
} // namespace NT

using namespace NT;


UCHAR KdpDataBuffer[256];
UCHAR KdpHdrBuffer[32];

#define MAKEBUFFER(b,x)					\
	{									\
		(b)->Buffer = (x);				\
		(b)->Length = sizeof(*(x));		\
		(b)->MaxLength = sizeof(*(x));	\
	}

BOOLEAN
KEAPI
KdpReceiveAndReplyPackets(
	)
/*++
	Receive and reply to DBGKD packets.
	Environment: kd port locked
--*/
{
	ULONG Size;
	CBUFFER Header;
	CBUFFER Data;
	UCHAR Code;
	USHORT Type;
	BOOLEAN bContinue = FALSE;

	DBGKD_MANIPULATE_STATE32 hdr;

	Header.Buffer = &hdr;
	Header.Length = Header.MaxLength = sizeof(hdr);

	MAKEBUFFER (&Data, &KdpDataBuffer);	

	Type = 0;
	Size = 0;
	Code = (UCHAR)KdReceivePacketWithType (&Type, &Header, &Data, &Size);
	
	if (Code != KDP_PACKET_RECEIVED)
	{
#if KD_TRACE_PORT_IO_ERRORS
		KiDebugPrint (" * * * KdpCheckPackets(): KdReceivePacket returned %d\n", Code);
#endif
		return bContinue;
	}

	if (Size != 0)
	{
		KiDebugPrint ("KdpReceiveAndReplyPackets: Got data with the following packet [size=%08x]:\n", Size);
	}

//	if (Type == PACKET_TYPE_KD_RESET)

	if (Type != PACKET_TYPE_KD_STATE_MANIPULATE)
	{
		KiDebugPrint ("KdpReceiveAndReplyPackets: Packet with type %d unexpected\n", Type);
		return 0;
	}

	switch (hdr.ApiNumber)
	{
	case DbgKdGetVersionApi:
		{
			KiDebugPrint ("Version requested.\n");

			hdr.u.GetVersion64.MajorVersion = 0x0F;
			hdr.u.GetVersion64.MinorVersion = 2600;
			hdr.u.GetVersion64.ProtocolVersion = DBGKD_PROTOCOL_VERSION;
			hdr.u.GetVersion64.Flags = 3;
			hdr.u.GetVersion64.MachineType = IMAGE_FILE_MACHINE_I386;
			hdr.u.GetVersion64.MaxPacketType = PACKET_TYPE_MAX;
			hdr.u.GetVersion64.MaxStateChange = DbgKdMaximumStateChange - DbgKdMinimumStateChange;
			hdr.u.GetVersion64.MaxManipulate = DbgKdMaximumManipulate - DbgKdMinimumManipulate;
			hdr.u.GetVersion64.Simulation = 0;
			hdr.u.GetVersion64.KernelBase =			MAKESIGNED64 (KERNEL_BASE);
			hdr.u.GetVersion64.PsLoadedModuleList =	MAKESIGNED64 (&NT::PsLoadedModuleList);
			hdr.u.GetVersion64.DebuggerDataList =	MAKESIGNED64 (&NT::KdpDebuggerDataListHead);

			hdr.ReturnStatus = STATUS_SUCCESS;

			KdSendPacket (PACKET_TYPE_KD_STATE_MANIPULATE, &Header, NULL);

			break;
		}

	case DbgKdReadVirtualMemoryApi:
		{
			KiDebugPrint ("VMREAD [%08x, len %08x] : ",
				(ULONG)hdr.u.ReadMemory64.TargetBaseAddress,
				hdr.u.ReadMemory64.TransferCount);

			ULONG addr = (ULONG)hdr.u.ReadMemory64.TargetBaseAddress;
			ULONG cnt = hdr.u.ReadMemory64.TransferCount;

			hdr.u.ReadMemory64.ActualBytesRead = cnt;
			hdr.ReturnStatus = STATUS_SUCCESS;

			CBUFFER data;
			UCHAR *cdata = (UCHAR*) ExAllocateHeap (FALSE, cnt);

			KeZeroMemory (cdata, cnt);

			if ((addr & 0xFFFFF000) == 0xFFDF0000)
			{
				KiDebugPrint (" KUSER_SHARED_DATA okay\n");

				addr ^= 0xFFDF0000;
				addr |= 0x7FFE0000;

				//KeCopyMemory ((void*)addr, cdata, cnt);
			}
			else
			{
				if (MmIsAddressValid ((void*)addr))
				{
					KiDebugPrint (" MAPPED okay [%08x=>%08x]\n", (ULONG)MAKESYSPTR (addr), addr);

					memcpy ((void*)addr, cdata, cnt);
				}
				else
				{
					KiDebugPrint (" access violation\n");
					hdr.ReturnStatus = STATUS_ACCESS_VIOLATION;
					ExFreeHeap (cdata);
					KdSendPacket (PACKET_TYPE_KD_STATE_MANIPULATE, &Header, 0);
					break;
				}
			}

			data.Buffer = cdata;
			data.Length = (USHORT)hdr.u.ReadMemory64.TransferCount;

			KdSendPacket (PACKET_TYPE_KD_STATE_MANIPULATE, &Header, &data);

			ExFreeHeap (cdata);

			break;
		}

	case DbgKdReadControlSpaceApi:
		{
			KiDebugPrint ("Control space read attempt [*=%08x, size=%08x]\n",
				(ULONG)hdr.u.ReadMemory64.TargetBaseAddress,
				hdr.u.ReadMemory64.TransferCount);

			hdr.u.ReadMemory64.ActualBytesRead = hdr.u.ReadMemory64.TransferCount;
			hdr.ReturnStatus = STATUS_SUCCESS;

			CBUFFER data;
			UCHAR *cdata = (UCHAR*) ExAllocateHeap (FALSE, hdr.u.ReadMemory64.TransferCount);

			memcpy (cdata, (UCHAR*)&NT::ProcessorControlSpace + (USHORT)hdr.u.ReadMemory64.TargetBaseAddress, hdr.u.ReadMemory64.TransferCount);

			data.Buffer = cdata;
			data.Length = (USHORT)hdr.u.ReadMemory64.TransferCount;

			KdSendPacket (PACKET_TYPE_KD_STATE_MANIPULATE, &Header, &data);

			ExFreeHeap (cdata);

			break;
		}

	case DbgKdWriteControlSpaceApi:
		{
			KiDebugPrint ("Control space write attempt [*=%08x, size=%08x]\n",
				(ULONG)hdr.u.WriteMemory64.TargetBaseAddress,
				hdr.u.WriteMemory64.TransferCount);

			hdr.u.WriteMemory64.ActualBytesWritten = hdr.u.WriteMemory64.TransferCount;
			hdr.ReturnStatus = STATUS_SUCCESS;

			memcpy ((UCHAR*)&ProcessorControlSpace + (USHORT)hdr.u.WriteMemory64.TargetBaseAddress, KdpDataBuffer, hdr.u.WriteMemory64.TransferCount);

			KdSendPacket (PACKET_TYPE_KD_STATE_MANIPULATE, &Header, 0);

			break;
		}

	case DbgKdRestoreBreakPointApi:
		{
			KiDebugPrint ("Breakpoint restore attempt [%08x].\n", hdr.u.RestoreBreakPoint.BreakPointHandle);

			hdr.ReturnStatus = STATUS_SUCCESS;

			KdSendPacket (PACKET_TYPE_KD_STATE_MANIPULATE, &Header, 0);
			break;
		}

	case DbgKdClearAllInternalBreakpointsApi:
		{
			KiDebugPrint ("Clear all internal breakpoints attempt.\n");

			hdr.ReturnStatus = STATUS_SUCCESS;

			KdSendPacket (PACKET_TYPE_KD_STATE_MANIPULATE, &Header, 0);
			break;
		}

	case DbgKdGetContextApi:
		{
			KiDebugPrint ("Get context attempt\n");

			hdr.ReturnStatus = STATUS_SUCCESS;
			NTCONTEXT ctx = {CONTEXT_FULL};

			/*if (!::ctx)
			{
				GetThreadContext (GetCurrentThread(), &ctx);
			}
			else
			{
				ctx = *::ctx;
			}

			ctx.Eip |= 0x80000000;
			ctx.Esp |= 0x80000000;
			ctx.Ebp |= 0x80000000;
			*/

			__asm {
				call _1
_1:				pop [ctx.Eip]
			}

			ctx.SegCs = KE_CODE_SEGMENT;
			ctx.SegSs = KE_STCK_SEGMENT;
			ctx.SegDs = KE_DATA_SEGMENT;
			ctx.SegEs = KE_DATA_SEGMENT;
			ctx.SegFs = KE_KPCR_SEGMENT;

			CBUFFER data;
			MAKEBUFFER (&data, &ctx);

			KiDebugPrint ("Context sent: EIP=%08x, ESP=%08x, EBP=%08x\n", ctx.Eip, ctx.Esp, ctx.Ebp);

			KdSendPacket (PACKET_TYPE_KD_STATE_MANIPULATE, &Header, &data);

			break;
		}

	case DbgKdSetContextApi:
		{
			KiDebugPrint ("Set context attempt\n");

			/**ctx = *(CONTEXT*) KdpDataBuffer;

			ctx->SegCs = 0x1b;
			ctx->SegDs = 0x23;
			ctx->SegEs = 0x23;
			ctx->SegSs = 0x23;
			ctx->SegFs = 0x3B;
			ctx->Eip &= 0x7FFFFFFF;
			ctx->Esp &= 0x7FFFFFFF;
			ctx->Ebp &= 0x7FFFFFFF;
			*/

			hdr.ReturnStatus = STATUS_SUCCESS;
			KdSendPacket (PACKET_TYPE_KD_STATE_MANIPULATE, &Header, 0);
			
			break;
		}

	case DbgKdContinueApi2:
		{
			KiDebugPrint ("Continue. Breaking again\n");

			hdr.ReturnStatus = STATUS_SUCCESS;
			KdSendPacket (PACKET_TYPE_KD_STATE_MANIPULATE, &Header, 0);

			bContinue = TRUE;

			break;
		}

	case DbgKdCauseBugCheckApi:
		{
			KiDebugPrint ("Bugcheck caused. Stopping simulation\n");
			
			KeBugCheck (MANUALLY_INITIATED_CRASH, 1, 0, 0, 0);
		}

	case DbgKdQueryMemoryApi:
		{
			ULONG addr = (ULONG)hdr.u.QueryMemory.Address;

			KiDebugPrint ("Memory queried [%08x] from address space %08x\n",
				addr,
				hdr.u.QueryMemory.AddressSpace);

			hdr.ReturnStatus = NTSTATUS_ACCESS_VIOLATION;

			if (MmIsAddressValid ((void*)addr))
			{
				hdr.u.QueryMemory.Flags = DBGKD_QUERY_MEMORY_READ | 
										  DBGKD_QUERY_MEMORY_WRITE |
										  DBGKD_QUERY_MEMORY_EXECUTE;

				hdr.ReturnStatus = STATUS_SUCCESS;
			}

			
			KdSendPacket (PACKET_TYPE_KD_STATE_MANIPULATE, &Header, 0);
			break;
		}

	case DbgKdWriteBreakPointApi:
		{
			ULONG addr = (ULONG) hdr.u.WriteBreakPoint.BreakPointAddress;
			KiDebugPrint ("Writing breakpoint to [%08x]\n", addr);

			hdr.ReturnStatus = STATUS_SUCCESS;
			KdSendPacket (PACKET_TYPE_KD_STATE_MANIPULATE, &Header, 0);
			break;
		}
				
	default:
		KiDebugPrint (" * * * KdpCheckPackets: Unexpected API number [%08x]\n", hdr.ApiNumber);

		hdr.ReturnStatus = NTSTATUS_NOT_IMPLEMENTED;
		KdSendPacket (PACKET_TYPE_KD_STATE_MANIPULATE, &Header, 0);

	}

	return bContinue;
}

BOOLEAN KdpKernelDebuggerActive = FALSE;

VOID
KEAPI
KdpSendStateChange(
	ULONG ExceptionCode,
	ULONG NumberParameters,
	ULONG Parameter1,
	ULONG Parameter2,
	ULONG Parameter3,
	ULONG Parameter4,
	ULONG ExceptionAddress,
	BOOLEAN FirstChance
)
{
	DBGKD_ANY_WAIT_STATE_CHANGE State64 = {0};

	State64.NewState = DbgKdExceptionStateChange;
	State64.ProcessorLevel = 0x0F;
	State64.Processor = 0;
	State64.NumberProcessors = 1;
	State64.Thread = 0;
	State64.ProgramCounter = 0;
	EXCEPTION_RECORD64 *e = &State64.u.Exception.ExceptionRecord;
	e->ExceptionCode = ExceptionCode;
	e->ExceptionFlags = 0;
	e->ExceptionRecord = 0;
	e->ExceptionAddress = MAKESIGNED64 (ExceptionAddress);
	e->NumberParameters = NumberParameters;
	e->ExceptionInformation[0] = Parameter1;
	e->ExceptionInformation[1] = Parameter2;
	e->ExceptionInformation[2] = Parameter3;
	e->ExceptionInformation[3] = Parameter4;
	State64.u.Exception.FirstChance = FirstChance;
	State64.ControlReport.Dr6 = 0;
	State64.ControlReport.Dr7 = 0;
	State64.ControlReport.InstructionCount = 0x0400;
	State64.ControlReport.ReportFlags = 0;
	State64.ControlReport.InstructionStream[0] = 0x33;
	State64.ControlReport.InstructionStream[1] = 0xC0;
	State64.ControlReport.InstructionStream[2] = 0xCC;
	State64.ControlReport.InstructionStream[3] = 0xC3;
	State64.ControlReport.InstructionStream[4] = 0x90;
	State64.ControlReport.InstructionStream[5] = 0x90;
	State64.ControlReport.InstructionStream[6] = 0x90;
	State64.ControlReport.InstructionStream[7] = 0x90;
	State64.ControlReport.InstructionStream[8] = 0x90;
	State64.ControlReport.InstructionStream[9] = 0x90;
	State64.ControlReport.InstructionStream[10] = 0x90;
	State64.ControlReport.InstructionStream[11] = 0x90;
	State64.ControlReport.InstructionStream[12] = 0x90;
	State64.ControlReport.InstructionStream[13] = 0x90;
	State64.ControlReport.InstructionStream[14] = 0x90;
	State64.ControlReport.InstructionStream[15] = 0x90;
	State64.ControlReport.SegCs = KE_CODE_SEGMENT;
	State64.ControlReport.SegDs = KE_DATA_SEGMENT;
	State64.ControlReport.SegEs = KE_DATA_SEGMENT;
	State64.ControlReport.SegFs = KE_KPCR_SEGMENT;
	State64.ControlReport.EFlags = 0x202;
	State64.__padding[2] = 0;

	CBUFFER hdr;

	MAKEBUFFER (&hdr, &State64);

	KdSendPacket (PACKET_TYPE_KD_STATE_CHANGE64, &hdr, NULL);
}

KESYSAPI
VOID
KEAPI
KdWakeUpDebugger(
	ULONG ExceptionCode,
	ULONG NumberParameters,
	ULONG Parameter1,
	ULONG Parameter2,
	ULONG Parameter3,
	ULONG Parameter4,
	ULONG ExceptionAddress,
	BOOLEAN FirstChance
	)
/*++
	This function initiates DBGKD connection
--*/
{
	if (KdDebuggerEnabled == FALSE)
	{
		KiDebugPrintRaw ("KdWakeUpDebugger : No kernel debugger\n");
		KiStopExecution ();
	}

	BOOLEAN OldState;
	KdLockPort (&OldState);
	KdpKernelDebuggerActive = TRUE;

	ULONG Code;

	KdpSendControlPacket (PACKET_TYPE_KD_RESET, 0);
	
	do
	{
		Code = KdReceivePacket (PACKET_TYPE_KD_RESET, 0, 0, 0);
	}
	while (Code != KDP_PACKET_RECEIVED);

	KdpSendStateChange (ExceptionCode, NumberParameters, Parameter1, Parameter2, Parameter3, Parameter4, 
		ExceptionAddress, FirstChance);

	while (KdpReceiveAndReplyPackets())
	{
		NOTHING;
	}

	KdpKernelDebuggerActive = FALSE;
	KdUnlockPort (OldState);
}