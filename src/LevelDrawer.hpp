#pragma once

#include "Canvas.hpp"
#include "LevelData.hpp"
#include "LevelParser.hpp"

#include <cstdint>
#include <filesystem>
#include <fmt/core.h>
#include <iostream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

class LevelDrawer {
public:
	using Point = LevelParser::Point;

	// A sub-rectangle of a source canvas to draw from.
	struct SrcRect {
		const Canvas* image = nullptr;
		int x               = 0;
		int y               = 0;
		int w               = 0;
		int h               = 0;
	};

	LevelDrawer(LevelParser& level_, int zoom)
		: level(level_) {
		Zm           = zoom;
		H            = level.MapHdr.BorT / 16;
		W            = level.MapHdr.BorR / 16;
		renderWidth  = (level.MapHdr.BorR / 16) * Zm;
		renderHeight = (level.MapHdr.BorT / 16) * Zm;
	};

	void Setup();
	void SetGraphics(Canvas* graphics);
	void LoadTilesheet();
	// PNG bytes for "img/tile/…" (per world) and "img/spritesheet.png", injected
	// by the host; LoadTilesheet decodes them from memory.
	void SetTilesheetData(const unsigned char* data, size_t size);
	void SetSpritesheetData(const unsigned char* data, size_t size);
	void SetIsOverworld(bool isOverworld);
	void SetLog(bool log);
	void SetOnlyInstructions();
	std::vector<DrawingInstruction>& GetInstructions();
	int GetWidth();
	int GetHeight();
	void ClearImageCache();
	void DrawBackground();
	void DrawGridlines();
	SrcRect GetCachedTile(int x, int y, int w, int h);
	SrcRect GetCachedPattern(int id);
	void DrawTile(int tileX, int tileY, int tileW, int tileH, int x, int y, int targetWidth, int targetHeight);
	void DrawImage(uint32_t id, int x, int y, int targetWidth, int targetHeight);
	void DrawImageOpacity(uint32_t id, double opacity, int x, int y, int targetWidth, int targetHeight);
	void DrawImageRotate(uint32_t id, double angle, int x, int y, int targetWidth, int targetHeight);
	void DrawImageRotateOpacity(
		uint32_t id, double angle, double opacity, int x, int y, int targetWidth, int targetHeight);
	void DrawCrp(unsigned char EX, int X, int Y);
	void DrawSnake(unsigned char EX, int X, int Y, int SW, int SH);
	void DrawMoveBlock(unsigned char ID, unsigned char EX, int X, int Y);
	void DrawItem(const std::unordered_set<short>& K, bool L);
	void ReGrdCode();
	void DrawGrd();
	int GetGrdType(const std::string& A);
	std::string GetGrdCode(int x, int y);
	int GetGrdBold(int x, int y);
	Point GetCorCode(int x, int y);
	int CalC(int x1, int y1, int x2, int y2, int x3, int y3, int x4, int y4);
	void DrawGrdCode();
	void DrawIce();
	void DrawTrack();
	void DrawCID();
	void DrawFireBar();
	void DrawFire();
	void DrawCPipe();

private:
	int TileW;
	int H            = 0;
	int W            = 0;
	int renderWidth  = 0;
	int renderHeight = 0;
	int Zm           = 16;
	int NowIO        = 0;

	Canvas* canvas = nullptr;
	LevelParser& level;
	bool doLogging = false;

	std::string tilesheetPath;
	const unsigned char* tilesheetData   = nullptr;
	size_t tilesheetSize                 = 0;
	const unsigned char* spritesheetData = nullptr;
	size_t spritesheetSize               = 0;

	bool addDrawingInstructions = true;
	bool noRender               = false;
	std::vector<DrawingInstruction> drawingInstructions;

	std::unordered_map<int, SrcRect> tileCache;
	std::unordered_map<int, SrcRect> patternCache;
	Canvas tilesheet;
	Canvas spritesheet;
};