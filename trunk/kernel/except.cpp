//
// FILE:		except.cpp
// CREATED:		28-Feb-2008  by Great
// PART:        KE
// ABSTRACT:
//			Exception dispather
//

#include "common.h"

#undef KdPrint

#if KI_TRACE_EXCEPTION_UNWIND
#define KdPrint(x) KiDebugPrint x
#else
#define KdPrint(x)
#endif

KESYSAPI
VOID
KEAPI
KeRaiseStatus(
	STATUS Status
	)
/*++
	Raise noncontinuable exception with specified status value
--*/
{
	CONTEXT_FRAME ContextFrame;
	EXCEPTION_ARGUMENTS Args;

	KeCaptureContext (&ContextFrame);
	Args.ExceptionAddress = ContextFrame.Eip;
	Args.ExceptionCode = Status;
	Args.ExceptionArguments = NULL;
	Args.NumberParameters = 0;
	Args.Flags = EH_NONCONTINUABLE;

	KeDispatchException (
		&Args,
		&ContextFrame
		);

}


KESYSAPI
VOID
KEAPI
KeRaiseException(
	PEXCEPTION_ARGUMENTS ExceptionArguments
	)
/*++
	Raise the specified exception
--*/
{
	CONTEXT_FRAME Context;
	KeCaptureContext (&Context);

	if (!(ExceptionArguments->Flags & EH_CONTINUING))
	{
		KeDispatchException (ExceptionArguments, &Context);
	}
}


VOID
_cdecl
_local_unwind2(
	IN PEXCEPTION_FRAME RegistrationFrame,
	IN int TryLevelPassed
	)
{
	KdPrint(("_local_unwind2: unwinding [Frame=%08x, TryLevelPassed=%08x, Frame->trylevel=%08x]\n", 
		RegistrationFrame, 
		TryLevelPassed,
		RegistrationFrame->trylevel));

	PSCOPE_TABLE scopetable = RegistrationFrame->ScopeTable;

	while (RegistrationFrame->trylevel != TRYLEVEL_NONE)
	{
		int trylevel = RegistrationFrame->trylevel;

		if (trylevel != TRYLEVEL_NONE &&
			RegistrationFrame->trylevel <= TryLevelPassed)
		{
			return;
		}

		KdPrint(("_local_unwind2: unwinding -> [trylevel=%08x, filter=%08x, handler=%08x]\n", 
			trylevel,
			scopetable[trylevel].Filter,
			scopetable[trylevel].Handler));

		RegistrationFrame->trylevel = scopetable[trylevel].PreviousTryLevel;
		if (scopetable[trylevel].Filter == NULL)
		{
			//
			// this is __try/__finally block.
			//
			// execute __finally {} block now
			//

			scopetable[trylevel].Handler();
		}
	}
}


VOID
KEAPI
RtlUnwind(
	PEXCEPTION_FRAME RegistrationFrame,
	PEXCEPTION_ARGUMENTS Args
	)
{
	EXCEPTION_ARGUMENTS localArgs;
	CONTEXT_FRAME Context;
	PPCB Pcb = PsGetCurrentPcb ();

	KeCaptureContext (&Context);

	KdPrint(("RtlUnwind: unwinding the frame %08x\n", RegistrationFrame));

	if (Args == 0)
	{
		Args = &localArgs;
		Args->ExceptionCode = STATUS_UNWIND;
		Args->Flags = 0;
		Args->ExceptionArguments = 0;
		Args->ExceptionAddress = Context.Eip;
		Args->Parameters[0] = 0;
	}

	Args->Flags |= EH_UNWINDING;

	if (RegistrationFrame == 0)
	{
		Args->Flags |= EH_EXIT_UNWIND;
	}

	PEXCEPTION_FRAME Frame = Pcb->ExceptionList, NewFrame = NULL;

	while (Frame)
	{
		KdPrint(("RtlUnwind: unwinding -> %08x\n", Frame));

		EXCEPTION_ARGUMENTS exc2;

		if (Frame == RegistrationFrame)
		{
			KdPrint(("RtlUnwind: got frame. Continuing\n"));
			//KeContinue (&Context);
			return;
		}
		else
		{
			if (RegistrationFrame && RegistrationFrame<=Frame)
			{
				exc2.ExceptionArguments = Args;
				exc2.ExceptionCode = STATUS_INVALID_UNWIND_TARGET;
				exc2.Flags = EH_NONCONTINUABLE;
				exc2.NumberParameters = 0;

				KeRaiseException (&exc2);
			}
		}

		ULONG retValue = Frame->Handler (
			Args, 
			Frame,
			&Context, 
			&NewFrame
			);

		if (retValue != EXCEPTION_CONTINUE_SEARCH)
		{
			if (retValue != EXCEPTION_COLLIDED_UNWIND)
			{
				exc2.ExceptionArguments = Args;
				exc2.ExceptionCode = STATUS_INVALID_DISPOSITION;
				exc2.Flags = EH_NONCONTINUABLE;
				exc2.NumberParameters = 0;

				KeRaiseException (&exc2);
			}
			else
			{
				Frame = NewFrame;
			}
		}

		Pcb->ExceptionList = Frame;
		Frame = Frame->Next;
	}
	
	//
	// End of EXCEPTION_FRAME list
	//

//	KeContinue (&Context);

	KdPrint(("RtlUnwind: hm.. we should never reach here...\n"));
	return;
}


VOID
_cdecl
_global_unwind2(
	IN PEXCEPTION_FRAME RegistrationFrame
	)
{
	KdPrint(("_global_unwind2: unwinding [Frame=%08x]\n", RegistrationFrame));
	RtlUnwind (
		RegistrationFrame,
		0
		);
}

UCHAR
_cdecl
_except_handler3(
	IN PEXCEPTION_ARGUMENTS ExceptionArguments,
	IN PEXCEPTION_FRAME EstablisherFrame,
	IN PCONTEXT_FRAME CallerContext,
	IN PVOID Reserved
	)
{
	//
	// Clear direction flag (do not make any assumptions!)
	//

	__asm cld;

	KdPrint(("_except_handler3 processing the exception code %08x\n", ExceptionArguments->ExceptionCode));

	//
	// If this is an unwinding
	//

	if (ExceptionArguments->Flags & (EH_UNWINDING | EH_EXIT_UNWIND))
	{
		KdPrint(("_except_handler3: unwinding\n"));
		_local_unwind2 (EstablisherFrame, TRYLEVEL_NONE);
		return EXCEPTION_CONTINUE_SEARCH;
	}

	// Build the EXCEPTION_POINTERS
	EXCEPTION_POINTERS excPtrs = {ExceptionArguments, CallerContext};

	ULONG trylevel = EstablisherFrame->trylevel;
	PSCOPE_TABLE scopetable = EstablisherFrame->ScopeTable;

	// Put address of exception pointers
	*((PULONG)EstablisherFrame - 1) = (ULONG) &excPtrs;

	while (trylevel != TRYLEVEL_NONE)
	{
		VCPP_FILTER Filter = scopetable[trylevel].Filter;

		KdPrint(("_except_handler3: found frame with trylevel=%08x [Filter=%08x, Handler=%08x]\n", trylevel, Filter, scopetable[trylevel].Handler));

		if (Filter)
		{
			ULONG OriginalEbp = (ULONG) &EstablisherFrame->_ebp;
			ULONG Disposition;

			__asm 
			{
				//
				// Switch to original ebp and execute filter
				//

				mov  eax, [OriginalEbp]
				mov  ecx, [Filter]

				push ebp
				mov  ebp, eax

				call ecx

				pop  ebp
				mov  [Disposition], eax
			}

			KdPrint(("_except_handler3: filter at trylevel %08x returned %08x\n", trylevel, Disposition));

			if (Disposition != EXCEPTION_CONTINUE_SEARCH)
			{
				if (Disposition == EXCEPTION_CONTINUE_EXECUTION)
					return EXCEPTION_CONTINUE_EXECUTION;

				//
				// EXCEPTION_EXECUTE_HANDLER
				//

				// Does the actual cleanup of registration frames.
				// Recursive function
				_global_unwind2 (EstablisherFrame);

				__asm
				{
					//
					// Save pointers in the registers,
					//  switch to original ebp and perform local unwinding
					//
					mov  edx, [trylevel]
					mov  eax, [OriginalEbp]
					mov  ecx, [EstablisherFrame]

					push ebp
					mov  ebp, eax

					push edx
					push ecx
					call _local_unwind2
					add  esp, 8

					pop  ebp
				}

				// Set the current tryleve to whatever SCOPETABLE entry
				// was being used when a handler was found
				EstablisherFrame->trylevel = scopetable->PreviousTryLevel;

				// Call the _except {} block. Never returns
				ULONG Handler = (ULONG)EstablisherFrame->ScopeTable[trylevel].Handler;

				__asm
				{
					//
					// Switch to original ebp and execute handler
					//

					mov  eax, [Handler]
					mov  ebp, [OriginalEbp]

					call eax
				}
			}
		}

		scopetable = EstablisherFrame->ScopeTable;
		trylevel = scopetable[trylevel].PreviousTryLevel;
	}

	return EXCEPTION_CONTINUE_SEARCH;
}

/*
VOID
KEAPI
KiUnwind(
	PEXCEPTION_ARGUMENTS ExceptionArguments,
	PEXCEPTION_FRAME InitialFrame,
	PEXCEPTION_FRAME LastFrame,
	PCONTEXT_FRAME ContextFrame
	)
{
	ExceptionArguments->Flags |= EH_UNWINDING;

	for (PEXCEPTION_FRAME Record = InitialFrame; Record != LastFrame; Record = Record->Next)
	{
		Record->Handler (
			ExceptionArguments,
			Record,
			ContextFrame,
			NULL
			);

		// Check that some did not clear this flag
		ASSERT (ExceptionArguments->Flags & EH_UNWINDING);

		if (!(ExceptionArguments->Flags & EH_UNWINDING))
		{
			ExceptionArguments->Flags |= EH_UNWINDING;
		}
	}
}
*/

KESYSAPI
VOID
KEAPI
KeDispatchException(
	PEXCEPTION_ARGUMENTS Args,
	CONTEXT_FRAME *ContextFrame
	)
/*++
	Common exception dispatcher
--*/
{
	PTHREAD Thread = PsGetCurrentThread ();
	PPCB Pcb = PsGetCurrentPcb ();

	KiDebugPrint ("KE: Exception dispatcher for th=%08x, code %08x\n", Thread, Args->ExceptionCode);

	if( Pcb->CurrentException )
	{
		//
		// Nested exception. Terminate current thread immediately
		//
		//BUGBUG: TODO: Handle this correctly in the future.
		//

		KiDebugPrint ("KE: Nested excpetions for th=%08x\n", Thread);

		Pcb->CurrentException = NULL;

		PsExitThread (Args->ExceptionCode);
	}

	//
	// Set current processing exception
	//

	Pcb->CurrentException = Args;

	//
	// Walk handler list
	//

	PEXCEPTION_FRAME Record = Pcb->ExceptionList;

	while (Record)
	{
		UCHAR Res = Record->Handler (
			Args,
			Record,
			ContextFrame,
			NULL
			);

		switch (Res)
		{
		case EXCEPTION_CONTINUE_SEARCH:
			
			//
			// Do nothing, just go through the list.
			//

			break;

		case EXCEPTION_CONTINUE_EXECUTION:

			if (Args->Flags & EH_NONCONTINUABLE)
			{
				KeRaiseStatus (EXCEPTION_INVALID_RESULUTION);
			}


			//
			// Initiate unwinding
			//

			//KiUnwind (Args, Pcb->ExceptionList, Record, ContextFrame);
			RtlUnwind (Pcb->ExceptionList, Args);

			//
			// Continue the execution. Clean up & return
			//

			Pcb->CurrentException = NULL;
			
			Args->Flags |= EH_CONTINUING;

			return;

		case EXCEPTION_EXECUTE_HANDLER:

		default:

			//
			// Unknown value returned. Terminate thread with EXCEPTION_INVALID_RESULUTION
			//

			KiDebugPrint ("KE: Invalid resolution returned from handler %08x [%x] for th=%08x\n", Record->Handler, Res, Thread);

			KeRaiseStatus (EXCEPTION_INVALID_RESULUTION);
		}

		Record = Record->Next;
	}

	//
	// No handlers processed the exception. Terminate current thread
	//

	KiDebugPrint ("KE: No handlers processed exception code %08x for th=%08x\n", Args->ExceptionCode, Thread);

	PsExitThread (Args->ExceptionCode);
}