//
// FILE:		ldr.cpp
// CREATED:		17-Feb-2008  by Great
// PART:        MM
// ABSTRACT:
//			Memory Management routines. PE loader
//

#include "common.h"


inline 
VOID
_MiGetHeaders(
	PCHAR ibase, 
	PIMAGE_FILE_HEADER *ppfh, 
	PIMAGE_OPTIONAL_HEADER *ppoh, 
	PIMAGE_SECTION_HEADER *ppsh
	)
/*++
	Get pointers to PE headers
--*/
{
	PIMAGE_DOS_HEADER mzhead = (PIMAGE_DOS_HEADER)ibase;
	PIMAGE_FILE_HEADER pfh;
	PIMAGE_OPTIONAL_HEADER poh;
	PIMAGE_SECTION_HEADER psh;

	pfh = (PIMAGE_FILE_HEADER)&ibase[mzhead->e_lfanew];
	pfh = (PIMAGE_FILE_HEADER)((PBYTE)pfh + sizeof(IMAGE_NT_SIGNATURE));
	poh = (PIMAGE_OPTIONAL_HEADER)((PBYTE)pfh + sizeof(IMAGE_FILE_HEADER));
	psh = (PIMAGE_SECTION_HEADER)((PBYTE)poh + sizeof(IMAGE_OPTIONAL_HEADER));

	if (ppfh) *ppfh = pfh;
	if (ppoh) *ppoh = poh;
	if (ppsh) *ppsh = psh;
}

#define MiGetHeaders(x,y,z,q) _MiGetHeaders((PCHAR)(x),(y),(z),(q))

#if LDR_TRACE_MODULE_LOADING
#define LdrTrace(x) KiDebugPrint x
#define LdrPrint(x) KiDebugPrint x
#else
#define LdrTrace(x)
#define LdrPrint(x)
#endif

PLDR_MODULE
KEAPI
LdrAddModuleToLoadedList(
	IN PUNICODE_STRING ModuleName,
	IN PVOID ImageBase,
	IN ULONG ImageSize
	)
/*++	
	Add module to PsLoadedModuleList
--*/
{
	PLDR_MODULE Module = (PLDR_MODULE) ExAllocateHeap (FALSE, sizeof(LDR_MODULE));

	Module->Base = ImageBase;
	Module->Size = ImageSize;
	RtlDuplicateUnicodeString( ModuleName, &Module->ModuleName );

	InterlockedInsertTailList (&PsLoadedModuleList, &Module->ListEntry);

	return Module;
}

VOID
KEAPI
LdrRemoveModuleFromLoadedList(
	IN PLDR_MODULE Module
	)
/**
 * Remove module from loaded list.
 */
{
	RtlFreeUnicodeString(&Module->ModuleName);
	InterlockedRemoveEntryList (&PsLoadedModuleList, &Module->ListEntry);
	ExFreeHeap(Module);
}

KESYSAPI
STATUS
KEAPI
LdrLookupModule(
	IN PUNICODE_STRING ModuleName,
	OUT PLDR_MODULE *pModule
	)
/*++
	Search module in PsLoadedModuleList.
	For each module its name is compared with the specified name. 
	On success, pointer to LDR_MODULE is returned.
--*/
{
	ExAcquireMutex (&PsLoadedModuleList.Lock);

	PLDR_MODULE Ret = NULL;
	PLDR_MODULE Module = CONTAINING_RECORD (PsLoadedModuleList.ListEntry.Flink, LDR_MODULE, ListEntry);
	STATUS Status = STATUS_NOT_FOUND;

	while (Module != CONTAINING_RECORD (&PsLoadedModuleList, LDR_MODULE, ListEntry))
	{
		if (!wcsicmp(ModuleName->Buffer, Module->ModuleName.Buffer))
		{
			Status = STATUS_SUCCESS;
			Ret = Module;
			break;
		}

		Module = CONTAINING_RECORD (Module->ListEntry.Flink, LDR_MODULE, ListEntry);
	}

	ExReleaseMutex (&PsLoadedModuleList.Lock);

	*pModule = Ret;
	return Status;
}

KESYSAPI
PVOID
KEAPI
LdrGetProcedureAddressByOrdinal(
	IN PVOID Base,
	IN USHORT Ordinal
	)
/*++
	Retrieves export symbol by ordinal
--*/
{
	PIMAGE_OPTIONAL_HEADER poh;
	PIMAGE_EXPORT_DIRECTORY pexd;
	PULONG AddressOfFunctions;
	
	// Get headers
	MiGetHeaders (Base, NULL, &poh, NULL);

	// Get export
	*(PBYTE*)&pexd = (PBYTE)Base + poh->DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].VirtualAddress;
	if( (PVOID)pexd == Base )
		return NULL;

	*(PBYTE*)&AddressOfFunctions = (PBYTE)Base + pexd->AddressOfFunctions;

	if( !pexd->NumberOfNames )
	{
		LdrPrint(("LDR: Assertion failed for pexd->NumberOfNames != 0\n"));
	}

	if (Ordinal - pexd->Base < pexd->NumberOfFunctions)
	{
		return (PVOID)((ULONG)Base + (ULONG)AddressOfFunctions[Ordinal - pexd->Base]);
	}

	return NULL;
}


PVOID
KEAPI
LdrGetProcedureAddressByName(
	IN PVOID Base,
	IN PCHAR FunctionName
	)
/*++
	Walks the export directory of the image and look for the specified function name.
	Entry point of this function will be returned on success.
	If FunctionName==NULL, return module entry point.
--*/
{
	PIMAGE_OPTIONAL_HEADER poh;
	PIMAGE_EXPORT_DIRECTORY pexd;
	PULONG AddressOfFunctions;
	PULONG AddressOfNames;
	PUSHORT AddressOfNameOrdinals;
	ULONG i;
	ULONG SizeOfExport;
	
	// Get headers
	MiGetHeaders (Base, NULL, &poh, NULL);

	if (FunctionName == NULL)
	{
		return (PVOID)((ULONG)Base + poh->AddressOfEntryPoint);
	}

	// Get export
	*(PBYTE*)&pexd = (PBYTE)Base + poh->DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].VirtualAddress;
	SizeOfExport = poh->DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].Size;
	if( (PVOID)pexd == Base )
		return NULL;

	*(PBYTE*)&AddressOfFunctions = (PBYTE)Base + pexd->AddressOfFunctions;
	*(PBYTE*)&AddressOfNames = (PBYTE)Base + pexd->AddressOfNames;
	*(PBYTE*)&AddressOfNameOrdinals = (PBYTE)Base + pexd->AddressOfNameOrdinals;

	if( !pexd->NumberOfNames )
	{
		LdrPrint(("LDR: Assertion failed for pexd->NumberOfNames != 0\n"));
	}

	// Find function
	for( i=0; i<pexd->NumberOfNames; i++ ) 
	{
		PCHAR name = ((char*)Base + AddressOfNames[i]);
		PVOID addr = (PVOID*)((DWORD)Base + AddressOfFunctions[AddressOfNameOrdinals[i]]);

		if( !strcmp( name, FunctionName ) )
		{
			//
			// Check for export forwarding.
			//

			if( ((ULONG)addr >= (ULONG)pexd) && 
				((ULONG)addr < (ULONG)pexd + SizeOfExport) )
			{
				LdrPrint(("LDR: GETPROC: Export forwarding found [%s to %s]\n", FunctionName, addr));

				char* tname = (char*)ExAllocateHeap(TRUE, strlen((char*)addr)+5);
				if (!tname)
				{
					LdrPrint(("LDR: GETPROC: Not enough resources\n"));
					return NULL;
				}
				memcpy( tname, (void*)addr, strlen((char*)addr)+1 );

				char* dot = strchr(tname, '.');
				if( !dot ) {
					LdrPrint(("LDR: GETPROC: Bad export forwarding for %s\n", addr));
					ExFreeHeap(tname);
					return NULL;
				}

				*dot = 0;
				dot++;      // dot    ->    func name
				            // tname  ->    mod  name

				char ModName[100];
				strcpy(ModName, tname);
				if( stricmp(tname, "kernel") )
					strcat(ModName, ".sys");

				PLDR_MODULE Module;
				UNICODE_STRING ModuleName;
				wchar_t wmod[200];
				STATUS Status;

				mbstowcs (wmod, ModName, -1);
				RtlInitUnicodeString( &ModuleName, wmod );

				Status = LdrLookupModule (&ModuleName, &Module);

				if( !SUCCESS(Status) ) 
				{
					LdrPrint(("LDR: GETPROC: Bad module in export forwarding: %s\n", tname));
					ExFreeHeap(tname);
					return NULL;
				}

				void* func = LdrGetProcedureAddressByName(Module->Base, dot);
				if( !func ) {
					LdrPrint(("LDR: GETPROC: Bad symbol in export forwarding: %s\n", dot));
					ExFreeHeap(tname);
					return NULL;
				}

				ExFreeHeap(tname);

				LdrPrint(("LDR: GETPROC: Export forwarding %s resolved to %08x\n", addr, func));
				return func;
			}
			else
			{
				return addr;
			}
		}
	}
	
	return NULL;
}

STATUS
KEAPI
LdrWalkImportDescriptor(
	IN PVOID ImageBase,
	IN PIMAGE_OPTIONAL_HEADER OptHeader,
	IN ULONG Flags
	)
/*++
	Walk image's import descriptor.
	For each 
--*/
{
	PIMAGE_IMPORT_DESCRIPTOR ImportDescriptor = (PIMAGE_IMPORT_DESCRIPTOR) RVATOVA(OptHeader->DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress, ImageBase);

	for ( ; ImportDescriptor->Name; ImportDescriptor++ )
	{
		PLDR_MODULE Module;
		STATUS Status;
		UNICODE_STRING Unicode;
		WCHAR wbuff[200];

		char* ModName = (char*) RVATOVA(ImportDescriptor->Name, ImageBase);

		mbstowcs (wbuff, ModName, -1);
		RtlInitUnicodeString( &Unicode, wbuff );

		LdrPrint(("LdrWalkImportDescriptor: searching module %S\n", wbuff));

		//
		// Find module
		//

		Status = LdrLookupModule (
			&Unicode,
			&Module
			);

		if (!SUCCESS(Status))
		{
			LdrPrint(("LdrWalkImportDescriptor: %S not found (St=%08x)\n", wbuff, Status));
			return Status;
		}

		// Bound import?
		if (ImportDescriptor->TimeDateStamp == -1 )
		{
			LdrPrint(("LdrWalkImportDescriptor: %S: bound import not supported\n", wbuff));
			return STATUS_NOT_SUPPORTED;
		}

		//
		// Process imports
		//
		for (
			PIMAGE_THUNK_DATA Thunk = (PIMAGE_THUNK_DATA) RVATOVA(ImportDescriptor->FirstThunk,ImageBase);
			Thunk->u1.Ordinal; 
			Thunk ++ )
		{
			if (IMAGE_SNAP_BY_ORDINAL(Thunk->u1.Ordinal))
			{
				Thunk->u1.Function = (ULONG) LdrGetProcedureAddressByOrdinal (Module->Base, IMAGE_ORDINAL(Thunk->u1.Ordinal));

				if (!Thunk->u1.Function)
				{
					LdrPrint(("Can't resolve inmport by ordinal %d from %S: not found\n", IMAGE_ORDINAL(Thunk->u1.Ordinal), wbuff));
					return STATUS_INVALID_FILE_FOR_IMAGE;
				}

				LdrTrace (("LDR: [Loading %S]: Resolved import by ordinal %d\n", wbuff, IMAGE_ORDINAL(Thunk->u1.Ordinal)));
			}
			else
			{
				PIMAGE_IMPORT_BY_NAME Name = (PIMAGE_IMPORT_BY_NAME) RVATOVA(Thunk->u1.AddressOfData, ImageBase);

				Thunk->u1.Function = (ULONG) LdrGetProcedureAddressByName (Module->Base, (char*) Name->Name);

				if (!Thunk->u1.Function)
				{
					LdrPrint(("Can't resolve inmport by name %s from %S: not found\n", Name->Name, wbuff));
					return STATUS_INVALID_FILE_FOR_IMAGE;
				}

				LdrTrace (("LDR: [Loading %S]: Resolved import by name '%s' -> %08x\n", wbuff, Name->Name, Thunk->u1.Function));
			}
		}
	}

	return STATUS_SUCCESS;
}


STATUS
KEAPI
LdrRelocateImage(
	IN PVOID ImageBase,
	IN PIMAGE_OPTIONAL_HEADER OptHeader
	)
/*++
	Apply relocations to the specified image(ImageBase) from the address OptHeader->ImageBase to ImageBase.
--*/
{
	ULONG Delta = (ULONG)ImageBase - OptHeader->ImageBase;
	PIMAGE_BASE_RELOCATION Relocation = IMAGE_FIRST_RELOCATION(ImageBase);

	LdrPrint(("LDR: Fixing image %08x, delta %08x\n", ImageBase, Delta));

	if( (PVOID)Relocation == ImageBase )
	{
		LdrPrint(("LDR: No relocation table present\n"));
		return FALSE;
	}

	BOOLEAN bFirstChunk = TRUE;

	while ( ((ULONG)Relocation - (ULONG)IMAGE_FIRST_RELOCATION(ImageBase)) < OptHeader->DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC].Size )
	{
		bFirstChunk = FALSE;

		PIMAGE_FIXUP_ENTRY pfe = (PIMAGE_FIXUP_ENTRY)((DWORD)Relocation + sizeof(IMAGE_BASE_RELOCATION));

		LdrPrint(("LDR: Relocs size %08x, diff=%08x\n", 
			OptHeader->DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC].Size,
			((ULONG)Relocation - (ULONG)IMAGE_FIRST_RELOCATION(ImageBase))
			));

		LdrPrint(("LDR: Processing relocation block %08x [va=%08x, size %08x]\n", ((ULONG)Relocation-(ULONG)ImageBase),
			Relocation->VirtualAddress, Relocation->SizeOfBlock));

		if (Relocation->SizeOfBlock > 0x10000)
			INT3

		for ( ULONG i = 0; i < (Relocation->SizeOfBlock - sizeof(IMAGE_BASE_RELOCATION))/2; i++, pfe++) 
		{
			if (pfe->Type == IMAGE_REL_BASED_HIGHLOW) 
			{
				ULONG dwPointerRva = Relocation->VirtualAddress + pfe->Offset;
				ULONG* Patch = (ULONG*)RVATOVA(dwPointerRva,ImageBase);

				if (*Patch >= OptHeader->ImageBase &&
					*Patch <= OptHeader->ImageBase + OptHeader->SizeOfImage)
				{
					*Patch += Delta;
				}
				else
				{
					LdrPrint(("LDR: Warn: Invalid relocation at offset %08x: %08x -> %08x\n", 
						pfe->Offset,
						*Patch,
						*Patch + Delta
						));
				}
			}
		}
		(*((ULONG*)&Relocation)) += Relocation->SizeOfBlock;
	}

	return TRUE;
}


MMSYSTEM_MODE MmSystemMode = NormalMode;

LOCKED_LIST MmExtenderList;

STATUS
KEAPI
MiCreateExtenderObject(
	IN PVOID ExtenderStart,
	IN PVOID ExtenderEnd,
	IN PVOID ExtenderEntry,
	IN PUNICODE_STRING ExtenderName,
	OUT PEXTENDER *ExtenderObject
	)
/*++
	Create EXTENDER object representing the extender being loaded into the system.
--*/
{
	STATUS Status;
	PEXTENDER Extender;

	Status = ObCreateObject (
		(PVOID*) &Extender,
		sizeof(EXTENDER),
		MmExtenderObjectType,
		ExtenderName,
		OB_OBJECT_OWNER_MM
		);

	if (SUCCESS(Status))
	{
		Extender->ExtenderStart = ExtenderStart;
		Extender->ExtenderEnd = ExtenderEnd;
		Extender->ExtenderEntry = (PEXTENDER_ENTRY) ExtenderEntry;

		Status = (Extender->ExtenderEntry) (Extender);

		if (!SUCCESS(Status))
		{
			ObpDeleteObject (Extender);
		}
		else
		{
			//
			// Insert EXTENDER object to the list MmExtenderListHead
			// Insert each callback to the appropriate global list.
			//

			InterlockedInsertTailList( &MmExtenderList, &Extender->ExtenderListEntry );

			if (Extender->CreateThread)
			{
				InterlockedInsertHeadList (&PsCreateThreadCallbackList, &Extender->CreateThread->InternalListEntry);
			}

			if (Extender->TerminateThread)
			{
				InterlockedInsertHeadList (&PsTerminateThreadCallbackList, &Extender->TerminateThread->InternalListEntry);
			}

			if (Extender->CreateProcess)
			{
				InterlockedInsertHeadList (&PsCreateProcessCallbackList, &Extender->CreateProcess->InternalListEntry);
			}

			if (Extender->TerminateProcess)
			{
				InterlockedInsertHeadList (&PsTerminateProcessCallbackList, &Extender->TerminateProcess->InternalListEntry);
			}

			if (Extender->BugcheckDispatcher)
			{
				InterlockedInsertHeadList (&KeBugcheckDispatcherCallbackList, &Extender->BugcheckDispatcher->InternalListEntry);
			}
		}
	}

	return Status;
}

PUNICODE_STRING
KEAPI
MiModuleNameFromDriverName(
	PUNICODE_STRING DriverName,
	PUNICODE_STRING ModuleName
	)
{
	*ModuleName = *DriverName;
	PWSTR pSlash = wcsrchr (ModuleName->Buffer, L'\\');
	if (pSlash)
	{
		ModuleName->Buffer = pSlash+1;
		ModuleName->Length -= (ModuleName->Buffer - DriverName->Buffer)*2;
		ModuleName->MaximumLength -= DriverName->Length - ModuleName->Length;
	}

	return ModuleName;
}

KESYSAPI
STATUS
KEAPI
MmLoadSystemImage(
	IN PUNICODE_STRING ImagePath,
	IN PUNICODE_STRING DriverName,
	IN PROCESSOR_MODE TargetMode,
	IN BOOLEAN Extender,
	OUT PVOID *ImageBase,
	OUT PVOID *ModuleObject,
	IN ULONG Flags
	)
/*++
	Attempt to load system image into the memory.

	if TargetMode == DriverMode, we're loading a non-critical driver
	if TargetMode == KernelMode && Extender==0 we're loading critical driver
	if TargetMode == KernelMode && Extender==1 we're loading an extender
--*/
{
	PFILE FileObject = 0;
	STATUS Status = STATUS_UNSUCCESSFUL;
	PIMAGE_FILE_HEADER FileHeader = 0;
	PIMAGE_OPTIONAL_HEADER OptHeader = 0;
	PIMAGE_SECTION_HEADER SectHeader = 0;
	PMMD Mmd = 0;
	IO_STATUS_BLOCK IoStatus = {0};
	PVOID Hdr = 0;
	ULONG AlignedImageSize = 0;
	PVOID Image = 0;

	if (!SUCCESS(SeAccessCheck (SE_1_LOAD_DRIVER,0)))
		return STATUS_ACCESS_DENIED;

  ASSERT (TargetMode < MaximumMode);
	ASSERT (TargetMode != UserMode);

#define RETURN(st)  { Status = (st); __leave; }

	if (Extender && MmSystemMode != UpdateMode)
	{
		return STATUS_PRIVILEGE_NOT_HELD;
	}

	__try
	{
		//
		// Open the file
		//

		LdrPrint (("LDR: Opening file\n"));

		Status = IoCreateFile(
			&FileObject,
			FILE_READ_DATA | FILE_NONCACHED,
			ImagePath,
			&IoStatus,
			FILE_OPEN_EXISTING, 
			0
			);
		if (!SUCCESS(Status))
			__leave;

		//
		// Allocate heap space for the header page
		//

		LdrPrint (("LDR: Allocating page\n"));

		//Hdr = ExAllocateHeap (TRUE, PAGE_SIZE);
		Hdr = MmAllocatePage();
		if (!Hdr)
			RETURN (STATUS_INSUFFICIENT_RESOURCES);

		//
		// Read header page
		//

		LdrPrint (("LDR: Reading page\n"));

		Status = IoReadFile (
			FileObject,
			Hdr,
			PAGE_SIZE,
			NULL,
			0,
			&IoStatus
			);
		if (!SUCCESS(Status))
		{
			LdrPrint(("LDR: FAILED %d\n", __LINE__));
			__leave;
		}

		ASSERT_EQUAL (IoStatus.Information, PAGE_SIZE);

		MiGetHeaders (Hdr, &FileHeader, &OptHeader, &SectHeader);
		AlignedImageSize = ALIGN_UP (OptHeader->SizeOfImage, PAGE_SIZE);

		//
		// Allocate physical pages enough to hold the whole image
		//

		LdrPrint (("LDR: Allocating phys pages\n"));

		Status = MmAllocatePhysicalPages (AlignedImageSize>>PAGE_SHIFT, &Mmd);
		if (!SUCCESS(Status))
		{
			LdrPrint(("LDR: FAILED %d\n", __LINE__));
			__leave;
		}

		//
		// Map physical pages
		//

		LdrPrint (("LDR: Mapping pages\n"));

		// BUGBUG: Last parameter should be TRUE (add to working set)
		Image = MmMapLockedPages (Mmd, TargetMode, TRUE, FALSE);
		if (Image == NULL)
			RETURN ( STATUS_INSUFFICIENT_RESOURCES );

		// Copy the first page
		memcpy (Image, Hdr, PAGE_SIZE);

		//ExFreeHeap (Hdr);
		MmFreePage (Hdr);
		Hdr = NULL;
		MiGetHeaders (Image, &FileHeader, &OptHeader, &SectHeader);

		LdrPrint(("LDR: Allocated space at %08x\n", Image));

		//
		// Go read sections from the file
		//

		LdrPrint (("LDR: Reading sections\n"));

		for (ULONG Section = 0; Section < FileHeader->NumberOfSections; Section++)
		{
			ULONG VaStart = SectHeader[Section].VirtualAddress;
			ULONG Size;

			if (Section == FileHeader->NumberOfSections-1)
			{
				Size = OptHeader->SizeOfImage - SectHeader[Section].VirtualAddress;
			}
			else
			{
				Size = SectHeader[Section+1].VirtualAddress - SectHeader[Section].VirtualAddress;
			}

			if (SectHeader[Section].Misc.VirtualSize > Size) {
				Size = SectHeader[Section].Misc.VirtualSize;
			}

#if LDR_TRACE_MODULE_LOADING
			char sname[9];
			strncpy (sname, (char*)SectHeader[Section].Name, 8);
			sname[8] = 0;

			LdrPrint (("LDR: Reading section %s: %08x, Raw %08x, size %08x\n", sname, VaStart, SectHeader[Section].PointerToRawData, Size));
#endif

			LARGE_INTEGER Offset = {0};
			Offset.LowPart = SectHeader[Section].PointerToRawData;
	
			PVOID Buffer = (PVOID) ((ULONG)Image + VaStart);

			Status = IoReadFile (
				FileObject,
				Buffer,
				Size,
				&Offset,
				0,
				&IoStatus
				);

			LdrPrint(("LDR: Read %08x bytes\n", IoStatus.Information));

			ASSERT_EQUAL (IoStatus.Information, Size);

			if (!SUCCESS(Status))
			{
				LdrPrint(("LDR: FAILED %d\n", __LINE__));
				__leave;
			}
		}

		//
		// Fixups
		//

		LdrPrint (("LDR: Relocating image\n"));

		Status = LdrRelocateImage (Image, OptHeader);
		if (!SUCCESS(Status))
		{
			LdrPrint(("LDR: FAILED %d\n", __LINE__));
			__leave;
		}

		//
		// Process imports
		//

		LdrPrint (("LDR: Resolving imports\n"));

		Status = LdrWalkImportDescriptor (Image, OptHeader, Flags);
		if (!SUCCESS(Status))
		{
			LdrPrint(("LDR: FAILED %d\n", __LINE__));
			__leave;
		}
		
		//
		// Add module to PsLoadedModuleList
		//

		UNICODE_STRING ModuleName;
		PLDR_MODULE Module = LdrAddModuleToLoadedList(
			MiModuleNameFromDriverName (ImagePath, &ModuleName),
			Image, 
			OptHeader->SizeOfImage);

		if (Extender) 
		{
			LdrPrint (("LDR: Creating extender object\n"));

			KeBugCheck (MEMORY_MANAGEMENT, __LINE__, STATUS_NOT_IMPLEMENTED, 0, 0);
		}
		else
		{
			PVOID DriverEntry = LdrGetProcedureAddressByName (Image, NULL);

			LdrPrint (("LDR: Creating driver object [DRVENTRY=%08x]\n", DriverEntry));

			Status = IopCreateDriverObject (
				Image,
				(PVOID)((ULONG)Image + OptHeader->SizeOfImage - 1),
				(TargetMode == KernelMode ? DRV_FLAGS_CRITICAL : 0) | Flags,
				(PDRIVER_ENTRY) DriverEntry,
				Module,
				DriverName,
				(PDRIVER*)ModuleObject
				);
		}

		if (!SUCCESS(Status))
		{
			LdrRemoveModuleFromLoadedList (Module);
		}

		LdrPrint (("LDR: Success (driver entry completed with status %lx)\n", Status));
	}
	__finally
	{
		if (!SUCCESS(Status))
		{
			if (Mmd)
				MmFreeMmd (Mmd);
		}
		else
		{
			*ImageBase = Image;
		}

		if (Hdr)
			//ExFreeHeap (Hdr);
			MmFreePage (Hdr);

		if (FileObject)
			IoCloseFile (FileObject);
	}

	return Status;
}
