//
// FILE:		ob.cpp
// CREATED:		16-Mar-2008  by Great
// PART:        OB
// ABSTRACT:
//			Objects support
//

#include "common.h"


//
// Root unnamed object directory.
//

POBJECT_DIRECTORY ObRootObjectDirectory;


//
// Directory object type
//

POBJECT_TYPE ObDirectoryObjectType;

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
	)
/*++
	Create object type
--*/
{
	POBJECT_TYPE Type;

	Type = (POBJECT_TYPE) ExAllocateHeap (TRUE, sizeof(OBJECT_TYPE));
	if (Type == NULL)
	{
		return STATUS_INSUFFICIENT_RESOURCES;
	}

	Type->OpenObjectRoutine = OpenObjectRoutine;
	Type->ParseRoutine = ParseRoutine;
	Type->CloseRoutine = CloseRoutine;
	Type->OwnerTag = OwnerTag;

	RtlDuplicateUnicodeString (&Type->ObjectTypeName, TypeName);

	*ObjectType = Type;
	return STATUS_SUCCESS;
}


KESYSAPI
STATUS
KEAPI
ObCreateObject(
   OUT PVOID *Object,
	IN ULONG ObjectSize,
	IN POBJECT_TYPE ObjectType,
	IN PUNICODE_STRING ObjectName	OPTIONAL,
	IN ULONG ObjectOwner
	)
/*++	
	Create an object.
--*/
{
	POBJECT_HEADER ObjectHeader;

	ObjectHeader = (POBJECT_HEADER) ExAllocateHeap (TRUE, sizeof(OBJECT_HEADER)+ObjectSize);
	if (ObjectHeader == NULL)
	{
		return STATUS_INSUFFICIENT_RESOURCES;
	}

	ObjectHeader->Owner = PsGetCurrentThread();
	ObjectHeader->OwnerTag = ObjectOwner;
	ObjectHeader->ObjectType = ObjectType;

	if (ARGUMENT_PRESENT (ObjectName))
	{
		RtlDuplicateUnicodeString (&ObjectHeader->ObjectName, ObjectName);
	}
	else
	{
		RtlInitUnicodeString (&ObjectHeader->ObjectName, RTL_NULL_UNICODE_STRING);
	}
	
	InitializeListHead (&ObjectHeader->DirectoryList);

	*Object = &ObjectHeader->Body;
	return STATUS_SUCCESS;
}


KESYSAPI
STATUS
KEAPI
ObInsertObject(
	IN POBJECT_DIRECTORY Directory OPTIONAL,
	IN PVOID Object
	)
/*++
	Insert object to the directory
--*/
{
	if (!ARGUMENT_PRESENT(Directory))
	{
		Directory = ObRootObjectDirectory;
	}

	POBJECT_HEADER ObjectHeader = OBJECT_TO_OBJECT_HEADER(Object);
	
	InsertTailList (&Directory->ObjectList, &ObjectHeader->DirectoryList);
	return STATUS_SUCCESS;
}



VOID
KEAPI
ObInitSystem(
	)
/*++
	Initialize objective subsystem
--*/
{
	STATUS Status;
	UNICODE_STRING Name;

	//
	// Create directory object type
	//

	RtlInitUnicodeString (&Name, L"Object Directory");

	Status = ObCreateObjectType (
		&ObDirectoryObjectType,
		&Name,
		NULL,
		NULL,
		NULL,
		OB_OBJECT_OWNER_OB
		);

	if (!SUCCESS(Status))
	{
		KeBugCheck (OB_INITIALIZATION_FAILED,
					__LINE__,
					Status,
					0,
					0);
	}

	//
	// Create root directory
	//

	Status = ObCreateObject (
		(PVOID*)&ObRootObjectDirectory,
		sizeof (OBJECT_DIRECTORY),
		ObDirectoryObjectType,
		NULL,
		OB_OBJECT_OWNER_OB
		);

	if (!SUCCESS(Status))
	{
		KeBugCheck (OB_INITIALIZATION_FAILED,
					__LINE__,
					Status,
					0,
					0);
	}

	//
	// Initialize root directory
	//

	InitializeListHead (&ObRootObjectDirectory->ObjectList);
}