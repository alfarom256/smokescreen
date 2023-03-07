#include "FltOps.h"
#include <ntifs.h>

void DriverUnload(_In_ PDRIVER_OBJECT DriverObject) {
	UNREFERENCED_PARAMETER(DriverObject);
	return;
}

NTSTATUS DriverEntry(_In_ PDRIVER_OBJECT pDriverObject, _In_ PUNICODE_STRING pRegStr) {
	NTSTATUS status = STATUS_SUCCESS;
	pDriverObject->DriverUnload = DriverUnload;
	UNREFERENCED_PARAMETER(pRegStr);
	
	status = InitFilter(pDriverObject);

	return status;
}