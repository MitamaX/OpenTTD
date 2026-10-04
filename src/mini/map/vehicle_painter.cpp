/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file vehicle_painter.cpp Vehicles on the top-down map: consists, silhouettes, headings and cargo. */

#include "../../stdafx.h"
#include "vehicle_painter.h"

#include "../../company_base.h"
#include "../../company_func.h"
#include "../../ground_vehicle.hpp"
#include "../../vehicle_base.h"
#include "../core/camera.h"
#include "../core/canvas.h"
#include "../core/tones.h"
#include "vehicle_motion.h"

#include "../../safeguards.h"

VehiclePainter _vehicle_painter;

static constexpr int8_t _dir_dx[8] = {-1, 0, 1, 1, 1, 0, -1, -1};
static constexpr int8_t _dir_dy[8] = {-1, -1, -1, 0, 1, 1, 1, 0};

/* Screen-space heading in degrees clockwise from up, per Direction. */
static constexpr int16_t _dir_angle[8] = {-45, 0, 45, 90, 135, 180, -135, -90};

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
static void DrawHeadingDot(int cx, int cy, int r, Direction dir)
{
	if (r < 3) return;
	int d = dir;
	double inv = (_dir_dx[d] != 0 && _dir_dy[d] != 0) ? 0.70710678 : 1.0;
	int px = cx + (int)std::lround(_dir_dx[d] * inv * 0.62 * r);
	int py = cy + (int)std::lround(_dir_dy[d] * inv * 0.62 * r);
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

/* A consist draws as one polyline through its unit centres. The game spaces
 * units in path steps, and a cardinal step moves both map axes, so raw
 * centres sit sqrt(2) apart on diagonals; the native isometric projection
 * cancels that but a top-down view shows it as a stretched train. Units are
 * therefore re-laid along their own polyline at true unit lengths from the
 * head, which keeps the drawn length constant on any mix of track. Ink
 * underlays the whole line before the colour pass, so joints stay clean. */
void VehiclePainter::PaintConsist(const Vehicle *head, int ppt)
{
	static std::vector<std::pair<double, double>> raw;
	static std::vector<double> arc;
	static std::vector<double> want;
	static std::vector<std::pair<int, int>> pts;
	raw.clear();
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
		auto [ux, uy] = _vehicle_motion.Position(u);
		raw.emplace_back(_camera.ExactScreenX(uy), _camera.ExactScreenY(ux));
		double len = u->GetGroundVehicleCache()->cached_veh_length * ppt / (double)TILE_SIZE;
		if (!want.empty()) s += (prev_len + len) * 0.5;
		want.push_back(s);
		prev_len = len;
		if (first == nullptr) first = u;
		tail = u;
	}
	if (raw.empty()) return;

	arc.resize(raw.size());
	arc[0] = 0.0;
	for (size_t i = 1; i < raw.size(); i++) {
		double dx = raw[i].first - raw[i - 1].first;
		double dy = raw[i].second - raw[i - 1].second;
		arc[i] = arc[i - 1] + std::sqrt(dx * dx + dy * dy);
	}

	size_t seg = 0;
	for (size_t i = 0; i < raw.size(); i++) {
		double t = want[i];
		double x, y;
		if (t >= arc.back()) {
			x = raw.back().first;
			y = raw.back().second;
			if (raw.size() >= 2) {
				double dx = x - raw[raw.size() - 2].first;
				double dy = y - raw[raw.size() - 2].second;
				double d = std::sqrt(dx * dx + dy * dy);
				if (d > 0.0) {
					x += dx / d * (t - arc.back());
					y += dy / d * (t - arc.back());
				}
			}
		} else {
			while (seg + 2 < raw.size() && arc[seg + 1] <= t) seg++;
			double span = arc[seg + 1] - arc[seg];
			double f = span > 0.0 ? (t - arc[seg]) / span : 0.0;
			x = raw[seg].first + (raw[seg + 1].first - raw[seg].first) * f;
			y = raw[seg].second + (raw[seg + 1].second - raw[seg].second) * f;
		}
		pts.emplace_back((int)std::lround(x), (int)std::lround(y));
	}

	/* The nose and tail stick out half a unit length past the end centres. */
	auto overhang = [&](const Vehicle *u, int sign) {
		double len = u->GetGroundVehicleCache()->cached_veh_length * ppt / (double)TILE_SIZE;
		double inv = (_dir_dx[u->direction] != 0 && _dir_dy[u->direction] != 0) ? 0.70710678 : 1.0;
		return std::pair<int, int>(
				(int)std::lround(sign * _dir_dx[u->direction] * inv * len * 0.5),
				(int)std::lround(sign * _dir_dy[u->direction] * inv * len * 0.5));
	};
	auto [nx, ny] = overhang(first, 1);
	pts.insert(pts.begin(), {pts.front().first + nx, pts.front().second + ny});
	auto [bx, by] = overhang(tail, -1);
	pts.emplace_back(pts.back().first + bx, pts.back().second + by);

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

void VehiclePainter::Paint(int ppt, MiniLayer filter)
{
	this->detail = ZoomDetail::For(ppt);
	this->filter = filter;
	for (const Vehicle *v : Vehicle::Iterate()) {
		if (v->type > VEH_AIRCRAFT) continue;
		if (v->type == VEH_TRAIN && this->detail.vehicle_shapes) {
			if (v->IsPrimaryVehicle()) this->PaintConsist(v, ppt);
			continue;
		}
		if (v->vehstatus.Test(VehState::Hidden)) continue;
		if (v->type == VEH_AIRCRAFT && !v->IsPrimaryVehicle()) continue;
		this->PaintUnit(v, ppt);
	}
}

void VehiclePainter::PaintUnit(const Vehicle *v, int ppt)
{
	int half = UnitHalf(ppt);
	int r = (v->type == VEH_SHIP || v->type == VEH_AIRCRAFT) ? half + 2 : half;
	auto [wx, wy] = _vehicle_motion.Position(v);
	int cx = _camera.ScreenX(wy);
	int cy = _camera.ScreenY(wx);
	if (cx < -r - 1 || cy < -r - 1 || cx >= _camera.Width() + r + 1 || cy >= _camera.Height() + r + 1) return;

	Tones tones = this->TonesOf(v);
	if (!this->detail.vehicle_shapes) {
		_canvas.FillRect(cx - 1, cy - 1, cx + 1, cy + 1, tones.fill);
		return;
	}

	MiniSprite silhouette = Silhouette(v->type);
	int angle = _dir_angle[v->direction];
	_canvas.FillShapeRot(silhouette, cx, cy, r + 1, angle, tones.ink);
	_canvas.FillShapeRot(silhouette, cx, cy, r, angle, tones.fill);
	if (tones.dim) return;

	/* The diamond only shows its axis when rotated; the head dot
	 * picks which end leads. */
	if (v->type != VEH_AIRCRAFT) DrawHeadingDot(cx, cy, r, v->direction);
	if (this->detail.cargo_dots && v->cargo_cap > 0 && IsValidCargoType(v->cargo_type)) DrawCargoDot(cx, cy, CargoDotRadius(ppt), v->cargo_type);
}

VehiclePainter::Tones VehiclePainter::TonesOf(const Vehicle *v) const
{
	uint32_t fill = Company::IsValidID(v->owner) ? _company_rgb[_company_colours[v->owner]] : COL_OBJ;
	if (this->InLayer(v->type)) return {fill, COL_INK, false};
	return {_canvas.Greyed(fill), _canvas.Greyed(COL_INK), true};
}
