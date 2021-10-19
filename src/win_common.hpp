#pragma once

#include <cstdio>

namespace win {
	typedef struct Process_T* Process;
	
	/* Outputs the last error message from a Windows API call */
	void print_last_error(const char* prefix = "", FILE* dest = stderr);

	/* Spawn a detached process using the given command */
	bool process_spawn_detached(char* command);

	/* Open a handle to a process with perms only to synchronize against it */
	Process process_sync_handle_open(unsigned long pid);

	/* Close a process handle */
	void process_handle_close(Process process);

	/* Wait on a process sync handle */
	void process_wait(Process process);
};

