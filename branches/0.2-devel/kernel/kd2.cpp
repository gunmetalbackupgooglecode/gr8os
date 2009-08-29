//
// FILE:		kd2.cpp
// CREATED:		28-Feb-2008  by Great
// PART:        KD2
// ABSTRACT:
//			Built-in kernel debugger
//

#include "common.h"

#define KD_VERBOSE 1

ULONG Kd2NextPacketId;
UCHAR Kd2ComPort;

#define KD_RETRY_COUNT                      16
#define KD_DELAY_LOOP                       0x00010000

#define KD_INITIAL_PACKET_ID                0x80800000
#define KD_SYNC_PACKET_ID                   0x00000800

#define KD_PACKET_TRAILING_BYTE             0xAA

struct 
{
	KD_PACKET Header;
	UCHAR Data[PAGE_SIZE - sizeof(KD_PACKET)];
} Kd2StaticPacket;

BOOLEAN
KEAPI
Kd2ComReceiveData(
	PVOID Buffer,
	ULONG Size
	)
{
	ULONG RSize = Size;
	STATUS St;

	St = HalReadComPort (Kd2ComPort, Buffer, &RSize);

	if (St != STATUS_SUCCESS)
		return FALSE;

	if (RSize != Size)
		return FALSE;

	return TRUE;
}

BOOLEAN
KEAPI
Kd2ComSendData(
	PVOID Buffer,
	ULONG Size
	)
{
	return (HalWriteComPort (Kd2ComPort, Buffer, Size) == STATUS_SUCCESS);
}
	

BOOLEAN
KEAPI
Kd2ComSendControlPacket(
    USHORT PacketType,
    ULONG PacketId
    )
{
	KD_PACKET Header = {0};

	Header.PacketLeader = CONTROL_PACKET_LEADER;
	Header.PacketType = PacketType;
	Header.PacketId = PacketId;

	if (Kd2ComSendData (&Header, sizeof(Header)) 
		== FALSE)
	{
		return FALSE;
	}

#if KD_VERBOSE
	KdPrint(("KD: Sent type %u control packet.\n", PacketType));
#endif

	return TRUE;
}

BOOLEAN
KEAPI
Kd2ReceivePacket (
	VOID
	)
{
	PKD_PACKET Header;
	UCHAR TrailingByte;

	Header = &Kd2StaticPacket.Header;

Retry:

	for (;;)
	{
		if (Kd2ComReceiveData (&Header->PacketLeader, 
			sizeof (Header->PacketLeader)) == FALSE)
		{
			return FALSE;
		}

		if (Header->PacketLeader == PACKET_LEADER)
		{
			break;
		}

		if (Header->PacketLeader == CONTROL_PACKET_LEADER)
		{
			break;
		}
	}

	if (Kd2ComReceiveData (&Header->PacketType,
		sizeof(Header->PacketType)) == FALSE)
	{
		return FALSE;
	}

	if (Kd2ComReceiveData (&Header->ByteCount,
		sizeof(Header->ByteCount)) == FALSE)
	{
		return FALSE;
	}

	if (Kd2ComReceiveData (&Header->PacketId,
		sizeof(Header->PacketId)) == FALSE)
	{
		return FALSE;
	}

	if (Kd2ComReceiveData (&Header->Checksum,
		sizeof(Header->Checksum)) == FALSE)
	{
		return FALSE;
	}

	if (Header->ByteCount > sizeof(Kd2StaticPacket.Data))
	{
		goto Retry;
	}

	if (Header->ByteCount > 0)
	{
		if (Kd2ComReceiveData (Kd2StaticPacket.Data,
			Header->ByteCount) == FALSE)
		{
			return FALSE;
		}

		if (Kd2ComReceiveData (&TrailingByte, 1) == FALSE)
		{
			return FALSE;
		}

		if (TrailingByte != KD_PACKET_TRAILING_BYTE)
		{
			goto Retry;
		}
	}

#if KD_VERBOSE
	KdPrint(("KD: Received type %u packet.\n", Header->PacketType));
#endif
	
	return TRUE;
}

ULONG
KEAPI
Kd2ComputeChecksum (
	PVOID Buffer,
	ULONG Length
	)
{
	ULONG Checksum = 0;
	ULONG Index;

	for (Index = 0; Index < Length; Index ++)
	{
		Checksum += ((PUCHAR)Buffer)[Index];
	}

	return Checksum;
}

BOOLEAN
KEAPI
Kd2ComSendPacket (
	USHORT PacketType,
	PVOID Header,
	USHORT HeaderSize,
	PVOID Data,
	USHORT DataSize
	)
{
	USHORT ByteCount;
	ULONG Checksum;
	KD_PACKET Packet;

	ASSERT (HeaderSize > 0);

Resend:

	ByteCount = HeaderSize;
	Checksum = Kd2ComputeChecksum (Header, HeaderSize);

	if (Data != NULL)
	{
		ASSERT (DataSize > 0);

		ByteCount += DataSize;
		Checksum += Kd2ComputeChecksum (Data, DataSize);
	}

	//
	// Send packet.
	//

	Packet.PacketLeader = PACKET_LEADER;
	Packet.PacketId = Kd2NextPacketId;
	Packet.PacketType = PacketType;
	Packet.ByteCount = ByteCount;
	Packet.Checksum = Checksum;

	if (Kd2ComSendData (&Packet, sizeof(Packet)) == FALSE)
	{
		return FALSE;
	}

	if (Kd2ComSendData (Header, HeaderSize) == FALSE)
	{
		return FALSE;
	}

	if (Data != NULL)
	{
		if (Kd2ComSendData (Data, DataSize) == FALSE)
		{
			return FALSE;
		}
	}

	UCHAR TrailingByte = KD_PACKET_TRAILING_BYTE;

	if (Kd2ComSendData (&TrailingByte, 1) == FALSE)
	{
		return FALSE;
	}

#if KD_VERBOSE
	KdPrint(("KD: Send type %u packet.\n", Packet.PacketType));
#endif

	//
	// Update packet ID.
	//

	Kd2NextPacketId &= (~SYNC_PACKET_ID);
	Kd2NextPacketId ^= 1;

	if (Kd2ReceivePacket () != FALSE)
	{
		switch (Kd2StaticPacket.Header.PacketType)
		{
		case PACKET_TYPE_KD_RESET:
			{
#if KD_VERBOSE
				KdPrint(("KD: Received RESET after send.\n"));
#endif
				Kd2ComSendControlPacket (PACKET_TYPE_KD_RESET, 0);

				goto Resend;
			}

		case PACKET_TYPE_KD_RESEND:
			{
#if KD_VERBOSE
				KdPrint(("LD: Received RESEND after send.\n"));
#endif

				goto Resend;
			}
		}
	}

	return TRUE;
}

BOOLEAN
KEAPI
Kd2ComConnect (
	VOID
	)
{
	for (UCHAR Index = 0; Index < 2; Index ++)
	{
		BOOLEAN Present = HalpCheckComPortConnected (Index);

		if (Present)
		{
#if KD_VERBOSE
			KdPrint(("KD: COM%u %s\n", Index, Present ? "found." : "not found."));
#endif
			Present = HalpInitializeComPort (Index, 115200);

			ASSERT (Present == TRUE);

			Kd2ComPort = Index;

			break;
		}
	}

	Kd2NextPacketId = INITIAL_PACKET_ID | SYNC_PACKET_ID;

	for (ULONG Retry = 0; Retry < KD_RETRY_COUNT; Retry ++)
	{
#if KD_VERBOSE
		KdPrint(("KD: Trying COM%u ...\n", Kd2ComPort));
#endif

		Kd2ComSendControlPacket (PACKET_TYPE_KD_RESET, 0);
		if (Kd2ReceivePacket () != FALSE)
		{
			return TRUE;
		}
	}

	Kd2ComPort = -1;
	return FALSE;
}

VOID
KEAPI
Kd2Spin (
	VOID
	)
{
	static UCHAR state = 0;

	KiWriteChar (0, ("+-|*" [state++ & 0x3]));	
}

BOOLEAN
KEAPI
Kd2PrintString (
	PSTR String
	)
{
	DBGKD_DEBUG_IO Packet;
	ULONG StringLength;

	StringLength = strlen (String);

	if (StringLength >= 0xFFFF) {
		return FALSE;
	}

	bzero (&Packet, sizeof(Packet));

	Packet.ApiNumber = DbgKdPrintStringApi;
	Packet.u.PrintString.LengthOfString = StringLength;

	return Kd2ComSendPacket (PACKET_TYPE_KD_DEBUG_IO,
		&Packet,
		sizeof(Packet),
		String,
		(USHORT)StringLength + 1);
}

VOID
KEAPI
Kd2Initialize (
	VOID
	)
{
	if (Kd2ComConnect() == FALSE) 
	{
		KdPrint(("KD: CANNOT CONNECT\n"));
		INT3
	}

	KdPrint(("KD: Connected to COM%u\n", Kd2ComPort));
}

