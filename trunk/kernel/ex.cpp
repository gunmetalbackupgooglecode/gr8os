//
// FILE:		ex.cpp
// CREATED:		18-Feb-2008  by Great
// PART:        EX
// ABSTRACT:
//			Executive subsystem support
//

#include "common.h"


PVOID ExpHeapArea;
ULONG ExpHeapSize;

LOCK ExpKernelHeapLock;

#define KdPrint(x) KiDebugPrint x

VOID
KEAPI
ExInitSystem(
	)
/*++
	Initialize executive
--*/
{
	ExpHeapSize = PAGE_SIZE * 16;

	//
	// Map heap physical pages
	//

	MiMapPhysicalPages( MM_KERNEL_HEAP, MM_KERNEL_HEAP_PHYS, ExpHeapSize >> PAGE_SHIFT );
	ExpHeapArea = (PVOID) MM_KERNEL_HEAP;

	//
	// Initialize kernel heap
	//

	ExInitializeHeap ();
}


USHORT ExpHeapCookie;

LIST_ENTRY ExpFreeBlockList;
LIST_ENTRY ExpAllocatedBlockList;
LIST_ENTRY ExpMemoryOrderBlockList;

ULONG 
KEAPI 
ExpChecksumBlock (
	PEHEAP_BLOCK Block
	)
/*++
	Calculate checksum of the important fields of the block. Caller should store this checksum
	 in the leading and trailing magic bytes.
	Environment: heap lock held.
--*/
{
	return (ULONG)( (ULONG)Block->PaddingSize + (ULONG)Block->BlockType + (ULONG)Block->Size + (ULONG)Block->Cookie);
}

VOID
KEAPI 
ExpRecalculateChecksumBlock(
	PEHEAP_BLOCK Block
	)
/*++
	Calculate new block checksum and store it in the appropriate places
	Environment: heap lock held.
--*/
{
	Block->Magic1 = Block->Magic2 = ExpChecksumBlock (Block);
}

VOID
KEAPI 
ExInitializeHeap(
	)
/*++
	Initialize the heap
--*/
{
	EHEAP_BLOCK *Initial = (PEHEAP_BLOCK) ExpHeapArea;
	InitializeListHead (&ExpFreeBlockList);
	InitializeListHead (&ExpAllocatedBlockList);
	InitializeListHead (&ExpMemoryOrderBlockList);

	InsertTailList (&ExpFreeBlockList, &Initial->List);
	InsertTailList (&ExpMemoryOrderBlockList, &Initial->MemoryOrderList);

	Initial->BlockType = HEAP_BLOCK_FREE;
	Initial->Size = (ExpHeapSize - sizeof(EHEAP_BLOCK))/EX_HEAP_ALIGN;

	__asm {
		rdtsc
		mov [ExpHeapCookie], ax
	}

	Initial->Cookie = ExpHeapCookie;
	ExpRecalculateChecksumBlock (Initial);

	memset (&Initial->Data, FREED_PADDING, Initial->Size*EX_HEAP_ALIGN);
}

VOID
KEAPI 
ExpAcquireHeapLock(
	PBOOLEAN OldState
	)
{
	//BUGBUG:!!!!!

	//*OldState = KeAcquireLock (&ExpKernelHeapLock);
}

VOID
KEAPI 
ExpReleaseHeapLock(
	BOOLEAN OldState
	)
{
	//KeReleaseLock (&ExpKernelHeapLock);
	//KeReleaseIrqState (OldState);
}


VOID
KEAPI 
ExpCheckBlockValid(
	PEHEAP_BLOCK Block
	)
/*++
	Check if the specified block is valid.
	Block is considered valid if the following conditions are satisfied:
	 1) First, the block's cookie should equal to global heap cookie. This condition is required
	  to prevent malevolent heap overflow.
	 2) Second, the leading and trailing magic dwords should be checked for equality with each other
	  and with block checksum.
	  With these two checks attacker cannot overwrite block header with valid header, because heap
	   cookie changes at each heap creation.
	 3) Third condition that is checked: unused space in block should contain the pattern, which
	  have been written there when the block was being allocated. This is required to prevent 
	  unpremeditated heap overflow.
	 If these checks succeed, the block is reputed as valid.
	Environment: heap lock held.
--*/
{
	// Check cookie
	if (Block->Cookie != ExpHeapCookie)
	{
		// Raise error: Cookie mismatch
		KdPrint (("ExpCheckBlockValid : Cookie mismatch"));
		
		KeBugCheck (EX_KERNEL_HEAP_FAILURE,
					HEAP_BLOCK_VALIDITY_CHECK_FAILED,
					HEAP_VALIDATION_COOKIE_MISMATCH,
					(ULONG) Block,
					0);
	}

	// Check magics
	ULONG ValidMagic = ExpChecksumBlock (Block);
	if (Block->Magic1 != ValidMagic ||
		Block->Magic2 != ValidMagic)
	{
		// Raise error: block checksum is not valid.
		KdPrint (("ExpCheckBlockValid : Checksum mismatch\n"));

		KeBugCheck (EX_KERNEL_HEAP_FAILURE,
					HEAP_BLOCK_VALIDITY_CHECK_FAILED,
					HEAP_VALIDATION_CHECKSUM_MISMATCH,
					(ULONG) Block,
					0);
	}

	// Check padding presence
	if (Block->BlockType == HEAP_BLOCK_ALLOCATED)
	{
		UCHAR *ptr = (UCHAR*)&Block->Data + Block->Size*EX_HEAP_ALIGN - Block->PaddingSize;

		for (int i=0; i<Block->PaddingSize; i++)
			if (ptr[i] != ALLOCATED_PADDING)
			{
				// Raise error: padding pattern corrupted.
				KdPrint (("ExpCheckBlockValid : Damage after allocated block %08x [padding pattern not found at [0x%08x,%d]: %02x, expected %02x\n", 
					Block,
					ptr, i, ptr[i],
					ALLOCATED_PADDING ));

				KeBugCheck (EX_KERNEL_HEAP_FAILURE,
							HEAP_BLOCK_VALIDITY_CHECK_FAILED,
							HEAP_VALIDATION_OVERFLOW_DETECTED,
							(ULONG) Block,
							ptr[i] );
			}
	}
	else if(Block->BlockType == HEAP_BLOCK_FREE)
	{
		UCHAR *ptr = (UCHAR*)&Block->Data;

		for (ULONG i=0; i<Block->Size*EX_HEAP_ALIGN; i++)
			if (ptr[i] != FREED_PADDING)
			{
				// Raise error: padding pattern corrupted.
				KdPrint (("ExpCheckBlockValid : Damage after free block %08x [padding pattern not found at [0x%08x,%d]: %02x, expected %02x\n", 
					Block,
					ptr, i, ptr[i],
					FREED_PADDING ));

				KeBugCheck (EX_KERNEL_HEAP_FAILURE,
							HEAP_BLOCK_VALIDITY_CHECK_FAILED,
							HEAP_VALIDATION_NO_FREE_PATTERN,
							(ULONG) Block,
							ptr[i] );
			}
	}
}

#define EXP_WRITE_BLOCK_ALLOC_PADDING(BLOCK) \
	memset ((BLOCK)->Data + (BLOCK)->Size*EX_HEAP_ALIGN - (BLOCK)->PaddingSize, ALLOCATED_PADDING, (BLOCK)->PaddingSize)

#define EXP_WRITE_BLOCK_FREED_PADDING(BLOCK) \
	memset ((BLOCK)->Data, FREED_PADDING, (BLOCK)->Size*EX_HEAP_ALIGN)

PVOID
KEAPI
ExpAllocateHeapInBlock(
	PEHEAP_BLOCK Block,
	ULONG Size
	)
/*++
	Allocate memory in the specified block appropriately.
	Environment: heap lock held.
--*/
{
	ULONG nSize;
	ULONG Diff;

	nSize = ALIGN_UP (Size, EX_HEAP_ALIGN);
	nSize /= EX_HEAP_ALIGN;

	//        BEFORE                AFTER
	//
	//  [  BLOCK HEADER  ]    [  BLOCK HEADER  ]
	//  [   .. FREE ..   ]    [   .. FREE ..   ]
	//  [   .. FREE ..   ]    [   .. FREE ..   ]
	//  [   .. FREE ..   ]    [  BLOCK HEADER  ]
	//  [   .. FREE ..   ]    [   .. FREE ..   ]
	//  [   .. FREE ..   ]    [   .. FREE ..   ]
	//  [   .. FREE ..   ]    [   .. FREE ..   ]
	//
	//  Block->Size = 6
	//  Size = 2
	//  Diff = 4


	Diff = Block->Size - nSize;

	if (Diff <= SIZEOF_HEAP_BLOCK)
	{
		//
		// This block is enough only to satisfy the allocation. New block cannot be created.
		//

		Block->BlockType = HEAP_BLOCK_ALLOCATED;
		Block->PaddingSize = (UCHAR)( Block->Size * EX_HEAP_ALIGN - Size );
		ExpRecalculateChecksumBlock (Block);

		//memset ( Block->Data + Size, ALLOCATED_PADDING, Block->PaddingSize );
		EXP_WRITE_BLOCK_ALLOC_PADDING (Block);

		RemoveEntryList (&Block->List);
		InsertTailList (&ExpAllocatedBlockList, &Block->List);

		KdPrint ((" ~ Allocation satisfied (1): BlockUsed=0x%08x, BlockSize=%d, AllocSize=%d, PaddingSize=%d, *=0x%08x\n",
			Block,
			Block->Size,
			Size,
			Block->PaddingSize,
			&Block->Data));

		if (ExIsCurrentThreadGuarded())
		{
			ExpLogAllocation (Block);
		}

		return &Block->Data;
	}
	else if (Diff > SIZEOF_HEAP_BLOCK)
	{
		//
		// This block is enough to hold required number of bytes + next new block
		//

		Diff -= SIZEOF_HEAP_BLOCK;
		PEHEAP_BLOCK NextBlock = (PEHEAP_BLOCK) ( (UCHAR*)Block + sizeof(EHEAP_BLOCK) + nSize*EX_HEAP_ALIGN );

		Block->Size = nSize;
		Block->BlockType = HEAP_BLOCK_ALLOCATED;
		Block->PaddingSize = (UCHAR)(nSize*EX_HEAP_ALIGN - Size);
		ExpRecalculateChecksumBlock (Block);

		//memset ( Block->Data + Size, ALLOCATED_PADDING, Block->PaddingSize );
		EXP_WRITE_BLOCK_ALLOC_PADDING (Block);

		RemoveEntryList (&Block->List);
		InsertTailList (&ExpAllocatedBlockList, &Block->List);

		NextBlock->Size = Diff;
		NextBlock->Cookie = ExpHeapCookie;
		NextBlock->BlockType = HEAP_BLOCK_FREE;
		ExpRecalculateChecksumBlock (NextBlock);
		InsertTailList (&ExpFreeBlockList, &NextBlock->List);
		// NextBlock is located into the place holded by free block, so it should also contain FREE_PADDING signature
		
		// Insert new block in the memory order list after Block.
		InsertHeadList (&Block->MemoryOrderList, &NextBlock->MemoryOrderList);

		KdPrint ((" ~ Allocation satisfied (2): BlockUsed=0x%08x, BlockSize=%d, AllocSize=%d, PaddingSize=%d, *=0x%08x\n",
			Block,
			Block->Size,
			Size,
			Block->PaddingSize,
			&Block->Data));

		if (ExIsCurrentThreadGuarded())
		{
			ExpLogAllocation (Block);
		}

		return &Block->Data;
	}

	return NULL;
}

PEHEAP_BLOCK
KEAPI
ExpFindIdealBlock(
	ULONG Size
	)
/*++
	Find ideal block for the new allocation of the specified size.
	Function makes a choice between all free blocks of the size greater or equal requested.
	Block with minimal size is returned.
	Environment: heap lock held
--*/
{
	PEHEAP_BLOCK Block;
	PEHEAP_BLOCK IdealBlock = NULL;
	ULONG nSize;

	Block = CONTAINING_RECORD (ExpFreeBlockList.Flink, EHEAP_BLOCK, List);
	nSize = ALIGN_UP (Size, EX_HEAP_ALIGN);
	nSize /= EX_HEAP_ALIGN;

	// Find the best block for this allocation request.
	do
	{
		if ((PLIST_ENTRY)&Block->List == &ExpFreeBlockList)
			break;

		ExpCheckBlockValid (Block);

		if (Block->Size >= nSize)
		{
			if (!IdealBlock || Block->Size < IdealBlock->Size)
				IdealBlock = Block;
		}

		Block = CONTAINING_RECORD (Block->List.Flink, EHEAP_BLOCK, List);
	}
	while (TRUE);

	return IdealBlock;
}

KESYSAPI
PVOID
KEAPI 
ExAllocateHeap(
	BOOLEAN Paged,
	ULONG Size
	)
/*++
	Allocate memory from the heap. 'Paged' argument is not supported yet.
	The following steps are performed:
	  For each free blocks  the check  of its size  is performed. If its size equals to allocation
	   size, use this block immediately. 
	   If its size is greater than the size requested, split this block into two blocks and return
	   the appropriate block. Second block is left unallocated.
	   Generally, it is enough to allocate space. However, if often allocations and freeing small
	    block take  place heap will  become fragemented  and the  following situation can occur:
		The sum of free space is enough to satisfy the allocation, but blocks placed too far one
		from another. So, the allocation wouldn't be satisfied. To prevent it, caller should use
		another type  of  the  heap -  autodefragmenting  heap,  which should be used when often 
		allocations of small amount of bytes are needed.
--*/
{
	BOOLEAN OldState;
	PEHEAP_BLOCK IdealBlock = NULL;
	PVOID Ptr = NULL;

	ExpAcquireHeapLock (&OldState);

	IdealBlock = ExpFindIdealBlock (Size);

	if (!IdealBlock)
	{
		goto _not_allocated;
	}

	Ptr = ExpAllocateHeapInBlock (IdealBlock, Size);

_not_allocated:
	ExpReleaseHeapLock (OldState);
	return Ptr;
}

VOID
KEAPI 
ExpRebuildFreeBlocks(
	PEHEAP_BLOCK Block
	)
/*++
	Perform free blocks reorganization in the following way:
	  Merge free block with the next block if it also is free.
	  Merge free block with the previous block if it also is free.
	  Peforms these steps recursively for the previous and next blocks.
	Supplied argument is the block being freed.
	Heap should remain locked when this routine is called.

	Environment: heap lock held.
--*/
{
	// Check that block is really valid
	ExpCheckBlockValid (Block);

	// Walk down
	PEHEAP_BLOCK PrevBlock = Block;
	PEHEAP_BLOCK CurrBlock = CONTAINING_RECORD (Block->MemoryOrderList.Flink, EHEAP_BLOCK, MemoryOrderList);

	do
	{
		if (CurrBlock->BlockType != HEAP_BLOCK_FREE)
			break;

		//
		// Merge two blocks
		//

		KdPrint ((" ~ Merged two free blocks [->] 0x%08x [size=%d] + 0x%08x [size=%d]\n",
			PrevBlock,
			PrevBlock->Size,
			CurrBlock,
			CurrBlock->Size));
 
		PrevBlock->Size += CurrBlock->Size + SIZEOF_HEAP_BLOCK;
		ExpRecalculateChecksumBlock (PrevBlock);
		
		RemoveEntryList (&CurrBlock->List);
		RemoveEntryList (&CurrBlock->MemoryOrderList);
		
		memset (CurrBlock, FREED_PADDING, sizeof(EHEAP_BLOCK));

		if (ExIsCurrentThreadGuarded())
		{
			ExpLogFreeing (CurrBlock);
		}

		// Go down
		CurrBlock = CONTAINING_RECORD (PrevBlock->MemoryOrderList.Flink, EHEAP_BLOCK, MemoryOrderList);
	}
	while ( &CurrBlock->MemoryOrderList != &ExpMemoryOrderBlockList );

	PrevBlock = Block;
	CurrBlock = CONTAINING_RECORD (Block->MemoryOrderList.Blink, EHEAP_BLOCK, MemoryOrderList);

	do
	{
		if (CurrBlock->BlockType != HEAP_BLOCK_FREE)
			break;

		//
		// Merge two blocks
		//

		KdPrint ((" ~ Merged two free blocks [<-] 0x%08x [size=%d] + 0x%08x [size=%d]\n",
			CurrBlock,
			CurrBlock->Size,
			PrevBlock,
			PrevBlock->Size));

		CurrBlock->Size += PrevBlock->Size + SIZEOF_HEAP_BLOCK;
		ExpRecalculateChecksumBlock (CurrBlock);
		
		RemoveEntryList (&PrevBlock->List);
		RemoveEntryList (&PrevBlock->MemoryOrderList);
		
		memset (PrevBlock, FREED_PADDING, sizeof(EHEAP_BLOCK));

		if (ExIsCurrentThreadGuarded())
		{
			ExpLogFreeing (PrevBlock);
		}

		// Go up
		CurrBlock = CONTAINING_RECORD (CurrBlock->MemoryOrderList.Blink, EHEAP_BLOCK, MemoryOrderList);
	}
	while ( &CurrBlock->MemoryOrderList != &ExpMemoryOrderBlockList );
}

VOID
KEAPI
ExpFreeHeapBlock(
	PEHEAP_BLOCK Block
	)
/*++
	Free the specified block directly.

	Environment: heap lock held.
--*/
{
	KdPrint ((" ~ Freeing block 0x%08x, BlockSize=%d, PaddingSize=%d, *=0x%08x\n",
		Block,
		Block->Size,
		Block->PaddingSize,
		&Block->Data));

	if (ExIsCurrentThreadGuarded())
	{
		ExpLogFreeing (Block);
	}

	Block->BlockType = HEAP_BLOCK_FREE;
	RemoveEntryList (&Block->List);
	InsertTailList (&ExpFreeBlockList, &Block->List);
	ExpRecalculateChecksumBlock (Block);

	//memset (&Block->Data, FREED_PADDING, Block->Size * EX_HEAP_ALIGN);
	EXP_WRITE_BLOCK_FREED_PADDING (Block);

	ExpRebuildFreeBlocks (Block);
}

KESYSAPI
VOID
KEAPI 
ExFreeHeap(
	PVOID Ptr
	)
/*++
	Free block. Also reorganize blocks via ExpRebuildFreeBlocks()
--*/
{
	BOOLEAN OldState;
	ExpAcquireHeapLock (&OldState);

	PEHEAP_BLOCK Block = CONTAINING_RECORD (Ptr, EHEAP_BLOCK, Data);

	if (Block->BlockType == HEAP_BLOCK_LOCKED)
	{
		KeBugCheck (EX_KERNEL_HEAP_FAILURE,
					HEAP_LOCKED_BLOCK_FREEING,
					FALSE,
					(ULONG) Block,
					0);
	}
	else if (Block->BlockType == HEAP_BLOCK_FREE || Block->BlockType == FREED_PADDING)
	{
		KeBugCheck (EX_KERNEL_HEAP_FAILURE,
					HEAP_DOUBLE_FREEING,
					0,
					(ULONG) Block,
					0);
	}

	ExpCheckBlockValid (Block);

	//
	// Now we're ready to free the block.
	//
	ExpFreeHeapBlock (Block);

	ExpReleaseHeapLock (OldState);
}

void DumpMemory( DWORD base, ULONG length, DWORD DisplayBase )
{
#define ptc(x) KiDebugPrint("%c", x)
#define is_print(c) ( (c) >= '!' && (c) <= '~' )
	bool left = true;
	bool newline = true;
	int based_length = length;
	int i;
	int baseoffs;

	if( DisplayBase==-1 ) DisplayBase = base;

	baseoffs = DisplayBase - (DisplayBase&0xFFFFFFF0);
	base -= baseoffs;
	DisplayBase &= 0xFFFFFFF0;

	length += baseoffs;

	if( length % 16 )
		based_length += 16-(length%16);

	for( i=0; i<based_length; i++ )
	{
		if( newline )
		{
			newline = false;
			KiDebugPrint("%08x [", DisplayBase+i);
		}

#define b (*((unsigned char*)base+i))

		if( left )
		{
			if( (unsigned)i < length && i >= baseoffs )
				KiDebugPrint(" %02x", b);
			else
				KiDebugPrint("   ");

			if( (i+1) % 16 == 0 )
			{
				left = false;
				i -= 16;
				KiDebugPrint(" ] ");
				continue;
			}

			if( (i+1) % 8 == 0 )
			{
				KiDebugPrint(" ");
			}

		}
		else
		{
			if( (unsigned)i < length && i >= baseoffs )
				ptc(is_print(b)?b:'.');
			else
				ptc(' ');

			if( (i+1) % 16 == 0 )
			{
				KiDebugPrint("\n");
				newline = true;
				left = true;
				continue;
			}
			if( (i+1) % 8 == 0 )
			{
				KiDebugPrint(" ");
			}
		}
	}
}

VOID
KEAPI
ExpDumpBlockData(
	PEHEAP_BLOCK Block
	)
/*++
	This function dumps data of the specified block.
	Environment: heap lock held
--*/
{
	KiDebugPrint(" ~ Dumping block %08x data [Size=%d, Padding=%d]\n", Block, Block->Size, Block->PaddingSize);
	DumpMemory( (ULONG)Block->Data, Block->Size*EX_HEAP_ALIGN-Block->PaddingSize, -1);

	KiDebugPrint("Block %08x padding\n", Block);
	DumpMemory( (ULONG)Block->Data + Block->Size*EX_HEAP_ALIGN-Block->PaddingSize, Block->PaddingSize, -1);

	KiDebugPrint("\n");
}

KESYSAPI
PVOID
KEAPI
ExReallocHeap(
	PVOID Ptr,
	ULONG NewSize
	)
/*++
	Reallocate allocated block in the following way:
	 1,2 Check if new size is smaller than current. If so, reduce the block padding or 
		split blocks, or merge with next block if it is free.
		Or if current block can satisfy reallocation request immediately 
	    (padding space is enough to complete request), also satisfy it and return.
	 3. If no, check if next block in memory order is free and its size is enough to 
		complete request. If so, satisfy the allocation.
	 4. If no, find any new block, which can satisfy the allocation
--*/
{
	BOOLEAN OldState;
	ExpAcquireHeapLock (&OldState);

	PEHEAP_BLOCK Block = CONTAINING_RECORD (Ptr, EHEAP_BLOCK, Data);

	if (Block->BlockType == HEAP_BLOCK_FREE)
	{
		KeBugCheck (EX_KERNEL_HEAP_FAILURE,
					HEAP_FREE_BLOCK_REALLOCATING,
					0,
					(ULONG) Block,
					0);
	}
	else if (Block->BlockType == HEAP_BLOCK_LOCKED)
	{
		KeBugCheck (EX_KERNEL_HEAP_FAILURE,
					HEAP_LOCKED_BLOCK_FREEING,
					TRUE,
					(ULONG) Block,
					0);
	}

	ExpCheckBlockValid (Block);

	// New units
	ULONG nSize = ALIGN_UP(NewSize, EX_HEAP_ALIGN) / EX_HEAP_ALIGN;

	//
	// 1,2. Check if current block can satisfy the allocation.
	// Though, we can't reduce the block, because not enough space is being freed.
	//

	if (nSize <= Block->Size &&
		Block->Size - nSize < SIZEOF_HEAP_BLOCK)
	{
		//
		// New size differs from current size with the value, smaller than heap alignment.
		// The only thing we're going to do is to change PaddingSize properly
		//

		Block->PaddingSize = (UCHAR)(Block->Size*EX_HEAP_ALIGN - NewSize);
		ExpRecalculateChecksumBlock (Block);

		//memset ( Block->Data + Block->Size*EX_HEAP_ALIGN - Block->PaddingSize, ALLOCATED_PADDING, Block->PaddingSize );
		EXP_WRITE_BLOCK_ALLOC_PADDING (Block);

		KdPrint ((" ~ Reallocation <- satisfied in current block for 0x%08x [*=0x%08x]. Padding reduced\n", Block, &Block->Data));

		ExpReleaseHeapLock (OldState);
		return &Block->Data;
	}
	
	//
	// Check if we should reduce the block and split into two blocks.
	//

	if (nSize <= Block->Size &&
		Block->Size - nSize >= SIZEOF_HEAP_BLOCK)
	{
		// Check, if next block is free, we can 
		//  move this free space to it

		PEHEAP_BLOCK NextBlock = CONTAINING_RECORD (Block->MemoryOrderList.Flink, EHEAP_BLOCK, MemoryOrderList);

		if (NextBlock->BlockType == HEAP_BLOCK_FREE)
		{
			//
			// Next block is free. Move it backward
			//

			NextBlock->Size += Block->Size - nSize;
			
			// Correct list pointers
			*(ULONG*)&NextBlock->List.Blink->Flink -= (Block->Size - nSize)*EX_HEAP_ALIGN;
			*(ULONG*)&NextBlock->List.Flink->Blink -= (Block->Size - nSize)*EX_HEAP_ALIGN;
			*(ULONG*)&NextBlock->MemoryOrderList.Blink->Flink -= (Block->Size - nSize)*EX_HEAP_ALIGN;
			*(ULONG*)&NextBlock->MemoryOrderList.Flink->Blink -= (Block->Size - nSize)*EX_HEAP_ALIGN;

			// Move block header
			//BUGBUG
			memcpy ( (UCHAR*)NextBlock - (Block->Size - nSize)*EX_HEAP_ALIGN, NextBlock, sizeof(EHEAP_BLOCK));
			memset ( NextBlock, FREED_PADDING, sizeof(EHEAP_BLOCK) );

			// Change pointer
			*(ULONG*)&NextBlock -= (Block->Size - nSize)*EX_HEAP_ALIGN;

			// Fill new free with the appropriate pattern
			memset (NextBlock->Data, FREED_PADDING, (Block->Size - nSize)*EX_HEAP_ALIGN);

			// Reduce our block
			Block->Size = nSize;
			Block->PaddingSize = (UCHAR)(Block->Size*EX_HEAP_ALIGN - NewSize);

			//memset (Block->Data + NewSize, ALLOCATED_PADDING, Block->PaddingSize);
			EXP_WRITE_BLOCK_ALLOC_PADDING (Block);

			ExpRecalculateChecksumBlock (Block);
			ExpRecalculateChecksumBlock (NextBlock);

			KdPrint ((" ~ Reallocation <- satisfied in current block for 0x%08x [*=0x%08x]. Next free block grown up\n", Block, &Block->Data));

			//DEBUG
//			ExpCheckBlockValid (NextBlock);
//			ExpCheckBlockValid (Block);
	
			ExpReleaseHeapLock (OldState);
			return &Block->Data;
		}

		// If not, we can split blocks only if  (Block->Size - nSize > SIZEOF_HEAP_BLOCK)
		if (Block->Size - nSize == SIZEOF_HEAP_BLOCK)
		{
			//
			// We cannot split blocks (not enough space), and next block 
			//  is not free. Reduce size of current block..
			//

			//
			// Notice: we are reducing size of current block, and next block is not free and the difference
			//  between new size and current size is not enough to allocate new block.
			// So, we are going to leave this space as padding. Notice, that PaddingSize will be
			//  greater or equal than 16.
			//

			Block->PaddingSize += (UCHAR)(NewSize - nSize*EX_HEAP_ALIGN);
			ExpRecalculateChecksumBlock (Block);

			//memset ( Block->Data + Block->Size*EX_HEAP_ALIGN - Block->PaddingSize, ALLOCATED_PADDING, Block->PaddingSize );
			EXP_WRITE_BLOCK_ALLOC_PADDING (Block);

			//DEBUG
			ExpCheckBlockValid (Block);

			ExpReleaseHeapLock (OldState);
			return &Block->Data;
		}

		// Next block is not free. But space in current block is enough to split blocks. Do it
		
//		ExpDumpBlockData (Block);

		NextBlock = (PEHEAP_BLOCK)( Block->Data + nSize*EX_HEAP_ALIGN );
		NextBlock->Size = Block->Size - nSize - SIZEOF_HEAP_BLOCK;
		NextBlock->Cookie = ExpHeapCookie;
		NextBlock->BlockType = HEAP_BLOCK_FREE;
		ExpRecalculateChecksumBlock (NextBlock);
		
		InsertHeadList (&Block->MemoryOrderList, &NextBlock->MemoryOrderList);
		InsertTailList (&ExpFreeBlockList, &NextBlock->List);

		Block->Size = nSize;
		Block->PaddingSize = (UCHAR)(Block->Size*EX_HEAP_ALIGN - NewSize);

		EXP_WRITE_BLOCK_ALLOC_PADDING (Block);

		ExpRecalculateChecksumBlock (Block);

		EXP_WRITE_BLOCK_FREED_PADDING (NextBlock);

		KdPrint ((" ~ Reallocation <- satisfied in current block for 0x%08x [*=0x%08x]. Block splitted\n", Block, &Block->Data));

//		ExpDumpBlockData (Block);

		//DEBUG
//		ExpCheckBlockValid (NextBlock);
//		ExpCheckBlockValid (Block);


		ExpReleaseHeapLock (OldState);
		return &Block->Data;
	}

	//
	// Requested size is larger than current. Check if the next block is free. If so,
	//  we can satisfy the reallocation by reducing next free block.
	//

	PEHEAP_BLOCK NextBlock = CONTAINING_RECORD (Block->MemoryOrderList.Flink, EHEAP_BLOCK, MemoryOrderList);
	if (NextBlock->BlockType == HEAP_BLOCK_FREE &&
		NextBlock->Size + Block->Size  > nSize + SIZEOF_HEAP_BLOCK)
	{
		// Correct list pointers
		*(ULONG*)&NextBlock->List.Blink->Flink += (nSize - Block->Size)*EX_HEAP_ALIGN;
		*(ULONG*)&NextBlock->List.Flink->Blink += (nSize - Block->Size)*EX_HEAP_ALIGN;
		*(ULONG*)&NextBlock->MemoryOrderList.Blink->Flink += (nSize - Block->Size)*EX_HEAP_ALIGN;
		*(ULONG*)&NextBlock->MemoryOrderList.Flink->Blink += (nSize - Block->Size)*EX_HEAP_ALIGN;

		// Move header

		memmove ( (UCHAR*)NextBlock + (nSize - Block->Size)*EX_HEAP_ALIGN, NextBlock, sizeof(EHEAP_BLOCK));
		memset ( NextBlock, FREED_PADDING, (nSize - Block->Size) == 1 ? (sizeof(EHEAP_BLOCK)/2) : sizeof(EHEAP_BLOCK) );

		// Change pointer
		*(ULONG*)&NextBlock += (nSize - Block->Size)*EX_HEAP_ALIGN;

		NextBlock->Size -= nSize - Block->Size;
		Block->Size = nSize;
		Block->PaddingSize = (UCHAR)(nSize*EX_HEAP_ALIGN - NewSize);

		ExpRecalculateChecksumBlock (Block);
		ExpRecalculateChecksumBlock (NextBlock);

		EXP_WRITE_BLOCK_ALLOC_PADDING (Block);

		KdPrint ((" ~ Reallocation -> satisfied in current block for 0x%08x [*=0x%08x]. Next free block reduced\n", Block, &Block->Data));

		ExpReleaseHeapLock (OldState);
		return &Block->Data;
	}

	// Next block is not free os is too small to satisfy the allocation.
	// Memory cannot be reallocated at place. Find new ideal block for this request

	PEHEAP_BLOCK IdealBlock = ExpFindIdealBlock (NewSize);

	if (IdealBlock)
	{
		//
		// Found ideal block for this allocation.
		// Move memory there.
		//
		
		PVOID NewPtr = ExpAllocateHeapInBlock (IdealBlock, NewSize);
		memcpy (NewPtr, Ptr, Block->Size*EX_HEAP_ALIGN - Block->PaddingSize);

		if (ExIsCurrentThreadGuarded())
		{
			ExpLogRellocation (Block, IdealBlock);
		}

		KdPrint ((" ~ Reallocation -> satisfied in new block 0x%08x for old 0x%08x [*=0x%08x,0x%08x]\n",
			CONTAINING_RECORD (NewPtr, EHEAP_BLOCK, Data),
			Block, 
			NewPtr,
			&Block->Data));

		ExpFreeHeapBlock (Block);

		ExpReleaseHeapLock (OldState);
		return NewPtr;	
	}

	ExpReleaseHeapLock (OldState);
	return 0;
}

KESYSAPI
VOID
KEAPI
ExLockHeapBlock(
	PVOID Ptr
	)
/*++
	Lock allocated block, locked block cannot be freed until it is unlocked
--*/
{
	BOOLEAN OldState;
	ExpAcquireHeapLock (&OldState);

	PEHEAP_BLOCK Block = CONTAINING_RECORD (Ptr, EHEAP_BLOCK, Data);

	if (Block->BlockType == HEAP_BLOCK_LOCKED)
	{
		KeBugCheck (EX_KERNEL_HEAP_FAILURE,
					HEAP_DOUBLE_LOCKING,
					0,
					(ULONG) Block,
					0);
	}
	else if (Block->BlockType == HEAP_BLOCK_FREE)
	{
		KeBugCheck (EX_KERNEL_HEAP_FAILURE,
					HEAP_FREE_BLOCK_LOCKING,
					0,
					(ULONG) Block,
					0);
	}

	ExpCheckBlockValid (Block);

	Block->BlockType = HEAP_BLOCK_LOCKED;
	ExpRecalculateChecksumBlock (Block);

	ExpReleaseHeapLock (OldState);
}

KESYSAPI
VOID
KEAPI
ExUnlockHeapBlock(
	PVOID Ptr
	)
/*++
	Unlock heap block, so it can be freed
--*/
{
	BOOLEAN OldState;
	ExpAcquireHeapLock (&OldState);

	PEHEAP_BLOCK Block = CONTAINING_RECORD (Ptr, EHEAP_BLOCK, Data);

	if (Block->BlockType != HEAP_BLOCK_LOCKED)
	{
		KeBugCheck (EX_KERNEL_HEAP_FAILURE,
					HEAP_NOTLOCKED_BLOCK_UNLOCKING,
					Block->BlockType,
					(ULONG) Block,
					0);
	}

	ExpCheckBlockValid (Block);

	Block->BlockType = HEAP_BLOCK_ALLOCATED;
	ExpRecalculateChecksumBlock (Block);

	ExpReleaseHeapLock (OldState);
}

VOID
KEAPI
ExpDumpHeap(
	)
/*++
	Dump the whole heap.

	Environment: heap lock held.
--*/
{
	PEHEAP_BLOCK Block = CONTAINING_RECORD (ExpMemoryOrderBlockList.Flink, EHEAP_BLOCK, MemoryOrderList);

	KdPrint (("\n"));

	do
	{
		ExpCheckBlockValid (Block);

		char tbuff[7];
		tbuff[6] = 0;
		strncpy (tbuff, (char*)Block->Data, 6);

		char *BlockTypes[] = {
			"Free",
			"Allocated",
			"Locked"
		};

		KdPrint (("Block 0x%08x [Data=0x%08x] Type=%s  Size=0x%x Padding=%d    [%s]\n",
			Block,
			Block->Data,
			BlockTypes[Block->BlockType-1],
			Block->Size,
			Block->PaddingSize,
			tbuff));

		Block = CONTAINING_RECORD (Block->MemoryOrderList.Flink, EHEAP_BLOCK, MemoryOrderList);
	}
	while ( &Block->MemoryOrderList != &ExpMemoryOrderBlockList );

	KdPrint (("\n"));
}


//BOOLEAN InExGuardedRegion;
//PEHEAP_GUARD_TABLE ExGuardTable;

VOID
KEAPI
ExpLogAllocation(
	PEHEAP_BLOCK BlockBeingAllocated
	)
/*++
	Writes allocation to the guard table
--*/
{
	PEHEAP_GUARD_TABLE GuardTable = ExCurrentThreadGuardTable();

	for (ULONG i=0; i<GuardTable->TableSize; i++)
	{
		if (GuardTable->Table[i] == NULL)
		{
			GuardTable->Table[i] = BlockBeingAllocated;
			return;
		}
	}
	
	// No free entry in the table, reallocate it
	KeBugCheck (0,0,0,0,0);
}

VOID
KEAPI
ExpLogFreeing(
	PEHEAP_BLOCK BlockBeingFreed
	)
/*++
	Removes allocation from the guard table
--*/
{
	PEHEAP_GUARD_TABLE GuardTable = ExCurrentThreadGuardTable();

	for (ULONG i=0; i<GuardTable->TableSize; i++)
	{
		if (GuardTable->Table[i] == BlockBeingFreed)
		{
			GuardTable->Table[i] = 0;
			return;
		}
	}
}

VOID
KEAPI
ExpLogRellocation(
	PEHEAP_BLOCK BlockBeingFreed,
	PEHEAP_BLOCK BlockBeingAllocated
	)
/*++
	Log reallocation request. Replace block pointers
--*/
{
	PEHEAP_GUARD_TABLE GuardTable = ExCurrentThreadGuardTable();

	for (ULONG i=0; i<GuardTable->TableSize; i++)
	{
		if (GuardTable->Table[i] == BlockBeingFreed)
		{
			GuardTable->Table[i] = BlockBeingAllocated;
			return;
		}
	}
}

KESYSAPI
VOID
KEAPI
ExEnterHeapGuardedRegion(
	)
/*++
	Enters heap guarded region. Guard table is created and all allocations will be written there.
	According call ExLeaveHeapGuardedRegion() will detect all memory leaks
--*/
{
	PEHEAP_GUARD_TABLE GuardTable;

	if (ExIsCurrentThreadGuarded())
	{
		KeBugCheck (EX_KERNEL_HEAP_FAILURE,
					HEAP_GUARD_FAILURE,
					HEAP_GUARD_ALREADY_GUARDED,
					(ULONG) ExCurrentThreadGuardTable(),
					0);
	}

	GuardTable = (PEHEAP_GUARD_TABLE) ExAllocateHeap (FALSE, sizeof(EHEAP_GUARD_TABLE));
	GuardTable->TableSize = EX_GUARD_TABLE_INITIAL_SIZE;
	GuardTable->Table = (PEHEAP_BLOCK*) ExAllocateHeap (FALSE, sizeof(PEHEAP_BLOCK)*EX_GUARD_TABLE_INITIAL_SIZE);

	memset (GuardTable->Table, 0, sizeof(PEHEAP_BLOCK)*EX_GUARD_TABLE_INITIAL_SIZE);

	ExIsCurrentThreadGuarded() = TRUE;
	ExCurrentThreadGuardTable() = GuardTable;
}
	
KESYSAPI
VOID
KEAPI
ExLeaveHeapGuardedRegion(
	)
/*++
	Leaves heap guarded region. Guard table is checked for unfreed memory and all memory leaks
	 will be detected.
--*/
{
	if (!ExIsCurrentThreadGuarded())
	{
		KeBugCheck (EX_KERNEL_HEAP_FAILURE,
					HEAP_GUARD_FAILURE,
					HEAP_GUARD_NOT_GUARDED,
					0,
					0);
	}

	ExIsCurrentThreadGuarded() = FALSE;
	PEHEAP_GUARD_TABLE GuardTable = ExCurrentThreadGuardTable();

	//
	// Perform checks
	//

	for (ULONG i=0; i<GuardTable->TableSize; i++)
	{
		if (GuardTable->Table[i] != NULL)
		{
			KeBugCheck (EX_KERNEL_HEAP_FAILURE,
						HEAP_GUARD_FAILURE,
						HEAP_GUARD_LEAK_DETECTED,
						(ULONG) GuardTable->Table[i],
						0
						);
		}
	}

	ExFreeHeap (GuardTable->Table);
	ExFreeHeap (GuardTable);

	ExCurrentThreadGuardTable() = NULL;
}