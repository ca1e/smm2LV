// WASM module API: core level logic only (decrypt, parse, render, export).
// All UI, networking and input live in the JavaScript side (index.html).
// Built with wasi-sdk targeting wasm32-wasip1; functions are exported via
// __attribute__((export_name)), assets arrive as in-memory PNG bytes.

#include "Canvas.hpp"
#include "LevelDrawer.hpp"
#include "LevelParser.hpp"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <fmt/core.h>
#include <gzip/decompress.hpp>
#include <rapidjson/document.h>
#include <rapidjson/writer.h>
#include <rapidjson/stringbuffer.h>
#include <string>
#include <unordered_set>
#include <vector>

static bool remove_grid               = false;
static bool render_objects_over_pipes = false;
static float image_scale              = 1.0f;

static LevelParser* parsers[2] = { nullptr, nullptr };
static LevelDrawer* drawers[2] = { nullptr, nullptr };

// Host-injected asset PNGs: [0] overworld tilesheet, [1] subworld tilesheet,
// [2] spritesheet.
static std::vector<unsigned char> assetBytes[3];

// Overlay data, parsed from the API's JSON shapes by the core (calibration of
// game-space coordinates to render pixels lives here, not in the frontend).
struct DeathPoint {
	int x, y;
	bool sub;
};
static std::vector<DeathPoint> deathPoints;

struct CommentMark {
	int x, y; // game-space pixels, as served by the API
	std::string text, poster, when, type;
	bool beaten;
};
static std::vector<CommentMark> commentMarks;

static std::vector<unsigned char> pngResult;
static std::string stringResult;

// Game-space -> render-space: the API serves x/y in game pixels with y up,
// identical units to the level boundaries (verified against NN5RC1LCG).
static void toRenderSpace(int gameX, int gameY, const LevelParser& level, int& px, int& py) {
	px = gameX - level.MapHdr.BorL;
	py = level.MapHdr.BorT - gameY;
}

// Comments don't carry a subworld flag; assign to the world whose boundaries
// contain the point, overworld first when both do.
static int commentWorld(const CommentMark& c) {
	for(int world = 0; world < 2; world++) {
		LevelParser* lv = parsers[world];
		if(lv == nullptr) continue;
		int px, py;
		toRenderSpace(c.x, c.y, *lv, px, py);
		if(px >= 0 && px <= lv->MapHdr.BorR - lv->MapHdr.BorL && py >= 0 && py <= lv->MapHdr.BorT) {
			return world;
		}
	}
	return -1;
}

// ---------------------------------------------------------------------------
// Core renderer: draws one world into a fresh Canvas (caller owns it).
// ---------------------------------------------------------------------------

static Canvas* render_world(int isOverworld) {
	LevelParser* level = parsers[isOverworld];
	if(level == nullptr) {
		return nullptr;
	}

	LevelDrawer* drawer = new LevelDrawer(*level, 16);
	drawer->Setup();
	// isOverworld is a world INDEX here (0 = overworld, 1 = subworld);
	// SetIsOverworld wants the actual flag, which drives goal/start placement.
	drawer->SetIsOverworld(isOverworld == 0);
	drawer->SetTilesheetData(assetBytes[isOverworld].data(), assetBytes[isOverworld].size());
	drawer->SetSpritesheetData(assetBytes[2].data(), assetBytes[2].size());

	Canvas* canvas = new Canvas(drawer->GetWidth(), drawer->GetHeight());
	drawer->SetGraphics(canvas);
	drawer->LoadTilesheet();

	drawer->DrawBackground();

	if(!remove_grid) {
		drawer->DrawGridlines();
	}

	// 3D平台 半碰撞 蘑菇平台 桥 蘑菇跳台 开关跳台
	drawer->DrawItem({ 132 }, false);
	drawer->DrawItem({ 16 }, false);
	drawer->DrawItem({ 14 }, false);
	drawer->DrawItem({ 17 }, false);
	drawer->DrawItem({ 113 }, false);
	drawer->DrawItem({ 71 }, false);

	// 箭头 单向板 中间旗 藤蔓
	drawer->DrawItem({ 66, 67, 106 }, false);
	drawer->DrawItem({ 64 }, false);
	drawer->DrawItem({ 90 }, false);

	// 树 长长吞食花
	drawer->DrawItem({ 106, 107 }, false);

	// 地面 传送带 开关 开关砖 P砖 冰锥 斜坡单独
	drawer->ReGrdCode();
	drawer->DrawGrd();
	drawer->DrawGrdCode();

	drawer->DrawItem({ 53, 94, 99, 100, 79 }, false);
	drawer->DrawIce();

	// 狼牙棒
	// 2026-01-24: render skewers before pipes so they don't get obscured.
	drawer->DrawItem({ 83 }, false);

	// 管道 门 蛇 传送箱
	drawer->DrawItem({ 9, 55, 84, 97 }, false);
	// 机动砖 轨道砖
	drawer->DrawItem({ 85, 119 }, false);
	// 夹子
	drawer->DrawItem({ 105 }, false);
	// 轨道
	drawer->DrawTrack();
	// 软砖 问号 硬砖 竹轮 云 音符 隐藏 刺 冰块 闪烁砖
	drawer->DrawItem({ 4, 5, 6, 21, 22, 23, 29, 43, 63, 110, 108 }, false);

	// 跷跷板 熔岩台 升降台
	drawer->DrawItem({ 91, 36, 11 }, false);

	// 齿轮 甜甜圈
	drawer->DrawItem({ 68, 82 }, false);

	// 道具
	drawer->DrawItem({ 0, 1, 2, 3, 8, 10, 12, 13, 15, 18, 19, 20, 25, 28, 30, 31, 32, 33, 34, 35, 39 }, false);
	drawer->DrawItem({ 40, 41, 42, 44, 45, 46, 47, 48, 52, 56, 57, 58, 60, 61, 62, 70, 74, 76, 77, 78, 81, 92, 95, 98,
						 102, 103, 104 },
		false);
	drawer->DrawItem(
		{ 111, 120, 121, 122, 123, 124, 125, 126, 112, 127, 128, 129, 130, 131, 72, 50, 51, 65, 80, 114, 116 }, false);
	drawer->DrawItem({ 96, 117, 86 }, false);
	// 喷枪 火棍
	drawer->DrawItem({ 24, 54 }, false);

	drawer->DrawItem({ 105 }, false);
	drawer->DrawTrack();
	drawer->DrawItem({ 105 }, true);

	// LINK
	drawer->DrawItem({ 4, 5, 6, 21, 22, 23, 29, 43, 63 }, true);

	drawer->DrawItem({ 91, 36, 11 }, true);
	drawer->DrawItem({ 68, 82 }, true);

	drawer->DrawItem({ 0, 1, 2, 3, 8, 10, 12, 13, 15, 18, 19, 20, 25, 28, 30, 31, 32, 33, 34, 35, 39 }, true);
	drawer->DrawItem({ 40, 41, 42, 44, 45, 46, 47, 48, 52, 56, 57, 58, 60, 61, 62, 70, 74, 76, 77, 78, 81, 92, 95, 98,
						 102, 103, 104 },
		true);
	drawer->DrawItem(
		{ 111, 120, 121, 122, 123, 124, 125, 126, 112, 127, 128, 129, 130, 131, 72, 50, 51, 65, 80, 114, 116 }, true);
	drawer->DrawItem({ 96, 117, 86 }, true);

	drawer->DrawCID();

	// 喷枪 火棍
	drawer->DrawItem({ 24, 54 }, true);
	drawer->DrawFireBar();
	drawer->DrawFire();

	// 透明管
	drawer->DrawCPipe();

	if(!render_objects_over_pipes) {
		drawer->DrawItem({ 9, 42 }, true);
	}

	drawer->ClearImageCache();

	delete drawers[isOverworld];
	drawers[isOverworld] = drawer;
	return canvas;
}

static std::string escape_html(const std::string& s) {
	std::string out;
	out.reserve(s.size());
	for(char c : s) {
		switch(c) {
		case '&': out += "&amp;"; break;
		case '<': out += "&lt;"; break;
		case '>': out += "&gt;"; break;
		case '"': out += "&quot;"; break;
		default: out += c;
		}
	}
	return out;
}

#define WEB_EXPORT(name) extern "C" __attribute__((export_name(name)))

// Loads level bytes (.bcd as served by the API, plain, or gzipped) and parses
// both worlds. Returns 0 on success, negative on error.
WEB_EXPORT("web_load_level") int web_load_level(unsigned char* data, int len) {
	std::string input((char*)data, len);
	std::string content;

	if(len == 0x5C000) {
		if(!LevelParser::DecryptLevelData(input, content)) {
			return -2;
		}
	} else if(len == 0x5BFC0) {
		content = input;
	} else if(len >= 2 && (unsigned char)input[0] == 0x78
			  && ((unsigned char)input[1] == 0x01 || (unsigned char)input[1] == 0x5E
				  || (unsigned char)input[1] == 0x9C || (unsigned char)input[1] == 0xDA)) {
		content = gzip::decompress(input.data(), input.size());
	} else {
		return -1;
	}

	if(content.size() != 0x5BFC0) {
		return -1;
	}

	delete parsers[0];
	delete parsers[1];
	delete drawers[0];
	delete drawers[1];
	parsers[0] = nullptr;
	parsers[1] = nullptr;
	drawers[0] = nullptr;
	drawers[1] = nullptr;
	deathPoints.clear();
	commentMarks.clear();

	parsers[0] = new LevelParser();
	parsers[0]->LoadLevelData(content, true);
	parsers[1] = new LevelParser();
	parsers[1]->LoadLevelData(content, false);
	return 0;
}

// Injects an asset PNG: kind 0/1 = overworld/subworld tilesheet, 2 = spritesheet.
// The bytes are copied, so the host may free its copy right after the call.
WEB_EXPORT("web_set_asset") int web_set_asset(int kind, unsigned char* data, int len) {
	if(kind < 0 || kind > 2 || data == nullptr || len <= 0) {
		return -1;
	}
	assetBytes[kind].assign(data, data + len);
	return 0;
}

// Overlay inputs, as JSON byte arrays straight from the API responses.
WEB_EXPORT("web_set_deaths") int web_set_deaths(unsigned char* data, int len) {
	if(data == nullptr || len <= 0) {
		return -1;
	}
	deathPoints.clear();
	rapidjson::Document doc;
	if(doc.Parse((char*)data, len).HasParseError() || !doc.IsArray()) {
		return -2;
	}
	for(const auto& v : doc.GetArray()) {
		if(!v.IsObject() || !v.HasMember("x") || !v.HasMember("y")) continue;
		deathPoints.push_back({ v["x"].GetInt(), v["y"].GetInt(), v["is_subworld"].GetBool() });
	}
	return 0;
}

WEB_EXPORT("web_set_comments") int web_set_comments(unsigned char* data, int len) {
	if(data == nullptr || len <= 0) {
		return -1;
	}
	commentMarks.clear();
	rapidjson::Document doc;
	if(doc.Parse((char*)data, len).HasParseError() || !doc.IsArray()) {
		return -2;
	}
	for(const auto& v : doc.GetArray()) {
		if(!v.IsObject() || !v.HasMember("x") || !v.HasMember("y")) continue;
		auto str = [&](const char* key) -> std::string {
			return v.HasMember(key) && v[key].IsString() ? v[key].GetString() : "";
		};
		commentMarks.push_back({ v["x"].GetInt(), v["y"].GetInt(), str("t"), str("p"), str("w"), str("ty"),
			v.HasMember("b") && v["b"].IsInt() && v["b"].GetInt() != 0 });
	}
	return 0;
}

// Renders the overlay layer (transparent PNG): death heatmap blobs under
// comment pins. Returns the PNG byte count, or <= 0 on error.
WEB_EXPORT("web_render_overlay") int web_render_overlay(int world, int deathsOn, int commentsOn) {
	LevelParser* level = parsers[world ? 1 : 0];
	if(level == nullptr) {
		return -1;
	}

	int width  = level->MapHdr.BorR - level->MapHdr.BorL;
	int height = level->MapHdr.BorT;
	Canvas canvas(width, height);

	if(deathsOn) {
		for(const DeathPoint& d : deathPoints) {
			if(d.sub != (world != 0)) continue;
			int px, py;
			toRenderSpace(d.x, d.y, *level, px, py);
			// red-orange in 0xAABBGGRR: r=FF g=30 b=20
			canvas.HeatDot(px, py, 110, 0x2030FF, 110);
		}
	}
	if(commentsOn) {
		for(const CommentMark& c : commentMarks) {
			if(commentWorld(c) != (world ? 1 : 0)) continue;
			int px, py;
			toRenderSpace(c.x, c.y, *level, px, py);
			// blue pin with a light ring, in 0xAABBGGRR
			canvas.DrawCircle(px, py, 5, 0xFFF08030, 0xFFE8E8E8);
		}
	}

	pngResult.clear();
	canvas.WritePngBytes(pngResult);
	return (int)pngResult.size();
}

// Comment pins converted to render pixels for the host's hover hit-testing.
// The string stays valid until the next call.
WEB_EXPORT("web_get_comment_marks") const char* web_get_comment_marks(int world) {
	LevelParser* level = parsers[world ? 1 : 0];
	if(level == nullptr) {
		return "";
	}
	rapidjson::StringBuffer sb;
	rapidjson::Writer<rapidjson::StringBuffer> writer(sb);
	writer.StartArray();
	for(const CommentMark& c : commentMarks) {
		if(commentWorld(c) != (world ? 1 : 0)) continue;
		int px, py;
		toRenderSpace(c.x, c.y, *level, px, py);
		writer.StartObject();
		writer.Key("px");
		writer.Int(px);
		writer.Key("py");
		writer.Int(py);
		writer.Key("text");
		writer.String(c.text);
		writer.Key("poster");
		writer.String(c.poster);
		writer.Key("when");
		writer.String(c.when);
		writer.Key("type");
		writer.String(c.type);
		writer.Key("beaten");
		writer.Bool(c.beaten);
		writer.EndObject();
	}
	writer.EndArray();
	stringResult = sb.GetString();
	return stringResult.c_str();
}

// Renders one world into an in-memory PNG. Returns the PNG byte count, or <= 0 on error.
WEB_EXPORT("web_render_level") int web_render_level(int world) {
	Canvas* canvas = render_world(world);
	if(canvas == nullptr) {
		return -1;
	}

	pngResult.clear();
	if(image_scale != 1.0f) {
		Canvas* scaled = new Canvas((int)(canvas->GetWidth() * image_scale), (int)(canvas->GetHeight() * image_scale));
		scaled->Blit(*canvas, 0, 0, canvas->GetWidth(), canvas->GetHeight(), 0, 0, scaled->GetWidth(),
			scaled->GetHeight(), 1.0);
		scaled->WritePngBytes(pngResult);
		delete scaled;
	} else {
		canvas->WritePngBytes(pngResult);
	}
	delete canvas;

	return (int)pngResult.size();
}

WEB_EXPORT("web_get_result") unsigned char* web_get_result() {
	return pngResult.data();
}

// HTML description of the loaded level. The string stays valid until the next call.
WEB_EXPORT("web_get_info_html") const char* web_get_info_html() {
	LevelParser* level = parsers[0];
	if(level == nullptr) {
		return "";
	}

	std::string html;
	html += "<div class='row'><b>" + escape_html(level->LH.Name) + "</b></div>";
	html += "<div class='row'>" + escape_html(level->LH.Desc) + "</div>";
	html += "<div class='row'>Style: " + levelMappings->NumToGameStyle.at(level->LH.GameStyle) + "</div>";
	html += "<div class='row'>Theme: " + levelMappings->NumToTheme.at(level->MapHdr.Theme) + "</div>";
	html += "<div class='row'>Night: " + std::string(level->MapHdr.Flag == 2 ? "yes" : "no") + "</div>";
	html += "<div class='row'>Clear time: " + levelMappings->FormatMillisecondTime(level->LH.ClearTime) + "</div>";
	html += "<div class='row'>Attempts: " + std::to_string(level->LH.ClearAttempts) + "</div>";
	html += "<div class='row'>Version: " + levelMappings->NumToGameVersion.at(level->LH.ClearVer) + "</div>";
	html += "<div class='row'>Timer: " + std::to_string(level->LH.Timer) + "</div>";
	html += "<div class='row'>Goal: (" + fmt::format("{}", level->LH.GoalX / 10.0) + ", "
			+ std::to_string(level->LH.GoalY) + ")</div>";
	std::string clear_condition = fmt::format(levelMappings->NumToClearCondition.at(level->LH.ClearCRC), level->LH.ClearCA);
	html += "<div class='row'>Condition: " + escape_html(clear_condition) + "</div>";
	html += "<div class='row'>Autoscroll: " + levelMappings->NumToAutoscrollSpeed.at(level->LH.AutoscrollSpd) + "</div>";
	html += "<div class='row'>Objects: " + std::to_string(level->MapHdr.ObjCount) + "</div>";

	stringResult = std::move(html);
	return stringResult.c_str();
}

// Structured JSON of one world. The string stays valid until the next call.
WEB_EXPORT("web_get_json") const char* web_get_json(int isOverworld) {
	LevelParser* parser = parsers[isOverworld ? 1 : 0];
	LevelDrawer* drawer = drawers[isOverworld ? 1 : 0];
	if(parser == nullptr || drawer == nullptr) {
		return "";
	}

	stringResult = parser->ExportToJSON(drawer->GetInstructions());
	return stringResult.c_str();
}

WEB_EXPORT("web_set_options") void web_set_options(int removeGrid, int objectsOverPipes) {
	remove_grid               = removeGrid != 0;
	render_objects_over_pipes = objectsOverPipes != 0;
}

WEB_EXPORT("web_set_image_scale") void web_set_image_scale(float scale) {
	if(scale < 0.125f) {
		scale = 0.125f;
	}
	if(scale > 8.0f) {
		scale = 8.0f;
	}
	image_scale = scale;
}

// Tilesheet ("img/tile/...") the given world needs; JS fetches it and passes the
// bytes back through web_set_asset, so assets load on demand.
WEB_EXPORT("web_tilesheet_path") const char* web_tilesheet_path(int world) {
	LevelParser* level = parsers[world ? 1 : 0];
	if(level == nullptr) {
		return "";
	}
	stringResult = fmt::format("img/tile/{}-{}{}.png", level->LH.GameStyle, level->MapHdr.Theme,
		(level->MapHdr.Flag == 2) ? "A" : "");
	return stringResult.c_str();
}
