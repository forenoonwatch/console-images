#pragma once

class Console {
	public:
		/* Get the PID of the parent console process */
		static unsigned long get_console_process_id();

		Console();
		~Console();

		/* Attach to the console of the given PID */
		[[nodiscard]] bool attach(unsigned long pid);

		void update(double deltaTime);

		unsigned long write(const char* text, unsigned long length);

		bool is_attached() const;

		Console(const Console&) = delete;
		Console(Console&&) = delete;

		void operator=(const Console&) = delete;
		void operator=(Console&&) = delete;
	private:
		class ConsoleImpl* impl;
};

