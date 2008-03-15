//
// FILE:		except.cpp
// CREATED:		28-Feb-2008  by Great
// PART:        KE
// ABSTRACT:
//			Exception dispather
//

#include "common.h"

KESYSAPI
VOID
KEAPI
KeDispatchException(
	ULONG ExceptionCode,
	ULONG NumberParameters,
	ULONG Parameter1,
	ULONG Parameter2,
	ULONG Parameter3,
	ULONG Parameter4,
	CONTEXT_FRAME *ContextFrame
	)
/*++
	Common exception dispatcher
--*/
{
	PTHREAD Thread = PsGetCurrentThread ();
	PPCB Pcb = PsGetCurrentPcb ();

	KiDebugPrint ("KE: Exception dispatcher for th=%08x, code %08x\n", Thread, ExceptionCode);

	if( Pcb->CurrentException )
	{
		//
		// Nested exception. Terminate current thread immediately
		//

		KiDebugPrint ("KE: Nested excpetions for th=%08x\n", Thread);

		PsExitThread (ExceptionCode);
	}
	
	//
	// Allocate struct for the new exception
	//

	PEXCEPTION_ARGUMENTS Args = (PEXCEPTION_ARGUMENTS) ExAllocateHeap (TRUE, sizeof(EXCEPTION_ARGUMENTS));
	Args->ExceptionCode = ExceptionCode;
	Args->NumberParameters = NumberParameters;
	Args->Parameters[0] = Parameter1;
	Args->Parameters[1] = Parameter2;
	Args->Parameters[2] = Parameter3;
	Args->Parameters[3] = Parameter4;

	//
	// Set current processing exception
	//

	Pcb->CurrentException = Args;

	//
	// Walk handler list
	//

	PEXCEPTION_RECORD Record = Pcb->ExceptionList;

	while (Record)
	{
		UCHAR Res = Record->Handler (Args, ContextFrame);

		switch (Res)
		{
		case EXCEPTION_CONTINUE_SEARCH:
			
			//
			// Do nothing, just go through the list.
			//

			break;

		case EXCEPTION_CONTINUE_EXECUTION:

			//
			// Continue the execution. Clean up & return
			//

			Pcb->CurrentException = NULL;
			ExFreeHeap (Args);

			return;

		default:

			//
			// Unknown value returned. Terminate thread with EXCEPTION_INVALID_RESULUTION
			//

			KiDebugPrint ("KE: Invalid resolution returned from handler %08x [%x] for th=%08x\n", Record->Handler, Res, Thread);

			PsExitThread (EXCEPTION_INVALID_RESULUTION);
		}

		Record = Record->Next;
	}

	//
	// No handlers processed the exception. Terminate current thread
	//

	KiDebugPrint ("KE: No handlers processed exception code %08x for th=%08x\n", ExceptionCode, Thread);

	ExFreeHeap (Args);
	PsExitThread (ExceptionCode);
}