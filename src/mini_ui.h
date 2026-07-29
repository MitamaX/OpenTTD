/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file mini_ui.h Stand-alone minimalist UI; reads game state, issues commands, owns its own framebuffer. */

#ifndef MINI_UI_H
#define MINI_UI_H

#include <vector>

#include "core/geometry_type.hpp"
#include "gfx_type.h"
#include "industry_type.h"
#include "station_type.h"
#include "tile_type.h"
#include "town_type.h"
#include "vehicle_type.h"
#include "video/raylib_wrap.h"
#include "window_type.h"

bool MiniUiActive();
void MiniUiToggle();
void MiniUiResetGameState();
void MiniUiFrame(uint delta_ms);
bool MiniUiHandleMouseEvents(bool native_capture);
bool MiniUiHandleKeypress(uint keycode, char32_t key);
bool MiniUiHidesWindow(WindowClass wc);
bool MiniUiWindowPlacement(int width, int height, Point &pt);
void MiniUiOverlayRects(std::vector<RlwRectI> &rects);
void MiniUiScrollTo(int x, int y);

bool MiniUiDrawControlGlyph(const Rect &r, Colours colour, SpriteID sprite);
bool MiniUiDrawCloseGlyph(const Rect &r, Colours colour);
bool MiniUiDrawCoverageGlyph(const Rect &r, Colours colour);
bool MiniUiDrawResizeGlyph(const Rect &r, Colours colour, bool at_left);

struct Vehicle;
bool ShowMiniVehicleWindow(const Vehicle *v);
bool ShowMiniStationWindow(StationID station);
bool ShowMiniTownWindow(TownID town);
bool ShowMiniIndustryWindow(IndustryID industry);
bool ShowMiniDepotWindow(TileIndex tile, VehicleType type);

#endif /* MINI_UI_H */
