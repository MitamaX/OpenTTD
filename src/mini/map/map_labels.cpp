/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file map_labels.cpp Name plates over towns, stations, industries and signs. */

#include "../../stdafx.h"
#include "map_labels.h"

#include "../../company_func.h"
#include "../../gfx_func.h"
#include "../../industry.h"
#include "../../signs_base.h"
#include "../../station_base.h"
#include "../../strings_func.h"
#include "../../town.h"
#include "../../mini_ui.h"
#include "../core/camera.h"
#include "../core/canvas.h"
#include "../core/tones.h"
#include "tile_shapes.h"
#include "zoom_detail.h"

#include "../../table/strings.h"

#include "../../safeguards.h"

static constexpr int PLATE_PAD = 3;
static constexpr int OFFSCREEN_MARGIN = 300;
static constexpr uint TRANSPARENT_PLATE_ALPHA = 0xAA;

MapLabels _map_labels;

static TextColour PlateTextColour(uint32_t c)
{
	return IsLightTone(c) ? TC_BLACK : TC_WHITE;
}

static Point TileAnchor(TileIndex tile)
{
	return ScreenOfTileCentre(TileX(tile), TileY(tile));
}

/* Town names always show for navigation; station and industry names join
 * at the infrastructure zoom tier. Labels sit centred above their sign
 * tile. */
void MapLabels::Paint(int ppt)
{
	ZoomDetail detail = ZoomDetail::For(ppt);
	this->plates.clear();

	/* Signs are the player's own notes, so they show at every zoom tier. */
	for (const Sign *si : Sign::Iterate()) {
		Point at = _camera.ScreenOf({si->x / (double)TILE_SIZE, si->y / (double)TILE_SIZE, si->z / (double)TILE_HEIGHT});
		if (si->name.empty() || !this->Visible(at)) continue;
		this->Place(at, si->name, COL_ST_BUOY, false, PlateTextColour(COL_ST_BUOY), si->index);
	}
	for (const Town *t : Town::Iterate()) {
		Point at = TileAnchor(t->xy);
		if ((!detail.all_town_names && !t->larger_town) || !this->Visible(at)) continue;
		this->Place(at, GetString(t->larger_town ? STR_VIEWPORT_TOWN_CITY_POP : STR_VIEWPORT_TOWN_POP, t->index, t->cache.population), MINI_CH_PANEL, true, TC_WHITE, t->index);
	}
	if (!detail.station_names) return;

	/* Oil rigs already carry the plate of their neutral station. */
	for (const Industry *ind : Industry::Iterate()) {
		Point at = TileAnchor(ind->location.GetCenterTile());
		if (ind->neutral_station != nullptr || !this->Visible(at)) continue;
		this->Place(at, GetString(STR_INDUSTRY_NAME, ind->index), COL_IND, false, PlateTextColour(COL_IND), ind->index);
	}
	for (const Station *st : Station::Iterate()) {
		Point at = TileAnchor(st->xy);
		if (!this->Visible(at)) continue;
		uint32_t plate = (st->owner == OWNER_NONE || !st->IsInUse()) ? COL_OBJ : _company_rgb[_company_colours[st->owner]];
		this->Place(at, GetString(STR_VIEWPORT_STATION, st->index, st->facilities), plate, false, PlateTextColour(plate), st->index);
	}
}

std::optional<LabelTarget> MapLabels::HitAt(int x, int y) const
{
	const Plate *hit = nullptr;
	for (const Plate &plate : this->plates) {
		if (!plate.area.Contains({x, y})) continue;
		if (hit == nullptr || plate.target.index() < hit->target.index()) hit = &plate;
	}
	if (hit == nullptr) return std::nullopt;
	return hit->target;
}

bool MapLabels::Visible(Point at) const
{
	int below = GetCharacterHeight(FS_NORMAL) + 20;
	return at.x >= -OFFSCREEN_MARGIN && at.y >= 0 && at.x < _camera.Width() + OFFSCREEN_MARGIN && at.y < _camera.Height() + below;
}

/* Flat mini-style plate; drawn in mini UI screen space because the native
 * sign kdtree lives in viewport coordinates. */
void MapLabels::Place(Point at, std::string_view str, uint32_t fill, bool transparent, TextColour tc, LabelTarget target)
{
	const CanvasText *e = _canvas.Text(str);
	int tw = e != nullptr ? e->w : (int)GetStringBoundingBox(str).width;
	int w = tw + 2 * PLATE_PAD;
	int h = GetCharacterHeight(FS_NORMAL) + 2 * PLATE_PAD;
	Rect r = {at.x - w / 2, at.y - h - 3, at.x - w / 2 + w - 1, at.y - 4};
	if (transparent) {
		_map_draw.FillRoundRect(r.left, r.top, r.right, r.bottom, PLATE_PAD, WithAlpha(fill, TRANSPARENT_PLATE_ALPHA));
	} else {
		_map_draw.FillRoundRect(r.left, r.top, r.right, r.bottom, PLATE_PAD, MINI_CH_EDGE);
		_map_draw.FillRoundRect(r.left + 1, r.top + 1, r.right - 1, r.bottom - 1, PLATE_PAD, fill);
	}
	if (e != nullptr) _canvas.DrawText(*e, r.left + PLATE_PAD, r.top + PLATE_PAD, TextTint(tc));
	this->plates.push_back({r, target});
}
