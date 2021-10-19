#include <atomic>
#include <chrono>
#include <thread>

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "win_common.hpp"
#include "console.hpp"

void await_parent_close(unsigned long pid);

std::atomic<bool> g_parentClosed = false;

int main(int argc, char** argv) {
	if (argc == 1) {
		unsigned long pid = Console::get_console_process_id();

		char* command = (char*)malloc(strlen(argv[0]) + 16); 
		sprintf(command, "%s %lu", argv[0], pid);

		bool res = win::process_spawn_detached(command);
		
		free(command);

		if (!res) {
			win::print_last_error("CreateProcessA");
			return 1;
		}
	}
	else if (argc == 2) {
		using namespace std::chrono;

		unsigned long pid = atol(argv[1]);
		std::thread parentWait(await_parent_close, pid);

		Console console;

		if (!console.attach(pid)) {
			FILE* file = fopen("log_file.txt", "w");
			win::print_last_error("AttachConsole", file);
			fclose(file);
		}

		auto lastTimePoint = high_resolution_clock::now();

		while (!g_parentClosed) {
			auto currTimePoint = high_resolution_clock::now();
			double passedTime = duration_cast<duration<double>>(
					currTimePoint - lastTimePoint).count();
			lastTimePoint = currTimePoint;

			console.update(passedTime);
		}

		parentWait.join();
	}

	return 0;
}

void await_parent_close(unsigned long pid) {
	win::Process proc = win::process_sync_handle_open(pid);

	if (!proc) {
		FILE* file = fopen("log_file.txt", "w");
		win::print_last_error("OpenProcess", file);
		fclose(file);

		exit(1);
	}

	win::process_wait(proc);
	win::process_handle_close(proc);

	g_parentClosed.store(true);
}

