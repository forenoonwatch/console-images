#include "graphic.hpp"

#include "stb_image.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cctype>

static void image_rgb_to_bgr(unsigned char* data, unsigned width,
		unsigned height);

static unsigned file_ext_get(char* dest, unsigned destSize, const char* src,
		unsigned srcSize);

// Graphic

Graphic* Graphic::load_file(HDC hWindowDC, const char* fileName) {
	char fileExt[10];
	unsigned fileExtLen = file_ext_get(fileExt, 10, fileName,
			strlen(fileName));

	if (!fileExtLen) {
		return nullptr;
	}
	else {
		for (unsigned i = 0; i < fileExtLen; ++i) {
			fileExt[i] = tolower(fileExt[i]);
		}
	}

	Graphic* obj = nullptr;

	int width, height, numComponents;

	stbi_set_flip_vertically_on_load(true);

	if (strncmp(fileExt, "gif", fileExtLen) == 0) {
		FILE* file = fopen(fileName, "rb");

		if (file) {
			fseek(file, 0, SEEK_END);
			unsigned long len = ftell(file);
			rewind(file);

			unsigned char* rawData = static_cast<unsigned char*>(malloc(len));
			fread(rawData, 1, len, file);
			fclose(file);

			int* rawDelays;
			int numFrames;
			unsigned char* imageData = stbi_load_gif_from_memory(rawData, len,
					&rawDelays, &width, &height, &numFrames, &numComponents,
					4);

			free(rawData);

			obj = new AnimatedGIF(hWindowDC, imageData, width, height,
					numFrames, rawDelays);

			stbi_image_free(rawDelays);
			stbi_image_free(imageData);
		}
	}
	else {
		unsigned char* data = stbi_load(fileName, &width, &height, &numComponents,
				4);

		if (data) {
			image_rgb_to_bgr(data, width, height);
			obj = new Image(hWindowDC, data, width, height);
		}

		stbi_image_free(data);
	}

	return obj;
}

bool Graphic::update(double deltaTime) {
	return false;
}

void Graphic::render(unsigned x, unsigned y, unsigned sizeX, unsigned sizeY) {
	StretchBlt(hWindowDC, x, y, sizeX, sizeY, hDC, 0, 0, width, height,
			SRCCOPY);
}

double Graphic::get_aspect_ratio() const {
	return static_cast<double>(width) / static_cast<double>(height);
}

Graphic::~Graphic() {
	DeleteObject(hBitmap);
	DeleteDC(hDC);
}

Graphic::Graphic(HDC hWindowDC, const unsigned char* data, unsigned width,
			unsigned height)
		: hWindowDC(hWindowDC)
		, hDC(CreateCompatibleDC(hWindowDC))
		, width(width)
		, height(height) {
	BITMAPINFO info{};
	info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
	info.bmiHeader.biWidth = width;
	info.bmiHeader.biHeight = height;
	info.bmiHeader.biPlanes = 1;
	info.bmiHeader.biBitCount = GetDeviceCaps(hDC, BITSPIXEL);
	info.bmiHeader.biCompression = BI_RGB;

	hBitmap = CreateDIBSection(hDC, &info, DIB_RGB_COLORS, &baseMemory, NULL,
			0);

	memcpy(baseMemory, data, width * height * 4);

	SelectObject(hDC, hBitmap);
}

// Image

Image::Image(HDC hWindowDC, const unsigned char* data, unsigned width,
			unsigned height)
		: Graphic(hWindowDC, data, width, height) {}

// AnimatedGIF

AnimatedGIF::AnimatedGIF(HDC hWindowDC, const unsigned char* dataIn,
			unsigned width, unsigned height, unsigned numFrames,
			const int* delaysIn)
		: Graphic(hWindowDC, dataIn, width, height)
		, numFrames(numFrames)
		, passedTime(0)
		, frameIndex(0)
		, lastSetFrame(0) {
	unsigned frameSize = width * height * 4;
	unsigned dataSize = frameSize * numFrames;

	data = static_cast<unsigned char*>(malloc(dataSize));
	memcpy(data, dataIn, dataSize);

	for (unsigned i = 0; i < numFrames; ++i) {
		image_rgb_to_bgr(data + frameSize * i, width, height);
	}
	
	delays = static_cast<double*>(malloc(numFrames * sizeof(double)));

	for (unsigned i = 0; i < numFrames; ++i) {
		delays[i] = static_cast<double>(delaysIn[i]) * 0.001;
	}
}

AnimatedGIF::~AnimatedGIF() {
	free(delays);
	free(data);
}

bool AnimatedGIF::update(double deltaTime) {
	passedTime += deltaTime;

	while (passedTime >= delays[frameIndex]) {
		passedTime -= delays[frameIndex];
		frameIndex = (frameIndex + 1) % numFrames;
	}

	if (frameIndex != lastSetFrame) {
		lastSetFrame = frameIndex;

		const unsigned frameSize = width * height * 4;
		memcpy(baseMemory, data + frameSize * frameIndex, frameSize);

		return true;
	}

	return false;
}

static void image_rgb_to_bgr(unsigned char* data, unsigned width,
		unsigned height) {
	unsigned char temp;

	for (unsigned i = 0, l = width * height * 4; i < l; i += 4) {
		temp = data[i];
		data[i] = data[i + 2];
		data[i + 2] = temp;
	}
}

static unsigned file_ext_get(char* dest, unsigned destSize, const char* src,
		unsigned srcSize) {
	const char* p = src + srcSize - 1;

	while (p > src && *p != '.' && *p) {
		--p;
	}

	if (*p == '.') {
		++p;
	}

	unsigned res = srcSize - (p - src);

	if (res <= destSize) {
		memcpy(dest, p, res);
		return res;
	}

	return 0;
}

