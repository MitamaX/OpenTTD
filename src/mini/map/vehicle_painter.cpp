/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file vehicle_painter.cpp Vehicles on the map: consists, silhouettes, headings and cargo. */

#include "../../stdafx.h"
#include "vehicle_painter.h"

#include <cmath>
#include <numbers>

#include "../../bridge_map.h"
#include "../../company_base.h"
#include "../../company_func.h"
#include "../../ground_vehicle.hpp"
#include "../../map_func.h"
#include "../../vehicle_base.h"
#include "../core/camera.h"
#include "../core/canvas.h"
#include "../core/tones.h"
#include "tile_shapes.h"
#include "vehicle_motion.h"

#include "../../safeguards.h"

static constexpr double HALF_TURN_DEGREES = 180.0;
static constexpr double HEAD_DOT_REACH = 0.62;
static constexpr double HALF_UNIT = 0.5;
static constexpr double ALOFT_LEVELS = 0.75;

VehiclePainter _vehicle_painter;

/* The map direction a vehicle facing this way moves along, one tile long. */
static MapVector Heading(Direction direction)
{
	TileIndexDiffC step = TileIndexDiffCByDir(direction);
	double length = std::hypot(step.x, step.y);
	return {step.x / length, step.y / length};
}

/* Screen degrees clockwise from up, the way silhouettes are turned. */
static float ScreenHeading(Direction direction)
{
	return static_cast<float>(_camera.HeadingDegrees(Heading(direction)));
}

bool VehiclePainter::InLayer(VehicleType vt) const
{
	switch (this->filter) {
		case MiniLayer::Rail: return vt == VEH_TRAIN;
		case MiniLayer::Road: return vt == VEH_ROAD;
		default: return true;
	}
}

/* Shapes without a clear nose carry a paper dot on the leading edge, the
 * same accent as the train head dot. */
static void DrawHeadingDot(int cx, int cy, int r, float heading)
{
	if (r < 3) return;
	double radians = heading * std::numbers::pi / HALF_TURN_DEGREES;
	int px = cx + static_cast<int>(std::lround(std::sin(radians) * HEAD_DOT_REACH * r));
	int py = cy - static_cast<int>(std::lround(std::cos(radians) * HEAD_DOT_REACH * r));
	_canvas.FillCircle(px, py, std::max(1, (r + 1) / 3), COL_PAPER);
}

static int UnitHalf(int ppt)
{
	return std::max(3, ppt * 2 / 5) / 2;
}

static int CargoDotRadius(int ppt)
{
	return std::max(1, UnitHalf(ppt) - 2);
}

static void DrawCargoDot(int cx, int cy, int r, CargoType cargo)
{
	_canvas.FillCircle(cx, cy, r + 1, COL_INK);
	_canvas.FillCircle(cx, cy, r, CargoRgb(cargo));
}

/* Silhouette tells the vehicle type apart: square train, round road
 * vehicle, diamond ship, triangle aircraft. The centre dot is the unit's
 * cargo in its legend colour. */
static MiniSprite Silhouette(VehicleType type)
{
	switch (type) {
		case VEH_SHIP: return MiniSprite::Ship;
		case VEH_AIRCRAFT: return MiniSprite::Aircraft;
		default: return MiniSprite::RoadVeh;
	}
}

static double UnitTiles(const Vehicle *u)
{
	return u->GetGroundVehicleCache()->cached_veh_length / static_cast<double>(TILE_SIZE);
}

/* A consist draws as one polyline through its unit centres. The game spaces
 * units in path steps, and a cardinal step moves both map axes, so raw
 * centres sit sqrt(2) apart on diagonals. Units are therefore re-laid along
 * their own polyline at true unit lengths from the head, which keeps the
 * length on the map constant on any mix of track. Ink underlays the whole
 * line before the colour pass, so joints stay clean. */
void VehiclePainter::PaintConsist(const Vehicle *head, int ppt)
{
	static std::vector<WorldPoint> raw;
	static std::vector<const Vehicle *> units;
	static std::vector<WorldPoint> laid;
	static std::vector<double> arc;
	static std::vector<double> want;
	static std::vector<std::pair<int, int>> pts;
	raw.clear();
	units.clear();
	laid.clear();
	arc.clear();
	want.clear();
	pts.clear();

	/* Units inside a depot are hidden one by one; the consist keeps drawing
	 * through its still-visible run. */
	const Vehicle *first = nullptr;
	const Vehicle *tail = nullptr;
	double s = 0.0;
	double prev_len = 0.0;
	for (const Vehicle *u = head; u != nullptr; u = u->Next()) {
		if (u->vehstatus.Test(VehState::Hidden)) continue;
		raw.push_back(_vehicle_motion.Position(u));
		units.push_back(u);
		double len = UnitTiles(u);
		if (!want.empty()) s += (prev_len + len) * 0.5;
		want.push_back(s);
		prev_len = len;
		if (first == nullptr) first = u;
		tail = u;
	}
	if (raw.empty()) return;

	arc.resize(raw.size());
	arc[0] = 0.0;
	for (size_t i = 1; i < raw.size(); i++) arc[i] = arc[i - 1] + std::hypot(raw[i].x - raw[i - 1].x, raw[i].y - raw[i - 1].y);

	size_t seg = 0;
	for (size_t i = 0; i < raw.size(); i++) {
		double t = want[i];
		if (t >= arc.back()) {
			WorldPoint end = raw.back();
			if (raw.size() >= 2) {
				const WorldPoint &prev = raw[raw.size() - 2];
				double d = std::hypot(end.x - prev.x, end.y - prev.y);
				if (d > 0.0) end = Between(prev, end, 1.0 + (t - arc.back()) / d);
			}
			laid.push_back(VehicleMotion::Grounded(units[i], end));
		} else {
			while (seg + 2 < raw.size() && arc[seg + 1] <= t) seg++;
			double span = arc[seg + 1] - arc[seg];
			laid.push_back(VehicleMotion::Grounded(units[i], Between(raw[seg], raw[seg + 1], span > 0.0 ? (t - arc[seg]) / span : 0.0)));
		}
	}

	/* The nose and tail stick out half a unit length past the end centres. */
	auto overhang = [](const Vehicle *u, const WorldPoint &centre, int sign) {
		MapVector heading = Heading(u->direction);
		double reach = sign * UnitTiles(u) * HALF_UNIT;
		return VehicleMotion::Grounded(u, {centre.x + heading.x * reach, centre.y + heading.y * reach, centre.level});
	};
	auto project = [](const WorldPoint &point) {
		Point at = _camera.ScreenOf(point);
		return std::pair<int, int>(at.x, at.y);
	};
	pts.push_back(project(overhang(first, laid.front(), 1)));
	for (const WorldPoint &point : laid) pts.push_back(project(point));
	pts.push_back(project(overhang(tail, laid.back(), -1)));

	int w = std::max(2, ppt / 4);
	int m = w + 2;
	int minx = pts[0].first, maxx = minx, miny = pts[0].second, maxy = miny;
	for (auto [x, y] : pts) {
		minx = std::min(minx, x);
		maxx = std::max(maxx, x);
		miny = std::min(miny, y);
		maxy = std::max(maxy, y);
	}
	if (maxx < -m || maxy < -m || minx >= _camera.Width() + m || miny >= _camera.Height() + m) return;

	Tones tones = this->TonesOf(head);
	for (size_t i = 0; i + 1 < pts.size(); i++) _canvas.ThickLine(pts[i].first, pts[i].second, pts[i + 1].first, pts[i + 1].second, w + 2, tones.ink);
	for (size_t i = 1; i + 1 < pts.size(); i++) _canvas.FillCircle(pts[i].first, pts[i].second, (w + 2) / 2, tones.ink);
	for (size_t i = 0; i + 1 < pts.size(); i++) _canvas.ThickLine(pts[i].first, pts[i].second, pts[i + 1].first, pts[i + 1].second, w, tones.fill);
	for (size_t i = 1; i + 1 < pts.size(); i++) _canvas.FillCircle(pts[i].first, pts[i].second, std::max(1, w / 2), tones.fill);
	if (tones.dim) return;

	if (first == head) _canvas.FillCircle(pts[0].first, pts[0].second, std::max(1, w / 2 - 1), COL_PAPER);
	if (!this->detail.cargo_dots) return;

	size_t i = 1;
	for (const Vehicle *u = head; u != nullptr; u = u->Next()) {
		if (u->vehstatus.Test(VehState::Hidden)) continue;
		if (u->cargo_cap != 0 && IsValidCargoType(u->cargo_type)) DrawCargoDot(pts[i].first, pts[i].second, CargoDotRadius(ppt), u->cargo_type);
		i++;
	}
}

/* An aircraft shows alone; its shadow and rotor ride behind it in its chain. */
static const Vehicle *UnitsEnd(const Vehicle *head)
{
	return head->type == VEH_AIRCRAFT ? head->Next() : nullptr;
}

static bool IsShown(const Vehicle *unit)
{
	return !unit->vehstatus.Test(VehState::Hidden);
}

/* A ground vehicle on a bridge or ramp is aloft however low the bridge, so the deck never hides it. */
static bool IsAloft(const Vehicle *unit)
{
	if (unit->type == VEH_SHIP) return false;
	WorldPoint at = _vehicle_motion.Position(unit);
	bool raised = at.level > GroundLevel(at.x, at.y) + ALOFT_LEVELS;
	return raised || (unit->type != VEH_AIRCRAFT && IsBridgeTile(unit->tile));
}

/* A consist goes over the buildings whole as soon as one unit in view is aloft. */
static VehicleTier TierOf(const Vehicle *head)
{
	for (const Vehicle *unit = head; unit != UnitsEnd(head); unit = unit->Next()) {
		if (IsShown(unit) && IsAloft(unit)) return VehicleTier::Aloft;
	}
	return VehicleTier::Grounded;
}

void VehiclePainter::Paint(int ppt, MiniLayer filter, VehicleTier tier)
{
	this->detail = ZoomDetail::For(ppt);
	this->filter = filter;
	for (const Vehicle *v : Vehicle::Iterate()) {
		if (v->type > VEH_AIRCRAFT || !v->IsPrimaryVehicle() || TierOf(v) != tier) continue;
		if (v->type == VEH_TRAIN && this->detail.vehicle_shapes) {
			this->PaintConsist(v, ppt);
			continue;
		}
		for (const Vehicle *unit = v; unit != UnitsEnd(v); unit = unit->Next()) {
			if (IsShown(unit)) this->PaintUnit(unit, ppt);
		}
	}
}

void VehiclePainter::PaintUnit(const Vehicle *v, int ppt)
{
	int half = UnitHalf(ppt);
	int r = (v->type == VEH_SHIP || v->type == VEH_AIRCRAFT) ? half + 2 : half;
	auto [cx, cy] = _camera.ScreenOf(_vehicle_motion.Position(v));
	if (cx < -r - 1 || cy < -r - 1 || cx >= _camera.Width() + r + 1 || cy >= _camera.Height() + r + 1) return;

	Tones tones = this->TonesOf(v);
	if (!this->detail.vehicle_shapes) {
		_canvas.FillRect(cx - 1, cy - 1, cx + 1, cy + 1, tones.fill);
		return;
	}

	MiniSprite silhouette = Silhouette(v->type);
	float heading = ScreenHeading(v->direction);
	_canvas.FillShapeRot(silhouette, cx, cy, r + 1, heading, tones.ink);
	_canvas.FillShapeRot(silhouette, cx, cy, r, heading, tones.fill);
	if (tones.dim) return;

	/* The diamond only shows its axis when rotated; the head dot
	 * picks which end leads. */
	if (v->type != VEH_AIRCRAFT) DrawHeadingDot(cx, cy, r, heading);
	if (this->detail.cargo_dots && v->cargo_cap > 0 && IsValidCargoType(v->cargo_type)) DrawCargoDot(cx, cy, CargoDotRadius(ppt), v->cargo_type);
}

VehiclePainter::Tones VehiclePainter::TonesOf(const Vehicle *v) const
{
	uint32_t fill = Company::IsValidID(v->owner) ? _company_rgb[_company_colours[v->owner]] : COL_OBJ;
	if (this->InLayer(v->type)) return {fill, COL_INK, false};
	return {_canvas.Greyed(fill), _canvas.Greyed(COL_INK), true};
}
