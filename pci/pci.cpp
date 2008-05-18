//
// Kernel driver for GR8OS
//
// PCI bus driver
//

#include "common.h"
#include "pci_vend.h"
#include "pci.h"

#define READ_PORT_ULONG KiInPortD
#define WRITE_PORT_ULONG KiOutPortD

char* 
PciGetVendorShort(
	IN USHORT VenId
	)

/*++

Routine Description

	This routine looks up the PCI vendors table and returns short vendor name for the specified vendor ID.

Arguments

	VenId      Vendor ID

Return Value

	Pointer to string, containing short vendor description or 'UNKVEN' if vendor will not be found.
	String pointer by this value should not be modified.

This function can be running at any IRQL

--*/

{
	for( int i=0; i<PCI_VENTABLE_LEN; i++ )
	{
		if( PciVenTable[i].VenId == VenId )
			return PciVenTable[i].VenShort;
	}

	return "UNKVEN";
}


char*
PciGetVendorFull(
	IN USHORT VenId
	)

/*++

Routine Description

	This routine looks up the PCI vendors table and returns full vendor name for the specified vendor ID.

Arguments

	VenId      Vendor ID

Return Value

	Pointer to string, containing full vendor description or 'Unknown Vendor' if vendor will not be found.
	String pointer by this value should not be modified.

This function can be running at any IRQL

--*/

{
	for( int i=0; i<PCI_VENTABLE_LEN; i++ )
	{
		if( PciVenTable[i].VenId == VenId )
			return PciVenTable[i].VenFull;
	}

	return "Unknown Vendor";
}


char*
PciGetDeviceShort(
	IN USHORT VenId,
	IN USHORT DevId
	)

/*++

Routine Description

	This routine looks up PCI devices table and returns short device name for the specified vendor ID and device ID.

Arguments

	VenId      Vendor ID
	DevId      Device ID

Return Value

	Pointer to string, containing short device description or 'UNKDEV' if device will not be found.
	String pointer by this value should not be modified.

This function can be running at any IRQL

--*/

{
	for( int i=0; i<PCI_DEVTABLE_LEN; i++ )
	{
		if( PciDevTable[i].VenId == VenId &&
			PciDevTable[i].DevId == DevId )
			return PciDevTable[i].Chip;
	}

	return "UNKDEV";
}


char* 
PciGetDeviceFull(
	IN USHORT VenId,
	IN USHORT DevId
	)

/*++

Routine Description

	This routine looks up PCI devices table and returns short device name for the specified vendor ID and device ID.

Arguments

	VenId      Vendor ID
	DevId      Device ID

Return Value

	Pointer to string, containing short device description or 'UNKDEV' if device will not be found.
	String pointer by this value should not be modified.

This function can be running at any IRQL

--*/

{
	for( int i=0; i<PCI_DEVTABLE_LEN; i++ )
	{
		if( PciDevTable[i].VenId == VenId &&
			PciDevTable[i].DevId == DevId )
			return PciDevTable[i].ChipDesc;
	}

	return "Unknown Device";
}


char*
PciGetBaseClassDesc(
	IN ULONG ClassCode
	)

/*++

Routine Description

	This routine looks up PCI class code table and returns class code description for the specified base class ID.

Arguments

	ClassCode      Base class code to look up for.

Return Value

	Pointer to string, containing base class code description or 'Unknown class' if class will not be found.
	String pointer by this value should not be modified.

This function can be running at any IRQL

--*/

{
	for( int i=0; i<PCI_CLASSCODETABLE_LEN; i++ )
	{
		if( PciClassCodeTable[i].BaseClass == (UCHAR)(ClassCode>>16) )
			return PciClassCodeTable[i].BaseDesc;
	}

	return "Unknown class";
}


USHORT
PciGetDeviceByClass(
	PCI_BASE_CLASS BaseClass
	)

/*++

Routine Description

	This routine searches for PCI device by its base class.

Arguments

	BaseClass      Base class code to search for.

Return Value

	Device number or -1 on error

This function can be running at any IRQL

--*/

{
	UCHAR PCIType = -1;
	
	ULONG tmp = READ_PORT_ULONG(PCI_CONFIG_ADDRESS_REGISTER);
	PCI_SET_ADDRESS( PCICONF_ADDRESS(0,0,0,0) );
	if( READ_PORT_ULONG(PCI_CONFIG_ADDRESS_REGISTER) == PCICONF_ADDRESS(0,0,0,0) )
		PCIType = 1;

	USHORT DeviceNumber = -1;

	switch( PCIType )
	{
	case 1:
		{
			for( USHORT i=0; i<512; i++ )
			{
				// Set address
				PCI_SET_ADDRESS( PCICONF_ADDRESS(0,i,0,0) );

				// Device present?
				if( ((tmp=PCI_GET_DATA()) & 0xFFFF) != 0xFFFF )
				{
					// Device present; check Base Class Code
					PCI_SET_ADDRESS( PCICONF_ADDRESS(0,i,0,0x08) );

					// Check base class
					if( PCI_BC_FROM_CC(PCI_GET_DATA() >> 8) == BaseClass )
					{
						// Found!
						DeviceNumber = i;
						break;
					}
				}
			}

			PCI_SET_ADDRESS( PCICONF_ADDRESS(0,0,0,0) );

			break;
		}
	}

	return DeviceNumber;
}


ULONG 
PciGetVideoAdapterLFBPhysAddress(
	IN USHORT VidAdapter,
	OUT PULONG LFBSize OPTIONAL
	)

/*++

Routine Description

	This routine retrieves LFB physical address of video adapter

Arguments

	VidAdapter      Video adapter device number got with IhalGetPciDeviceByClass()
	LFBSize         Optionally specifies a variable where LFB size should be stored.

Return Value

	LFB physical address or NULL on error

This function can be running at any IRQL

--*/

{
	// Get configuration space data at address 0x00 [DevId VenId]
	PCI_SET_ADDRESS( PCICONF_ADDRESS(0,VidAdapter,0,0) );
	ULONG tmp = PCI_GET_DATA();

	USHORT VenId = (USHORT)(tmp&0xFFFF);
	if( VenId == 0xFFFF )
	{
		PCI_SET_ADDRESS( PCICONF_ADDRESS(0,0,0,0) );
		return 0;
	}

	// Read BARs
	for( int i=0; i<6; i++ )
	{
		PCI_SET_ADDRESS( PCICONF_ADDRESS(0,VidAdapter,0,0x10 + i) );
		PCI_BAR bar;
		tmp = PCI_GET_DATA();
		*(ULONG*)&bar = tmp;

		if( bar.m.IoOrMemorySpace == 0 )
		{
			// Memory-Space prefetchable BAR
			if( bar.m.Type == PCI_BAR_MEM_TYPE_32BIT && bar.m.Prefetchable )
			{
				// 32-bit Memory-Space BAR; LFB phys. address
				if( ARGUMENT_PRESENT(LFBSize) )
				{
					ULONG size;

					PCI_SET_ADDRESS( PCICONF_ADDRESS(0,VidAdapter,0,0x10 + i) );
					PCI_SET_DATA( 0xFFFFFFFF );
					size = PCI_GET_DATA();
					PCI_SET_DATA( tmp );

					size = (~(size & 0xFFFFFFF0)) + 1;

					*LFBSize = size;
				}

				PCI_SET_ADDRESS( PCICONF_ADDRESS(0,0,0,0) );
				return (tmp & 0xFFFFFFF0);
			}
		}
	}

	PCI_SET_ADDRESS( PCICONF_ADDRESS(0,0,0,0) );
	return 0;
}

STATUS DriverEntry(PDRIVER DriverObject)
{
	KdPrint(("[~] PCI DriverEntry()\n"));
	
	ULONG tmp = READ_PORT_ULONG(PCI_CONFIG_ADDRESS_REGISTER);
	PCI_SET_ADDRESS( PCICONF_ADDRESS(0,0,0,0) );

	for( ULONG Bus=0; Bus<=255; Bus++)
	{
		for (ULONG Device=0; Device<32; Device++)
		{
			for( ULONG func=0; func<7; func++ )
			{
				// Set address
				PCI_SET_ADDRESS( PCICONF_ADDRESS(Bus,Device,func,0) );

				// Device present?
				if( ((tmp=PCI_GET_DATA()) & 0xFFFF) != 0xFFFF )
				{
					// Device present; check Base Class Code
					PCI_SET_ADDRESS( PCICONF_ADDRESS(Bus,Device,func,0x08) );

					// Check base class
					ULONG bc = (PCI_GET_DATA() >> 8);

					KdPrint(("Device [%d:%d:%d] %04x-%04x is %s : %s %s %s\n",
						Bus,
						Device,
						func,
						tmp & 0xFFFF,
						tmp>>16,
						PciGetBaseClassDesc (bc),
						PciGetVendorFull (tmp & 0xFFFF),
						PciGetDeviceShort (tmp & 0xFFFF, tmp>>16),
						PciGetDeviceFull (tmp & 0xFFFF, tmp>>16)
						));
				}
			}
		}
	}

	PCI_SET_ADDRESS( PCICONF_ADDRESS(0,0,0,0) );

	return STATUS_SUCCESS;
}
