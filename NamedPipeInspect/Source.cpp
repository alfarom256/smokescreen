#include <stdio.h>
#include <Windows.h>
#include <fltUser.h>
#pragma comment(lib, "fltlib")

#define GLE( x, hr ) { if(hr != 0){ printf("%s:%d:%s failed with - %lx\n", __FILE__, __LINE__, x, hr); } else { printf("%s:%d:%s failed with - %lx\n", __FILE__, __LINE__, x, GetLastError());} }

#define OPERATION_READ 0
#define OPERATION_WRITE 1

typedef struct _READ_WRITE_MESSAGE_HEADER {
	HANDLE Pid;
	ULONG SizeMessage;
	ULONG Operation;
	UCHAR Buf[1];
} READ_WRITE_MESSAGE_HEADER, * PREAD_WRITE_MESSAGE_HEADER;

typedef struct _READ_WRITE_MESSAGE {
	FILTER_MESSAGE_HEADER header;
	READ_WRITE_MESSAGE_HEADER flt_msg;
} RW_MSG, *PRW_MSG;

void DumpHex(PVOID data, SIZE_T size)
{
	char ascii[17];
	size_t i, j;
	ascii[16] = '\0';
	for (i = 0; i < size; ++i) {
		printf("%02X ", ((unsigned char*)data)[i]);
		if (((unsigned char*)data)[i] >= ' ' && ((unsigned char*)data)[i] <= '~') {
			ascii[i % 16] = ((unsigned char*)data)[i];
		}
		else {
			ascii[i % 16] = '.';
		}
		if ((i + 1) % 8 == 0 || i + 1 == size) {
			printf(" ");
			if ((i + 1) % 16 == 0) {
				printf("|  %s \n", ascii);
			}
			else if (i + 1 == size) {
				ascii[(i + 1) % 16] = '\0';
				if ((i + 1) % 16 <= 8) {
					printf(" ");
				}
				for (j = (i + 1) % 16; j < 16; ++j) {
					printf("   ");
				}
				printf("|  %s \n", ascii);
			}
		}
	}
}

int main() {
	HANDLE hPort = NULL;
	HRESULT hRes = FilterConnectCommunicationPort(
		LR"(\pipemonport)",
		0,
		NULL,
		0,
		NULL,
		&hPort
	);

	if (!SUCCEEDED(hRes)) {
		GLE("FilterConnectCommunicationPort", hRes);
		return -1;
	}

	while (TRUE) {
		PVOID buffer = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, 0x10000);
		
		if (!buffer) {
			GLE("HeapAlloc", 0);
			continue;
		}

		hRes = FilterGetMessage(
			hPort,
			(PFILTER_MESSAGE_HEADER)buffer, 
			0x10000,
			NULL
		);

		if (SUCCEEDED(hRes)) {
			PRW_MSG message = (PRW_MSG)buffer;
			printf("\n=============================================================\n");
			printf(
				"PID %p %s pipe - len %d\n", 
				message->flt_msg.Pid, 
				message->flt_msg.Operation == OPERATION_READ ? "reads" : "writes",
				message->flt_msg.SizeMessage
			);
			DumpHex(&message->flt_msg.Buf, message->flt_msg.SizeMessage);
			HeapFree(GetProcessHeap(), NULL, buffer);
		}
		else {
			HeapFree(GetProcessHeap(), NULL, buffer);
			GLE("FilterGetMessage", hRes);
		}
	}

}