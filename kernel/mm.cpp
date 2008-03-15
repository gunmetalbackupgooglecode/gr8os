//
// FILE:		mm.cpp
// CREATED:		17-Feb-2008  by Great
// PART:        MM
// ABSTRACT:
//			Memory Management routines
//

#include "common.h"

VOID
KEAPI
MiMapPhysicalPages(
	PVOID VirtualAddress,
	ULONG PhysicalAddress,
	ULONG PageCount
	)
/*++
	Map physical pages to kernel virtual address space
--*/
{
	PMMPTE PointerPte = MiGetPteAddress (VirtualAddress);

	for( ULONG i=0; i<PageCount; i++ )
	{
		MiWriteValidKernelPte (PointerPte);
		PointerPte->PageFrameNumber = (PhysicalAddress >> PAGE_SHIFT) + i;

		MmInvalidateTlb (VirtualAddress);

		PointerPte = MiNextPte (PointerPte);
		*(ULONG*)&VirtualAddress += PAGE_SIZE;
	}
}


VOID
KEAPI
MiUnmapPhysicalPages(
	PVOID VirtualAddress,
	ULONG PageCount
	)
/*++
	Unmap physical pages from the kernel virtual address space
--*/
{
	PMMPTE PointerPte = MiGetPteAddress (VirtualAddress);

	KeZeroMemory( PointerPte, PageCount*sizeof(MMPTE) );
}

char MmDebugBuffer[1024];

VOID
KEAPI
MmAccessFault(
	PVOID VirtualAddress,
	PVOID FaultingAddress
	)
/*++
	Resolve access fault
--*/
{
	sprintf( MmDebugBuffer, "MM: Page fault at %08x, referring code: %08x\n", VirtualAddress, FaultingAddress );
	KiDebugPrintRaw( MmDebugBuffer );

	PMMPTE PointerPte = MiGetPteAddress (VirtualAddress);

	switch (PointerPte->PteType)
	{
	case PTE_TYPE_NORMAL_OR_NOTMAPPED:
		{
			sprintf( MmDebugBuffer, "MM: Access violation in %08x [invalid page]\n", VirtualAddress );
			KiDebugPrintRaw( MmDebugBuffer );

			__asm lea esi, [MmDebugBuffer]
			__asm xor ax, ax
			__asm int 0x30

			break;
		}

	case PTE_TYPE_PAGEDOUT:
		{
			sprintf( MmDebugBuffer, "MM: Access violation in %08x [paging not supported]\n", VirtualAddress );
			KiDebugPrintRaw( MmDebugBuffer );

			__asm lea esi, [MmDebugBuffer]
			__asm xor ax, ax
			__asm int 0x30

			break;
		}

	case PTE_TYPE_TRIMMED:
		{
			sprintf( MmDebugBuffer, "MM: Access violation in %08x [trimming not supported]\n", VirtualAddress );
			KiDebugPrintRaw( MmDebugBuffer );

			__asm lea esi, [MmDebugBuffer]
			__asm xor ax, ax
			__asm int 0x30

			break;
		}

	case PTE_TYPE_VIEW:
		{
			sprintf( MmDebugBuffer, "MM: Access violation in %08x [views not supported]\n", VirtualAddress );
			KiDebugPrintRaw( MmDebugBuffer );

			__asm lea esi, [MmDebugBuffer]
			__asm xor ax, ax
			__asm int 0x30

			break;
		}
	}

	INT3
	KiStopExecution( );
}


VOID
KEAPI
MmCreateAddressSpace(
	PPROCESS Process
	)
/*++
	Creates process' address space
--*/
{
	KiDebugPrintRaw( "MM: MmCreateAddressSpace unimplemented\n" );
}


KESYSAPI
BOOLEAN
KEAPI
MmIsAddressValid(
	IN PVOID VirtualAddress
	)
/*++
	This function checks that specified virtual address is valid
--*/
{
	PMMPTE PointerPte = MiGetPteAddress (VirtualAddress);
	return PointerPte->Valid;
}


VOID
KEAPI
MiZeroPageThread(
	)
{
	for(;;)
	{
	}
}


VOID
KEAPI
MmInitSystem(
	)
{

}