#pragma once

#define PCI_CONFIG_ADDRESS_REGISTER (0xCF8)
#define PCI_CONFIG_DATA_REGISTER (0xCFC)
#define PCICONF_ADDRESS(PCI_BUS,DEVICE,FUNC,REG)  (0x80000000 | (PCI_BUS<<16) | (DEVICE<<11) | (FUNC<<8) | (REG & 0xfc))

#define PCI_SET_ADDRESS(ADDR) WRITE_PORT_ULONG(PCI_CONFIG_ADDRESS_REGISTER, ADDR)
#define PCI_GET_DATA() READ_PORT_ULONG(PCI_CONFIG_DATA_REGISTER)
#define PCI_SET_DATA(DATA) WRITE_PORT_ULONG(PCI_CONFIG_DATA_REGISTER,DATA)

#define PCI_BC_FROM_CC(CC) ((UCHAR)((CC)>>16))
#define PCI_SC_FROM_CC(CC) ((UCHAR)((CC)>>8)&0xFF)
#define PCI_PI_FROM_CC(CC) ((UCHAR)((CC)&0xFF))

enum PCI_BASE_CLASS {
	PciBaseClassPCISpecificationDevice,
	PciBaseClassMassStorageController,
	PciBaseClassNetworkController,
	PciBaseClassDisplayController,
	PciBaseClassMultimediaDevice,
	PciBaseClassMemoryController,
	PciBaseClassBridgeDevice,
	PciBaseClassSimpleCommunicationsController,
	PciBaseClassBaseSystemsPeripheral,
	PciBaseClassInputDevice,
	PciBaseClassDockingStation,
	PciBaseClassProcessor,
	PciBaseClassSerialBusController,
	PciBaseClassUnknown=0xFF
};

struct PCICommand
{
	USHORT IoSpace:1;
	USHORT MemorySpace:1;
	USHORT BusMaster:1;
	USHORT SpecialCycles:1;
	USHORT MemoryWriteAndInvalidateEnable:1;
	USHORT VGAPaletteSnoop:1;
	USHORT ParityErrorResponse:1;
	USHORT SteppingControl:1;
	USHORT SERREnable:1;
	USHORT FastBacktoBackEnable:1;
	USHORT Reserved:6;
};
STATIC_ASSERT( sizeof(PCICommand) == sizeof(USHORT) );

struct PCIStatus
{
	USHORT Reserved:4;
	USHORT CapatibilitiesList:1;
	USHORT _66MHzCapable:1;
	USHORT Reserved2:1;
	USHORT FastBacktoBackCapable:1;
	USHORT MasterDataParityError:1;
	USHORT DEVSELtiming:2;
	USHORT SignaledTargetAbort:1;
	USHORT ReceivedTargetAbort:1;
	USHORT ReceivedMasterAbort:1;
	USHORT SignaledSystemError:1;
	USHORT DetectedParityError:1;
};
STATIC_ASSERT( sizeof(PCIStatus) == sizeof(USHORT) );

struct PCI_BAR
{
	union
	{
		struct
		{
			ULONG IoOrMemorySpace:1; // =0
			ULONG Type:2;
			ULONG Prefetchable:1;
			ULONG BaseAddress:28;
		} m;

		struct
		{
			ULONG IoOrMemorySpace:1; // =1
			ULONG Reserved:1;
			ULONG BaseAddress:30;
		} i;
	};
};
STATIC_ASSERT( sizeof(PCI_BAR) == sizeof(ULONG) );

#define PCI_BAR_MEM_TYPE_32BIT  0x0
#define PCI_BAR_MEM_TYPE_64BIT  0x2
