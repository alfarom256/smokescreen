#pragma once
#pragma comment(lib, "FltMgr.lib")
#include <fltKernel.h>
#include <ntifs.h>

#define PRINT_SEP( ) { DbgPrint("\n-------------------------------------------\n"); }

// TYPES

typedef struct _READ_WRITE_MESSAGE_HEADER {
	HANDLE Pid;
	ULONG SizeMessage;
	ULONG Operation;
	UCHAR Buf[1];
} READ_WRITE_MESSAGE_HEADER, *PREAD_WRITE_MESSAGE_HEADER;


# define TAG_MESSAGE_HEADER 'l41n'

#define OPERATION_READ 0
#define OPERATION_WRITE 1

// END TYPES


// FUNCTIONS

// https://gist.github.com/ccbrown/9722406
void DumpHex(
	_In_ PVOID data,
	_In_ SIZE_T size
);

//void DumpRWBuffer(IO)

NTSTATUS InitFilter(
	_In_ PDRIVER_OBJECT pDrvObj
);

FLT_POSTOP_CALLBACK_STATUS PostRWCallback(
	_In_ PFLT_CALLBACK_DATA pCallbackData,
	_In_ PCFLT_RELATED_OBJECTS pRelatedObjects,
	_In_ PVOID pCompletionContext,
	_In_ FLT_POST_OPERATION_FLAGS Flags
);

NTSTATUS FsFilterUnload(
	_In_ FLT_FILTER_UNLOAD_FLAGS Flags
);

NTSTATUS ConnectNotify(
	_In_ PFLT_PORT ClientPort,
	_In_ PVOID ServerPortCookie,
	_In_ PVOID ConnectionContext,
	_In_ ULONG SizeOfContext,
	_Out_ PVOID* ConnectionPortCookie
);

VOID DisconnectNotify(
	_In_ PVOID ConnectionCookie
);

// END FUNCTIONS


// GLOBALS

static PFLT_FILTER glb_Filter;
static UNICODE_STRING glb_targetFile = RTL_CONSTANT_STRING(L"\\Device\\NamedPipe\\...");
static UNICODE_STRING glb_commPortName = RTL_CONSTANT_STRING(L"\\pipemonport");
static PFLT_PORT glb_serverPort = NULL;
static PFLT_PORT glb_clientPort = NULL;

static const FLT_OPERATION_REGISTRATION Callbacks[] = {
	{
		IRP_MJ_READ,
		0,
		NULL,
		PostRWCallback,
	},
	{
		IRP_MJ_WRITE,
		0,
		NULL,
		PostRWCallback,
	},
	{ IRP_MJ_OPERATION_END }
};

static const FLT_REGISTRATION FltRegistration = {
	sizeof(FLT_REGISTRATION),
	FLT_REGISTRATION_VERSION,
	FLTFL_REGISTRATION_SUPPORT_NPFS_MSFS,
	NULL,
	Callbacks,
	FsFilterUnload,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL
};

// END GLOBALS
