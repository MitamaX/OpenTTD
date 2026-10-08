/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file mini_ui.h Stand-alone minimalist UI; reads game state, issues commands, owns its own framebuffer. */

#ifndef MINI_UI_H
#define MINI_UI_H

#include <functional>
#include <string>
#include <string_view>

#include "company_type.h"
#include "core/geometry_type.hpp"
#include "economy_type.h"
#include "engine_type.h"
#include "gfx_type.h"
#include "industry_type.h"
#include "mini_ui_skin.h"
#include "station_type.h"
#include "tile_type.h"
#include "town_type.h"
#include "vehicle_type.h"
#include "window_type.h"

void MiniUiToggle();
void MiniUiResetGameState();
void MiniUiTileChanged(TileIndex tile);
void MiniUiFrame();
bool MiniUiHandleMouseEvents(bool native_capture);
bool MiniUiHandleKeypress(uint keycode, char32_t key);
bool MiniUiHandleTextInput(std::string_view text, bool marked);
bool MiniUiTyping();
bool TextInputFocused();
uint8_t MiniUiPanKeys();
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

/* The _dirkeys bits, one per direction, as the drivers set them. */
static constexpr uint8_t DIRKEY_LEFT = 1;
static constexpr uint8_t DIRKEY_UP = 2;
static constexpr uint8_t DIRKEY_RIGHT = 4;
static constexpr uint8_t DIRKEY_DOWN = 8;

uint8_t MiniUiPanBit(char32_t key);
uint8_t MiniUiHeldPanBits(const std::function<bool(char32_t key)> &held);
void MiniUiHoldTurnKeys(const std::function<bool(char32_t key)> &held);
void MiniUiTrackTurnKey(char32_t key, bool down);
int MiniUiTurnKeys();

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
