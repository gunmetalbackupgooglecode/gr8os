//
// Console virtual device driver for GR8OS
//
// (C) Great, 2008
//

#include "common.h"

struct CONSOLE_EXTENSION
{
  CHAR ScreenBuffer[25][80];
};

PDRIVER gDriverObject;
WCHAR wDeviceName[32] = L"\\Device\\Console0";
UNICODE_STRING gDeviceName;

PDEVICE gDeviceObject;
PDEVICE gConsoles[12];
CONSOLE_EXTENSION* gExtensions[12];

STATUS ConsoleCompleteRequest (PIRP Irp, STATUS CompletionStatus, ULONG Info=0, ULONG QuantumIncrenemt=0)
{
	Irp->IoStatus.Status = CompletionStatus;
	Irp->IoStatus.Information = Info;
	IoCompleteRequest (Irp, 0);
	return CompletionStatus;
}

//
// Create/Close handler
// Simply allow the request.
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

#define IOCTL_CONSOLE_CREATE_CONSOLE CTL_CODE (DEVICE_TYPE_VIDEO, 1, METHOD_BUFFERED, FILE_ANY_ACCESS)
struct CREATE_CONSOLE_PARAMETERS
{
  ULONG ConsoleNumber;
};

STATUS ConsoleControl( IN PDEVICE DeviceObject, PIRP Irp )
{
	STATUS Status = STATUS_NOT_IMPLEMENTED;
  PIRP_STACK_LOCATION irpSl = IoGetCurrentIrpStackLocation (Irp);

  switch (irpSl->Parameters.IoCtl.IoControlCode)
  {
  case IOCTL_CONSOLE_CREATE_CONSOLE:
    {
      KdPrint(("[IOCTL_CONSOLE_CREATE_CONSOLE] entered\n"));
      if (Irp->BufferLength != sizeof(CREATE_CONSOLE_PARAMETERS))
      {
        KdPrint(("buffer length for IOCTL_CONSOLE_CREATE_CONSOLE was invalid (%d), should be %d\n", Irp->BufferLength, sizeof(CREATE_CONSOLE_PARAMETERS)));
        Status = STATUS_INVALID_PARAMETER;
        break;
      }

      CREATE_CONSOLE_PARAMETERS* p = (CREATE_CONSOLE_PARAMETERS*) Irp->SystemBuffer;
      KdPrint(("Creating console %d\n", p->ConsoleNumber));
      if (p->ConsoleNumber > 12)
      {
        KdPrint(("Too big console number, must be <12\n"));
        Status = STATUS_INVALID_PARAMETER;
        break;
      }
      if (gConsoles[p->ConsoleNumber] != NULL)
      {
        KdPrint(("Console already exists.\n"));
      }
      else
      {
        // Create new console
        // \Device\Console0
        // 0123456789012345
        UCHAR highNumber = ((UCHAR)p->ConsoleNumber/10);
        UCHAR lowNumber = (UCHAR)(p->ConsoleNumber%10);
        wDeviceName[15] = L'0' + highNumber;
        wDeviceName[16] = L'0' + lowNumber;
        wDeviceName[17] = L'\0';
        RtlInitUnicodeString(&gDeviceName, wDeviceName);

        Status = IoCreateDevice (gDriverObject, sizeof(CONSOLE_EXTENSION), &gDeviceName, DEVICE_TYPE_VIDEO, &gConsoles[p->ConsoleNumber]);
        if (!SUCCESS(Status))
        {
          KdPrint(("Console: IoCreateDevice failed with status %08x\n", Status));
          break;
        }
        
        gExtensions[p->ConsoleNumber] = gConsoles[p->ConsoleNumber]->DeviceExtension;
        memset (gExtensions[p->ConsoleNumber]->ScreenBuffer, ' ', 80*25);

        KdPrint(("Console has been created successfully\n"));
      }
      break;
    }

  default:
    KdPrint(("ConsoleControl: unknown console control code %08x\n", irpSl->Parameters.IoCtl.IoControlCode));
    Status = STATUS_NOT_IMPLEMENTED;
  }

	return ConsoleCompleteRequest (Irp, Status);
}

// Driver entry point
STATUS DriverEntry(IN PDRIVER DriverObject)
{
	STATUS Status;

	KdPrint(("[~] Console: DriverEntry()\n"));

	//
	// Create main console. Child consoles can be created optionally by
	// user-mode startup code with the help of IOCTL request to this device
	//

	RtlInitUnicodeString (&gDeviceName, wDeviceName);
	Status = IoCreateDevice (DriverObject, sizeof(CONSOLE_EXTENSION), &gDeviceName, DEVICE_TYPE_VIDEO, &gDeviceObject);
	if (!SUCCESS(Status))
	{
		KdPrint(("Console: IoCreateDevice failed with status %08x\n", Status));
		return Status;
	}

	gDeviceObject->Flags |= DEVICE_FLAGS_BUFFERED_IO;
  gConsoles[0] = gDeviceObject;

	DriverObject->IrpHandlers[IRP_CREATE] = 
	DriverObject->IrpHandlers[IRP_CLOSE] = ConsoleCreateClose;
	DriverObject->IrpHandlers[IRP_WRITE] = ConsoleWrite;
	DriverObject->IrpHandlers[IRP_IOCTL] = ConsoleControl;
	
  gDriverObject = DriverObject;

	KdPrint(("[+] Console: Driver initialization successful\n"));
	return STATUS_SUCCESS;
}
