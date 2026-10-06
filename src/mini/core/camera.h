/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file camera.h The orbiting perspective camera of the mini UI: where it looks, how close, which way it faces, and how it moves. */

#ifndef MINI_CORE_CAMERA_H
#define MINI_CORE_CAMERA_H

#include <array>
#include <cmath>
#include <optional>
#include <utility>

#include "../../core/geometry_type.hpp"
#include "../../tile_type.h"
#include "space.h"

inline constexpr double MIN_PPT = 2.0;
inline constexpr double MAX_PPT = 160.0;
inline constexpr double LEVEL_TILES = static_cast<double>(TILE_HEIGHT) / TILE_SIZE;
inline constexpr double MIN_PITCH = 20.0;
inline constexpr double MAX_PITCH = 89.0;

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

constexpr MapVector operator+(const MapVector &a, const MapVector &b)
{
	return {a.x + b.x, a.y + b.y};
}

constexpr MapVector operator-(const MapVector &a, const MapVector &b)
{
	return {a.x - b.x, a.y - b.y};
}

constexpr MapVector operator*(const MapVector &v, double scale)
{
	return {v.x * scale, v.y * scale};
}

constexpr double Dot(const MapVector &a, const MapVector &b)
{
	return a.x * b.x + a.y * b.y;
}

/* The direction a quarter turn clockwise from this one, as the map is seen from above. */
constexpr MapVector RightOf(const MapVector &v)
{
	return {v.y, -v.x};
}

inline MapVector Unit(const MapVector &v)
{
	double length = std::hypot(v.x, v.y);
	return length > 0.0 ? v * (1.0 / length) : v;
}

struct ExactPoint {
	double x;
	double y;
};

/* A view framed from outside: the ground point at the screen centre, pixels per tile there, the compass bearing
 * the view faces clockwise from map north and how far the view dips below the horizon. */
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
double LevelRise();
Vec3 RenderPoint(const WorldPoint &point);
double SmoothStep(double edge0, double edge1, double x);

class Camera {
public:
	Camera();

	double Ppt() const { return this->ppt; }
	int TilePixels() const;
	int Width() const { return this->width; }
	int Height() const { return this->height; }

	void SetViewport(int width, int height);
	void SetPeak(uint level) { this->peak = level; }

	const Vec3 &Eye() const { return this->eye; }
	double Focal() const;
	double FocusDistance() const;
	double Near() const { return this->near; }
	double Far() const { return this->far; }
	Mat4 ViewMatrix() const;
	Mat4 ProjectionMatrix() const;
	Vec3 SightThrough(double sx, double sy) const;
	MapVector Toward(TilePoint at) const;

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
	void GlideTo(double tx, double ty);
	void Grab(int sx, int sy);
	void HoldOrbit(int sx, int sy);
	bool ReleaseOrbit();
	void Zoom(bool in);
	void ZoomAt(int sx, int sy, bool in);
	void FaceNorth();
	void Halt();
	void Update(uint delta_ms, std::optional<WorldPoint> chase);

private:
	struct Orbit {
		Point last;
		Point start;
		bool turning;
	};

	double ZoomDistance() const;
	WorldPoint FocusPoint() const;
	Vec3 Forward() const;
	WorldPoint GroundUnder(double sx, double sy) const;
	void Frame();
	void Place(TilePoint focus);
	void Pin(const WorldPoint &ground, double sx, double sy);
	void MoveToward(TilePoint target, double share);
	void MoveBy(double dx, double dy);
	void Pan(uint delta_ms);
	void EdgeScroll(uint delta_ms);
	void Glide(uint delta_ms);
	void Spin(uint delta_ms);
	void Swing();
	void Settle(uint delta_ms);
	void Rest(uint delta_ms, std::optional<double> level);
	void FollowGrab();

	int width = 1;
	int height = 1;

	/* The ground point the view turns about, pixels per tile there, and the view's bearing and dip in degrees. */
	TilePoint focus{};
	double focus_level = 0.0;
	double ppt = 16.0;
	double dest_ppt = 16.0;
	double yaw = 0.0;
	double pitch = 50.0;
	double spin = 0.0;
	uint peak = 0;

	Vec3 eye{};
	Vec3 right{};
	Vec3 up{};
	Vec3 back{};
	double near = 1.0;
	double far = 100.0;

	bool anchored = false;
	Point anchor_screen{};
	WorldPoint anchor{};

	bool gliding = false;
	TilePoint glide{};

	std::optional<WorldPoint> grab;
	std::optional<Orbit> orbit;

	double pan_vx = 0.0;
	double pan_vy = 0.0;
};

extern Camera _camera;

#endif /* MINI_CORE_CAMERA_H */
