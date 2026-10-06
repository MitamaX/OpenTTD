/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file map_labels.cpp Name plates over towns, stations, industries and signs. */

#include "../../stdafx.h"
#include "map_labels.h"

#include <algorithm>

#include "../../company_func.h"
#include "../../gfx_func.h"
#include "../../industry.h"
#include "../../signs_base.h"
#include "../../station_base.h"
#include "../../strings_func.h"
#include "../../town.h"
#include "../../mini_ui.h"
#include "../core/canvas.h"
#include "../core/ground_trace.h"
#include "../core/tones.h"
#include "tile_shapes.h"

#include "../../table/strings.h"

#include "../../safeguards.h"

static constexpr int PLATE_PAD = 3;
static constexpr int PLATE_GAP = 3;
static constexpr int PLATE_CLEARANCE = 2;
static constexpr uint TRANSPARENT_PLATE_ALPHA = 0xAA;

static constexpr double PLACE_LIFT_LEVELS = 2.0;
static constexpr double TOWN_LIFT_LEVELS = 4.0;
static constexpr double SIGN_LIFT_LEVELS = 0.5;
static constexpr double CITY_SHOWN_FROM_PIXELS = 0.0;
static constexpr double TOWN_SHOWN_FROM_PIXELS = 4.0;
static constexpr double STATION_SHOWN_FROM_PIXELS = 12.0;
static constexpr double INDUSTRY_SHOWN_FROM_PIXELS = 16.0;
static constexpr double FADE_IN_SPAN = 0.4;
static constexpr double OPACITY_STEP = 0.2;
static constexpr double CLICKABLE_OPACITY = 0.5;
static constexpr double HIDDEN_OPACITY = 0.01;
static constexpr double OCCLUSION_MARGIN = 0.5;

MapLabels _map_labels;

static TextColour PlateTextColour(uint32_t c)
{
	return IsLightTone(c) ? TC_BLACK : TC_WHITE;
}

static WorldPoint LiftedOver(TileIndex tile, double lift)
{
	auto [x, y] = TileCentre(TileX(tile), TileY(tile));
	return {x, y, TileGround(tile).Level(x, y) + lift};
}

static double DistanceTo(const WorldPoint &point)
{
	return Length(RenderPoint(point) - _camera.Eye());
}

/* Hills between the eye and the plate's anchor hide it; buildings do not, as the plate stands over them. */
static bool HiddenByGround(const WorldPoint &anchor)
{
	Vec3 offset = RenderPoint(anchor) - _camera.Eye();
	double distance = Length(offset);
	if (distance <= OCCLUSION_MARGIN) return false;
	return TraceGround({_camera.Eye(), offset * (1.0 / distance), LevelRise()}, distance - OCCLUSION_MARGIN).has_value();
}

static bool Overlaps(const Rect &area, const std::vector<Rect> &taken)
{
	Rect wide = {area.left - PLATE_CLEARANCE, area.top - PLATE_CLEARANCE, area.right + PLATE_CLEARANCE, area.bottom + PLATE_CLEARANCE};
	return std::ranges::any_of(taken, [&](const Rect &other) {
		return wide.left <= other.right && other.left <= wide.right && wide.top <= other.bottom && other.top <= wide.bottom;
	});
}

std::vector<MapLabels::Label> MapLabels::Gather() const
{
	std::vector<Label> labels;
	for (const Sign *si : Sign::Iterate()) {
		if (si->name.empty()) continue;
		WorldPoint anchor = {si->x / (double)TILE_SIZE, si->y / (double)TILE_SIZE, si->z / (double)TILE_HEIGHT + SIGN_LIFT_LEVELS};
		labels.push_back({anchor, si->name, COL_ST_BUOY, false, si->index, 0.0, {LabelBand::Sign, DistanceTo(anchor)}});
	}
	for (const Town *t : Town::Iterate()) {
		std::string text = GetString(t->larger_town ? STR_VIEWPORT_TOWN_CITY_POP : STR_VIEWPORT_TOWN_POP, t->index, t->cache.population);
		double shown_from = t->larger_town ? CITY_SHOWN_FROM_PIXELS : TOWN_SHOWN_FROM_PIXELS;
		labels.push_back({LiftedOver(t->xy, TOWN_LIFT_LEVELS), std::move(text), MINI_CH_PANEL, true, t->index, shown_from, {LabelBand::Town, -static_cast<double>(t->cache.population)}});
	}
	for (const Station *st : Station::Iterate()) {
		WorldPoint anchor = LiftedOver(st->xy, PLACE_LIFT_LEVELS);
		uint32_t plate = (st->owner == OWNER_NONE || !st->IsInUse()) ? COL_OBJ : _company_rgb[_company_colours[st->owner]];
		labels.push_back({anchor, GetString(STR_VIEWPORT_STATION, st->index, st->facilities), plate, false, st->index, STATION_SHOWN_FROM_PIXELS, {LabelBand::Station, DistanceTo(anchor)}});
	}
	/* Oil rigs already carry the plate of their neutral station. */
	for (const Industry *ind : Industry::Iterate()) {
		if (ind->neutral_station != nullptr) continue;
		WorldPoint anchor = LiftedOver(ind->location.GetCenterTile(), PLACE_LIFT_LEVELS);
		labels.push_back({anchor, GetString(STR_INDUSTRY_NAME, ind->index), COL_IND, false, ind->index, INDUSTRY_SHOWN_FROM_PIXELS, {LabelBand::Industry, DistanceTo(anchor)}});
	}
	std::ranges::sort(labels, {}, &Label::rank);
	return labels;
}

/* The plate stands centred over its anchor; nothing when the anchor is off screen. */
std::optional<Rect> MapLabels::Area(const Label &label) const
{
	const CanvasText *text = _canvas.Text(label.text);
	int text_width = text != nullptr ? text->w : static_cast<int>(GetStringBoundingBox(label.text).width);
	int w = text_width + 2 * PLATE_PAD;
	int h = GetCharacterHeight(FS_NORMAL) + 2 * PLATE_PAD;
	Point at = _camera.ScreenOf(label.anchor);
	Rect area = {at.x - w / 2, at.y - h - PLATE_GAP, at.x - w / 2 + w - 1, at.y - PLATE_GAP - 1};
	if (area.right < 0 || area.bottom < 0 || area.left >= _camera.Width() || area.top >= _camera.Height()) return std::nullopt;
	return area;
}

/* A plate is wanted where its anchor is near enough, in sight and clear of every plate ranked above it; its opacity eases toward that each frame.
 * Plates are drawn lowest rank last, so the plates that matter most lie on top. */
void MapLabels::Paint()
{
	this->plates.clear();
	std::vector<Rect> taken;
	std::vector<std::pair<const Label *, Rect>> drawn;
	std::map<LabelTarget, double> eased;
	std::vector<Label> labels = this->Gather();
	for (const Label &label : labels) {
		std::optional<Rect> area = this->Area(label);
		if (!area.has_value()) continue;

		double pixels = _camera.Focal() / std::max(DistanceTo(label.anchor), _camera.Near());
		double nearness = label.shown_from_pixels <= 0.0 ? 1.0 : SmoothStep(label.shown_from_pixels, label.shown_from_pixels * (1.0 + FADE_IN_SPAN), pixels);
		bool wanted = nearness > 0.0 && !Overlaps(*area, taken) && !HiddenByGround(label.anchor);
		if (wanted) taken.push_back(*area);

		auto previous = this->opacities.find(label.target);
		double from = previous != this->opacities.end() ? previous->second : 0.0;
		double goal = wanted ? nearness : 0.0;
		double opacity = from < goal ? std::min(goal, from + OPACITY_STEP) : std::max(goal, from - OPACITY_STEP);
		if (opacity <= HIDDEN_OPACITY) continue;

		eased[label.target] = opacity;
		drawn.emplace_back(&label, *area);
		if (opacity >= CLICKABLE_OPACITY) this->plates.push_back({*area, label.target});
	}
	this->opacities = std::move(eased);
	for (auto it = drawn.rbegin(); it != drawn.rend(); ++it) this->Draw(*it->first, it->second, this->opacities[it->first->target]);
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

/* Flat mini-style plate; drawn in mini UI screen space because the native sign kdtree lives in viewport coordinates. */
void MapLabels::Draw(const Label &label, const Rect &area, double opacity)
{
	if (label.transparent) {
		_map_draw.FillRoundRect(area.left, area.top, area.right, area.bottom, PLATE_PAD, WithAlpha(label.fill, FadedAlpha(opacity, TRANSPARENT_PLATE_ALPHA)));
	} else {
		_map_draw.FillRoundRect(area.left, area.top, area.right, area.bottom, PLATE_PAD, WithAlpha(MINI_CH_EDGE, FadedAlpha(opacity, Alpha(MINI_CH_EDGE))));
		_map_draw.FillRoundRect(area.left + 1, area.top + 1, area.right - 1, area.bottom - 1, PLATE_PAD, WithAlpha(label.fill, FadedAlpha(opacity)));
	}
	TextColour colour = label.transparent ? TC_WHITE : PlateTextColour(label.fill);
	uint32_t tint = TextTint(colour);
	if (const CanvasText *text = _canvas.Text(label.text); text != nullptr) _canvas.DrawText(*text, area.left + PLATE_PAD, area.top + PLATE_PAD, WithAlpha(tint, FadedAlpha(opacity)));
}
