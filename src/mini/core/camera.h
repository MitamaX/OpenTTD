/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file camera.h The dimetric camera of the mini UI: where it looks, how close, which way it faces, and how it moves. */

#ifndef MINI_CORE_CAMERA_H
#define MINI_CORE_CAMERA_H

#include <array>
#include <numbers>
#include <optional>
#include <utility>

#include "../../core/geometry_type.hpp"
#include "../../tile_type.h"

inline constexpr double MIN_PPT = 4.0;
inline constexpr double MAX_PPT = 64.0;
inline constexpr double LEVEL_TILES = static_cast<double>(TILE_HEIGHT) / TILE_SIZE;
inline constexpr int GROUND_SEARCH_STEPS = 24;
inline constexpr double GROUND_SEARCH_SHARE = 0.5;
inline constexpr double VIEW_DEPTH = 0.5;
inline constexpr double NATIVE_VIEW_RISE = 1.0 / (4.0 * std::numbers::sqrt2);
/* Beyond this the steepest slope nears the line of sight and the ground search stops converging. */
inline constexpr double MAX_HEIGHT_SCALE = 1.5;

using TilePoint = std::pair<double, double>;

struct WorldPoint {
	double x;
	double y;
	double level;
};

struct MapVector {
	double x;
	double y;
};

struct ExactPoint {
	double x;
	double y;
};

/* A view framed from outside: the ground point at the screen centre, pixels per tile there, the compass bearing
 * the view faces clockwise from map north and how far the view dips below the horizon, which a fixed-tilt camera leaves alone. */
struct ViewAim {
	TilePoint focus;
	double zoom;
	double yaw;
	double pitch;
};

struct TileSpan {
	int tx0;
	int ty0;
	int tx1;
	int ty1;
};

double GroundLevel(double tx, double ty);
double ViewRise();
double SmoothStep(double edge0, double edge1, double x);

class Camera {
public:
	Camera();

	double X() const { return this->x; }
	double Y() const { return this->y; }
	double Ppt() const { return this->ppt; }
	MapVector Right() const { return this->right; }
	MapVector Toward() const { return this->toward; }
	int TilePixels() const;
	int Width() const { return this->width; }
	int Height() const { return this->height; }

	void SetViewport(int width, int height);
	void SetPeak(uint level) { this->peak = level; }

	ExactPoint ScreenStep(MapVector step) const;
	ExactPoint ExactScreenOf(const WorldPoint &point) const;
	Point ScreenOf(const WorldPoint &point) const;
	Point ScreenOfGround(double tx, double ty) const;
	double HeadingDegrees(MapVector direction) const;
	TilePoint MapAt(int sx, int sy) const;
	TileSpan VisibleTiles(double raised_levels = 0.0) const;
	std::array<TilePoint, 4> ViewCorners() const;

	void CentreOn(double tx, double ty);
	void Aim(const ViewAim &aim);
	void PlaceCentre(TilePoint plane);
	void GlideTo(double tx, double ty);
	void Grab(int sx, int sy);
	void Zoom(bool in);
	void ZoomAt(int sx, int sy, bool in);
	void Turn(int quarters);
	void FaceNorth();
	void Halt();
	void Update(uint delta_ms, std::optional<WorldPoint> chase);

private:
	TilePoint Shifted(TilePoint plane, double dx, double dy) const;
	double Lift(double levels) const;
	WorldPoint Sighted(double dx, double dy, double level) const;
	WorldPoint GroundAt(double dx, double dy) const;
	WorldPoint GroundUnder(int sx, int sy) const;
	TilePoint Focus(const WorldPoint &point) const;
	TilePoint GroundFocus(TilePoint ground) const;
	bool Turning() const;
	void SetHeading(double degrees);
	void Face(double degrees);
	void MoveToward(TilePoint target, double share);
	void MoveBy(double dx, double dy);
	void Pan(uint delta_ms);
	void EdgeScroll(uint delta_ms);
	void Glide(uint delta_ms);
	void Spin(uint delta_ms);
	void Settle(uint delta_ms);
	void FollowGrab();
	void Pin(const WorldPoint &ground, int sx, int sy);

	int width = 0;
	int height = 0;

	/* The level-0 plane point under the screen centre in tiles; ppt is pixels per tile across the screen. */
	double x = 0.0;
	double y = 0.0;
	double ppt = 16.0;
	double dest_ppt = 16.0;

	/* The map direction pointing down the screen, in degrees turned from map X toward map Y. */
	double heading = 0.0;
	double heading_target = 0.0;
	MapVector right{};
	MapVector toward{};
	uint peak = 0;

	bool anchored = false;
	int anchor_sx = 0;
	int anchor_sy = 0;
	WorldPoint anchor{};

	bool gliding = false;
	TilePoint glide{};

	std::optional<WorldPoint> grab;

	double pan_vx = 0.0;
	double pan_vy = 0.0;
};

extern Camera _camera;

#endif /* MINI_CORE_CAMERA_H */
