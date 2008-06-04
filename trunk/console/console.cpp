//
// Console virtual device driver for GR8OS
//
// (C) Great, 2008
//

typedef unsigned short wchar_t;

#include "common.h"

UNICODE_STRING gDeviceName;
PDEVICE gDeviceObject;

STATUS ConsoleCompleteRequest (PIRP Irp, STATUS CompletionStatus, ULONG Info=0, ULONG QuantumIncrenemt=0)
{
	Irp->IoStatus.Status = CompletionStatus;
	Irp->IoStatus.Information = Info;
	IoCompleteRequest (Irp, 0);
	return CompletionStatus;
}

//
// Create/Close handler
// Simply allow the requst.
// Maybe in the future there will be simple access check that
//  the caller can access the console.
//

STATUS ConsoleCreateClose( IN PDEVICE DeviceObject, PIRP Irp )
{
	return ConsoleCompleteRequest (Irp, STATUS_SUCCESS);
}


STATUS ConsoleWrite( IN PDEVICE DeviceObject, PIRP Irp )
{
	STATUS Status;
	PIRP_STACK_LOCATION irpSl = IoGetCurrentIrpStackLocation (Irp);
	
	KePrintActiveConsole ( (PSTR)Irp->SystemBuffer );
	Status = STATUS_SUCCESS;
	return ConsoleCompleteRequest (Irp, Status);
}

STATUS ConsoleControl( IN PDEVICE DeviceObject, PIRP Irp )
{
	STATUS Status = STATUS_NOT_IMPLEMENTED;

	//
	// BUGBUG: to do: not implemented..
	//

	return ConsoleCompleteRequest (Irp, Status);
}

// Driver entry point
STATUS DriverEntry(IN PDRIVER DriverObject)
{
	STATUS Status;

	KdPrint(("[~] Console: DriverEntry()\n"));

	//
	// Create main console. Child consoles can be created optinally by
	// user-mode startup code with the help of IOCTL request to this device
	//

	RtlInitUnicodeString( &gDeviceName, L"\\Device\\Console0" );
	Status = IoCreateDevice ( DriverObject, 0, &gDeviceName, DEVICE_TYPE_VIDEO, &gDeviceObject );
	if (!SUCCESS(Status))
	{
		KdPrint(("Console: IoCreateDevice failed with status %08x\n", Status));
		return Status;
	}

	gDeviceObject->Flags |= DEVICE_FLAGS_BUFFERED_IO;

	DriverObject->IrpHandlers[IRP_CREATE] = 
	DriverObject->IrpHandlers[IRP_CLOSE] = ConsoleCreateClose;
	DriverObject->IrpHandlers[IRP_WRITE] = ConsoleWrite;
	DriverObject->IrpHandlers[IRP_IOCTL] = ConsoleControl;
	

	KdPrint(("[+] Console: Driver initialization successful\n"));
	return STATUS_SUCCESS;
}
