
//
// Floppy Disk Driver
//

#include "common.h"


#define FdPrintWarn(x) KdPrint(x)
#define FdPrintInfo(x) KdPrint(x)
#define FdPrintErr(x)  KdPrint(x)

PDEVICE FdDeviceObject;
UNICODE_STRING FdDeviceName;

#define COMPLETE_IRP(Irp,xStatus,Info) {			\
		(Irp)->IoStatus.Status = (xStatus);			\
		(Irp)->IoStatus.Information = (Info);		\
		IoCompleteRequest ((Irp), 0);				\
		return (xStatus);							\
	}

STATUS
KEAPI
FdCreateClose(
	PDEVICE DeviceObject,
	PIRP Irp
	)
/*++
	Dispatch IRP_CREATE & IRP_CLOSE irps for the floppy device.
	Simply allow opening & closing
--*/
{
	FdPrintInfo (("FdCreateClose: MajorFunction=%d, DeviceObject=%08x\n", Irp->MajorFunction, DeviceObject));

	COMPLETE_IRP (Irp, STATUS_SUCCESS, 0);
}


STATUS
KEAPI
FdDeviceIoControl(
	PDEVICE DeviceObject,
	PIRP Irp
	)
/*++
	Dispatch IRP_IOCTL irp for the floopy device.
	However, they are not supported yet.
--*/
{
	COMPLETE_IRP (Irp, STATUS_NOT_SUPPORTED, 0);
}

STATUS
KEAPI
FdRead(
	PDEVICE DeviceObject,
	PIRP Irp
	)
/*++
	Dispatch IRP_READ irp for the floopy device.
	However, they are not supported yet.
--*/
{
	ULONG Size = Irp->BufferLength;
	STATUS Status = STATUS_INVALID_PARAMETER;
	PVOID Buffer;
	ULONG LbaSector, RelativeOffset;

	if (Irp->Flags & IRP_FLAGS_BUFFERED_IO)
	{
		Buffer = Irp->SystemBuffer;
	}
	else if (Irp->Flags & IRP_FLAGS_NEITHER_IO)
	{
		Buffer = Irp->UserBuffer;
	}

	if (Irp->CurrentStackLocation->Parameters.ReadWrite.OffsetSpecified)
	{
		LbaSector = (ULONG)(Irp->CurrentStackLocation->Parameters.ReadWrite.Offset.LowPart >> FD_SECTOR_SHIFT);
	}
	else
	{
		LbaSector = Irp->FileObject->CurrentOffset.LowPart >> FD_SECTOR_SHIFT;
	}
	
	RelativeOffset = Irp->CurrentStackLocation->Parameters.ReadWrite.Offset.LowPart % FD_SECTOR_SIZE;

	if (RelativeOffset || (Size % FD_SECTOR_SIZE))
	{
		COMPLETE_IRP (Irp, STATUS_DATATYPE_MISALIGNMENT, 0);
	}

	Status = FdPerformRead (Buffer, &Size, LbaSector, (Irp->Flags&IRP_FLAGS_SYNCHRONOUS_IO) == IRP_FLAGS_SYNCHRONOUS_IO);

	if (SUCCESS(Status))
	{
		Irp->FileObject->CurrentOffset.LowPart = (LbaSector << FD_SECTOR_SHIFT) + Size;
	}

	COMPLETE_IRP (Irp, Status, 0);
}

STATUS
KEAPI
FdWrite(
	PDEVICE DeviceObject,
	PIRP Irp
	)
{
	COMPLETE_IRP (Irp, STATUS_NOT_SUPPORTED, 0);
}

STATUS
KEAPI 
FdDriverEntry(
	PDRIVER DriverObject
	)
/*++
	Entry point of the FD built-in driver
--*/
{
	STATUS Status;

	RtlInitUnicodeString (&FdDeviceName, L"\\Device\\fdd0");

	Status = IoCreateDevice (
		DriverObject,
		&FdDeviceName,
		DEVICE_TYPE_DISK, 
		&FdDeviceObject
		);

	if (!SUCCESS(Status))
	{
		return Status;
	}

	//
	// Support the both.
	//

	FdDeviceObject->Flags |= DEVICE_FLAGS_BUFFERED_IO | DEVICE_FLAGS_NEITHER_IO;

	KdPrint (("Fd: Device created\n"));

	DriverObject->IrpHandlers[IRP_CREATE]=
	DriverObject->IrpHandlers[IRP_CLOSE] = FdCreateClose;
	DriverObject->IrpHandlers[IRP_IOCTL] = FdDeviceIoControl;
	DriverObject->IrpHandlers[IRP_READ]  = FdRead;
	DriverObject->IrpHandlers[IRP_WRITE] = FdWrite;

	DriverObject->DriverUnload = NULL; // We don't support unloading.

	return FdInit();
}
