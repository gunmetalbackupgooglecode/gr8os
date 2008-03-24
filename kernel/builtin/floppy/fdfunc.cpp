#include "common.h"

//
// Out byte to data port
//
void FdpOut(UCHAR parm)
{
	while (!(KiInPort(FD_STATUS) & FD_STATUS_RDY_FOR_IO));
	
	KiOutPort (FD_DATA, parm);
}

//
// Read byte from data port
//
UCHAR FdpIn()
{
	while (!(KiInPort(FD_STATUS) & FD_STATUS_RDY_FOR_IO));

	return KiInPort (FD_DATA);
}

//
// Wait for FD IRQ
//
void FdpWait()
{
	_enable();

	while (!FdpIrqState.FddIrqGot);
	FdpIrqState.FddIrqGot = 0;
}


void FdpDmaInit()
{
}

VOID
KEAPI
FdInit(
	)
{
	KdPrint(("FdInit()\n"));
//	for(;;);
}


STATUS
KEAPI
FdPerformRead(
	OUT PVOID Buffer,
	IN OUT PULONG BufferLength,
	IN ULONG LbaSector
	)
{
	return STATUS_UNSUCCESSFUL;
}

STATUS
KEAPI
FdPerformWrite(
	OUT PVOID Buffer,
	IN OUT PULONG BufferLength,
	IN ULONG LbaSector
	)
{
	return STATUS_UNSUCCESSFUL;
}
