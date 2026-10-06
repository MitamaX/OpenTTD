/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file camera.cpp The dimetric camera of the mini UI: where it looks, how close, which way it faces, and how it moves. */

#include "../../stdafx.h"
#include "camera.h"

#include <ranges>
#include <tuple>

#include "../../core/math_func.hpp"
#include "../../gfx_func.h"
#include "../../map_func.h"
#include "../../mini_ui.h"
#include "../input/pointer_router.h"
#include "../map/world_tiles.h"
#include "tuning.h"

#include "../../safeguards.h"

static constexpr double MS_PER_SECOND = 1000.0;
static constexpr double PAN_STOP_SPEED = 5.0;
static constexpr double GLIDE_SNAP_PX = 0.5;
static constexpr double ZOOM_SNAP_SHARE = 0.002;
static constexpr double DEGREES_PER_RADIAN = 180.0 / std::numbers::pi;
static constexpr double NORTH_UP_HEADING = 45.0;
static constexpr double QUARTER_TURN_DEGREES = 90.0;
static constexpr double TURN_SNAP_DEGREES = 0.02;

Camera _camera;

static double Approach(uint delta_ms, double time_constant_ms)
{
	return 1.0 - std::exp(delta_ms / -time_constant_ms);
}

static double Travel(double pixels_per_second, uint delta_ms)
{
	return pixels_per_second * delta_ms / MS_PER_SECOND;
}

/* A held axis takes its velocity directly so movement starts instantly; a
 * released axis decays on its own, so letting go of one diagonal key keeps
 * the other axis coasting. */
static double AxisVelocity(double velocity, bool negative, bool positive, double speed, double decay)
{
	if (negative || positive) return (positive ? speed : 0.0) - (negative ? speed : 0.0);
	velocity -= velocity * decay;
	return std::abs(velocity) < PAN_STOP_SPEED ? 0.0 : velocity;
}

static double Dot(MapVector a, MapVector b)
{
	return a.x * b.x + a.y * b.y;
}

static TilePoint Confined(TilePoint point)
{
	return {Clamp<double>(point.first, 0.0, static_cast<double>(Map::SizeX())), Clamp<double>(point.second, 0.0, static_cast<double>(Map::SizeY()))};
}

static int FirstTile(double from)
{
	return std::max(0, static_cast<int>(std::floor(from)));
}

static int LastTile(double to, uint last)
{
	return std::min(static_cast<int>(last), static_cast<int>(std::floor(to)));
}

double GroundLevel(double tx, double ty)
{
	auto [x, y] = Confined({tx, ty});
	uint column = std::min(static_cast<uint>(x), Map::MaxX());
	uint row = std::min(static_cast<uint>(y), Map::MaxY());
	return FacetLevel(_world_tiles.SurfaceAt(TileXY(column, row)), x - column, y - row);
}

double SmoothStep(double edge0, double edge1, double x)
{
	double t = Clamp((x - edge0) / (edge1 - edge0), 0.0, 1.0);
	return t * t * (3.0 - 2.0 * t);
}

Camera::Camera()
{
	this->FaceNorth();
}

int Camera::TilePixels() const
{
	return std::max(1, static_cast<int>(std::lround(this->ppt)));
}

void Camera::SetViewport(int width, int height)
{
	this->width = width;
	this->height = height;
}

ExactPoint Camera::ScreenStep(MapVector step) const
{
	return {Dot(step, this->right) * this->ppt, Dot(step, this->toward) * VIEW_DEPTH * this->ppt};
}

/* The plane point that shows dx, dy pixels away from where the given plane point shows. */
TilePoint Camera::Shifted(TilePoint plane, double dx, double dy) const
{
	double across = dx / this->ppt;
	double along = dy / (this->ppt * VIEW_DEPTH);
	return {plane.first + this->right.x * across + this->toward.x * along, plane.second + this->right.y * across + this->toward.y * along};
}

double ViewRise()
{
	return NATIVE_VIEW_RISE * _tuning.height_scale;
}

double Camera::Lift(double levels) const
{
	return levels * ViewRise() * this->ppt;
}

ExactPoint Camera::ExactScreenOf(const WorldPoint &point) const
{
	ExactPoint step = this->ScreenStep({point.x - this->x, point.y - this->y});
	return {this->width * 0.5 + step.x, this->height * 0.5 + step.y - this->Lift(point.level)};
}

Point Camera::ScreenOf(const WorldPoint &point) const
{
	ExactPoint at = this->ExactScreenOf(point);
	return {static_cast<int>(std::lround(at.x)), static_cast<int>(std::lround(at.y))};
}

Point Camera::ScreenOfGround(double tx, double ty) const
{
	return this->ScreenOf({tx, ty, GroundLevel(tx, ty)});
}

/* Degrees clockwise from screen-up. */
double Camera::HeadingDegrees(MapVector direction) const
{
	ExactPoint step = this->ScreenStep(direction);
	return std::atan2(step.x, -step.y) * DEGREES_PER_RADIAN;
}

/* The world point that shows dx, dy pixels from the screen centre when it stands this many levels high. */
WorldPoint Camera::Sighted(double dx, double dy, double level) const
{
	auto [tx, ty] = this->Shifted({this->x, this->y}, dx, dy + this->Lift(level));
	return {tx, ty, level};
}

/* No surface rises as fast as the sight line, so closing a share of the gap per step down from the peak never passes the surface nearest the viewer. */
WorldPoint Camera::GroundAt(double dx, double dy) const
{
	double level = this->peak;
	std::optional<double> walled;
	for (int step = 0; step < GROUND_SEARCH_STEPS; step++) {
		WorldPoint sighted = this->Sighted(dx, dy, level);
		double ground = GroundLevel(sighted.x, sighted.y);
		/* Only over a wall facing the viewer does the sight line pass below the ground, and there it is on the wall's own tile. */
		if (ground > level) walled = level;
		level = std::lerp(level, ground, GROUND_SEARCH_SHARE);
	}
	return this->Sighted(dx, dy, walled.value_or(level));
}

WorldPoint Camera::GroundUnder(int sx, int sy) const
{
	return this->GroundAt(sx - this->width * 0.5, sy - this->height * 0.5);
}

TilePoint Camera::MapAt(int sx, int sy) const
{
	WorldPoint ground = this->GroundUnder(sx, sy);
	return {ground.x, ground.y};
}

/* The far edge shows the lowest ground, the near edge may show ground and raised work lifted up from below the screen. */
TileSpan Camera::VisibleTiles(double raised_levels) const
{
	TilePoint centre{this->x, this->y};
	double half_width = this->width * 0.5;
	double half_height = this->height * 0.5;
	double near_reach = half_height + this->Lift(this->peak + raised_levels);
	std::array<TilePoint, 4> reach{
		this->Shifted(centre, -half_width, -half_height),
		this->Shifted(centre, half_width, -half_height),
		this->Shifted(centre, half_width, near_reach),
		this->Shifted(centre, -half_width, near_reach),
	};
	auto [x_low, x_high] = std::ranges::minmax(reach | std::views::keys);
	auto [y_low, y_high] = std::ranges::minmax(reach | std::views::values);
	return {FirstTile(x_low), FirstTile(y_low), LastTile(x_high, Map::MaxX()), LastTile(y_high, Map::MaxY())};
}

std::array<TilePoint, 4> Camera::ViewCorners() const
{
	return {this->MapAt(0, 0), this->MapAt(this->width, 0), this->MapAt(this->width, this->height), this->MapAt(0, this->height)};
}

/* The plane point that puts a world point at the screen centre. */
TilePoint Camera::Focus(const WorldPoint &point) const
{
	return this->Shifted({point.x, point.y}, 0.0, -this->Lift(point.level));
}

TilePoint Camera::GroundFocus(TilePoint ground) const
{
	auto [tx, ty] = ground;
	return this->Focus({tx, ty, GroundLevel(tx, ty)});
}

void Camera::CentreOn(double tx, double ty)
{
	this->PlaceCentre(this->GroundFocus({tx, ty}));
}

void Camera::PlaceCentre(TilePoint plane)
{
	std::tie(this->x, this->y) = Confined(plane);
}

void Camera::GlideTo(double tx, double ty)
{
	this->anchored = false;
	this->gliding = true;
	this->glide = {tx, ty};
	this->dest_ppt = std::max(this->dest_ppt, _tuning.jump_ppt);
}

/* The world point under a middle press stays under the pointer until the button comes up. */
void Camera::Grab(int sx, int sy)
{
	this->grab = this->GroundUnder(sx, sy);
}

void Camera::Zoom(bool in)
{
	double factor = in ? _tuning.zoom_step : 1.0 / _tuning.zoom_step;
	this->dest_ppt = Clamp(this->dest_ppt * factor, MIN_PPT, MAX_PPT);
	this->gliding = false;
}

/* Anchor the world point under the cursor; the camera follows it every
 * frame while the scale animates, so the point never drifts. */
void Camera::ZoomAt(int sx, int sy, bool in)
{
	this->Zoom(in);
	this->anchor_sx = sx;
	this->anchor_sy = sy;
	this->anchor = this->GroundUnder(sx, sy);
	this->anchored = true;
}

void Camera::Turn(int quarters)
{
	this->heading_target += quarters * QUARTER_TURN_DEGREES;
}

void Camera::FaceNorth()
{
	this->heading_target = NORTH_UP_HEADING;
	this->SetHeading(NORTH_UP_HEADING);
}

void Camera::Halt()
{
	this->anchored = false;
	this->gliding = false;
	if (this->Turning()) this->Face(this->heading_target);
}

void Camera::Update(uint delta_ms, std::optional<WorldPoint> chase)
{
	this->Pan(delta_ms);
	this->EdgeScroll(delta_ms);
	if (chase.has_value()) {
		this->gliding = false;
		this->MoveToward(this->Focus(*chase), Approach(delta_ms, _tuning.glide_ms));
	}
	this->Glide(delta_ms);
	this->Spin(delta_ms);
	this->Settle(delta_ms);
	this->FollowGrab();
}

bool Camera::Turning() const
{
	return this->heading != this->heading_target;
}

void Camera::SetHeading(double degrees)
{
	this->heading = degrees;
	double radians = degrees / DEGREES_PER_RADIAN;
	this->toward = {std::cos(radians), std::sin(radians)};
	this->right = {-this->toward.y, this->toward.x};
}

/* The view turns about the ground point under the screen centre, which stays put. */
void Camera::Face(double degrees)
{
	WorldPoint pivot = this->GroundAt(0.0, 0.0);
	this->SetHeading(degrees);
	this->PlaceCentre(this->Focus(pivot));
}

void Camera::MoveToward(TilePoint target, double share)
{
	this->PlaceCentre({this->x + (target.first - this->x) * share, this->y + (target.second - this->y) * share});
}

void Camera::MoveBy(double dx, double dy)
{
	this->PlaceCentre(this->Shifted({this->x, this->y}, dx, dy));
}

/* WASD and arrows arrive via _dirkeys; pan speed is constant in screen space. */
void Camera::Pan(uint delta_ms)
{
	uint8_t keys = MiniUiPanKeys();
	if (keys != 0) {
		this->anchored = false;
		this->gliding = false;
	} else if (this->gliding || this->anchored) {
		this->pan_vx = 0.0;
		this->pan_vy = 0.0;
	}

	double speed = _shift_pressed ? _tuning.pan_speed_fast : _tuning.pan_speed;
	double decay = Approach(delta_ms, _tuning.pan_smooth_ms);
	this->pan_vx = AxisVelocity(this->pan_vx, (keys & DIRKEY_LEFT) != 0, (keys & DIRKEY_RIGHT) != 0, speed, decay);
	this->pan_vy = AxisVelocity(this->pan_vy, (keys & DIRKEY_UP) != 0, (keys & DIRKEY_DOWN) != 0, speed, decay);
	if (this->pan_vx == 0.0 && this->pan_vy == 0.0) return;

	this->MoveBy(Travel(this->pan_vx, delta_ms), Travel(this->pan_vy, delta_ms));
}

void Camera::EdgeScroll(uint delta_ms)
{
	if (_tuning.edge_scroll == 0 || !_pointer.OnMap() || _middle_button_down) return;

	double step = Travel(_tuning.edge_scroll_speed, delta_ms);
	double ex = 0.0;
	double ey = 0.0;
	if (_cursor.pos.x < _tuning.edge_margin) ex = -step;
	if (_cursor.pos.x >= this->width - _tuning.edge_margin) ex = step;
	if (_cursor.pos.y < _tuning.edge_margin) ey = -step;
	if (_cursor.pos.y >= this->height - _tuning.edge_margin) ey = step;
	if (ex == 0.0 && ey == 0.0) return;

	this->anchored = false;
	this->gliding = false;
	this->MoveBy(ex, ey);
}

/* The turn moves the plane point under a raised ground point, so the glide aims at its ground point anew each frame. */
void Camera::Glide(uint delta_ms)
{
	if (!this->gliding) return;

	TilePoint target = Confined(this->GroundFocus(this->glide));
	this->MoveToward(target, Approach(delta_ms, _tuning.glide_ms));
	ExactPoint gap = this->ScreenStep({target.first - this->x, target.second - this->y});
	if (std::abs(gap.x) >= GLIDE_SNAP_PX || std::abs(gap.y) >= GLIDE_SNAP_PX) return;

	this->PlaceCentre(target);
	this->gliding = false;
}

void Camera::Spin(uint delta_ms)
{
	if (!this->Turning()) return;

	double turned = this->heading + (this->heading_target - this->heading) * Approach(delta_ms, _tuning.turn_smooth_ms);
	this->Face(std::abs(this->heading_target - turned) < TURN_SNAP_DEGREES ? this->heading_target : turned);
}

/* The scale changes before the anchor is pinned, so the anchored ground point stays under the pointer. */
void Camera::Settle(uint delta_ms)
{
	if (this->ppt != this->dest_ppt) {
		double share = Approach(delta_ms, _tuning.zoom_smooth_ms);
		this->ppt = std::exp(std::log(this->ppt) + (std::log(this->dest_ppt) - std::log(this->ppt)) * share);
		if (std::abs(this->dest_ppt - this->ppt) < this->dest_ppt * ZOOM_SNAP_SHARE) this->ppt = this->dest_ppt;
	}
	if (!this->anchored) return;

	this->Pin(this->anchor, this->anchor_sx, this->anchor_sy);
	if (this->ppt == this->dest_ppt) this->anchored = false;
}

/* The grab is placed from where the pointer is now, not from how far it moved, so
 * whatever the pointer crossed in between, panels or the window edge, the grabbed
 * point is back under it. It comes last and overrules every other motion. */
void Camera::FollowGrab()
{
	if (!this->grab.has_value()) return;
	if (!_middle_button_down) {
		this->grab.reset();
		return;
	}

	this->Halt();
	this->pan_vx = 0.0;
	this->pan_vy = 0.0;
	this->Pin(*this->grab, _cursor.pos.x, _cursor.pos.y);
}

/* The ground point keeps its own height, so it stays under the pointer while the plane moves. */
void Camera::Pin(const WorldPoint &ground, int sx, int sy)
{
	this->PlaceCentre(this->Shifted(this->Focus(ground), this->width * 0.5 - sx, this->height * 0.5 - sy));
}
