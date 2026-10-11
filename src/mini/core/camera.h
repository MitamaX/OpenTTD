/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file camera.h The orbiting perspective camera of the mini UI: where it looks, how close, which way it faces, and how it moves. */

#ifndef MINI_CORE_CAMERA_H
#define MINI_CORE_CAMERA_H

#include <algorithm>
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

inline double Length(const MapVector &v)
{
	return std::hypot(v.x, v.y);
}

inline MapVector Unit(const MapVector &v)
{
	double length = Length(v);
	return length > 0.0 ? v * (1.0 / length) : v;
}

inline double ShareAlong(const MapVector &point, const MapVector &from, const MapVector &to)
{
	MapVector along = to - from;
	return std::clamp(Dot(point - from, along) / Dot(along, along), 0.0, 1.0);
}

inline double SegmentDistance(const MapVector &point, const MapVector &from, const MapVector &to)
{
	return Length(from + (to - from) * ShareAlong(point, from, to) - point);
}

/* Where a point lies on the map, whatever its level. */
constexpr MapVector Flat(const WorldPoint &point)
{
	return {point.x, point.y};
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

	bool Holds(int tx, int ty) const { return tx >= this->tx0 && ty >= this->ty0 && tx <= this->tx1 && ty <= this->ty1; }

	template <typename Visit>
	void ForEach(Visit visit) const
	{
		for (int tx = this->tx0; tx <= this->tx1; tx++) {
			for (int ty = this->ty0; ty <= this->ty1; ty++) visit(tx, ty);
		}
	}
};

/* The eye and the lens it looks through, which size a tile by how far off it stands. */
struct Lens {
	Vec3 eye;
	double focal;
	double near_plane;

	double TilePixelsAt(double distance) const { return this->focal / std::max(distance, this->near_plane); }
	/* Where a box comes nearest the eye, and where it lies farthest from it. */
	Vec3 NearestPoint(const Vec3 &low, const Vec3 &high) const;
	Vec3 FarthestPoint(const Vec3 &low, const Vec3 &high) const;
	/* Tile pixels at each of them. */
	double NearestTilePixels(const Vec3 &low, const Vec3 &high) const;
	double FarthestTilePixels(const Vec3 &low, const Vec3 &high) const;
};

inline double SquaredDistanceToTile(double x, double y, int tx, int ty)
{
	double dx = std::max({tx - x, 0.0, x - (tx + 1)});
	double dy = std::max({ty - y, 0.0, y - (ty + 1)});
	return dx * dx + dy * dy;
}

double GroundLevel(double tx, double ty);
double LevelRise();
Vec3 RenderPoint(const WorldPoint &point);
double SmoothStep(double edge0, double edge1, double x);
/* The level way a compass bearing faces, in degrees clockwise from map north. */
Vec3 Bearing(double degrees);

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
	Lens EyeLens() const;
	double Focal() const;
	double FocusDistance() const;
	double Near() const { return this->near_plane; }
	double Far() const { return this->far_plane; }
	Mat4 ViewMatrix() const;
	Mat4 ProjectionMatrix() const;
	Vec3 SightThrough(double sx, double sy) const;
	MapVector Toward(TilePoint at) const;

	ExactPoint ScreenStep(MapVector step) const;
	ExactPoint ExactScreenOf(const WorldPoint &point) const;
	double Ahead(const WorldPoint &point) const;
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
	void Update(double delta_ms, std::optional<WorldPoint> chase);

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
	void Pan(double delta_ms);
	void EdgeScroll(double delta_ms);
	void Glide(double delta_ms);
	void Spin(double delta_ms);
	void Swing();
	void Settle(double delta_ms);
	void Rest(double delta_ms, std::optional<double> level);
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
	double near_plane = 1.0;
	double far_plane = 100.0;

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
