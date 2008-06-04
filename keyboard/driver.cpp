//
// i8042 device driver for GR8OS
//
// (C) Great, 2008
//

typedef unsigned short wchar_t;

#include "common.h"
#include "keybd.h"

UNICODE_STRING gDeviceName;
PDEVICE gDeviceObject;

// This event is set when new key appers in the buffer.
EVENT KeybdReadSynchEvent;

STATUS KeybdCompleteRequest (PIRP Irp, STATUS CompletionStatus, ULONG Info=0, ULONG QuantumIncrenemt=0)
{
	Irp->IoStatus.Status = CompletionStatus;
	Irp->IoStatus.Information = Info;
	IoCompleteRequest (Irp, 0);
	return CompletionStatus;
}

//
// IRP_CREATE / IRP_CLOSE handler
//

STATUS KeybdCreateClose( IN PDEVICE DeviceObject, PIRP Irp )
{
	return KeybdCompleteRequest (Irp, STATUS_SUCCESS);
}

VOID KeybdSynchPerformRead (
							PSTR LockedBuffer, 
							ULONG nBytesRequested
							)
							/*++
							Perform actual read of processed ascii codes.
							Environment: cyclic buffer lock held
							--*/
{
	for (ULONG i=0; i<nBytesRequested; i++)
	{
		LockedBuffer[i] = KeybdCyclicBuffer[KeybdCurrentPos-i];
	}

	KeybdCurrentPos -= nBytesRequested;
}

//
// IRP_READ handler
//

#define KBD_ACQUIRE_LOCK() OldIrqState = KeAcquireLock ( &KeybdCyclicBufferLock );

#define KBD_RELEASE_LOCK()	\
{						\
	KeReleaseLock ( &KeybdCyclicBufferLock );	\
	KeReleaseIrqState ( OldIrqState );			\
}


STATUS KeybdRead( IN PDEVICE DeviceObject, PIRP Irp )
{
	ULONG nBytesRequested = Irp->BufferLength;
	PSTR LockedBuffer = (PSTR) Irp->SystemBuffer;

	BOOLEAN OldIrqState;

	//
	// If there is not enough symbols, wait for them
	//

	KBD_ACQUIRE_LOCK();

	if (KeybdCurrentPos < ((int)nBytesRequested-1))
	{
		BOOLEAN Complete = FALSE;

		//
		// Not enough symbols, release lock & wait
		//

		KBD_RELEASE_LOCK();

		do 
		{
			//
			// Suspend thread until the data appears
			//

			KeWaitForSingleObject ( &KeybdReadSynchEvent, FALSE, NULL );

			//
			// Acquire lock & read
			//

			KBD_ACQUIRE_LOCK();
			{
				ASSERT (KeybdCurrentPos >= 0);

				if (KeybdCurrentPos >= ((int)nBytesRequested-1))
				{
					KeybdSynchPerformRead (LockedBuffer, nBytesRequested);

					Complete = TRUE;
				}
			}
			KBD_RELEASE_LOCK();

		}
		while (!Complete);
	}
	else
	{
		//
		// Else simply read
		//

		KeybdSynchPerformRead  (LockedBuffer, nBytesRequested);

		KBD_RELEASE_LOCK ();
	}

	return KeybdCompleteRequest (Irp, STATUS_SUCCESS, nBytesRequested);
}

STATUS KeybdControl (PDEVICE DeviceObject, PIRP Irp)
{
	PIRP_STACK_LOCATION irpSl = IoGetCurrentIrpStackLocation (Irp);
	STATUS Status = STATUS_NOT_IMPLEMENTED;

	switch (irpSl->Parameters.IoCtl.IoControlCode)
	{
	case IOCTL_KEYBD_SET_LED_INDICATORS:
		{
			KeybdIndicators = *(PKEYBOARD_INDICATORS)Irp->SystemBuffer;
			KeybdSetIndicators();
			Status = STATUS_SUCCESS;
			break;
		}
	}

	return KeybdCompleteRequest (Irp, Status);
}




// Driver entry point
STATUS DriverEntry(IN PDRIVER DriverObject)
{
	STATUS Status;

	KeybdStatusByte->Alt = KeybdStatusByte->Ctrl = KeybdStatusByte->Shift = 0;
	KeybdIndicators.CapsLock = KeybdIndicators.NumLock = KeybdIndicators.ScrollLock = 0;

	KdPrint(("[~] Keyboard: DriverEntry()\n"));

	init_kbd();

	//
	// Initialize synchronization objects
	//

	KeInitializeEvent ( &KeybdReadSynchEvent, SynchronizationEvent, FALSE );

	//
	// Create & initialize keyboard device object
	//

	RtlInitUnicodeString( &gDeviceName, L"\\Device\\Keyboard0" );
	Status = IoCreateDevice ( DriverObject, 0, &gDeviceName, DEVICE_TYPE_KEYBOARD , &gDeviceObject );
	if (!SUCCESS(Status))
	{
		KdPrint(("Keyboard: IoCreateDevice failed with status %08x\n", Status));
		return Status;
	}

	gDeviceObject->Flags |= DEVICE_FLAGS_BUFFERED_IO;

	//
	// Setup IRP handlers
	//

	DriverObject->IrpHandlers[IRP_CREATE] = 
		DriverObject->IrpHandlers[IRP_CLOSE] = KeybdCreateClose;
	DriverObject->IrpHandlers[IRP_READ] = KeybdRead;
	DriverObject->IrpHandlers[IRP_IOCTL] = KeybdControl;

	//
	// Set new IRQ1 handler
	//

	KiConnectInterrupt (1, irq1_handler);

	//
	// Initialization finished
	//

	KdPrint(("[+] Keyboard: Driver initialization successful\n"));
	return STATUS_SUCCESS;
}
