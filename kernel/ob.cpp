//
// FILE:		ob.cpp
// CREATED:		16-Mar-2008  by Great
// PART:        OB
// ABSTRACT:
//			Objects support
//

#include "common.h"

#define ObPrint(x)

//
// Root unnamed object directory.
//

POBJECT_DIRECTORY ObRootObjectDirectory;
POBJECT_DIRECTORY ObGlobalObjectDirectory;
POBJECT_DIRECTORY ObDrvGlobalObjectDirectory;


//
// Directory object type
//
POBJECT_TYPE ObDirectoryObjectType;

//
// Symbolic link type
//
POBJECT_TYPE ObSymbolicLinkObjectType;



STATUS
KEAPI
ObpParseSymbolicLink(
	IN POBJECT_HEADER Object,
	IN PUNICODE_STRING FullObjectPath,
	IN PUNICODE_STRING RemainingPath,
	IN PVOID ParseContext,
	OUT PUNICODE_STRING ReparsePath
	)
/*++
	Parse symbolic link
--*/
{
	POBJECT_SYMBOLIC_LINK Symlink = OBJECT_HEADER_TO_OBJECT (Object, OBJECT_SYMBOLIC_LINK);

	if ((ULONG)ParseContext != 1)
	{
		KdPrint(("ObpParseSymbolicLink: FullPath[%S], RemainingPath[%S]\n", FullObjectPath->Buffer,
			RemainingPath->Buffer));

		ReparsePath->Length = Symlink->Target.Length + RemainingPath->Length;
		ReparsePath->MaximumLength = ReparsePath->Length + 2;
		ReparsePath->Buffer = (PWSTR) ExAllocateHeap (TRUE, ReparsePath->MaximumLength);
		if (!ReparsePath->Buffer)
			return STATUS_INSUFFICIENT_RESOURCES;

		wcscpy (ReparsePath->Buffer, Symlink->Target.Buffer);
		wcscat (ReparsePath->Buffer, RemainingPath->Buffer);

		return STATUS_REPARSE;
	}
	else
	{
		//
		// If ParseContext==1, caller wants to retrieve pointer to the symbolic link,
		//  not to the its object.
		//

		return STATUS_SUCCESS;
	}
}

VOID
KEAPI
ObpDeleteSymbolicLink(
	IN POBJECT_HEADER Object
	)
/*++
	Called when symbolic link is being deleted
--*/
{
	POBJECT_SYMBOLIC_LINK Symlink = OBJECT_HEADER_TO_OBJECT (Object, OBJECT_SYMBOLIC_LINK);

	RtlFreeUnicodeString (&Symlink->Target);
	ObDereferenceObject (OBJECT_HEADER_TO_OBJECT(Symlink->TargetObject,VOID));
}


KESYSAPI
STATUS
KEAPI
ObCreateSymbolicLink(
	PUNICODE_STRING SymlinkName,
	PUNICODE_STRING TargetPath
	)
/*++
	Create new symbolic link with SymlinkName, which will point to TargetPath
--*/
{
	STATUS Status;
	POBJECT_SYMBOLIC_LINK Symlink;
	UNICODE_STRING RelativeLinkName;
	PWSTR rel;
	PVOID Object;

	Status = ObReferenceObjectByName (
		TargetPath,
		NULL,
		KernelMode,
		0,
		0,
		&Object
		);

	if (!SUCCESS(Status))
		return Status;
	
	rel = wcsrchr(SymlinkName->Buffer+1, L'\\');
	if (!rel)
	{
		rel = SymlinkName->Buffer;
	}
	RtlInitUnicodeString (&RelativeLinkName, rel+1);

	Status = ObCreateObject(
		(PVOID*)&Symlink, 
		sizeof(OBJECT_SYMBOLIC_LINK),
		ObSymbolicLinkObjectType,
		&RelativeLinkName,
		OB_OBJECT_OWNER_IO
		);
	
	if (!SUCCESS(Status))
	{
		ObDereferenceObject (Object);
		return Status;
	}

	POBJECT_DIRECTORY Directory;
	if (rel != SymlinkName->Buffer)
	{
		WCHAR *wDirectoryName = (WCHAR*) ExAllocateHeap(TRUE, 
			(ULONG)rel-(ULONG)SymlinkName->Buffer+2);
		wcssubstr (SymlinkName->Buffer, 0, ((ULONG)rel-(ULONG)SymlinkName->Buffer)/2, wDirectoryName);

		UNICODE_STRING DirectoryName;
		RtlInitUnicodeString (&DirectoryName, wDirectoryName);

		Status = ObReferenceObjectByName (
			&DirectoryName,
			ObDirectoryObjectType,
			KernelMode,
			0,
			0,
			(PVOID*) &Directory
			);

		ExFreeHeap (wDirectoryName);
	}
	else
	{
		ObReferenceObject (ObRootObjectDirectory);
		Directory = ObRootObjectDirectory;
		Status = STATUS_SUCCESS;
	}

	if (!SUCCESS(Status))
	{
		ObpDeleteObjectInternal (Symlink);
		ObDereferenceObject (Object);
		return Status;
	}

	RtlDuplicateUnicodeString( TargetPath, &Symlink->Target );
	Symlink->TargetObject = OBJECT_TO_OBJECT_HEADER(Object);

	Status = ObInsertObject (Directory, Symlink);

	ObDereferenceObject (Directory);

	if (!SUCCESS(Status))
	{
		ObpDeleteObjectInternal (Symlink);
		ObDereferenceObject (Object);
		return Status;
	}

	return STATUS_SUCCESS;
}


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

	Type->OpenRoutine = OpenRoutine;
	Type->ParseRoutine = ParseRoutine;
	Type->CloseRoutine = CloseRoutine;
	Type->DeleteRoutine = DeleteRoutine;
	Type->QueryNameRoutine = QueryNameRoutine;
	Type->OwnerTag = OwnerTag;
	Type->ObjectCount = 0;

	RtlDuplicateUnicodeString (TypeName, &Type->ObjectTypeName);

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
	Create an object with the specified type, size and (optionally) name.
	The memory is initialized with zeroes
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
	ObjectHeader->ReferenceCount = 1;
	ObjectHeader->HandleCount = 0;
	ObjectType->ObjectCount ++;
	ObjectHeader->Flags = 0;

	if (ARGUMENT_PRESENT (ObjectName))
	{
		RtlDuplicateUnicodeString (ObjectName, &ObjectHeader->ObjectName);
	}
	else
	{
		RtlInitUnicodeString (&ObjectHeader->ObjectName, RTL_NULL_UNICODE_STRING);
	}
	
	InitializeListHead (&ObjectHeader->DirectoryList);
	ObjectHeader->ParentDirectory = NULL;

	ExInitializeMutex (&ObjectHeader->ObjectLock);

	*Object = &ObjectHeader->Body;

	memset (*Object, 0, ObjectSize);

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
	
	ExAcquireMutex (&Directory->DirectoryLock);

	InsertTailList (&Directory->ObjectList, &ObjectHeader->DirectoryList);
	ObjectHeader->ParentDirectory = Directory;

	ExReleaseMutex (&Directory->DirectoryLock);

	return STATUS_SUCCESS;
}


KESYSAPI
STATUS
KEAPI
ObCreateDirectory(
	OUT POBJECT_DIRECTORY *Directory,
	IN PUNICODE_STRING Name OPTIONAL,
	IN ULONG ObjectOwner,
	IN POBJECT_DIRECTORY InsertInto
	)
/*++
	Create a directory object
--*/
{
	STATUS Status;

	Status = ObCreateObject ((PVOID*)Directory, sizeof(OBJECT_DIRECTORY), ObDirectoryObjectType, Name, ObjectOwner);
	if (!SUCCESS(Status))
		return Status;

	Status = ObInsertObject (InsertInto, *Directory);
	if (!SUCCESS(Status))
	{
		ObpDeleteObjectInternal (*Directory);
		return Status;
	}

	InitializeListHead (&(*Directory)->ObjectList);
	ExInitializeMutex (&(*Directory)->DirectoryLock);

	return Status;
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
	ExInitializeMutex (&ObRootObjectDirectory->DirectoryLock);


#if OBEMU
	HandleTable = (POBJECT_HANDLE) ExAllocateHeap (TRUE, sizeof(OBJECT_HANDLE)*OB_MAX_HANDLES);
	memset (HandleTable, 0, sizeof(OBJECT_HANDLE)*OB_MAX_HANDLES);
#endif

	//
	// Create symbolic link object type
	//

	RtlInitUnicodeString (&Name, L"Symbolic Link");

	Status = ObCreateObjectType (
		&ObSymbolicLinkObjectType,
		&Name,
		NULL,
		ObpParseSymbolicLink,
		NULL,
		ObpDeleteSymbolicLink,
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
	// Create symlinks \Global and \DrvGlobal
	//

	RtlInitUnicodeString (&Name, L"Global");

	Status = ObCreateDirectory (
		&ObGlobalObjectDirectory, 
		&Name,
		OB_OBJECT_OWNER_OB,
		ObRootObjectDirectory
		);

	RtlInitUnicodeString (&Name, L"DrvGlobal");

	Status = ObCreateDirectory (
		&ObDrvGlobalObjectDirectory, 
		&Name,
		OB_OBJECT_OWNER_OB,
		ObRootObjectDirectory
		);

}

KESYSAPI
VOID
KEAPI
ObReferenceObject(
	PVOID Object
	)
/*++
	Increment object's reference count
--*/
{
	ULONG *refcount = &(OBJECT_TO_OBJECT_HEADER(Object)->ReferenceCount);
	ObInterlockedIncrement( refcount );

#if OB_TRACE_REF_DEREF
	KiDebugPrint("Object %08x [%S] of type [%S] referenced, new ref count %d\n", 
		Object, 
		OBJECT_TO_OBJECT_HEADER(Object)->ObjectName.Buffer ? OBJECT_TO_OBJECT_HEADER(Object)->ObjectName.Buffer : L"",
		OBJECT_TO_OBJECT_HEADER(Object)->ObjectType->ObjectTypeName.Buffer,
		OBJECT_TO_OBJECT_HEADER(Object)->ReferenceCount
		);
#endif
}

KESYSAPI
VOID
KEAPI
ObDereferenceObject(
	PVOID Object
	)
/*++
	Decrement object's reference count
--*/
{
	POBJECT_HEADER ObjectHeader = OBJECT_TO_OBJECT_HEADER(Object);
	
	ObInterlockedDecrement (&ObjectHeader->ReferenceCount);

#if OB_TRACE_REF_DEREF
	KiDebugPrint("Object %08x [%S] of type [%S] dereferenced, new ref count %d\n", 
		Object, 
		OBJECT_TO_OBJECT_HEADER(Object)->ObjectName.Buffer ? OBJECT_TO_OBJECT_HEADER(Object)->ObjectName.Buffer : L"",
		OBJECT_TO_OBJECT_HEADER(Object)->ObjectType->ObjectTypeName.Buffer,
		ObjectHeader->ReferenceCount);
#endif

	if ( ObjectHeader->ReferenceCount == 0 && 
		 ( (!(ObjectHeader->Flags & OBJ_PERMANENT)) || (ObjectHeader->Flags & OBJ_DELETE_PENDING) )
		 )
	{
		ASSERT (ObjectHeader->HandleCount == 0);

		ObpDeleteObjectInternal (Object);

#if OB_TRACE_REF_DEREF
		KiDebugPrint("Unused object %08x deleted\n", Object);
#endif
	}
}

KESYSAPI
VOID
KEAPI
ObDereferenceObjectEx(
	PVOID Object,
	ULONG Count
	)
/*++
	This function decrements object's reference count by the specified value
--*/
{
	POBJECT_HEADER ObjectHeader = OBJECT_TO_OBJECT_HEADER (Object);
	ULONG *refcount = &ObjectHeader->ReferenceCount;

#if OB_TRACE_REF_DEREF
	KiDebugPrint("Object %08x [%S] of type [%S] dereferenced by %d, new ref count %d\n", 
		Object, 
		OBJECT_TO_OBJECT_HEADER(Object)->ObjectName.Buffer ? OBJECT_TO_OBJECT_HEADER(Object)->ObjectName.Buffer : L"",
		OBJECT_TO_OBJECT_HEADER(Object)->ObjectType->ObjectTypeName.Buffer,
		Count,
		ObjectHeader->ReferenceCount);
#endif

	ObInterlockedExchangeAdd (refcount, -2);
}


KESYSAPI
VOID
KEAPI
ObMakeTemporaryObject(
	PVOID Object
	)
/*++
	Make this object temporary. Object will be deleted when all references are closed.
--*/
{
	OBJECT_TO_OBJECT_HEADER(Object)->Flags &= ~OBJ_PERMANENT;
}

KESYSAPI
VOID
KEAPI
ObLockObject(
	PVOID Object
	)
/*++
	This function locks an object.
	Caller can safely access the object
--*/
{
	ExAcquireMutex ( &OBJECT_TO_OBJECT_HEADER(Object)->ObjectLock );
}

KESYSAPI
VOID
KEAPI
ObUnlockObject(
	PVOID Object
	)
/*++
	This function unlocks the object after use
--*/
{
	ExReleaseMutex ( &OBJECT_TO_OBJECT_HEADER(Object)->ObjectLock );
}


STATUS
KEAPI
ObpDeleteObjectInternal(
	PVOID Object
	)
/*++
	Completely delete object if its reference count is zero
--*/
{
	POBJECT_HEADER ObjectHeader = OBJECT_TO_OBJECT_HEADER(Object);

	//
	// Can't delete object if it's in use.
	//

	if( ObjectHeader->ReferenceCount != 0)
		return STATUS_IN_USE;

	//
	// Delete object from the directory
	//

	if (!IsListEmpty(&ObjectHeader->DirectoryList))
	{
		ExAcquireMutex (&ObjectHeader->ParentDirectory->DirectoryLock);
		RemoveEntryList (&ObjectHeader->DirectoryList);
		ExReleaseMutex (&ObjectHeader->ParentDirectory->DirectoryLock);
	}

	//
	// Call delete routine
	//

	if (ObjectHeader->ObjectType->DeleteRoutine)
	{
		ObjectHeader->ObjectType->DeleteRoutine (ObjectHeader);
	}

	//
	// Free object name
	//

	if (ObjectHeader->ObjectName.Buffer && ObjectHeader->ObjectName.Length)
	{
		ExFreeHeap (ObjectHeader->ObjectName.Buffer);
	}

	//
	// Free object
	//

	ExFreeHeap (ObjectHeader);
	return STATUS_SUCCESS;
}

STATUS
KEAPI
ObpDeleteObject(
	PVOID Object
	)
/*++
	Deletes an object if it is not used now or mark it as delete-pending
--*/
{
	POBJECT_HEADER ObjectHeader = OBJECT_TO_OBJECT_HEADER (Object);

	if (ObjectHeader->ReferenceCount > 1)		
	//  ObCreateObject referenced it when created.
	{
		ObjectHeader->Flags |= OBJ_DELETE_PENDING;
		
		//
		// Delete reference to this object:
		//   ObCreateObject referenced it when created it
		//
		//  So if someone will delete the last reference to it,
		//   the object will gone away, because it is marked
		//   as delete-pending.
		//

		ObDereferenceObject (Object);
		return STATUS_SUCCESS;
	}

	ObDereferenceObject (Object);

	//ObpDeleteObjectInternal (Object);
	return STATUS_SUCCESS;
}

KESYSAPI
STATUS
KEAPI
ObDeleteObject(
	PUNICODE_STRING ObjectName
	)
/*++
	Delete object by its name
--*/
{
	PVOID Object;
	STATUS Status;

	Status = ObReferenceObjectByName (
		ObjectName,
		NULL,
		KernelMode,
		0,
		(PVOID)1,		// If this is a symbolic link, don't resolve it.
		&Object
		);

	if (!SUCCESS(Status))
	{
		return Status;
	}

	POBJECT_HEADER ObjectHeader = OBJECT_TO_OBJECT_HEADER (Object);
	if (ObjectHeader->ReferenceCount > 2)		
	// We've just referenced it above (1) + 
	//  ObCreateObject referenced it when created.
	{
		ObjectHeader->Flags |= OBJ_DELETE_PENDING;
		
		//
		// Delete references to this object:
		//   ObReferenceObjectByName referenced it above
		//   ObCreateObject referenced it when created it
		//
		//  So if someone will delete the last reference to it,
		//   the object will gone away, because it is marked
		//   as delete-pending.
		//

		ObDereferenceObjectEx (Object, 2);
		return STATUS_SUCCESS;
	}

	// Decrement by 2
	ObDereferenceObjectEx (Object, 2);

	ObpDeleteObjectInternal (Object);
	return STATUS_SUCCESS;
}


STATUS
KEAPI
ObpFindObjectInDirectory(
	IN POBJECT_DIRECTORY Directory,
	IN PWSTR ObjectName,
	OUT POBJECT_HEADER *ObjectHeader
	)
/*++
	Searches object in the specified directory
--*/
{
	ExAcquireMutex (&Directory->DirectoryLock);

	POBJECT_HEADER obj = CONTAINING_RECORD (Directory->ObjectList.Flink, OBJECT_HEADER, DirectoryList);

	while (obj != CONTAINING_RECORD(&Directory->ObjectList, OBJECT_HEADER, DirectoryList))
	{
		ObPrint(("OBFIND: Comparing '%S' & '%S'\n", ObjectName, obj->ObjectName.Buffer));

		if(!wcscmp(obj->ObjectName.Buffer, ObjectName))
		{
			//
			// Found.
			//

			ExReleaseMutex (&Directory->DirectoryLock);
			*ObjectHeader = obj;
			return STATUS_SUCCESS;
		}

		obj = CONTAINING_RECORD (obj->DirectoryList.Flink, OBJECT_HEADER, DirectoryList);
	}

	ExReleaseMutex (&Directory->DirectoryLock);
	return STATUS_NOT_FOUND;
}


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
	)
/*++
	Reference object by it's name.
--*/
{
	PWSTR wc=ObjectName->Buffer, prevslash;
	POBJECT_DIRECTORY CurrentDirectory;
	STATUS Status;
	POBJECT_HEADER ObjectHeader;
	UNICODE_STRING ReparsePath = {0};

	PWSTR ParseBuffer = ObjectName->Buffer;
	ULONG ParseLength = ObjectName->Length;

	UNICODE_STRING ParsePath;

reparse:

	CurrentDirectory = ObRootObjectDirectory;

	if (*wc != L'\\')
	{
		//
		// Only absolute path can be specified
		//

		if (ReparsePath.Buffer)
			ExFreeHeap (ReparsePath.Buffer);

		return STATUS_NOT_FOUND;
	}

	prevslash = wc++;

	ObPrint(("OBREF: Starting search for '%S'\n", prevslash));

	for (;; wc++)
	{
		if (*wc == L'\\' || (((ULONG)wc-(ULONG)ParseBuffer)>=ParseLength))
		{
			PWSTR TempName = (PWSTR) ExAllocateHeap (TRUE, ((ULONG)wc-(ULONG)prevslash));
			
			if (TempName == NULL)
			{
				if (ReparsePath.Buffer)
					ExFreeHeap (ReparsePath.Buffer);

				return STATUS_INSUFFICIENT_RESOURCES;
			}

			wcssubstr(prevslash+1, 0, ((ULONG)wc-(ULONG)prevslash)/2-1, TempName);

			ObPrint(("OBREF: Found part '%S'\n", TempName));

			//
			// Find next directory
			//

			Status = ObpFindObjectInDirectory (CurrentDirectory, TempName, &ObjectHeader);
			if (!SUCCESS(Status))
			{
				ExFreeHeap (TempName);
				if (ReparsePath.Buffer)
					ExFreeHeap (ReparsePath.Buffer);
				return Status;
			}

			ExFreeHeap (TempName);

			//
			// Call object type parse routine
			//

			if (ObjectHeader->ObjectType != ObDirectoryObjectType &&
				ObjectHeader->ObjectType->ParseRoutine)
			{
				UNICODE_STRING RemainingPath;
				STATUS Status;

				RtlInitUnicodeString (&RemainingPath, wc);
				RtlInitUnicodeString (&ParsePath, ParseBuffer);

				//
				// Check if this is not a first reparsing.
				//  If so, remember the pointer ReparsePath.Buffer to free it after the following call
				//  to the reparse routine
				//

				PWSTR ReparseBuffer = ReparsePath.Buffer;

				Status = (ObjectHeader->ObjectType->ParseRoutine) (
					ObjectHeader,
					&ParsePath,
					&RemainingPath,
					ParseContext,
					&ReparsePath
					);

				//
				// Free the reparse buffer
				//

				if (ReparseBuffer)
				{
					ExFreeHeap (ReparseBuffer);
				}

				if (!SUCCESS(Status))
				{
					//
					// WARN: If ParseRoutine returns error status code it SHOULD NOT touch
					//  the ReparsePath unicode string.
					//

					if (ReparsePath.Buffer)
						ExFreeHeap (ReparsePath.Buffer);

					return Status;
				}

				if (Status == STATUS_REPARSE)
				{
					wc = ReparsePath.Buffer;
					ParseBuffer = wc;
					ParseLength = ReparsePath.Length;

					goto reparse;
				}
				else
				{
					ReparsePath.Buffer = NULL;
				}

				if (Status == STATUS_FINISH_PARSING)
				{
					//
					// We should stop here.
					//

				
					if (ReparsePath.Buffer)
						ExFreeHeap (ReparsePath.Buffer);

					goto finish;
				}
			}

			//
			// Last name
			//
			if (((ULONG)wc-(ULONG)ParseBuffer)>=ParseLength)
			{
				if (ReparsePath.Buffer)
					ExFreeHeap (ReparsePath.Buffer);

				goto finish;
			}

			//
			// This should be a directory object
			//
			if (ObjectHeader->ObjectType != ObDirectoryObjectType)
			{
				if (ReparsePath.Buffer)
					ExFreeHeap (ReparsePath.Buffer);

				return STATUS_NOT_FOUND;
			}

			CurrentDirectory = (POBJECT_DIRECTORY) &ObjectHeader->Body;

			prevslash = wc;
		}
	}

finish:

	//
	// Free reparse buffer
	//

	if (ReparsePath.Buffer)
		ExFreeHeap (ReparsePath.Buffer);

	//
	// Perform the checks
	//

	if (RequestorMode == UserMode && ObjectHeader->ObjectType != ObjectType)
	{
		return STATUS_ACCESS_DENIED;
	}

	//
	// Reference the object and return it
	//

	*Object = &ObjectHeader->Body;
	ObReferenceObject (*Object);

	return STATUS_SUCCESS;
}

#if DBG

VOID
ObpDumpDirectory(
	POBJECT_DIRECTORY Directory,
	int Indent
	)
/*++
	Internal routine dumps the specified directory content to the debug output
--*/
{
	ExAcquireMutex (&Directory->DirectoryLock);

	POBJECT_HEADER dirhdr = OBJECT_TO_OBJECT_HEADER (Directory);
	POBJECT_HEADER obj = CONTAINING_RECORD (Directory->ObjectList.Flink, OBJECT_HEADER, DirectoryList);

	for (int i=0; i<Indent; i++ ) KdPrint((" "));
	KdPrint(("\\%S\n", dirhdr->ObjectName.Buffer));
	
	while (obj != CONTAINING_RECORD (&Directory->ObjectList, OBJECT_HEADER, DirectoryList))
	{
		if (obj->ObjectType != ObDirectoryObjectType)
		{
			for (int i=0; i<Indent+3; i++ ) KdPrint((" "));
			KdPrint(("%S [%S] Refs=%d Handles=%d ", 
				obj->ObjectName.Buffer,
				obj->ObjectType->ObjectTypeName.Buffer,
				obj->ReferenceCount,
				obj->HandleCount ));

			if (obj->ObjectType == ObSymbolicLinkObjectType)
			{
				POBJECT_SYMBOLIC_LINK symlink = OBJECT_HEADER_TO_OBJECT (obj, OBJECT_SYMBOLIC_LINK);

				KdPrint(("Target=[%S]", symlink->Target.Buffer));
			}

			KdPrint(("\n"));
		}
		else
		{
			ObpDumpDirectory ((POBJECT_DIRECTORY)&obj->Body, Indent+3);
		}

		obj = CONTAINING_RECORD (obj->DirectoryList.Flink, OBJECT_HEADER, DirectoryList);
	}

	KdPrint(("\n"));

	ExReleaseMutex (&Directory->DirectoryLock);
}

#endif

KESYSAPI
STATUS
KEAPI
ObQueryObjectName(
	IN PVOID Object,
	OUT PUNICODE_STRING ObjectName
	)
/*++
	This function retrieves object name for the specified object
--*/
{
	POBJECT_HEADER ObjectHeader = OBJECT_TO_OBJECT_HEADER (Object);

	if (ObjectHeader->ObjectType->QueryNameRoutine)
	{
		return ObjectHeader->ObjectType->QueryNameRoutine (ObjectHeader, ObjectName);
	}

	if (ObjectHeader->ObjectName.Buffer)
	{
		PWSTR ObjName = (PWSTR) ExAllocateHeap (TRUE, ObjectHeader->ObjectName.MaximumLength);
		wcscpy (ObjName, ObjectHeader->ObjectName.Buffer);

		ULONG CurrLen = ObjectHeader->ObjectName.Length;

		for (POBJECT_DIRECTORY CurrDir = ObjectHeader->ParentDirectory; CurrDir != NULL; )
		{
			PWSTR TempName, OldName = ObjName;
			ObjectHeader = OBJECT_TO_OBJECT_HEADER (CurrDir);

			CurrLen += 4 + ObjectHeader->ObjectName.Length; // '\\' + length of parent dir. name + NULL

			TempName = (PWSTR) ExAllocateHeap (TRUE, CurrLen);
			wcscpy (TempName, ObjectHeader->ObjectName.Buffer);
			wcscat (TempName, L"\\");
			wcscat (TempName, OldName);
			ObjName = TempName;

			ExFreeHeap (OldName);

			CurrDir = ObjectHeader->ParentDirectory;
		}

		RtlInitUnicodeString (ObjectName, ObjName);
		return STATUS_SUCCESS;
	}

	return STATUS_UNSUCCESSFUL;
}


#if OBEMU
POBJECT_HANDLE HandleTable;
#endif

HANDLE
KEAPI
ObpCreateHandle(
	IN PVOID Object,
	IN ULONG GrantedAccess
	)
/*++
	Create new handle for the specified object with the appropriate access rights
--*/
{
	POBJECT_HANDLE ObjectTable = ObGetCurrentThreadObjectTable ();
	PTHREAD Thread = PsGetCurrentThread();

	ObLockObjectTable();

	for (int i=0; i<OB_MAX_HANDLES; i++)
	{
		if (ObjectTable->Object == NULL)
		{
			ObjectTable->Object = Object;
			ObjectTable->GrantedAccess = GrantedAccess;
			ObjectTable->Owner = Thread;

			ObUnlockObjectTable();

			return (HANDLE)i;
		}
	}

	ObUnlockObjectTable();

	return INVALID_HANDLE_VALUE;
}


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
	)
/*++
	Open object by its name
--*/
{
	PVOID Object;
	STATUS Status;

	Status = ObReferenceObjectByName (
		ObjectName,
		ObjectType,
		RequestorMode,
		DesiredAccess,
		ParseContext,
		&Object
		);

	if (!SUCCESS(Status))
		return Status;

	HANDLE Handle = ObpCreateHandle (Object, DesiredAccess);

	if (Handle == INVALID_HANDLE_VALUE)
	{
		ObDereferenceObject (Object);
		return STATUS_UNSUCCESSFUL;
	}

	POBJECT_HEADER ObjectHeader = OBJECT_TO_OBJECT_HEADER (Object);

	//
	// Increment object's handle count.
	// Reference count is already incremented by ObReferenceObjectByName
	//

	ObInterlockedIncrement (&ObjectHeader->HandleCount);

	*ObjectHandle = Handle;

	return STATUS_SUCCESS;
}


KESYSAPI
STATUS
KEAPI
ObOpenObjectByPointer(
	IN PVOID Object,
	IN POBJECT_TYPE ObjectType	OPTIONAL,
	IN PROCESSOR_MODE RequestorMode,
	IN ULONG DesiredAccess UNIMPLEMENTED,
	OUT PHANDLE ObjectHandle
	)
/*++
	Open object handle by its pointer
--*/
{
	POBJECT_HEADER ObjectHeader = OBJECT_TO_OBJECT_HEADER (Object);
	
	if (ObjectHeader->ObjectType != ObjectType && RequestorMode != KernelMode)
	{
		return STATUS_ACCESS_DENIED;
	}

	//
	// Increment object's reference count
	//

	ObReferenceObject (Object);

	HANDLE Handle = ObpCreateHandle (Object, DesiredAccess);

	if (Handle == INVALID_HANDLE_VALUE)
	{
		ObDereferenceObject (Object);
		return STATUS_UNSUCCESSFUL;
	}

	//
	// Increment object's handle count
	//

	ObInterlockedIncrement (&ObjectHeader->HandleCount);

	*ObjectHandle = Handle;

	return STATUS_SUCCESS;
}

STATUS
KEAPI
ObpDeleteHandle(
	IN HANDLE Handle,
	IN BOOLEAN AlreadyLocked OPTIONAL
	)
/*++
	Delete object handle from handle table
--*/
{
	POBJECT_HANDLE ObjectTable = ObGetCurrentThreadObjectTable ();

	if ( ((ULONG)Handle & 0xFFFFFFF) > 0xFFFF )
	{
		return STATUS_INVALID_HANDLE;
	}

	if (!AlreadyLocked) ObLockObjectTable();

	POBJECT_HANDLE ObjHandle = &ObjectTable[(ULONG)Handle];

	if (ObjHandle->Object == NULL)
	{
		ObUnlockObjectTable();
		return STATUS_INVALID_HANDLE;
	}

	ObjHandle->Object = NULL;

	ObUnlockObjectTable();
	return STATUS_SUCCESS;
}

STATUS
KEAPI
ObpMapHandleToPointer(
	IN HANDLE Handle,
	IN ULONG DesiredAccess OPTIONAL,
	OUT PVOID *Object,
	IN BOOLEAN KeepLock OPTIONAL
	)
/*++
	This function maps handle to pointer and performs access check (if DesiredAccess!=0)
--*/
{
	POBJECT_HANDLE ObjectTable = ObGetCurrentThreadObjectTable ();

	if ( ((ULONG)Handle & 0xFFFFFFF) > 0xFFFF )
	{
		return STATUS_INVALID_HANDLE;
	}

	ObLockObjectTable();

	POBJECT_HANDLE ObjHandle = &ObjectTable[(ULONG)Handle];

	if (ObjHandle->Object == NULL)
	{
		if (!KeepLock) ObUnlockObjectTable();
		return STATUS_INVALID_HANDLE;
	}

	if ( DesiredAccess && ((ObjHandle->GrantedAccess & DesiredAccess) == 0) )
	{
		if (!KeepLock) ObUnlockObjectTable();
		return STATUS_ACCESS_DENIED;
	}

	*Object = ObjHandle->Object;

	if (!KeepLock) ObUnlockObjectTable();
	return STATUS_SUCCESS;
}


STATUS
KEAPI
ObClose(
	IN HANDLE Handle
	)
/*++
	Close the handle and decrement its handle and reference count
--*/
{
	PVOID Object;
	STATUS Status;
	POBJECT_HEADER ObjectHeader;

	//
	// Map handle to object pointer and keep handle table lock
	//

	Status = ObpMapHandleToPointer (Handle, 0, &Object, TRUE);
	if (!SUCCESS(Status))
	{
		return Status;
	}

	ObjectHeader = OBJECT_TO_OBJECT_HEADER (Object);

	ObDereferenceObject (Object);
	ObInterlockedDecrement (&ObjectHeader->HandleCount);

	return ObpDeleteHandle (
		Handle,	
		TRUE				// Already locked by ObpMapHandleToPointer
		);
}
	
KESYSAPI
STATUS
KEAPI
ObReferenceObjectByHandle(
	IN HANDLE ObjectHandle,
	IN PROCESSOR_MODE RequestorMode UNIMPLEMENTED,
	IN ULONG DesiredAccess,
	OUT PVOID *ObjectPointer
	)
/*++
	Reference object by its handle
--*/
{
	PVOID Object;
	STATUS Status;

	UNREFERENCED_PARAMETER (RequestorMode);

	Status = ObpMapHandleToPointer (ObjectHandle, DesiredAccess, &Object, FALSE);
	if (!SUCCESS(Status))
	{
		return Status;
	}

	ObReferenceObject (Object);
	*ObjectPointer = Object;
	
	return STATUS_SUCCESS;
}