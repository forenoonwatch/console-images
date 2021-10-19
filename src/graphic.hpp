#pragma once

#include <windows.h>

class Graphic {
	public:
		[[nodiscard]] static Graphic* load_file(HDC hWindowDC,
				const char* fileName);

		/* Returns whether the image should be redrawn */
		virtual bool update(double deltaTime);

		void render(unsigned x, unsigned y, unsigned sizeX,
				unsigned sizeY);

		double get_aspect_ratio() const;

		virtual ~Graphic();
	protected:
		Graphic(HDC hWindowDC, const unsigned char* data, unsigned width,
				unsigned height);

		HDC hWindowDC;
		HDC hDC;
		HBITMAP hBitmap;
		void* baseMemory;
		unsigned width;
		unsigned height;
};

class Image final : public Graphic {
	public:
		Image(HDC hWindowDC, const unsigned char* data, unsigned width,
				unsigned height);
	private:
};

class AnimatedGIF final : public Graphic {
	public:
		AnimatedGIF(HDC hWindowDC, const unsigned char* data, unsigned width,
				unsigned height, unsigned numFrames, const int* delays);
		~AnimatedGIF();

		bool update(double deltaTime) override;
	private:
		unsigned char* data;
		unsigned numFrames;
		double* delays;

		double passedTime;

		unsigned frameIndex;
		unsigned lastSetFrame;
};

