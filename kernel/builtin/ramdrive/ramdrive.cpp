#include "common.h"

PDEVICE RdDeviceObject;
UNICODE_STRING RdDeviceName;

#define COMPLETE_IRP(Irp,xStatus,Info) {			\
		(Irp)->IoStatus.Status = (xStatus);			\
		(Irp)->IoStatus.Information = (Info);		\
		IoCompleteRequest ((Irp), 0);				\
		return (xStatus);							\
	}

#define RdPrint(x) KdPrint(x)

STATUS
KEAPI
RdDriverEntry (
	PDRIVER DriverObject
	)
/*++
	Initialize the FS FAT driver
--*/
{
	RdPrint(("RAMDISK: Unimplemented\n"));
	return STATUS_NOT_IMPLEMENTED;
}
