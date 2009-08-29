//
// FILE:		se.cpp
// CREATED:		15-Jun-2008  by Great
// PART:        SE
// ABSTRACT:
//			Security support for OS
//

#include "common.h"


PSE_ACCESS_TOKEN
KEAPI
PsGetProcessAccessTokenAndLock(
	PPROCESS Process
	)
/*++
	Get access token for process
--*/
{
	PSE_ACCESS_TOKEN Token = PsGetProcessAccessToken (Process);

	ExAcquireMutex (&Token->TokenLock);

	return Token;
}


PSE_ACCESS_TOKEN
KEAPI
PsGetThreadAccessTokenAndLock(
	PTHREAD Thread
	)
/*++
	Get access token for thread
--*/
{
	PSE_ACCESS_TOKEN Token = PsGetThreadAccessToken (Thread);

	ExAcquireMutex (&Token->TokenLock);

	return Token;
}

VOID
KEAPI
SeUnlockAccessToken(
	PSE_ACCESS_TOKEN Token
	)
/*++
	Unlock the access token
--*/
{
	ExReleaseMutex (&Token->TokenLock);
}


KESYSAPI
STATUS
KEAPI
SeAccessCheckEx(
	PTHREAD Thread,
	ULONG Privileges1,
	ULONG Privileges2
	)
/*++
	Check that the thread has the supplied privilege set.
--*/
{
	PSE_ACCESS_TOKEN Token = PsGetThreadAccessTokenAndLock (Thread);

	ULONG Temp1 = (Token->Privileges1 & Privileges1);
	ULONG Temp2 = (Token->Privileges2 & Privileges2);

	SeUnlockAccessToken (Token);

	return ((Temp1 == Privileges1) && (Temp2 == Privileges2)) ? STATUS_SUCCESS : STATUS_ACCESS_DENIED;
}

PSE_ACCESS_TOKEN
KEAPI
SeCreateAccessToken(
	PSE_LOGGED_USER UserToken
	)
/*++
	Create an access token
--*/
{
	PSE_ACCESS_TOKEN Token = (PSE_ACCESS_TOKEN) ExAllocateHeap (TRUE, sizeof(SE_ACCESS_TOKEN));
	if (Token == NULL)
		return NULL;

	ExInitializeMutex (&Token->TokenLock);
	Token->UserToken = UserToken;
	Token->Privileges1 = UserToken->DefaultPrivileges1;
	Token->Privileges2 = UserToken->DefaultPrivileges2;
	Token->Exclusive = TRUE;
	Token->ShareCount = 1;
	return Token;
}

VOID
KEAPI
SeDeleteAccessToken(
	PSE_ACCESS_TOKEN Token
	)
/*++
	Delete access token
--*/
{
	ExAcquireMutex (&Token->TokenLock);
	InterlockedDecrement ( (PLONG) &Token->ShareCount);

	if (Token->ShareCount == 0)
	{
		ExFreeHeap (Token);
	}
	else
	{
		SeUnlockAccessToken (Token);
	}
}


STATUS
KEAPI
SeInheritAccessTokenProcess(
	IN PPROCESS Process,
	IN ULONG MaximumPrivileges1,
	IN ULONG MaximumPrivileges2,
	OUT PSE_ACCESS_TOKEN *NewToken
	)
/*++
	Inherit process' access token
--*/
{
	PSE_ACCESS_TOKEN Token = PsGetProcessAccessTokenAndLock (Process);

	STATUS Status = SepInheritAccessToken (Token, MaximumPrivileges1, MaximumPrivileges2, NewToken);

	SeUnlockAccessToken (Token);
	return Status;
}


STATUS
KEAPI
SeInheritAccessTokenThread(
	IN PTHREAD Thread,
	IN ULONG MaximumPrivileges1,
	IN ULONG MaximumPrivileges2,
	OUT PSE_ACCESS_TOKEN *NewToken
	)
/*++
	Inherit thread's access token
--*/
{
	PSE_ACCESS_TOKEN Token = PsGetThreadAccessTokenAndLock (Thread);

	STATUS Status = SepInheritAccessToken (Token, MaximumPrivileges1, MaximumPrivileges2, NewToken);

	SeUnlockAccessToken (Token);
	return Status;
}

#define Raised(x,y) ( ((x)^(y)) & (~(x)) )

LOCKED_LIST SeLoggedUsers;
PSE_LOGGED_USER SeSystemUser;

STATUS
KEAPI
SepInheritAccessToken(
	IN PSE_ACCESS_TOKEN Token,
	IN ULONG MaximumPrivileges1,
	IN ULONG MaximumPrivileges2,
	OUT PSE_ACCESS_TOKEN *pNewToken
	)
/*++
	Inherit access token.
	
	Environment: Access token lock held.
--*/
{
	if ( (Token->Privileges1 != MaximumPrivileges1) ||
		 (Token->Privileges2 != MaximumPrivileges2) )
	{
		//
		// Clone current token.
		//

		if ( Raised (Token->Privileges1, MaximumPrivileges1) ||
			 Raised (Token->Privileges1, MaximumPrivileges1))
		{
			//
			// Cannot raise privileges
			//

			return STATUS_ACCESS_DENIED;
		}


		PSE_ACCESS_TOKEN NewToken = (PSE_ACCESS_TOKEN) ExAllocateHeap (TRUE, sizeof(SE_ACCESS_TOKEN));
		if (NewToken == NULL)
			return STATUS_INSUFFICIENT_RESOURCES;

		ExInitializeMutex (&NewToken->TokenLock);
		NewToken->UserToken = Token->UserToken;

		NewToken->Privileges1 = MaximumPrivileges1;
		NewToken->Privileges2 = MaximumPrivileges2;

		NewToken->Exclusive = TRUE;
		NewToken->ShareCount = 1;

		*pNewToken = NewToken;
		return STATUS_SUCCESS;
	}

	//
	// Use current token for new thread/process
	//

	InterlockedIncrement ( (PLONG) &Token->ShareCount );
	Token->Exclusive = FALSE;
	*pNewToken = Token;
	return STATUS_SUCCESS;
}


//
// TO DO: SeLogonUser and othrz.
//
