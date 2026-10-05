/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file mini_ui.h Stand-alone minimalist UI; reads game state, issues commands, owns its own framebuffer. */

#ifndef MINI_UI_H
#define MINI_UI_H

#include <string>
#include <string_view>

#include "company_type.h"
#include "core/geometry_type.hpp"
#include "economy_type.h"
#include "engine_type.h"
#include "gfx_type.h"
#include "industry_type.h"
#include "station_type.h"
#include "tile_type.h"
#include "town_type.h"
#include "vehicle_type.h"
#include "window_type.h"

bool MiniUiActive();
void MiniUiToggle();
void MiniUiResetGameState();
void MiniUiFrame(uint delta_ms);
bool MiniUiHandleMouseEvents(bool native_capture);
bool MiniUiHandleKeypress(uint keycode, char32_t key);
bool MiniUiHandleTextInput(std::string_view text, bool marked);
bool MiniUiTyping();
bool MiniUiHidesWindow(WindowClass wc);
bool MiniUiWindowPlacement(int width, int height, Point &pt);
void MiniUiScrollTo(int x, int y);
bool MiniUiShowError(std::string summary, std::string detail, bool warn);
bool MiniUiCatchEstimate(Money cost);

bool MiniUiBeginPaint();
void MiniUiEndPaint();
void MiniUiReleaseGraphics();

struct NewsItem;
bool MiniUiShowNews(const NewsItem *ni);

/* Chrome tones. The GPU chrome and the native widget skin share them so mini
 * windows and the official windows embedded in them read as one surface. */
static constexpr uint32_t MINI_CH_PANEL = 0xFF262624U;
static constexpr uint32_t MINI_CH_EDGE = 0xFF191916U;
static constexpr uint32_t MINI_CH_SUNKEN = 0xFF1F1F1DU;
static constexpr uint32_t MINI_CH_TILE = 0xFF2E2E2BU;
static constexpr uint32_t MINI_CH_ACTIVE = 0xFF3D5A5CU;
static constexpr uint32_t MINI_CH_TEXT = 0xFFC5C0B2U;
static constexpr uint32_t MINI_CH_DIM = 0xFF6E6A5EU;
static constexpr uint32_t MINI_CH_ACCENT = 0xFF8FE0E8U;

PixelColour MiniUiSkinTone(uint32_t argb);
PixelColour MiniUiSkinFrameFill(bool lowered, bool darkened);
PixelColour MiniUiSkinFrameBorder(bool lowered, bool darkened);
PixelColour MiniUiSkinTextColour(PixelColour colour);

bool MiniUiDrawControlGlyph(const Rect &r, Colours colour, SpriteID sprite);
bool MiniUiDrawCloseGlyph(const Rect &r, Colours colour);
bool MiniUiDrawCoverageGlyph(const Rect &r, Colours colour);

struct Vehicle;
bool ShowMiniVehicleWindow(const Vehicle *v);
bool ShowMiniStationWindow(StationID station);
bool ShowMiniWaypointWindow(StationID waypoint);
bool ShowMiniTownWindow(TownID town);
bool ShowMiniIndustryWindow(IndustryID industry);
bool ShowMiniDepotWindow(TileIndex tile, VehicleType type);
bool ShowMiniEnginePreview(EngineID engine);
bool ShowMiniBuyCompany(CompanyID company, bool hostile_takeover);

#endif /* MINI_UI_H */
