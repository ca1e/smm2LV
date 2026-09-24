#pragma once

#include <cstdint>
#include <string>
#include <vector>

// Minimal software raster canvas replacing cairo. Pixels are RGBA8888 in byte
// order R,G,B,A (straight, not premultiplied), uploadable to OpenGL as GL_RGBA.
class Canvas {
public:
	Canvas();
	Canvas(int width, int height);

	int GetWidth() const;
	int GetHeight() const;
	uint32_t* Pixels();
	const uint32_t* Pixels() const;

	void Resize(int width, int height);
	void Clear();

	// Decodes a PNG from memory. The caller keeps `data` alive across the call.
	bool LoadPngFromMemory(const unsigned char* data, size_t size);
	bool WritePng(const std::string& path) const;
	bool WritePngBytes(std::vector<unsigned char>& out) const;

	// Nearest-neighbour scaled blit with alpha, clipped to the destination.
	void Blit(const Canvas& source, int sx, int sy, int sw, int sh, int dx, int dy, int dw, int dh, double alpha = 1.0);
	// Source rect scaled to (dw, dh) and rotated by angle radians around (centerX, centerY).
	void BlitRot(const Canvas& source, int sx, int sy, int sw, int sh, double centerX, double centerY, double dw,
		double dh, double angle, double alpha = 1.0);
	// color is 0xRRGGBBAA.
	void FillRect(int x, int y, int w, int h, uint32_t color);
	void HLine(int x1, int x2, int y, uint32_t color);
	void VLine(int x, int y1, int y2, uint32_t color);
	// Digits and '-', top-left anchored.
	void Text(const std::string& text, int x, int y, uint32_t color, int height);
	// Soft radial blob for heatmap overlays; color in the canvas-internal
	// 0xAABBGGRR byte order, alpha decays quadratically from maxAlpha (0-255)
	// at the center to 0 at radius.
	void HeatDot(int cx, int cy, int radius, uint32_t color, int maxAlpha);
	// Filled circle with a contrasting border ring, for pin-like markers.
	void DrawCircle(int cx, int cy, int radius, uint32_t fillColor, uint32_t borderColor);

private:
	void BlendPixel(int x, int y, uint32_t color);
	int W = 0;
	int H = 0;
	std::vector<uint32_t> px;
};
