// begin_ddk

#pragma once

#define SE_1_ASSIGN_TOKEN_PROCESS		0x00000001
#define SE_1_ASSIGN_TOKEN_THREAD		0x00000002
#define SE_1_LOAD_DRIVER				0x00000004
#define SE_1_DEBUG_PROCESS				0x00000008
#define SE_1_CREATE_GLOBAL				0x00000010
#define SE_1_INCREASE_PRIORITY			0x00000020
#define SE_1_INCREASE_QUANTUM			0x00000040
#define SE_1_LOCK_PAGES_IN_MEMORY		0x00000080
#define SE_1_SHUTDOWN_MACHINE			0x00000100
#define SE_1_SYSTEM_TIME_CHANGE			0x00000200
#define SE_1_CAN_ENTER_UPDATE_MODE		0x00000400
#define SE_1_CREATE_PROCESS				0x00000800
#define SE_1_CREATE_THREAD				0x00001000
#define SE_1_READ_SYSTEM_FILES			0x00002000
#define SE_1_WRITE_SYSTEM_FILES			0x00004000

// end_ddk

#pragma pack(2)

typedef struct SE_LOGGED_USER
{
	LIST_ENTRY LoggedUsersEntry;

	USHORT UserId;
	USHORT GroupId;

	//
	// Full user's privileges.
	//
	ULONG Privileges1;					// First  32 privileges (SE_1_*)
	ULONG Privileges2;					// Second 32 privileges (SE_2_*)

	//
	// Default privileges assigned to new threads (processes) owned by this user.
	// They can be raised to full user's privileges.
	//
	ULONG DefaultPrivileges1;
	ULONG DefaultPrivileges2;

} *PSE_LOGGED_USER;

KEVAR LOCKED_LIST SeLoggedUsers;
KEVAR PSE_LOGGED_USER SeSystemUser;

typedef struct SE_ACCESS_TOKEN
{
	MUTEX TokenLock;

	//
	// Current token's privileges
	//

	ULONG Privileges1;					// First  32 privileges (SE_1_*)
	ULONG Privileges2 UNIMPLEMENTED;	// Second 32 privileges (SE_2_*)

	BOOLEAN Exclusive;	// Is this token shared by some threads/processes or it is exclusive.
	ULONG ShareCount;

	PSE_LOGGED_USER UserToken;
} *PSE_ACCESS_TOKEN;

#pragma pack()


#define PsGetProcessAccessToken(Process) ((Process)->AccessToken)
#define PsGetCurrentProcessAccessToken() (PsGetCurrentProcess()->AccessToken)

PSE_ACCESS_TOKEN
KEAPI
PsGetProcessAccessTokenAndLock(
	PPROCESS Process
	);

#define PsGetCurrentProcessAccessTokenAndLock() PsGetProcessAccessTokenAndLock (PsGetCurrentProcess())


#define PsGetThreadAccessToken(Thread) ((Thread)->AccessToken)
#define PsGetCurrentThreadAccessToken() (PsGetCurrentThread()->AccessToken)

PSE_ACCESS_TOKEN
KEAPI
PsGetThreadAccessTokenAndLock(
	PTHREAD Thread
	);

#define PsGetCurrentThreadAccessTokenAndLock() PsGetThreadAccessTokenAndLock (PsGetCurrentThread())

VOID
KEAPI
SeUnlockAccessToken(
	PSE_ACCESS_TOKEN Token
	);

PSE_ACCESS_TOKEN
KEAPI
SeCreateAccessToken(
	PSE_LOGGED_USER UserToken
	);

VOID
KEAPI
SeDeleteAccessToken(
	PSE_ACCESS_TOKEN Token
	);


STATUS
KEAPI
SeInheritAccessTokenProcess(
	IN PPROCESS Process,
	IN ULONG MaximumPrivileges1,
	IN ULONG MaximumPrivileges2,
	OUT PSE_ACCESS_TOKEN *NewToken
	);

STATUS
KEAPI
SeInheritAccessTokenThread(
	IN PTHREAD Thread,
	IN ULONG MaximumPrivileges1,
	IN ULONG MaximumPrivileges2,
	OUT PSE_ACCESS_TOKEN *NewToken
	);

STATUS
KEAPI
SepInheritAccessToken(
	IN PSE_ACCESS_TOKEN Token,
	IN ULONG MaximumPrivileges1,
	IN ULONG MaximumPrivileges2,
	OUT PSE_ACCESS_TOKEN *NewToken
	);

	


// begin_ddk

KESYSAPI
STATUS
KEAPI
SeAccessCheckEx(
	PTHREAD Thread,
	ULONG Privileges1,
	ULONG Privileges2
	);

#define SeAccessCheck(Privileges1,Privileges2) SeAccessCheckEx(PsGetCurrentThread(),Privileges1,Privileges2)

// end_ddk
