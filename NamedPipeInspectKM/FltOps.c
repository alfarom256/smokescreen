#include "FltOps.h"

_Use_decl_annotations_
void DumpHex(PVOID data, SIZE_T size)
{
	char ascii[17];
	size_t i, j;
	ascii[16] = '\0';
	for (i = 0; i < size; ++i) {
		DbgPrint("%02X ", ((unsigned char*)data)[i]);
		if (((unsigned char*)data)[i] >= ' ' && ((unsigned char*)data)[i] <= '~') {
			ascii[i % 16] = ((unsigned char*)data)[i];
		}
		else {
			ascii[i % 16] = '.';
		}
		if ((i + 1) % 8 == 0 || i + 1 == size) {
			DbgPrint(" ");
			if ((i + 1) % 16 == 0) {
				DbgPrint("|  %s \n", ascii);
			}
			else if (i + 1 == size) {
				ascii[(i + 1) % 16] = '\0';
				if ((i + 1) % 16 <= 8) {
					DbgPrint(" ");
				}
				for (j = (i + 1) % 16; j < 16; ++j) {
					DbgPrint("   ");
				}
				DbgPrint("|  %s \n", ascii);
			}
		}
	}
}

_Use_decl_annotations_
NTSTATUS InitFilter(PDRIVER_OBJECT pDrvObj)
{
	NTSTATUS status = STATUS_SUCCESS;

	PSECURITY_DESCRIPTOR pSec = NULL;
	status = FltBuildDefaultSecurityDescriptor(&pSec, FLT_PORT_ALL_ACCESS);
	if (!NT_SUCCESS(status)) {
		return status;
	}

	OBJECT_ATTRIBUTES FilterAttr = { 0 };
	InitializeObjectAttributes(
		&FilterAttr,
		&glb_commPortName,
		OBJ_KERNEL_HANDLE,
		NULL,
		pSec
	);

	status = FltRegisterFilter(
		pDrvObj,
		&FltRegistration,
		&glb_Filter
	);

	if (!NT_SUCCESS(status)) {
		FltFreeSecurityDescriptor(pSec);
		return status;
	}

	status = FltStartFiltering(glb_Filter);
	if (!NT_SUCCESS(status)) {
		FltFreeSecurityDescriptor(pSec);
		return status;
	}

	status = FltCreateCommunicationPort(
		glb_Filter,
		&glb_serverPort,
		&FilterAttr, 
		NULL,
		ConnectNotify,
		DisconnectNotify,
		NULL,
		1
	);

	FltFreeSecurityDescriptor(pSec);
	return status;
}


// todo clean this up
_Use_decl_annotations_
FLT_POSTOP_CALLBACK_STATUS PostRWCallback(
	PFLT_CALLBACK_DATA pCallbackData,
	PCFLT_RELATED_OBJECTS pRelatedObjects,
	PVOID pCompletionContext,
	FLT_POST_OPERATION_FLAGS Flags
)
{
	UNREFERENCED_PARAMETER(Flags);
	UNREFERENCED_PARAMETER(pRelatedObjects);
	UNREFERENCED_PARAMETER(pCompletionContext);

	NTSTATUS status = STATUS_SUCCESS;
	PFLT_FILE_NAME_INFORMATION FileNameInfo = NULL;
	LARGE_INTEGER Timeout = { 0 };
	Timeout.QuadPart = 0;

	// Skip all reads/writes which failed and all IRP_MN_MDL operations
	if (!NT_SUCCESS(pCallbackData->IoStatus.Status) || pCallbackData->Iopb->MinorFunction == IRP_MN_MDL) {
		return FLT_POSTOP_FINISHED_PROCESSING;
	}

	status = FltGetFileNameInformation(pCallbackData, FLT_FILE_NAME_OPENED | FLT_FILE_NAME_QUERY_DEFAULT, &FileNameInfo);
	if (!NT_SUCCESS(status)) {
		DbgPrint("[Post] Query File Name Info Failed - %lx\n", status);
		return FLT_POSTOP_FINISHED_PROCESSING;
	}

	HANDLE CurrentProcess = PsGetCurrentProcessId();
	PUNICODE_STRING FileName = &FileNameInfo->Name;
	DbgPrint("[Post] FileName -> %wZ | PID %p\n", FileName, CurrentProcess);

	if (!RtlCompareUnicodeString(&glb_targetFile, FileName, FALSE) && glb_clientPort) {
		PMDL* pMdl = NULL;
		PVOID* pBuffer = NULL;
		PULONG pLength = NULL;
		status = FltDecodeParameters(pCallbackData, &pMdl, &pBuffer, &pLength, NULL);

		PVOID Buffer = NULL;
		ULONG BufferLen = *pLength;

		// map user buffer mdl if it exists
		if (*pMdl) {
			Buffer = MmGetSystemAddressForMdlSafe(
				*pMdl,
				NormalPagePriority
			);
		}
		else if(*pBuffer) {
			// otherwise, lock the user buffer
			status = FltLockUserBuffer(pCallbackData);
			// make sure that succeeded, and the created mdl is not NULL
			if (!NT_SUCCESS(status) || !*pMdl) {
				DbgPrint("Failed to lock user buffer - %lx\n", status);
				FltReleaseFileNameInformation(FileNameInfo);
				return FLT_POSTOP_FINISHED_PROCESSING;
			}
			// then map the created mdl
			Buffer = MmGetSystemAddressForMdlSafe(
				*pMdl,
				NormalPagePriority
			);
		}

		if (!Buffer) {
			FltReleaseFileNameInformation(FileNameInfo);
			return FLT_POSTOP_FINISHED_PROCESSING;
		}

		// allocate READ_WRITE_MESSAGE
		PREAD_WRITE_MESSAGE_HEADER pMessage = ExAllocatePool2(
			POOL_FLAG_PAGED,
			sizeof(READ_WRITE_MESSAGE_HEADER) + BufferLen,
			TAG_MESSAGE_HEADER
		);

		if (!pMessage) {
			DbgPrint("Failed to alloc message header and buffer");
			FltReleaseFileNameInformation(FileNameInfo);
			return FLT_POSTOP_FINISHED_PROCESSING;
		}

		pMessage->Operation = pCallbackData->Iopb->MajorFunction == IRP_MJ_READ ? OPERATION_READ : OPERATION_WRITE;
		pMessage->Pid = CurrentProcess;
		pMessage->SizeMessage = BufferLen;

		RtlCopyMemory(&pMessage->Buf[0], Buffer, BufferLen);
		ULONG ulLen = sizeof(READ_WRITE_MESSAGE_HEADER) + BufferLen;
		status = FltSendMessage(
			glb_Filter,
			&glb_clientPort,
			pMessage,
			ulLen,
			NULL,
			0,
			NULL
		);

		if (!(status == STATUS_TIMEOUT || NT_SUCCESS(status))) {
			ExFreePoolWithTag(pMessage, TAG_MESSAGE_HEADER);
			FltReleaseFileNameInformation(FileNameInfo);
			return FLT_POSTOP_FINISHED_PROCESSING;
		}
		else {
			ExFreePoolWithTag(pMessage, TAG_MESSAGE_HEADER);
		}
	}

	FltReleaseFileNameInformation(FileNameInfo);
	return FLT_POSTOP_FINISHED_PROCESSING;
}

_Use_decl_annotations_
NTSTATUS FsFilterUnload(FLT_FILTER_UNLOAD_FLAGS Flags)
{
	if (glb_clientPort) {
		FltCloseClientPort(glb_Filter, &glb_clientPort);
	}

	if (glb_serverPort) {
		FltCloseCommunicationPort(glb_serverPort);
	}

	if (glb_Filter) {
		FltUnregisterFilter(glb_Filter);
	}

	UNREFERENCED_PARAMETER(Flags);
	return STATUS_SUCCESS;
}

_Use_decl_annotations_
NTSTATUS ConnectNotify(PFLT_PORT ClientPort, PVOID ServerPortCookie, PVOID ConnectionContext, ULONG SizeOfContext, PVOID* ConnectionPortCookie)
{
	NTSTATUS status = STATUS_SUCCESS;
	UNREFERENCED_PARAMETER(ServerPortCookie);
	UNREFERENCED_PARAMETER(ConnectionContext);
	UNREFERENCED_PARAMETER(SizeOfContext);
	if (ClientPort) {
		glb_clientPort = ClientPort;
	}
	*(PULONG)ConnectionPortCookie = (ULONG)'doot'; // for shame!
	return status;
}

_Use_decl_annotations_
VOID DisconnectNotify(PVOID ConnectionCookie)
{
	UNREFERENCED_PARAMETER(ConnectionCookie);
	FltCloseClientPort(glb_Filter, &glb_clientPort);
	return;
}
