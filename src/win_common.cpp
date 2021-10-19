#include "win_common.hpp"

#include <windows.h>

void win::print_last_error(const char* prefix, FILE* dest) {
	LPVOID lpMsgBuf;
	
	FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER
			| FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
			NULL, GetLastError(), MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
			(LPTSTR)&lpMsgBuf, 0, NULL);

	if (*prefix) {
		fprintf(dest, "%s: %s\n", prefix, (LPCTSTR)lpMsgBuf);
	}
	else {
		fprintf(dest, "%s\n", (LPCTSTR)lpMsgBuf);
	}

	LocalFree(lpMsgBuf);
}

bool win::process_spawn_detached(char* command) {
	PROCESS_INFORMATION procInfo{};
	STARTUPINFO startInfo{};
	startInfo.cb = sizeof(STARTUPINFO);

	return CreateProcessA(NULL, command, NULL, NULL, FALSE, DETACHED_PROCESS,
			NULL, NULL, &startInfo, &procInfo);
}

win::Process win::process_sync_handle_open(unsigned long pid) {
	HANDLE proc = OpenProcess(SYNCHRONIZE, FALSE, pid);
	return reinterpret_cast<Process>(proc);
}

void win::process_handle_close(Process proc) {
	CloseHandle(reinterpret_cast<HANDLE>(proc));
}

void win::process_wait(Process proc) {
	WaitForSingleObject(reinterpret_cast<HANDLE>(proc), INFINITE);
}

