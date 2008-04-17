//
// <ob.h> built by header file parser at 09:59:08  17 Apr 2008
// This is a part of gr8os include files for GR8OS Driver & Extender Development Kit (DEDK)
//

#pragma once


typedef struct OBJECT_HEADER *POBJECT_HEADER;
typedef struct THREAD *PTHREAD;

typedef enum OB_OPEN_REASON
{
	ObCreateHandle,
	ObOpenHandle,
	ObInheritHandle,
	ObDuplicateHandle
} *POB_OPEN_REASON;

typedef 
STATUS 
(KEAPI
 *POPEN_OBJECT_ROUTINE)(
	IN OB_OPEN_REASON OpenReason,
	IN PTHREAD Caller,
	IN POBJECT_HEADER Object, 
	IN ULONG DesiredAccess,
	IN PUNICODE_STRING RemainingPath
	);
/*++
	Open object routine.
	Is called when the handle to the object is to be opened.

	This routine should perform access check if the caller is really able to
	 open handle to such an object.
	Also it can allocate some buffers and initialize some data related to objects
	 of this type.

	Return value
		
		STATUS_SUCCESS	
			The handle opening attempt will be satisfied to the caller.

		Error status code
			The callee will return this status code to the caller.

--*/

typedef 
STATUS
(KEAPI
  *PCLOSE_OBJECT_ROUTINE)(
	IN POBJECT_HEADER Object
	);
/*++
	Close object routine.
	Is called when the handle to the object is being closed.

	This routine generally frees all resources allocated by OpenObjectRoutine

	Return value
		
		STATUS_SUCCESS	
			The handle will be successfully closed.

		Error status code
			The callee will return this status code to the caller.

--*/

typedef
STATUS
(KEAPI
  *PPARSE_OBJECT_ROUTINE)(
	IN POBJECT_HEADER Object,
	IN PUNICODE_STRING FullObjectPath,
	IN PUNICODE_STRING RemainingPath,
	IN PVOID ParseContext OPTIONAL,
	OUT PUNICODE_STRING ReparsePath
	);
/*++
	Parse object routine.
	This routine is called from ObReferenceObjectByName during parsing object path.

	Arguments

		Object
			Pointer to the object header being parsed.

		FullObjectPath
			Full path specified to the callee

		RemainingPath
			The tail of the path

		ReparsePath
			Output buffer where this function should store reparse path
	
	This routine should do the following things:
		Access check for the specified object.
		Path translation (if need)

	Return value can be one of the following:

		STATUS_REPARSE
			In this case the ObReferenceObjectByName will start from the beginning.
			Routine should store new path in the ReparsePath, allocating the appropriate buffer.
			Buffer will be freed in the end of parsing.

		STATUS_FINISH_PARSING
			ObReferenceObjectByName should finish parsing now.
			Generally it is used in file system devices while parsing
			 names like '\Device\SomePartitionDevice\RelativePath\Filename.ext'
			File system driver usually stops parsing at SomePartitionDevice and the
			 path tail '\RelativePath\Filename.ext' goes to IRP_CREATE packet.

		STATUS_SUCCESS
			Parsing should be continued normally.

		Error status value can be returned (i.e. if access check fails) and this value
		 will be directly passed to the caller of ObReferenceObjectByName

--*/

typedef
VOID
(KEAPI
 *PDELETE_OBJECT_ROUTINE)(
	IN POBJECT_HEADER Object
	);
/*++
	Delete object routine.
	Is called when the object has been dereferenced last time and is being deleted.

	Return value
		
		This function does not return a value, this is just a notification routine.

--*/

typedef
STATUS
(KEAPI
 *PQUERY_OBJECT_NAME_ROUTINE)(
	IN POBJECT_HEADER Object,
	OUT PUNICODE_STRING ObjectName
	);
/*++
	Query object name routine.
	Is called during the object name querying request.

	This routine should return a full object name

	Return value
		
		STATUS_SUCCESS	
			The returned object name will be passed to the caller.

		Error status code
			The callee will return this status code to the caller.

--*/

typedef struct OBJECT_TYPE
{
	UNICODE_STRING ObjectTypeName;
	POPEN_OBJECT_ROUTINE OpenRoutine;
	PPARSE_OBJECT_ROUTINE ParseRoutine;
	PCLOSE_OBJECT_ROUTINE CloseRoutine;
	PDELETE_OBJECT_ROUTINE DeleteRoutine;
	PQUERY_OBJECT_NAME_ROUTINE QueryNameRoutine;
	BOOLEAN PagedPool;
	ULONG OwnerTag;
	ULONG ObjectCount;
} *POBJECT_TYPE;

extern POBJECT_TYPE ObDirectoryObjectType;
extern POBJECT_TYPE IoDeviceObjectType;
extern POBJECT_TYPE IoDriverObjectType;
//extern POBJECT_TYPE KeEventObjectType;
extern POBJECT_TYPE MmExtenderObjectType;
//extern POBJECT_TYPE PsThreadObjectType;
//extern POBJECT_TYPE PsProcessObjectType;

typedef struct OBJECT_DIRECTORY
{
	MUTEX DirectoryLock;
	LIST_ENTRY ObjectList;
} *POBJECT_DIRECTORY;

typedef struct OBJECT_SYMBOLIC_LINK
{
	UNICODE_STRING Target;
	OBJECT_HEADER* TargetObject;
} *POBJECT_SYMBOLIC_LINK;

#define OBJ_PERMANENT			0x00000001
#define OBJ_DELETE_PENDING		0x00000002

typedef struct OBJECT_HEADER
{
	POBJECT_DIRECTORY ParentDirectory;
	LIST_ENTRY DirectoryList;
	POBJECT_TYPE ObjectType;
	UNICODE_STRING ObjectName;
	PTHREAD Owner;
	ULONG OwnerTag;
	ULONG ReferenceCount;
	ULONG HandleCount;		// Always <= ReferenceCount
	ULONG Flags;
	MUTEX ObjectLock;

#pragma warning(disable:4200)
	UCHAR Body[0];
#pragma warning(default:4200)
} *POBJECT_HEADER;

//
// Public owners
//

#define OB_OBJECT_OWNER_KE	'  eK'
#define OB_OBJECT_OWNER_IO	'  oI'
#define OB_OBJECT_OWNER_PS	'  sP'
#define OB_OBJECT_OWNER_OB	'  bO'
#define OB_OBJECT_OWNER_MM	'  mM'
#define OB_OBJECT_OWNER_KD	'  dK'
#define OB_OBJECT_OWNER_EX	'  xE'
#define OB_OBJECT_OWNER_RTL	' ltR'
#define OB_OBJECT_OWNER_DRV	' vrD'				// Drivers
#define OB_OBJECT_OWNER_CDRV	'vrDC'			// Critical drivers
#define OB_OBJECT_OWNER_EXT	' txE'				// Extenders


//
// Internal owners
//

#define OB_OBJECT_OWNER_KI	'  iK'
#define OB_OBJECT_OWNER_IOP	' poI'
#define OB_OBJECT_OWNER_PSP	' psP'
#define OB_OBJECT_OWNER_OBP	' pbO'
#define OB_OBJECT_OWNER_MI	'  iM'
#define OB_OBJECT_OWNER_KDP	' pdK'
#define OB_OBJECT_OWNER_EXP	' pxE'
#define OB_OBJECT_OWNER_RTLP 'pltR'

#define OBJECT_TO_OBJECT_HEADER(x) CONTAINING_RECORD ((x), OBJECT_HEADER, Body)
#define OBJECT_HEADER_TO_OBJECT(x,type) ((type*)&((x)->Body))

#define DIRECTORY_MAX_OBEJCTS	65536


extern POBJECT_DIRECTORY ObRootObjectDirectory;

KESYSAPI
STATUS
KEAPI
ObCreateObject(
   OUT PVOID *Object,
	IN ULONG ObjectSize,
	IN POBJECT_TYPE ObjectType,
	IN PUNICODE_STRING ObjectName	OPTIONAL,
	IN ULONG ObjectOwner
	);

KESYSAPI
STATUS
KEAPI
ObCreateObjectType(
   OUT POBJECT_TYPE *ObjectType,
	IN PUNICODE_STRING TypeName,
	IN POPEN_OBJECT_ROUTINE OpenRoutine		OPTIONAL,
	IN PPARSE_OBJECT_ROUTINE ParseRoutine	OPTIONAL,
	IN PCLOSE_OBJECT_ROUTINE CloseRoutine	OPTIONAL,
	IN PDELETE_OBJECT_ROUTINE DeleteRoutine	OPTIONAL,
	IN PQUERY_OBJECT_NAME_ROUTINE QueryNameRoutine OPTIONAL,
	IN ULONG OwnerTag
	);

KESYSAPI
STATUS
KEAPI
ObInsertObject(
	IN POBJECT_DIRECTORY Directory OPTIONAL,
	IN PVOID Object
	);


KESYSAPI
VOID
KEAPI
ObReferenceObject(
	PVOID Object
	);

KESYSAPI
VOID
KEAPI
ObLockObject(
	PVOID Object
	);

KESYSAPI
VOID
KEAPI
ObUnlockObject(
	PVOID Object
	);

KESYSAPI
VOID
KEAPI
ObDereferenceObject(
	PVOID Object
	);

KESYSAPI
VOID
KEAPI
ObDereferenceObjectEx(
	PVOID Object,
	ULONG Count
	);


KESYSAPI
STATUS
KEAPI
ObDeleteObject(
	PUNICODE_STRING ObjectName
	);

KESYSAPI
STATUS
KEAPI
ObCreateDirectory(
	OUT POBJECT_DIRECTORY *Directory,
	IN PUNICODE_STRING Name OPTIONAL,
	IN ULONG ObjectOwner,
	IN POBJECT_DIRECTORY InsertInto
	);


KESYSAPI
STATUS
KEAPI
ObReferenceObjectByName(
	IN PUNICODE_STRING ObjectName,
	IN POBJECT_TYPE ObjectType	OPTIONAL,
	IN PROCESSOR_MODE RequestorMode,
	IN ULONG DesiredAccess UNIMPLEMENTED,
	IN PVOID ParseContext OPTIONAL,
	OUT PVOID* Object
	);

KESYSAPI
VOID
KEAPI
ObMakeTemporaryObject(
	PVOID Object
	);

KESYSAPI
STATUS
KEAPI
ObQueryObjectName(
	IN PVOID Object,
	OUT PUNICODE_STRING ObjectName
	);

typedef ULONG  HANDLE, *PHANDLE;
#define INVALID_HANDLE_VALUE ((HANDLE)-1)

KESYSAPI
STATUS
KEAPI
ObOpenObjectByName(
	IN PUNICODE_STRING ObjectName,
	IN POBJECT_TYPE ObjectType	OPTIONAL,
	IN PROCESSOR_MODE RequestorMode,
	IN ULONG DesiredAccess UNIMPLEMENTED,
	IN PVOID ParseContext OPTIONAL,
	OUT PHANDLE ObjectHandle
	);

KESYSAPI
STATUS
KEAPI
ObOpenObjectByPointer(
	IN PVOID Object,
	IN POBJECT_TYPE ObjectType	OPTIONAL,
	IN PROCESSOR_MODE RequestorMode,
	IN ULONG DesiredAccess UNIMPLEMENTED,
	OUT PHANDLE ObjectHandle
	);

STATUS
KEAPI
ObClose(
	IN HANDLE Handle
	);

KESYSAPI
STATUS
KEAPI
ObReferenceObjectByHandle(
	IN HANDLE ObjectHandle,
	IN PROCESSOR_MODE RequestorMode UNIMPLEMENTED,
	IN ULONG DesiredAccess,
	OUT PVOID *ObjectPointer
	);


extern POBJECT_TYPE ObSymbolicLinkObjectType;


KESYSAPI
STATUS
KEAPI
ObCreateSymbolicLink(
	PUNICODE_STRING SymlinkName,
	PUNICODE_STRING TargetPath
	);

