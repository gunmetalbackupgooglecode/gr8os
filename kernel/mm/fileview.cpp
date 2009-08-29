//
// FILE:		fileview.cpp
// CREATED:		17-Feb-2008  by Great
// PART:        MM
// ABSTRACT:
//			Memory Management routines. File views support
//

#include "common.h"


UCHAR
KEAPI
MiFileAccessToPageProtection(
	ULONG GrantedAccess
	)
/*++
	Convert file access to MM page protection
--*/
{
	// If writing is allowed, return read/write protection.
	if (GrantedAccess & FILE_WRITE_DATA)
		return MM_READWRITE;

	// Writing is not allowed, but reading is allowed. Return readonly protection
	if (GrantedAccess & FILE_READ_DATA)
		return MM_READONLY;

	// Both reading and writing are disabled. Return MM_NOACCESS
	return MM_NOACCESS;
}

ULONG
KEAPI
MiPageProtectionToGrantedAccess(
	UCHAR Protection
	)
/*++
	Convert page protection to file access
--*/
{
	ASSERT (Protection >= MM_NOACCESS && Protection <= MM_GUARD);

	Protection &= MM_MAXIMUM_PROTECTION; // select first three bits.

	switch (Protection)
	{
	case MM_NOACCESS:		
	case MM_GUARD:
		return 0;

	case MM_READONLY:
	case MM_EXECUTE_READONLY:	
	case MM_WRITECOPY:
	case MM_EXECUTE_WRITECOPY:
		return FILE_READ_DATA;

	case MM_READWRITE:
	case MM_EXECUTE_READWRITE:	
		return FILE_READ_DATA|FILE_WRITE_DATA;
	}

	return 0; // prevent compiler warning.
}

KESYSAPI
STATUS
KEAPI
MmCreateFileMapping(
	IN PFILE FileObject,
	IN PROCESSOR_MODE MappingMode,
	IN UCHAR StrongestProtection,
	OUT PHANDLE MappingHandle
	)
/*++
	Create file mapping to the specified file object at target processor mode.
	To map particular part of file caller should call MmMapViewOfFile later.
	Strongest acceptable protection should be specified.
	Pointer to the MAPPED_FILE is returned.
--*/
{
	STATUS Status;

	if (FileObject->CacheMap == NULL)
	{
		//
		// File should be cached if the caller wants us to map it
		//

		return STATUS_INVALID_PARAMETER_2;
	}

	if ( MiFileAccessToPageProtection(FileObject->DesiredAccess) < StrongestProtection )
	{
		//
		// StrongestProtection is greater than granted file access.
		// Cannot map
		//

		return STATUS_ACCESS_DENIED;
	}

	//
	// Allocate space for the structure
	//

	PMAPPED_FILE MappedFile;
	
	Status = ObCreateObject (	(PVOID*)&MappedFile, 
								sizeof(MAPPED_FILE), 
								MmFileMappingObjectType,
								NULL,
								OB_OBJECT_OWNER_MM
							);

	if (!SUCCESS(Status))
		return Status;

//	MappedFile = (PMAPPED_FILE) ExAllocateHeap (TRUE, sizeof(MAPPED_FILE));
//	if (!MappedFile)
//	{
//		return STATUS_INSUFFICIENT_RESOURCES;
//	}

	// File cannot go away until all its views are unmapped.
	ObReferenceObject (FileObject);

	MappedFile->FileObject = FileObject;
	MappedFile->StrongestProtection = StrongestProtection;
	MappedFile->TargetMode = MappingMode;
	InitializeLockedList (&MappedFile->ViewList);

	//
	// Insert into locked list of file mappings for this file
	//

	InterlockedInsertTailList (&FileObject->MappingList, &MappedFile->FileMappingsEntry);

	HANDLE hMapping = ObpCreateHandle (MappedFile, MiPageProtectionToGrantedAccess (StrongestProtection));
	if (hMapping == INVALID_HANDLE_VALUE)
	{
		//ExFreeHeap (MappedFile);
		ObpDeleteObject (MappedFile);
		ObDereferenceObject (FileObject);

		return STATUS_INSUFFICIENT_RESOURCES;
	}

	//
	// Return
	//

	*MappingHandle = hMapping;

	return STATUS_SUCCESS;
}

PVOID MiStartAddressesForProcessorModes[MaximumMode] = {
	(PVOID) MM_CRITICAL_AREA,		// ring0
	(PVOID) MM_DRIVER_AREA,			// ring1
	(PVOID) NULL,					// unused
	(PVOID) MM_USERMODE_AREA		// ring3
};

PVOID MiEndAddressesForProcessorModes[MaximumMode] = {
	(PVOID) MM_CRITICAL_AREA_END,		// ring0
	(PVOID) MM_DRIVER_AREA_END,			// ring1
	(PVOID) NULL,						// unused
	(PVOID) MM_USERMODE_AREA_END		// ring3
};

STATUS
KEAPI
MiFindAndReserveVirtualAddressesForFileView(
	IN PVOID StartAddress OPTIONAL,
	IN PROCESSOR_MODE MappingMode,
	IN ULONG ViewPages,
	OUT PVOID *RealAddress
	)
/*++
	(Optionally) Find and reserve virtual address range for file view.
	Real address will be stored in *RealAddress on success.

	Environment: MmPageDatabaseLock held
--*/
{
	ASSERT (MiStartAddressesForProcessorModes[MappingMode] != NULL);
	ASSERT (MiEndAddressesForProcessorModes[MappingMode] != NULL);

	if (StartAddress)
	{
		//
		// Caller has already chosen some virtual address for the view, check if it does really meet the requirements.
		//

		if (StartAddress < MiStartAddressesForProcessorModes[MappingMode] ||
			StartAddress >= MiEndAddressesForProcessorModes[MappingMode])
		{
			return STATUS_INVALID_PARAMETER;
		}

		//
		// Check that this address range is really free
		//

		for ( PVOID va = StartAddress; (ULONG)va < (ULONG)StartAddress + ViewPages*PAGE_SIZE; *(ULONG*)&va += PAGE_SIZE )
		{
			if (MmIsAddressValidEx (va) != PageStatusFree)
				return STATUS_INVALID_PARAMETER;
		}

		*RealAddress = StartAddress;
		return STATUS_SUCCESS;
	}
	else
	{
		//
		// Find the appropriate address range
		//

		for ( PVOID va = MiStartAddressesForProcessorModes[MappingMode];
			  va < MiEndAddressesForProcessorModes[MappingMode];
			  *(ULONG*)&va += PAGE_SIZE )
		{
			if (MmIsAddressValidEx (va) == PageStatusFree)
			{
				bool free = true;
				ULONG i;

				//
				// Check that all pages are free
				//

				for (i=1; i<ViewPages; i++)
				{
					free &= (MmIsAddressValidEx ((PVOID)((ULONG)va + i*PAGE_SIZE)) == PageStatusFree);
				}

				if (free)
				{
					//
					// Found range of free pages.
					//

					PMMPTE PointerPte = MiGetPteAddress (va);
					for (i=0; i<ViewPages; i++)
					{
						MiZeroPte (PointerPte);

						PointerPte->u1.e3.PteType = PTE_TYPE_VIEW;	// file view
						PointerPte->u1.e3.FileDescriptorNumber = 0;	// not mapped yet
						PointerPte->u1.e3.Protection = 0;			// not protected yet
						
						PointerPte = MiNextPte (PointerPte);
					}

					// Return

					*RealAddress = va;
					return STATUS_SUCCESS;

				} // if(free)

			} // if (MmIsAddressValidEx(..) == PageStatusFree)

		} // for (..)

		return STATUS_INSUFFICIENT_RESOURCES;

	} // if (StartAddress == NULL)
}


VOID
KEAPI
MiInitializeViewPages (
	IN OUT PMAPPED_VIEW View
	)
/*++
	Initialize properly view pages
--*/
{
	PMMPTE PointerPte = View->StartPte;

	for (ULONG i=0; i < View->ViewSize/PAGE_SIZE; i++)
	{
		PointerPte->u1.e3.FileDescriptorNumber = (USHORT) View->hMapping;
		PointerPte->u1.e3.Protection = View->Protection;

		PointerPte = MiNextPte (PointerPte);
	}
}
	

KESYSAPI
STATUS
KEAPI
MmMapViewOfFile(
	IN HANDLE hMapping,
	IN ULONG OffsetStart,
	IN ULONG OffsetStartHigh,
	IN ULONG ViewSize,
	IN UCHAR Protection,
	IN OUT PVOID *VirtualAddress // on input optinally contains desired VA, on output - real mapped VA of view.
	)
/*++
	Map view of the specified file, which is already being mapped (MmCreateFileMapping should be called before this call)
	Offset and size of the view should be specified.
	Protection should be lower or equal strongest protection of the specified mapping.
	Virtual address is returned in *VirtualAddress
--*/
{
	PMAPPED_FILE Mapping;
	STATUS Status;
	
	Status = ObpMapHandleToPointer (hMapping, -1 /*any access*/, (PVOID*)&Mapping, FALSE);

	if (!SUCCESS(Status))
		return Status;

	if (ObIsObjectGoingAway(Mapping))
		return STATUS_DELETE_PENDING;

	if (Mapping->StrongestProtection < Protection)
	{
		return STATUS_ACCESS_DENIED;
	}

	PMMPTE StartPte;
	PVOID va;

	ExAcquireMutex (&MmPageDatabaseLock);

	ViewSize = ALIGN_UP (ViewSize, PAGE_SIZE);

	//
	// Find and reserve virtual address range for file view
	//

	Status = MiFindAndReserveVirtualAddressesForFileView (
		*VirtualAddress, 
		Mapping->TargetMode,
		ViewSize/PAGE_SIZE, 
		&va);

	if (!SUCCESS(Status))
	{
		ExReleaseMutex (&MmPageDatabaseLock);
		return Status;
	}

	StartPte = MiGetPteAddress (va);

	//
	// Allocate space for the structure
	//

	PMAPPED_VIEW View = (PMAPPED_VIEW) ExAllocateHeap (TRUE, sizeof(MAPPED_VIEW));
	if (!View)
	{
		ExReleaseMutex (&MmPageDatabaseLock);
		return STATUS_INSUFFICIENT_RESOURCES;
	}

	View->StartOffsetInFile.LowPart = OffsetStart;
	View->StartOffsetInFile.HighPart = OffsetStartHigh;
	View->ViewSize = ViewSize;
	View->StartPte = StartPte;
	View->StartVa = va;
	View->Mapping = Mapping;
	View->hMapping = hMapping; // save handle
	View->Protection = Protection & MM_MAXIMUM_PROTECTION;

	//
	// Insert structure
	//

	InterlockedInsertTailList (&Mapping->ViewList, &View->ViewListEntry);

	//
	// Initialize view properly
	//

	MiInitializeViewPages (View);

	//
	// Return
	//

	*VirtualAddress = va;

	ExReleaseMutex (&MmPageDatabaseLock);

	return STATUS_SUCCESS;
}

STATUS
KEAPI
MiUnmapViewOfFile(
	IN PMAPPED_VIEW View
	)
/*++
	Internal routine used to unmap view of file
	
	Environment: mapping list locked.
--*/
{
	return STATUS_NOT_SUPPORTED;
}


KESYSAPI
STATUS
KEAPI
MmUnmapViewOfFile(
	IN HANDLE hMapping,
	IN PVOID VirtualAddress
	)
/*++
	Unmap the specified view of file
--*/
{
	return STATUS_NOT_SUPPORTED;
}

VOID
KEAPI
MiDeleteMapping(
	IN POBJECT_HEADER Object
	)
/*++
	See remarks to the PDELETE_OBJECT_ROUTINE typedef for the general explanations
	 of the type of such routines.

	This routine is called when the file mapping object is being deleted.
	We should dereference the corresponding file object
--*/
{
	PMAPPED_FILE FileMapping = OBJECT_HEADER_TO_OBJECT (Object, MAPPED_FILE);

	KdPrint(("MiDeleteMapping\n"));

	ExAcquireMutex (&FileMapping->ViewList.Lock);

	if (!IsListEmpty (&FileMapping->ViewList.ListEntry))
	{
		//
		// There are some unmapped views.
		//

		PMAPPED_VIEW View = CONTAINING_RECORD (FileMapping->ViewList.ListEntry.Flink, MAPPED_VIEW, ViewListEntry), NextView;

		while ( View != CONTAINING_RECORD (&FileMapping->ViewList.ListEntry, MAPPED_VIEW, ViewListEntry) )
		{
			KdPrint(("Unmapping not unmapped view: VA=%08x, Size=%08x\n", View->StartVa, View->ViewSize));

			NextView = CONTAINING_RECORD (View->ViewListEntry.Flink, MAPPED_VIEW, ViewListEntry);

			MiUnmapViewOfFile (View);

			View = NextView;
		}
	}

	ExReleaseMutex (&FileMapping->ViewList.Lock);

	if (!ObIsObjectGoingAway(FileMapping->FileObject))
	{
		InterlockedRemoveEntryList (&FileMapping->FileObject->MappingList, &FileMapping->FileMappingsEntry);
	}

	ObDereferenceObject (FileMapping->FileObject);
}
