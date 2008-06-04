//
// Kernel driver for GR8OS
//
// Compact Disk File System driver
//

typedef unsigned short wchar_t;
#include "common.h"

// Driver entry point
STATUS DriverEntry(IN PDRIVER DriverObject)
{
	KdPrint(("CDFS: In DriverEntry()\n"));

	return STATUS_SUCCESS;
}
