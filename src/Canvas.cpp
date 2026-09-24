#include "Canvas.hpp"

#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#include "stb/stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb/stb_image_write.h"

#include <algorithm>
#include <cmath>

Canvas::Canvas() = default;

Canvas::Canvas(int width, int height) {
	Resize(width, height);
}

int Canvas::GetWidth() const {
	return W;
}

int Canvas::GetHeight() const {
	return H;
}

uint32_t* Canvas::Pixels() {
	return px.data();
}

const uint32_t* Canvas::Pixels() const {
	return px.data();
}

void Canvas::Resize(int width, int height) {
	W = width;
	H = height;
	px.assign((size_t)W * H, 0);
}

void Canvas::Clear() {
	std::fill(px.begin(), px.end(), 0);
}

bool Canvas::LoadPngFromMemory(const unsigned char* data, size_t size) {
	int channels = 0;
	unsigned char* decoded = stbi_load_from_memory(data, (int)size, &W, &H, &channels, 4);
	if(decoded == nullptr) {
		return false;
	}

	px.resize((size_t)W * H);
	uint32_t* out = px.data();
	for(size_t i = 0; i < (size_t)W * H; i++) {
		out[i] = (uint32_t)decoded[i * 4 + 0] | ((uint32_t)decoded[i * 4 + 1] << 8) | ((uint32_t)decoded[i * 4 + 2] << 16)
				 | ((uint32_t)decoded[i * 4 + 3] << 24);
	}

	stbi_image_free(decoded);
	return true;
}

static void stbi_write_callback(void* context, void* data, int size) {
	((std::vector<unsigned char>*)context)->insert(((std::vector<unsigned char>*)context)->end(), (unsigned char*)data,
		(unsigned char*)data + size);
}

bool Canvas::WritePng(const std::string& path) const {
	return stbi_write_png(path.c_str(), W, H, 4, px.data(), W * 4) != 0;
}

bool Canvas::WritePngBytes(std::vector<unsigned char>& out) const {
	return stbi_write_png_to_func(stbi_write_callback, &out, W, H, 4, px.data(), W * 4) != 0;
}

void Canvas::BlendPixel(int x, int y, uint32_t color) {
	if(x < 0 || y < 0 || x >= W || y >= H) {
		return;
	}

	uint32_t sa = (color >> 24);
	if(sa == 0) {
		return;
	}

	uint32_t& dst = px[(size_t)y * W + x];
	if(sa == 255) {
		dst = color;
		return;
	}

	uint32_t da = (dst >> 24);
	uint32_t na = sa + da * (255 - sa) / 255;

	uint32_t out = na << 24;
	for(int shift = 0; shift < 24; shift += 8) {
		uint32_t sc = (color >> shift) & 0xFF;
		uint32_t dc = (dst >> shift) & 0xFF;
		out |= (((sc * sa + dc * (255 - sa)) / 255) & 0xFF) << shift;
	}

	dst = out;
}

void Canvas::Blit(const Canvas& source, int sx, int sy, int sw, int sh, int dx, int dy, int dw, int dh, double alpha) {
	if(sw <= 0 || sh <= 0 || dw <= 0 || dh <= 0 || source.Pixels() == nullptr) {
		return;
	}

	uint32_t amul = (uint32_t)(alpha * 255.0);
	if(amul == 0) {
		return;
	}

	int x0 = dx < 0 ? -dx : 0;
	int y0 = dy < 0 ? -dy : 0;
	int x1 = dw < W - dx ? dw : W - dx;
	int y1 = dh < H - dy ? dh : H - dy;

	for(int ty = y0; ty < y1; ty++) {
		int srow = sy + (int)((int64_t)ty * sh / dh);
		if(srow < 0) {
			srow = 0;
		}
		if(srow >= source.H) {
			srow = source.H - 1;
		}
		for(int tx = x0; tx < x1; tx++) {
			int scol = sx + (int)((int64_t)tx * sw / dw);
			if(scol < 0) {
				scol = 0;
			}
			if(scol >= source.W) {
				scol = source.W - 1;
			}

			uint32_t color = source.px[(size_t)srow * source.W + scol];
			if(amul < 255) {
				uint32_t ca = ((color >> 24) * amul) / 255;
				color       = (color & 0x00FFFFFF) | (ca << 24);
			}
			BlendPixel(dx + tx, dy + ty, color);
		}
	}
}

void Canvas::BlitRot(const Canvas& source, int sx, int sy, int sw, int sh, double centerX, double centerY, double dw,
	double dh, double angle, double alpha) {
	if(sw <= 0 || sh <= 0 || dw <= 0 || dh <= 0 || source.Pixels() == nullptr) {
		return;
	}

	uint32_t amul = (uint32_t)(alpha * 255.0);
	if(amul == 0) {
		return;
	}

	double hw  = dw / 2.0;
	double hh  = dh / 2.0;
	double rad = angle;
	double c   = std::cos(rad);
	double s   = std::sin(rad);

	// Destination-space bounding box of the rotated rect
	double cornersX[4] = {-hw, hw, hw, -hw};
	double cornersY[4] = {-hh, -hh, hh, hh};
	double minX = 1e18, minY = 1e18, maxX = -1e18, maxY = -1e18;
	for(int i = 0; i < 4; i++) {
		double rx = centerX + cornersX[i] * c - cornersY[i] * s;
		double ry = centerY + cornersX[i] * s + cornersY[i] * c;
		if(rx < minX) minX = rx;
		if(rx > maxX) maxX = rx;
		if(ry < minY) minY = ry;
		if(ry > maxY) maxY = ry;
	}

	int x0 = (int)std::floor(minX) - 1;
	int y0 = (int)std::floor(minY) - 1;
	int x1 = (int)std::ceil(maxX) + 1;
	int y1 = (int)std::ceil(maxY) + 1;
	if(x0 < 0) x0 = 0;
	if(y0 < 0) y0 = 0;
	if(x1 > W) x1 = W;
	if(y1 > H) y1 = H;

	for(int py = y0; py < y1; py++) {
		double dyPix = py + 0.5 - centerY;
		for(int pxi = x0; pxi < x1; pxi++) {
			double dxPix = pxi + 0.5 - centerX;

			// Inverse rotation into unrotated destination rect
			double lx = dxPix * c + dyPix * s;
			double ly = -dxPix * s + dyPix * c;
			if(lx < -hw || lx >= hw || ly < -hh || ly >= hh) {
				continue;
			}

			int scol = sx + (int)((lx + hw) / dw * sw);
			int srow = sy + (int)((ly + hh) / dh * sh);
			if(scol < 0) scol = 0;
			if(srow < 0) srow = 0;
			if(scol >= source.W) scol = source.W - 1;
			if(srow >= source.H) srow = source.H - 1;

			uint32_t color = source.px[(size_t)srow * source.W + scol];
			if(amul < 255) {
				uint32_t ca = ((color >> 24) * amul) / 255;
				color       = (color & 0x00FFFFFF) | (ca << 24);
			}
			BlendPixel(pxi, py, color);
		}
	}
}

void Canvas::FillRect(int x, int y, int w, int h, uint32_t color) {
	int x0 = x < 0 ? 0 : x;
	int y0 = y < 0 ? 0 : y;
	int x1 = x + w < W ? x + w : W;
	int y1 = y + h < H ? y + h : H;

	for(int py = y0; py < y1; py++) {
		for(int pxi = x0; pxi < x1; pxi++) {
			BlendPixel(pxi, py, color);
		}
	}
}

void Canvas::HLine(int x1, int x2, int y, uint32_t color) {
	FillRect(x1, y, x2 - x1, 1, color);
}

void Canvas::VLine(int x, int y1, int y2, uint32_t color) {
	FillRect(x, y1, 1, y2 - y1, color);
}

void Canvas::HeatDot(int cx, int cy, int radius, uint32_t color, int maxAlpha) {
	int r = color & 0xFF;
	int g = (color >> 8) & 0xFF;
	int b = (color >> 16) & 0xFF;
	int x0 = cx - radius < 0 ? 0 : cx - radius;
	int y0 = cy - radius < 0 ? 0 : cy - radius;
	int x1 = cx + radius < W ? cx + radius : W;
	int y1 = cy + radius < H ? cy + radius : H;

	for(int py = y0; py < y1; py++) {
		for(int px = x0; px < x1; px++) {
			int dx = px - cx, dy = py - cy;
			int d2 = dx * dx + dy * dy;
			if(d2 > radius * radius) continue;
			double t = 1.0 - std::sqrt((double)d2) / radius;
			int a = (int)(maxAlpha * t * t);
			if(a <= 0) continue;
			BlendPixel(px, py, ((uint32_t)a << 24) | ((uint32_t)b << 16) | ((uint32_t)g << 8) | r);
		}
	}
}

void Canvas::DrawCircle(int cx, int cy, int radius, uint32_t fillColor, uint32_t borderColor) {
	for(int dy = -radius - 1; dy <= radius + 1; dy++) {
		for(int dx = -radius - 1; dx <= radius + 1; dx++) {
			int d = dx * dx + dy * dy;
			if(d > (radius + 1) * (radius + 1)) continue;
			uint32_t color = d <= radius * radius ? fillColor : borderColor;
			BlendPixel(cx + dx, cy + dy, color);
		}
	}
}

// 3x5 digit font, one entry per glyph, 5 row masks of 3 bits (top to bottom).
static const uint8_t digitGlyphs[11][5] = {
	{0b111, 0b101, 0b101, 0b101, 0b111}, // 0
	{0b010, 0b110, 0b010, 0b010, 0b111}, // 1
	{0b111, 0b001, 0b111, 0b100, 0b111}, // 2
	{0b111, 0b001, 0b111, 0b001, 0b111}, // 3
	{0b101, 0b101, 0b111, 0b001, 0b001}, // 4
	{0b111, 0b100, 0b111, 0b001, 0b111}, // 5
	{0b111, 0b100, 0b111, 0b101, 0b111}, // 6
	{0b111, 0b001, 0b001, 0b010, 0b010}, // 7
	{0b111, 0b101, 0b111, 0b101, 0b111}, // 8
	{0b111, 0b101, 0b111, 0b001, 0b111}, // 9
	{0b000, 0b000, 0b111, 0b000, 0b000}, // -
};

void Canvas::Text(const std::string& text, int x, int y, uint32_t color, int height) {
	if(height < 5) {
		height = 5;
	}

	int scale = height / 5;
	int glyphW = 3 * scale;
	int spacing = scale;

	int cursor = x;
	for(char ch : text) {
		int glyph = -1;
		if(ch >= '0' && ch <= '9') {
			glyph = ch - '0';
		} else if(ch == '-') {
			glyph = 10;
		} else {
			cursor += glyphW + spacing;
			continue;
		}

		for(int row = 0; row < 5; row++) {
			for(int col = 0; col < 3; col++) {
				if(digitGlyphs[glyph][row] & (1 << (2 - col))) {
					FillRect(cursor + col * scale, y + row * scale, scale, scale, color);
				}
			}
		}

		cursor += glyphW + spacing;
	}
}
