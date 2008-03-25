#include "common.h"

STATUS
KEAPI
FsFatDriverEntry (
	PDRIVER DriverObject
	)
/*++
	Initialize the FS FAT driver
--*/
{
	KdPrint(("FSFAT: INIT\n"));

	return STATUS_SUCCESS;
}