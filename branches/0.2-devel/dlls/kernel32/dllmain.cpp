#include "common.h"

ULONG
KEAPI
GetVersion(
	)
{
	return 0x80000004;	// Windows Me/98/95
}


STATUS
KEAPI 
DriverEntry(
	PDRIVER DriverObject
	)
{
	KdPrint(("kernel32.dll driver entry\n"));

	return STATUS_SUCCESS;
}
