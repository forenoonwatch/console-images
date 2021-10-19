#include "console.hpp"

#include <windows.h>

#include <cassert>
#include <cstring>
#include <cstdlib>

#include <vector>

#include "win_common.hpp"
#include "graphic.hpp"

static constexpr const unsigned INPUT_RECORDS_PER_FRAME = 8;

static constexpr const char* IMAGE_BEGIN_TAG = "[img=";
static constexpr const char* IMAGE_END_TAG = "]";

static constexpr const unsigned IMAGE_BEGIN_TAG_LENGTH
		= strlen(IMAGE_BEGIN_TAG);
static constexpr const unsigned IMAGE_END_TAG_LENGTH = strlen(IMAGE_END_TAG);

static constexpr const unsigned CONSOLE_IMAGE_HEIGHT = 10;

static BOOL WINAPI console_ctrl_handler(DWORD dwCtrlType);

struct GraphicEntry {
	Graphic* graphic;
	unsigned posX;
	unsigned posY;
};

class ConsoleImpl {
	public:
		ConsoleImpl();
		~ConsoleImpl();

		bool attach(unsigned long pid);

		void update(double deltaTime);

		unsigned long write(const char* text, unsigned long length);

		bool is_attached() const;
	private:
		HANDLE hInput;
		HANDLE hOutput;
		HWND hWnd;
		HDC hDC;
		CONSOLE_FONT_INFO fontInfo;
		CONSOLE_SCREEN_BUFFER_INFO bufInfo;

		DWORD lastTop;
		DWORD lastWidth;
		DWORD lastHeight;

		COORD lastSize;
		COORD lastCursorPos;

		TCHAR* visibleBuffer;
		DWORD visibleBufferSize;

		std::vector<GraphicEntry> graphics;

		bool attached;
		bool readConsole;
		bool redrawImages;

		void read_current_window();
		void check_image_tag(const char* tagStart, const char* fileName,
				unsigned fileNameLen);
		void adjust_console_text(const char* tagStart, unsigned fileNameLen);

		void update_images(double deltaTime);
		void draw_images();

		void handle_mouse_event(const MOUSE_EVENT_RECORD& rec);
		void handle_key_event(const KEY_EVENT_RECORD& rec);
		void handle_window_event(const WINDOW_BUFFER_SIZE_RECORD& rec);
};

// Console Implementation

ConsoleImpl::ConsoleImpl()
		: lastTop(0)
		, lastWidth(0)
		, lastHeight(0)
		, lastSize{}
		, lastCursorPos{}
		, visibleBuffer(nullptr)
		, visibleBufferSize(0)
		, attached(false) {}

ConsoleImpl::~ConsoleImpl() {
	for (GraphicEntry& entry : graphics) {
		delete entry.graphic;
	}

	free(visibleBuffer);

	if (is_attached()) {
		ReleaseDC(hWnd, hDC);
		DeleteDC(hDC);
	}
}

bool ConsoleImpl::attach(unsigned long pid) {
	if (!AttachConsole(pid)) {
		return false;
	}

	if ((hInput = GetStdHandle(STD_INPUT_HANDLE)) == INVALID_HANDLE_VALUE) {
		return false;
	}

	if ((hOutput = GetStdHandle(STD_OUTPUT_HANDLE)) == INVALID_HANDLE_VALUE) {
		return false;
	}

	if (!(hWnd = FindWindowA("ConsoleWindowClass", NULL))) {
		return false;
	}

	if (!(hDC = GetDC(hWnd))) {
		return false;
	}

	DWORD mode;
	if (GetConsoleMode(hInput, &mode)) {
		SetConsoleMode(hInput, mode | ENABLE_WINDOW_INPUT);
	}

	SetConsoleCtrlHandler(console_ctrl_handler, TRUE);

	GetCurrentConsoleFont(hOutput, FALSE, &fontInfo);
	GetConsoleScreenBufferInfo(hOutput, &bufInfo);

	lastTop = bufInfo.dwSize.Y + 1; // invalidate lastTop

	attached = true;
	return true;
}

void ConsoleImpl::update(double deltaTime) {
	assert(is_attached());

	readConsole = false;
	redrawImages = false;

	DWORD eventCount;
	GetNumberOfConsoleInputEvents(hInput, &eventCount);

	if (eventCount > 0) {
		INPUT_RECORD recs[INPUT_RECORDS_PER_FRAME];

		ReadConsoleInput(hInput, recs, INPUT_RECORDS_PER_FRAME, &eventCount);

		for (DWORD i = 0; i < eventCount; ++i) {
			switch (recs[i].EventType) {
				case MOUSE_EVENT:
					handle_mouse_event(recs[i].Event.MouseEvent);
					break;
				case KEY_EVENT:
					handle_key_event(recs[i].Event.KeyEvent);
					break;
				case WINDOW_BUFFER_SIZE_EVENT:
					handle_window_event(recs[i].Event.WindowBufferSizeEvent);
					break;
			}
		}
	}

	GetConsoleScreenBufferInfo(hOutput, &bufInfo);

	DWORD curWidth = bufInfo.srWindow.Right - bufInfo.srWindow.Left;
	DWORD curHeight = bufInfo.srWindow.Bottom - bufInfo.srWindow.Top;

	if (curWidth != lastWidth || curHeight != lastHeight
			|| bufInfo.srWindow.Top != lastTop) {
		readConsole = true;
		redrawImages = true;
	}

	if (bufInfo.dwCursorPosition.X != lastCursorPos.X
			|| bufInfo.dwCursorPosition.Y != lastCursorPos.Y) {
		readConsole = true;
	}

	if (bufInfo.dwSize.X != lastSize.X || bufInfo.dwSize.Y == lastSize.Y) {
		readConsole = true;

		if (visibleBufferSize != bufInfo.dwSize.X * curHeight) {
			visibleBufferSize = bufInfo.dwSize.X * curHeight;
			visibleBuffer = static_cast<TCHAR*>(realloc(visibleBuffer,
					(visibleBufferSize + 1) * sizeof(TCHAR)));
		}
	}

	lastTop = bufInfo.srWindow.Top;
	lastWidth = curWidth;
	lastHeight = curHeight;

	lastSize = bufInfo.dwSize;
	lastCursorPos = bufInfo.dwCursorPosition;

	if (readConsole) {
		read_current_window();
	}

	update_images(deltaTime);

	if (redrawImages) {
		draw_images();
	}
}

unsigned long ConsoleImpl::write(const char* text, unsigned long length) {
	DWORD numWritten;
	WriteConsoleA(hOutput, text, length, &numWritten, NULL);

	return numWritten;
}

bool ConsoleImpl::is_attached() const {
	return attached;
}

void ConsoleImpl::read_current_window() {
	COORD curPos{bufInfo.srWindow.Left, bufInfo.srWindow.Top};
	DWORD numRead{};

	ReadConsoleOutputCharacter(hOutput, visibleBuffer, visibleBufferSize,
			curPos, &numRead);
	visibleBuffer[numRead] = '\0';

	const char* start = strstr(visibleBuffer, IMAGE_BEGIN_TAG);
	
	if (start) {
		const char* end = strstr(start, IMAGE_END_TAG);

		if (end) {
			const unsigned fileNameLen = end - start - IMAGE_BEGIN_TAG_LENGTH
					- IMAGE_END_TAG_LENGTH + 1;
			char* fileName = static_cast<char*>(malloc((fileNameLen + 1)
						* sizeof(char)));
			memcpy(fileName, start + IMAGE_BEGIN_TAG_LENGTH, fileNameLen);
			fileName[fileNameLen] = '\0';

			check_image_tag(start, fileName, fileNameLen);
			
			free(fileName);
		}
	}
}

void ConsoleImpl::check_image_tag(const char* tagStart, const char* fileName,
		unsigned fileNameLen) {
	Graphic* g = Graphic::load_file(hDC, fileName);

	if (g) {
		redrawImages = true;

		unsigned tagIndex = tagStart - visibleBuffer;
		unsigned tagY = tagIndex / bufInfo.dwSize.X;

		graphics.push_back({g, 0, tagY + 1});

		adjust_console_text(tagStart, fileNameLen);
	}
}

void ConsoleImpl::adjust_console_text(const char* tagStart,
		unsigned fileNameLen) {
	COORD curPos{bufInfo.srWindow.Left, bufInfo.srWindow.Top};
	unsigned bufHeight = bufInfo.dwSize.Y - curPos.Y;

	COORD bufferSize{bufInfo.dwSize.X, static_cast<SHORT>(bufHeight)};

	unsigned snapshotSize = bufferSize.X * bufferSize.Y;
	CHAR_INFO* snapshot = static_cast<CHAR_INFO*>(malloc(snapshotSize
			* sizeof(CHAR_INFO)));

	SMALL_RECT readRegion;
	readRegion.Left = bufInfo.srWindow.Left;
	readRegion.Top = bufInfo.srWindow.Top;
	readRegion.Right = bufInfo.dwSize.X;
	readRegion.Bottom = bufInfo.dwSize.Y - bufInfo.srWindow.Top;

	ReadConsoleOutput(hOutput, snapshot, bufferSize, curPos,
			&readRegion);

	unsigned fullTagLen = fileNameLen + IMAGE_BEGIN_TAG_LENGTH
			+ IMAGE_END_TAG_LENGTH;

	unsigned tagIndex = tagStart - visibleBuffer;
	unsigned tagY = tagIndex / bufInfo.dwSize.X;
	unsigned offset = fullTagLen
			+ bufInfo.dwSize.X * (CONSOLE_IMAGE_HEIGHT + 1);

	CHAR_INFO* snapTagStart = snapshot + tagIndex;

	// move remaining text after the keyword forward
	memmove(snapTagStart + offset, snapTagStart + fullTagLen,
			(snapshotSize - (tagIndex + offset)) * sizeof(CHAR_INFO));

	// zero out all text including & after the keyword, and before the rest of
	// the line, making room for the image
	for (CHAR_INFO *c = snapTagStart, *e = snapTagStart + offset; c != e;
			++c) {
		c->Char.AsciiChar = 0;
	}

	// slide non-null text to the left, since the shifted line is still
	// aligned to where the tag was
	CHAR_INFO* shiftOverLine = snapshot + (bufInfo.dwSize.X
			* (tagY + CONSOLE_IMAGE_HEIGHT + 1));
	
	for (CHAR_INFO *c = shiftOverLine, *e = shiftOverLine + bufInfo.dwSize.X;
			c != e; ++c) {
		// first nonzero char
		if (c->Char.AsciiChar) {
			unsigned remainingLen = e - c;
			memmove(shiftOverLine, c, remainingLen * sizeof(CHAR_INFO));
			
			for (CHAR_INFO *cc = shiftOverLine + remainingLen; cc != e; ++cc) {
				cc->Char.AsciiChar = 0;
			}

			break;
		}
	}

	SetConsoleCursorPosition(hOutput, curPos);
	WriteConsoleOutput(hOutput, snapshot, bufferSize, curPos, &readRegion);
	SetConsoleCursorPosition(hOutput, {0,
			static_cast<SHORT>(bufInfo.dwCursorPosition.Y
			+ CONSOLE_IMAGE_HEIGHT + 1)});

	free(snapshot);
}

void ConsoleImpl::update_images(double deltaTime) {
	for (auto& ge : graphics) {
		if (ge.graphic->update(deltaTime)) {
			redrawImages = true;
		}
	}
}

void ConsoleImpl::draw_images() {
	const unsigned imageHeight = CONSOLE_IMAGE_HEIGHT * fontInfo.dwFontSize.Y;
	
	for (auto& ge : graphics) {
		unsigned renderX = ge.posX - bufInfo.srWindow.Left;
		unsigned renderY = ge.posY - bufInfo.srWindow.Top;
		unsigned imageWidth = static_cast<unsigned>(imageHeight
				* ge.graphic->get_aspect_ratio());

		ge.graphic->render(renderX * fontInfo.dwFontSize.X,
				renderY * fontInfo.dwFontSize.Y, imageWidth, imageHeight);
	}
}

void ConsoleImpl::handle_mouse_event(const MOUSE_EVENT_RECORD& rec) {
}

void ConsoleImpl::handle_key_event(const KEY_EVENT_RECORD& rec) {
	if (rec.bKeyDown) {
		readConsole = true;
	}
}

void ConsoleImpl::handle_window_event(const WINDOW_BUFFER_SIZE_RECORD& rec) {
	readConsole = true;
	redrawImages = true;
}

// Console API

unsigned long Console::get_console_process_id() {
	unsigned long pid{};
	HWND hwnd = FindWindowA("ConsoleWindowClass", NULL);
	
	GetWindowThreadProcessId(hwnd, &pid);

	return pid;
}

Console::Console()
		: impl(new ConsoleImpl()) {}

bool Console::attach(unsigned long pid) {
	return impl->attach(pid);
}

void Console::update(double deltaTime) {
	impl->update(deltaTime);
}

bool Console::is_attached() const {
	return impl->is_attached();
}

unsigned long Console::write(const char* text, unsigned long length) {
	return impl->write(text, length);
}

Console::~Console() {
	delete impl;
}

static BOOL WINAPI console_ctrl_handler(DWORD dwCtrlType) {
	return TRUE;
}
