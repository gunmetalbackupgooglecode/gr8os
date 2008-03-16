#pragma once

struct OBJECT_HEADER;

typedef STATUS (KEAPI *POPEN_INSTANCE_ROUTINE)(PVOID Object, UNICODE_STRING Path);

typedef struct OBJECT_TYPE
{
	UNICODE_STRING ObjectTypeName;
	PVOID OpenObjectRoutine;
	PVOID ParseRoutine;
	PVOID CloseRoutine;
	ULONG OwnerTag;
} *POBJECT_TYPE;

extern POBJECT_TYPE ObDirectoryObjectType;
extern POBJECT_TYPE IoDeviceObjectType;
extern POBJECT_TYPE IoDriverObjectType;
extern POBJECT_TYPE KeEventObjectType;
extern POBJECT_TYPE MmViewObjectType;
extern POBJECT_TYPE PsThreadObjectType;
extern POBJECT_TYPE PsProcessObjectType;

typedef struct OBJECT_HEADER
{
	LIST_ENTRY DirectoryList;
	POBJECT_TYPE ObjectType;
	UNICODE_STRING ObjectName;
	PTHREAD Owner;
	ULONG OwnerTag;

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

#define DIRECTORY_MAX_OBEJCTS	65536

typedef struct OBJECT_DIRECTORY
{
	LIST_ENTRY ObjectList;
} *POBJECT_DIRECTORY;

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
	IN PVOID OpenObjectRoutine	OPTIONAL,
	IN PVOID ParseRoutine		OPTIONAL,
	IN PVOID CloseRoutine		OPTIONAL,
	IN ULONG OwnerTag
	);

KESYSAPI
STATUS
KEAPI
ObInsertObject(
	IN POBJECT_DIRECTORY Directory OPTIONAL,
	IN PVOID Object
	);

VOID
KEAPI
ObInitSystem(
	);
