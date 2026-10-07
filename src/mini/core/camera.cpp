/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file camera.cpp The orbiting perspective camera of the mini UI: where it looks, how close, which way it faces, and how it moves. */

#include "../../stdafx.h"
#include "camera.h"

#include <algorithm>
#include <numbers>

#include "../../core/math_func.hpp"
#include "../../gfx_func.h"
#include "../../map_func.h"
#include "../../mini_ui.h"
#include "../input/pointer_router.h"
#include "../map/world_tiles.h"
#include "ground_trace.h"
#include "tuning.h"

#include "../../safeguards.h"

static constexpr double MS_PER_SECOND = 1000.0;
static constexpr double PAN_STOP_SPEED = 5.0;
static constexpr double GLIDE_SNAP_PX = 0.5;
static constexpr double ZOOM_SNAP_SHARE = 0.002;
static constexpr double SPIN_STOP_SPEED = 0.5;
static constexpr double DEGREES_PER_RADIAN = 180.0 / std::numbers::pi;
static constexpr double FULL_TURN_DEGREES = 360.0;
static constexpr double QUARTER_TURN_DEGREES = 90.0;
static constexpr double NEAR_SHARE = 0.03;
static constexpr double MIN_NEAR = 0.05;
static constexpr double FAR_SHARE = 24.0;
static constexpr double FAR_MARGIN = 64.0;
static constexpr double EYE_CLEARANCE = 0.75;
static constexpr double STEP_PROBE = 1e-3;
static constexpr double NEGLIGIBLE = 1e-9;
static constexpr int ORBIT_START_PX = 4;
static constexpr double OFF_SCREEN_PX = 1e5;
static constexpr double SMALLEST_SHOWN_TILE_PX = 3.0;
static constexpr Vec3 SKY = {0.0, 0.0, 1.0};
static constexpr Vec3 NORTH = {-std::numbers::sqrt2 / 2.0, -std::numbers::sqrt2 / 2.0, 0.0};
static constexpr Vec3 EAST = {-std::numbers::sqrt2 / 2.0, std::numbers::sqrt2 / 2.0, 0.0};

Camera _camera;

static double Approach(uint delta_ms, double time_constant_ms)
{
	return 1.0 - std::exp(delta_ms / -time_constant_ms);
}

static double Travel(double per_second, uint delta_ms)
{
	return per_second * delta_ms / MS_PER_SECOND;
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

static TilePoint Confined(TilePoint point)
{
	return {Clamp<double>(point.first, 0.0, static_cast<double>(Map::SizeX())), Clamp<double>(point.second, 0.0, static_cast<double>(Map::SizeY()))};
}

/* The bearing clockwise from map north as a horizontal direction in the map's plane. */
Vec3 Bearing(double degrees)
{
	double radians = degrees / DEGREES_PER_RADIAN;
	return NORTH * std::cos(radians) + EAST * std::sin(radians);
}

double GroundLevel(double tx, double ty)
{
	auto [x, y] = Confined({tx, ty});
	uint column = std::min(static_cast<uint>(x), Map::MaxX());
	uint row = std::min(static_cast<uint>(y), Map::MaxY());
	return FacetLevel(_world_tiles.SurfaceAt(TileXY(column, row)), x - column, y - row);
}

/* How high one height level stands in the 3D world, in tile widths. */
double LevelRise()
{
	return LEVEL_TILES * _tuning.elevation_scale;
}

Vec3 RenderPoint(const WorldPoint &point)
{
	return {point.x, point.y, point.level * LevelRise()};
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
	this->width = std::max(width, 1);
	this->height = std::max(height, 1);
	this->Frame();
}

/* Pixels per tile one tile width away from the eye. */
double Camera::Focal() const
{
	return this->height / (2.0 * std::tan(_tuning.view_fov / 2.0 / DEGREES_PER_RADIAN));
}

/* How far back the zoom asks the eye to stand from the focus. */
double Camera::ZoomDistance() const
{
	return this->Focal() / this->ppt;
}

/* How far the eye stands from the ground point it turns about, once lifted clear of the hills. */
double Camera::FocusDistance() const
{
	return Length(this->eye - RenderPoint(this->FocusPoint()));
}

WorldPoint Camera::FocusPoint() const
{
	return {this->focus.first, this->focus.second, this->focus_level};
}

Vec3 Camera::Forward() const
{
	return Bearing(this->yaw);
}

/* The eye sits back along the view's dip from the focus, lifted clear of any hill behind it. */
void Camera::Frame()
{
	double dip = this->pitch / DEGREES_PER_RADIAN;
	Vec3 target = RenderPoint(this->FocusPoint());
	this->eye = target + (this->Forward() * -std::cos(dip) + SKY * std::sin(dip)) * this->ZoomDistance();
	this->eye.z = std::max(this->eye.z, GroundLevel(this->eye.x, this->eye.y) * LevelRise() + EYE_CLEARANCE);

	this->back = Normalised(this->eye - target);
	this->right = Bearing(this->yaw + QUARTER_TURN_DEGREES);
	this->up = Cross(this->back, this->right);
	double sight = this->FocusDistance();
	this->near = std::max(sight * NEAR_SHARE, MIN_NEAR);
	this->far = sight * FAR_SHARE + FAR_MARGIN;
}

Mat4 Camera::ViewMatrix() const
{
	return Mat4::View(this->eye, this->right, this->up, this->back);
}

Mat4 Camera::ProjectionMatrix() const
{
	double focal = this->Focal();
	return Mat4::Perspective(2.0 * focal / this->width, 2.0 * focal / this->height, this->near, this->far);
}

/* The map direction from a ground point toward the eye, which is the way down the screen there. */
MapVector Camera::Toward(TilePoint at) const
{
	MapVector toward = {this->eye.x - at.first, this->eye.y - at.second};
	double length = std::hypot(toward.x, toward.y);
	if (length > NEGLIGIBLE) return {toward.x / length, toward.y / length};
	Vec3 forward = this->Forward();
	return {-forward.x, -forward.y};
}

/* How a short step from the focus shows on screen, per tile. */
ExactPoint Camera::ScreenStep(MapVector step) const
{
	WorldPoint from = this->FocusPoint();
	WorldPoint to = {from.x + step.x * STEP_PROBE, from.y + step.y * STEP_PROBE, from.level};
	ExactPoint a = this->ExactScreenOf(from);
	ExactPoint b = this->ExactScreenOf(to);
	return {(b.x - a.x) / STEP_PROBE, (b.y - a.y) / STEP_PROBE};
}

/* A point behind the near plane is thrown far off the screen on its own side, so a shape reaching behind the eye runs off the screen's edge and nothing behind the eye shows. */
ExactPoint Camera::ExactScreenOf(const WorldPoint &point) const
{
	Vec3 offset = RenderPoint(point) - this->eye;
	double ahead = this->Ahead(point);
	double across = Dot(offset, this->right);
	double rise = Dot(offset, this->up);
	if (ahead >= this->near) {
		double scale = this->Focal() / ahead;
		return {this->width * 0.5 + across * scale, this->height * 0.5 - rise * scale};
	}

	double aside = std::hypot(across, rise);
	if (aside <= NEGLIGIBLE) return {this->width * 0.5, this->height * 0.5 + OFF_SCREEN_PX};
	return {this->width * 0.5 + across / aside * OFF_SCREEN_PX, this->height * 0.5 - rise / aside * OFF_SCREEN_PX};
}

/* How far in front of the eye a point stands, along the way the camera looks. */
double Camera::Ahead(const WorldPoint &point) const
{
	return -Dot(RenderPoint(point) - this->eye, this->back);
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

/* The unit direction from the eye through a screen point. */
Vec3 Camera::SightThrough(double sx, double sy) const
{
	double focal = this->Focal();
	return Normalised(this->right * ((sx - this->width * 0.5) / focal) - this->up * ((sy - this->height * 0.5) / focal) - this->back);
}

/* A sight line that misses the map lands on the sea level plane, or as far as the view reaches when it rises. */
WorldPoint Camera::GroundUnder(double sx, double sy) const
{
	SightLine sight = {this->eye, this->SightThrough(sx, sy), LevelRise()};
	if (std::optional<GroundHit> hit = TraceGround(sight, this->far); hit.has_value()) return {hit->x, hit->y, hit->level};

	double t = sight.direction.z < -NEGLIGIBLE ? -this->eye.z / sight.direction.z : this->far;
	Vec3 at = sight.At(std::min(t, this->far));
	return {at.x, at.y, std::max(at.z, 0.0) / LevelRise()};
}

TilePoint Camera::MapAt(int sx, int sy) const
{
	WorldPoint ground = this->GroundUnder(sx, sy);
	return {ground.x, ground.y};
}

/* The ground and whatever stands raised on it lies between sea level and the peak's top, inside the view's frustum
 * and no farther than where a tile still spans a few pixels. */
TileSpan Camera::VisibleTiles(double raised_levels) const
{
	double focal = this->Focal();
	double reach = std::min(this->far, focal / SMALLEST_SHOWN_TILE_PX);
	double half_x = this->width * 0.5 / focal;
	double half_y = this->height * 0.5 / focal;
	std::array<Vec3, 8> corners;
	for (int corner = 0; corner < 4; corner++) {
		double across = (corner == 1 || corner == 2) ? half_x : -half_x;
		double down = corner >= 2 ? half_y : -half_y;
		Vec3 ray = this->right * across - this->up * down - this->back;
		corners[corner] = this->eye + ray * this->near;
		corners[corner + 4] = this->eye + ray * reach;
	}

	double top = (this->peak + raised_levels) * LevelRise();
	double x_low = Map::SizeX();
	double y_low = Map::SizeY();
	double x_high = 0.0;
	double y_high = 0.0;
	auto include = [&](const Vec3 &point) {
		x_low = std::min(x_low, point.x);
		y_low = std::min(y_low, point.y);
		x_high = std::max(x_high, point.x);
		y_high = std::max(y_high, point.y);
	};
	auto cut = [&](const Vec3 &from, const Vec3 &to) {
		for (double plane : {0.0, top}) {
			if ((from.z - plane) * (to.z - plane) < 0.0) include(from + (to - from) * ((plane - from.z) / (to.z - from.z)));
		}
	};
	for (int corner = 0; corner < 8; corner++) {
		if (corners[corner].z >= 0.0 && corners[corner].z <= top) include(corners[corner]);
	}
	for (int corner = 0; corner < 4; corner++) {
		cut(corners[corner], corners[(corner + 1) % 4]);
		cut(corners[corner + 4], corners[(corner + 1) % 4 + 4]);
		cut(corners[corner], corners[corner + 4]);
	}

	auto first = [](double from) { return std::max(0, static_cast<int>(std::floor(from))); };
	auto last = [](double to, uint edge) { return std::min(static_cast<int>(edge), static_cast<int>(std::floor(to))); };
	return {first(x_low), first(y_low), last(x_high, Map::MaxX()), last(y_high, Map::MaxY())};
}

std::array<TilePoint, 4> Camera::ViewCorners() const
{
	return {this->MapAt(0, 0), this->MapAt(this->width, 0), this->MapAt(this->width, this->height), this->MapAt(0, this->height)};
}

void Camera::Place(TilePoint focus)
{
	this->focus = Confined(focus);
	this->Frame();
}

void Camera::CentreOn(double tx, double ty)
{
	this->focus_level = GroundLevel(tx, ty);
	this->Place({tx, ty});
}

void Camera::Aim(const ViewAim &aim)
{
	this->Halt();
	this->dest_ppt = Clamp(aim.zoom, MIN_PPT, MAX_PPT);
	this->ppt = this->dest_ppt;
	this->yaw = aim.yaw;
	this->pitch = Clamp(aim.pitch, MIN_PITCH, MAX_PITCH);
	this->CentreOn(aim.focus.first, aim.focus.second);
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

/* A right press turns the view once the pointer moves; a press that never moves stays a click. */
void Camera::HoldOrbit(int sx, int sy)
{
	this->orbit = Orbit{{sx, sy}, {sx, sy}, false};
}

bool Camera::ReleaseOrbit()
{
	bool turned = this->orbit.has_value() && this->orbit->turning;
	this->orbit.reset();
	return turned;
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
	this->anchor_screen = {sx, sy};
	this->anchor = this->GroundUnder(sx, sy);
	this->anchored = true;
}

void Camera::FaceNorth()
{
	this->yaw = 0.0;
	this->pitch = Clamp(_tuning.view_pitch, MIN_PITCH, MAX_PITCH);
	this->spin = 0.0;
	this->Frame();
}

void Camera::Halt()
{
	this->anchored = false;
	this->gliding = false;
	this->spin = 0.0;
}

void Camera::Update(uint delta_ms, std::optional<WorldPoint> chase)
{
	this->Pan(delta_ms);
	this->EdgeScroll(delta_ms);
	if (chase.has_value()) {
		this->gliding = false;
		this->MoveToward({chase->x, chase->y}, Approach(delta_ms, _tuning.glide_ms));
	}
	this->Glide(delta_ms);
	this->Spin(delta_ms);
	this->Swing();
	this->Settle(delta_ms);
	this->Rest(delta_ms, chase.has_value() ? std::optional<double>(chase->level) : std::nullopt);
	this->FollowGrab();
}

void Camera::MoveToward(TilePoint target, double share)
{
	this->Place({this->focus.first + (target.first - this->focus.first) * share, this->focus.second + (target.second - this->focus.second) * share});
}

/* Pixels across and down the screen move the focus as far as they span at the focus, along the ground. */
void Camera::MoveBy(double dx, double dy)
{
	Vec3 across = this->right * (dx / this->ppt);
	Vec3 down = this->Forward() * (-dy / this->ppt);
	this->Place({this->focus.first + across.x + down.x, this->focus.second + across.y + down.y});
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
	if (_tuning.edge_scroll == 0 || !_pointer.OnMap() || _middle_button_down || _right_button_down) return;

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

void Camera::Glide(uint delta_ms)
{
	if (!this->gliding) return;

	TilePoint target = Confined(this->glide);
	this->MoveToward(target, Approach(delta_ms, _tuning.glide_ms));
	double gap = std::hypot(target.first - this->focus.first, target.second - this->focus.second) * this->ppt;
	if (gap >= GLIDE_SNAP_PX) return;

	this->Place(target);
	this->gliding = false;
}

/* The turn keys spin the view about its focus, easing into and out of their full speed. */
void Camera::Spin(uint delta_ms)
{
	int turn = MiniUiTurnKeys();
	this->spin += (turn * _tuning.turn_speed - this->spin) * Approach(delta_ms, _tuning.turn_smooth_ms);
	if (turn == 0 && std::abs(this->spin) < SPIN_STOP_SPEED) this->spin = 0.0;
	if (this->spin == 0.0) return;

	this->yaw = std::fmod(this->yaw + Travel(this->spin, delta_ms) + FULL_TURN_DEGREES, FULL_TURN_DEGREES);
	this->Frame();
}

/* Dragging with the right button held turns the view across and tips it up and down. */
void Camera::Swing()
{
	if (!this->orbit.has_value()) return;
	if (!_right_button_down) {
		this->orbit.reset();
		return;
	}

	Orbit &orbit = *this->orbit;
	Point now = _cursor.pos;
	if (!orbit.turning && std::max(std::abs(now.x - orbit.start.x), std::abs(now.y - orbit.start.y)) >= ORBIT_START_PX) orbit.turning = true;
	if (orbit.turning) {
		this->anchored = false;
		this->yaw = std::fmod(this->yaw + (now.x - orbit.last.x) * _tuning.orbit_speed + FULL_TURN_DEGREES, FULL_TURN_DEGREES);
		this->pitch = Clamp(this->pitch + (now.y - orbit.last.y) * _tuning.orbit_speed, MIN_PITCH, MAX_PITCH);
		this->Frame();
	}
	orbit.last = now;
}

/* The scale changes before the anchor is pinned, so the anchored ground point stays under the pointer. */
void Camera::Settle(uint delta_ms)
{
	if (this->ppt != this->dest_ppt) {
		double share = Approach(delta_ms, _tuning.zoom_smooth_ms);
		this->ppt = std::exp(std::log(this->ppt) + (std::log(this->dest_ppt) - std::log(this->ppt)) * share);
		if (std::abs(this->dest_ppt - this->ppt) < this->dest_ppt * ZOOM_SNAP_SHARE) this->ppt = this->dest_ppt;
		this->Frame();
	}
	if (!this->anchored) return;

	this->Pin(this->anchor, this->anchor_screen.x, this->anchor_screen.y);
	if (this->ppt == this->dest_ppt) this->anchored = false;
}

/* The focus rides the ground under it, or the vehicle it follows, except while a pinned point must stay put. */
void Camera::Rest(uint delta_ms, std::optional<double> level)
{
	if (this->anchored || this->grab.has_value()) return;

	double wanted = level.value_or(GroundLevel(this->focus.first, this->focus.second));
	if (wanted == this->focus_level) return;
	this->focus_level += (wanted - this->focus_level) * Approach(delta_ms, _tuning.glide_ms);
	this->Frame();
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

/* Sliding the eye level along keeps every sight line's direction, so the one through the screen point is moved onto the ground point. */
void Camera::Pin(const WorldPoint &ground, double sx, double sy)
{
	Vec3 target = RenderPoint(ground);
	Vec3 sight = this->SightThrough(sx, sy);
	if (sight.z > -NEGLIGIBLE) return;

	double t = (target.z - this->eye.z) / sight.z;
	if (t <= 0.0) return;
	Vec3 shift = target - sight * t - this->eye;
	this->Place({this->focus.first + shift.x, this->focus.second + shift.y});
}
