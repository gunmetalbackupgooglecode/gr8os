// begin_ddk
#pragma once

// end_ddk

//
// Pointers to the appropriate object directories of I/O subsystem
//

extern POBJECT_DIRECTORY IoDeviceDirectory;
extern POBJECT_DIRECTORY IoDriverDirectory;

// begin_ddk

#define SECTOR_SIZE 512


//
// Device Type
//

#define DEVICE_TYPE_UNKNOWN		0x00000001
#define DEVICE_TYPE_DISK		0x00000002
#define DEVICE_TYPE_FS			0x00000003
#define DEVICE_TYPE_KEYBOARD	0x00000004
#define DEVICE_TYPE_VIDEO		0x00000005
#define DEVICE_TYPE_DISK_FILE_SYSTEM	0x00000006

#define DEVICE_FLAGS_BUFFERED_IO	0x00000001
#define DEVICE_FLAGS_NEITHER_IO		0x00000002

typedef struct DRIVER *PDRIVER;
typedef struct IRP *PIRP;
typedef struct DEVICE *PDEVICE;
typedef struct VPB *PVPB;

//
// Device object
//

typedef struct DEVICE
{
	PDRIVER DriverObject;	// Pointer to the corresponding driver object.

	// Double-linked list of the devices in the device stack
	PDEVICE NextDevice;
	PDEVICE AttachedDevice;

	ULONG Flags;			// Device object flags. See DEV_FLAGS_*
	UCHAR StackSize;		// Device stack size
	ULONG DeviceType;		// Device type. See DEVICE_TYPE_*

	LIST_ENTRY IopInternalLinks;	// Points into IopFileSystemListHead - linked list of file systems.

	PVPB Vpb;					// This points to the VPB

	//
	// DeviceObject (DEVICE_TYPE_DISK) points to VPB of this device.
	// Vpb has two back links - back to disk device object and to the file system device object.
	//
	//  VPB exists when the file system is mounted.
	//
} *PDEVICE;

//
// Driver entry proto
//

typedef STATUS (KEAPI *PDRIVER_ENTRY)(PDRIVER DriverObject);
typedef STATUS (KEAPI *PDRIVER_UNLOAD)(PDRIVER DriverObject);
typedef STATUS (KEAPI *PIRP_HANDLER)(PDEVICE DeviceObject, PIRP Irp);

//
// Flags for driver object
//

#define DRV_FLAGS_BUILTIN	0x00000001	// Driver is built-in. Its functions are within the kernel,
										//  start and end addresses are zero
#define DRV_FLAGS_CRITICAL	0x00000002	// Driver is critical. It is executed in ring0. Otherwise, 
										//  it is executed in ring1. Built-in drivers are always critical

#define IRP_CREATE		0
#define IRP_READ		1
#define IRP_WRITE		2
#define IRP_IOCTL		3
#define IRP_FSCTL		4
#define IRP_QUERY_INFO	5
#define IRP_SET_INFO	6	
#define IRP_CLOSE		7
#define MAX_IRP			8

#define IRP_MN_MOUNT	1
#define IRP_MN_DISMOUNT	2

struct DRIVER
{
	PVOID DriverStart;			// Start address of the driver in the memory. 0 for built-in drivers
	PVOID DriverEnd;			// End   address of the driver in the memory. 0 for built-in drivers
	ULONG Flags;				// Driver object flags. See DRV_FLAGS_*
	PDRIVER_ENTRY DriverEntry;	// Pointer to the driver entry routine. 0 for built-in drivers
	PDRIVER_UNLOAD DriverUnload;		// Pointer to the driver unload routine. May be NULL
	PIRP_HANDLER IrpHandlers[MAX_IRP];	// Pointer to the IRP handlers of this driver.
};

typedef struct IRP_STACK_LOCATION
{
	PDEVICE DeviceObject;		// Device object in current stack location

	union
	{
		//
		// Arguments for IRP_CREATE
		//

		struct
		{
			ULONG DesiredAccess;
			ULONG Disposition;
			ULONG Options;
		} Create;

		//
		// Arguments for IRP_READ / IRP_WRITE
		//
		struct
		{
			BOOLEAN OffsetSpecified;
			LARGE_INTEGER Offset;
		} ReadWrite;

		//
		// Arguments for IRP_IOCTL
		//
		struct
		{
			ULONG IoControlCode;
			PVOID OutputUserBuffer;
			ULONG OutputBufferLength;

			//
			//  System buffer is being allocated to hold the length of MAX(InputLength,OutputLength)
			//   (if buffered I/O used)
			//  User pointer to input buffer stored in the IRP::UserBuffer
			//  Input  buffer length is stored into IRP::BufferLength
			//  Output buffer length is stored into IRP_STACK_LOCATION::Paramters.IoCtl.OutputBufferLength
			//  User pointer to output buffer stored in the IRP_STACK_LOCATION::Parameters.IoCtl.OutputUserBuffer
			//
		} IoCtl;

		//
		// Arguments for IRP_QUERY_INFO / IRP_SET_INFO
		//

		struct
		{
			ULONG InfoClass;
		} QuerySetInfo;

		//
		// Arguments for IRP_MN_MOUNT
		//

		struct
		{
			PDEVICE DeviceObject;
		} MountVolume;

		//
		// Other driver specified arguments
		//

		struct 
		{
            PVOID Argument1;
            PVOID Argument2;
            PVOID Argument3;
            PVOID Argument4;
        } Others;

	} Parameters;
} *PIRP_STACK_LOCATION;


//
// Flags for IRP
//

#define IRP_FLAGS_PAGING_IO				0x00000001
#define IRP_FLAGS_BUFFERED_IO			0x00000002
#define IRP_FLAGS_DEALLOCATE_BUFFER		0x00000004
#define IRP_FLAGS_NEITHER_IO			0x00000008
#define IRP_FLAGS_INPUT_OPERATION		0x00000010
#define IRP_FLAGS_SYNCHRONOUS_IO		0x00000020


typedef struct FILE *PFILE;
typedef struct EVENT *PEVENT;

typedef struct IO_STATUS_BLOCK
{
	STATUS Status;
	ULONG Information;
} *PIO_STATUS_BLOCK;


struct IRP
{
	ULONG Size;							// Full size of this struct
	ULONG Flags;						// Flags. See IRP_FLAGS_*
	ULONG MajorFunction;				// Major function of this IRP. See IRP_*
	ULONG MinorFunction  UNIMPLEMENTED;	// Minor function of this IRP. Unimplemented
	PVOID SystemBuffer;					// System buffer for buffered I/O. NULL if neither I/O used of no buffer required.
	PVOID UserBuffer;					// Pointer to user buffer. NULL if no buffer required for this type of IRP
	ULONG BufferLength;					// Length of user  buffer. 0 if no buffer required for this type of IRP
	IO_STATUS_BLOCK IoStatus;			// Final status of the IRP.
	PIO_STATUS_BLOCK UserIosb;			// User pointer to the IRP
	PROCESSOR_MODE RequestorMode;		// Requestor processor mode
	PTHREAD CallerThread;				// Caller thread
	PFILE FileObject;					// File object used to queue this IRP
	PEVENT UserEvent;					// Pointer to the user event in asynchronous i/o
	LIST_ENTRY ThreadListEntry;			// Double-linked list with the IRPs of the specified thread. See THREAD::IrpList
	PIRP AssosiatedIrp;					// An IRP, associated with this.

	UCHAR StackSize;					// Stack size (= IRP::Size-FIELD_OFFSET(IrpStackLocations))/sizeof(IRP_STACK_LOCATION)
	UCHAR CurrentLocation;				// Current stack location number
	PIRP_STACK_LOCATION CurrentStackLocation;	// Current stack location pointer
	IRP_STACK_LOCATION IrpStackLocations[1];	// Variable-length array
};

#pragma pack(1)
struct FILE
{
	PDEVICE DeviceObject;
	PVOID FsContext;
	PVOID FsContext2;
	union
	{
		ULONG DesiredAccess UNIMPLEMENTED;
		struct
		{
			BOOLEAN ReadAccess UNIMPLEMENTED;
			BOOLEAN WriteAccess UNIMPLEMENTED;
			BOOLEAN DeleteAccess UNIMPLEMENTED;
			BOOLEAN SharedRead UNIMPLEMENTED;
			BOOLEAN SharedWrite UNIMPLEMENTED;
			BOOLEAN SharedDelete UNIMPLEMENTED;
			BOOLEAN ReadThrough;
			BOOLEAN WriteThrough;
		};
	};
	UNICODE_STRING RelativeFileName;
	LARGE_INTEGER CurrentOffset;
	STATUS FinalStatus;
	EVENT Event;
	PCCFILE_CACHE_MAP CacheMap;
};
#pragma pack()

#define IS_FILE_CACHED(File) ((File)->ReadThrough==FALSE || (File)->WriteThrough==FALSE)
#define SET_FILE_NONCACHED(File) { (File)->ReadThrough = (File)->WriteThrough = TRUE; }

//#define SYNCHRONIZE				0x00000001

#define FILE_READ_ATTRIBUTES	0x00000002
#define FILE_WRITE_ATTRIBUTES	0x00000004
#define FILE_READ_DATA			0x00000008
#define FILE_WRITE_DATA			0x00000010
#define FILE_DELETE				0x00000020

#define FILE_READ_THROUGH		0x00000040
#define FILE_WRITE_THROUGH		0x00000080
#define FILE_NONCACHED			(FILE_READ_THROUGH|FILE_WRITE_THROUGH)

// dispositions

#define FILE_OPEN_EXISTING		0x00000001
#define FILE_OPEN_ALWAYS		0x00000002
#define FILE_CREATE_NEW			0x00000003
#define FILE_CREATE_ALWAYS		0x00000004
#define FILE_TRUNCATE_EXISTING	0x00000005

KESYSAPI
PIRP
KEAPI
IoAllocateIrp(
	IN UCHAR StackSize
	);

KESYSAPI
STATUS
KEAPI
IoCallDriver(
	IN PDEVICE Device,
	IN PIRP Irp
	);

//
// IoSkipCurrentIrpStackLocation.
//  Skips current IRP stack location and copied to next.
//

#define IoSkipCurrentIrpStackLocation(Irp) {	\
		(Irp)->CurrentLocation++;				\
		(Irp)->CurrentStackLocation++;			\
	}

#define IoGetCurrentIrpStackLocation(Irp) ((Irp)->CurrentStackLocation)
#define IoGetNextIrpStackLocation(Irp) (++(Irp)->CurrentStackLocation)

// end_ddk

typedef struct IO_PARSE_DEVICE_PACKET
{
	UNICODE_STRING RemainingPath;
} *PIO_PARSE_DEVICE_PACKET;

// begin_ddk

KESYSAPI
STATUS
KEAPI
IoCreateFile(
	OUT PFILE *FileObject,
	IN ULONG DesiredAccess UNIMPLEMENTED,
	IN PUNICODE_STRING FileName,
	OUT PIO_STATUS_BLOCK IoStatus,
	IN ULONG Disposition UNIMPLEMENTED,
	IN ULONG Options UNIMPLEMENTED
	);

KESYSAPI
STATUS
KEAPI
IoCloseFile(
	IN PFILE FileObject
	);


KESYSAPI
STATUS
KEAPI
IoReadFile(
	IN PFILE FileObject,
	OUT PVOID Buffer,
	IN ULONG Length,
	IN PLARGE_INTEGER FileOffset OPTIONAL,
	OUT PIO_STATUS_BLOCK IoStatus
	);

KESYSAPI
STATUS
KEAPI
IoWriteFile(
	IN PFILE FileObject,
	IN PVOID Buffer,
	IN ULONG Length,
	IN PLARGE_INTEGER FileOffset OPTIONAL,
	OUT PIO_STATUS_BLOCK IoStatus
	);

KESYSAPI
STATUS
KEAPI
IoDeviceIoControlFile(
	IN PFILE FileObject,
	IN ULONG IoControlCode,
	IN PVOID InputBuffer,
	IN ULONG InputBufferLength,
	OUT PVOID OutputBuffer,
	IN ULONG OutputBufferLength,
	OUT PIO_STATUS_BLOCK IoStatus
	);

KESYSAPI
STATUS
KEAPI
IoCreateDevice(
	IN PDRIVER DriverObject,
	IN ULONG DeviceExtensionSize,
	IN PUNICODE_STRING DeviceName OPTIONAL,
	IN ULONG DeviceType,
	OUT PDEVICE *DeviceObject
	);

KESYSAPI
VOID
KEAPI
IoAttachDevice(
	IN PDEVICE SourceDevice,
	IN PDEVICE TargetDevice
	);

KESYSAPI
VOID
KEAPI
IoDetachDevice(
	IN PDEVICE TargetDevice
	);

KESYSAPI
PDEVICE
KEAPI
IoGetNextDevice(
	IN PDEVICE DeviceObject
	);

KESYSAPI
PDEVICE
KEAPI
IoGetAttachedDevice(
	IN PDEVICE DeviceObject
	);

KESYSAPI
VOID
KEAPI
IoDeleteDevice(
	IN PUNICODE_STRING DeviceName
	);

// end_ddk

STATUS
KEAPI
IopCreateDriverObject(
	IN PVOID DriverStart,
	IN PVOID DriverEnd,
	IN ULONG Flags,
	IN PDRIVER_ENTRY DriverEntry,
	IN PUNICODE_STRING DriverName,
	OUT PDRIVER *DriverObject
	);

// begin_ddk

KESYSAPI
VOID
KEAPI
IoCompleteRequest(
	IN PIRP Irp,
	IN ULONG QuantumIncrement
	);

KESYSAPI
PIRP
KEAPI
IoBuildDeviceRequest(
	IN PDEVICE DeviceObject,
	IN ULONG MajorFunction,
	OUT PIO_STATUS_BLOCK IoStatus,
	IN PROCESSOR_MODE RequestorMode,
	IN OUT PVOID Buffer,
	IN ULONG BufferSize,
	OUT PULONG ReturnedLength
	);

KESYSAPI
PIRP
KEAPI
IoBuildDeviceIoControlRequest(
	IN PDEVICE DeviceObject,
	OUT PIO_STATUS_BLOCK IoStatus,
	IN PROCESSOR_MODE RequestorMode,
	IN ULONG IoControlCode,
	IN PVOID InputBuffer,
	IN ULONG InputBufferSize,
	IN PVOID OutputBuffer,
	IN ULONG OutputBufferSize
	);

// end_ddk

VOID
KEAPI
IoInitSystem(
	);

STATUS
KEAPI 
IopInvalidDeviceRequest(
	PDEVICE DeviceObject,
	PIRP Irp
	);

VOID
KEAPI
IopQueueThreadIrp(
	IN PIRP Irp
	);

VOID
KEAPI
IopDequeueThreadIrp(
	IN PIRP Irp
	);

extern MUTEX IopFileSystemListLock;
extern LIST_ENTRY IopFileSystemListHead;

// begin_ddk

KESYSAPI
VOID
KEAPI
IoRegisterFileSystem(
	PDEVICE DeviceObject
	);

KESYSAPI
VOID
KEAPI
IoUnregisterFileSystem(
	PDEVICE DeviceObject
	);

extern POBJECT_TYPE IoVpbObjectType;

#define VPB_MOUNTED		0x00000001
#define VPB_LOCKED		0x00000002

#define MAX_VOLUME_LABEL_LEN	128

typedef struct VPB
{
	USHORT Flags;					// see VPB_*
	USHORT VolumeLabelLength;		// in bytes
	PDEVICE FsDeviceObject;			// file system CDO
	PDEVICE PhysicalDeviceObject;	// PDO
	ULONG SerialNumber;				// Volume serial number
	// Volume label
	WCHAR VolumeLabel [MAX_VOLUME_LABEL_LEN/sizeof(WCHAR)];
} *PVPB;

// end_ddk

STATUS
KEAPI
IopCreateVpb(
	PDEVICE DeviceObject
	);

// begin_ddk

KESYSAPI
STATUS
KEAPI
IoMountVolume(
	PDEVICE RealDevice,
	PDEVICE FsDevice
	);

KESYSAPI
STATUS
KEAPI
IoDismountVolume(
	PDEVICE RealDevice
	);

KESYSAPI
STATUS
KEAPI
IoRequestDismount(
	PDEVICE RealDevice,
	PDEVICE FsDevice
	);

KESYSAPI
STATUS
KEAPI
IoRequestMount(
	PDEVICE RealDevice,
	PDEVICE FsDevice
	);

// end_ddk

extern ULONG IopMountedDrivesMask;

// begin_ddk

KESYSAPI
STATUS
KEAPI
IoAllocateMountDriveLetter(
	OUT PCHAR DriveLetter
	);

KESYSAPI
VOID
KEAPI
IoFreeMountedDriveLetter(
	IN CHAR DriveLetter
	);

// end_ddk
