/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file camera.cpp The top-down camera of the mini UI: where it looks, how close, and how it moves. */

#include "../../stdafx.h"
#include "camera.h"

#include "../../core/math_func.hpp"
#include "../../gfx_func.h"
#include "../../map_func.h"
#include "tuning.h"

#include "../../safeguards.h"

static constexpr uint8_t DIRKEY_LEFT = 1;
static constexpr uint8_t DIRKEY_UP = 2;
static constexpr uint8_t DIRKEY_RIGHT = 4;
static constexpr uint8_t DIRKEY_DOWN = 8;

static constexpr double MS_PER_SECOND = 1000.0;
static constexpr double PAN_STOP_SPEED = 5.0;
static constexpr double GLIDE_SNAP_PX = 0.5;
static constexpr double ZOOM_SNAP_SHARE = 0.002;

Camera _camera;

static double Approach(uint delta_ms, double time_constant_ms)
{
	return 1.0 - std::exp(delta_ms / -time_constant_ms);
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

int Camera::TilePixels() const
{
	return std::max(1, static_cast<int>(std::lround(this->ppt)));
}

void Camera::SetViewport(int width, int height)
{
	this->width = width;
	this->height = height;
}

/* Transposed projection: map X runs down the screen and map Y runs right,
 * matching the native isometric orientation's handedness. */
double Camera::BaseX() const
{
	return this->width * 0.5 - this->y * this->ppt;
}

double Camera::BaseY() const
{
	return this->height * 0.5 - this->x * this->ppt;
}

double Camera::ExactScreenX(double ty) const
{
	return ty * this->ppt + this->BaseX();
}

double Camera::ExactScreenY(double tx) const
{
	return tx * this->ppt + this->BaseY();
}

int Camera::ScreenX(double ty) const
{
	return static_cast<int>(std::lround(this->ExactScreenX(ty)));
}

int Camera::ScreenY(double tx) const
{
	return static_cast<int>(std::lround(this->ExactScreenY(tx)));
}

double Camera::MapXAt(int sy) const
{
	return (sy - this->BaseY()) / this->ppt;
}

double Camera::MapYAt(int sx) const
{
	return (sx - this->BaseX()) / this->ppt;
}

TilePoint Camera::MapAt(int sx, int sy) const
{
	return {this->MapXAt(sy), this->MapYAt(sx)};
}

Rect Camera::AreaRect(int tx0, int ty0, int tx1, int ty1) const
{
	return {this->ScreenX(ty0), this->ScreenY(tx0), this->ScreenX(ty1 + 1) - 1, this->ScreenY(tx1 + 1) - 1};
}

Rect Camera::TileRect(int tx, int ty) const
{
	return this->AreaRect(tx, ty, tx, ty);
}

Rect Camera::TileRect(TileIndex tile) const
{
	return this->TileRect(TileX(tile), TileY(tile));
}

void Camera::CentreOn(double tx, double ty)
{
	this->x = tx;
	this->y = ty;
	this->Confine();
}

void Camera::GlideTo(double tx, double ty)
{
	this->anchored = false;
	this->gliding = true;
	this->glide = {tx, ty};
	this->dest_ppt = std::max(this->dest_ppt, _tuning.jump_ppt);
}

void Camera::Drag(int dx, int dy)
{
	this->anchored = false;
	this->gliding = false;
	this->pan_vx = 0.0;
	this->pan_vy = 0.0;
	this->y -= dx * _tuning.drag_pan_multiplier / this->ppt;
	this->x -= dy * _tuning.drag_pan_multiplier / this->ppt;
	this->Confine();
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
	this->anchor = this->MapAt(sx, sy);
	this->anchored = true;
}

void Camera::Halt()
{
	this->anchored = false;
	this->gliding = false;
}

void Camera::Update(uint delta_ms, std::optional<TilePoint> chase)
{
	this->Pan(delta_ms);
	this->EdgeScroll(delta_ms);
	if (chase.has_value()) {
		this->gliding = false;
		this->MoveToward(*chase, Approach(delta_ms, _tuning.glide_ms));
	}
	this->Glide(delta_ms);
	this->Settle(delta_ms);
}

double Camera::TilesFor(double pixels_per_second, uint delta_ms) const
{
	return pixels_per_second * delta_ms / MS_PER_SECOND / this->ppt;
}

void Camera::Confine()
{
	this->x = Clamp<double>(this->x, 0.0, static_cast<double>(Map::SizeX()));
	this->y = Clamp<double>(this->y, 0.0, static_cast<double>(Map::SizeY()));
}

void Camera::MoveToward(TilePoint target, double share)
{
	this->x += (target.first - this->x) * share;
	this->y += (target.second - this->y) * share;
	this->Confine();
}

/* WASD and arrows arrive via _dirkeys; pan speed is constant in screen space. */
void Camera::Pan(uint delta_ms)
{
	if (_dirkeys != 0) {
		this->anchored = false;
		this->gliding = false;
	} else if (this->gliding || this->anchored) {
		this->pan_vx = 0.0;
		this->pan_vy = 0.0;
	}

	double speed = _shift_pressed ? _tuning.pan_speed_fast : _tuning.pan_speed;
	double decay = Approach(delta_ms, _tuning.pan_smooth_ms);
	this->pan_vx = AxisVelocity(this->pan_vx, (_dirkeys & DIRKEY_LEFT) != 0, (_dirkeys & DIRKEY_RIGHT) != 0, speed, decay);
	this->pan_vy = AxisVelocity(this->pan_vy, (_dirkeys & DIRKEY_UP) != 0, (_dirkeys & DIRKEY_DOWN) != 0, speed, decay);
	if (this->pan_vx == 0.0 && this->pan_vy == 0.0) return;

	this->y += this->TilesFor(this->pan_vx, delta_ms);
	this->x += this->TilesFor(this->pan_vy, delta_ms);
	this->Confine();
}

void Camera::EdgeScroll(uint delta_ms)
{
	if (_tuning.edge_scroll == 0 || !_cursor.in_window || _middle_button_down) return;

	double step = this->TilesFor(_tuning.edge_scroll_speed, delta_ms);
	double ex = 0.0;
	double ey = 0.0;
	if (_cursor.pos.x < _tuning.edge_margin) ex = -step;
	if (_cursor.pos.x >= this->width - _tuning.edge_margin) ex = step;
	if (_cursor.pos.y < _tuning.edge_margin) ey = -step;
	if (_cursor.pos.y >= this->height - _tuning.edge_margin) ey = step;
	if (ex == 0.0 && ey == 0.0) return;

	this->anchored = false;
	this->gliding = false;
	this->y += ex;
	this->x += ey;
	this->Confine();
}

void Camera::Glide(uint delta_ms)
{
	if (!this->gliding) return;

	this->MoveToward(this->glide, Approach(delta_ms, _tuning.glide_ms));
	bool arrived = std::abs(this->glide.first - this->x) * this->ppt < GLIDE_SNAP_PX && std::abs(this->glide.second - this->y) * this->ppt < GLIDE_SNAP_PX;
	if (!arrived) return;

	this->CentreOn(this->glide.first, this->glide.second);
	this->gliding = false;
}

void Camera::Settle(uint delta_ms)
{
	if (this->ppt != this->dest_ppt) {
		double share = Approach(delta_ms, _tuning.zoom_smooth_ms);
		this->ppt = std::exp(std::log(this->ppt) + (std::log(this->dest_ppt) - std::log(this->ppt)) * share);
		if (std::abs(this->dest_ppt - this->ppt) < this->dest_ppt * ZOOM_SNAP_SHARE) this->ppt = this->dest_ppt;
	}
	if (!this->anchored) return;

	this->x = this->anchor.first - (this->anchor_sy - this->height * 0.5) / this->ppt;
	this->y = this->anchor.second - (this->anchor_sx - this->width * 0.5) / this->ppt;
	this->Confine();
	if (this->ppt == this->dest_ppt) this->anchored = false;
}
