#include "kernel32.h"

HANDLE
KEAPI 
CreateEventW(
  PVOID lpEventAttributes,
  BOOLEAN bManualReset,
  BOOLEAN bInitialState,
  PWSTR lpName
)
{
	UNICODE_STRING EventName;
	STATUS Status;
	HANDLE hEvent;

	UNREFERENCED_PARAMETER (lpEventAttributes);

	RtlInitUnicodeString (&EventName, lpName);

	Status = KsCreateEvent (
		&hEvent, 
		bManualReset ? NotificationEvent : SynchronizationEvent,
		bInitialState,
		&EventName
		);

	return SUCCESS(Status) ? hEvent : NULL;
}

HANDLE
KEAPI 
CreateEventA(
  PVOID lpEventAttributes,
  BOOLEAN bManualReset,
  BOOLEAN bInitialState,
  PSTR lpName
)
{
	HANDLE hEvent;
	PWSTR UnicodeEventName = (PWSTR) ExAllocateHeap (TRUE, strlen(lpName)*2+2);
	mbstowcs (UnicodeEventName, lpName, -1);

	hEvent = CreateEventW(
		lpEventAttributes,
		bManualReset,
		bInitialState,
		UnicodeEventName
		);

	ExFreeHeap (UnicodeEventName);

	return hEvent;
}

BOOLEAN
KEAPI 
SetEvent(
  HANDLE hEvent
)
{
	return KsSetEvent (hEvent, 0);
}
