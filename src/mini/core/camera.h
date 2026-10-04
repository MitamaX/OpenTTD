/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file camera.h The top-down camera of the mini UI: where it looks, how close, and how it moves. */

#ifndef MINI_CORE_CAMERA_H
#define MINI_CORE_CAMERA_H

#include <optional>
#include <utility>

inline constexpr double MIN_PPT = 4.0;
inline constexpr double MAX_PPT = 64.0;

using TilePoint = std::pair<double, double>;

class Camera {
public:
	double X() const { return this->x; }
	double Y() const { return this->y; }
	double Ppt() const { return this->ppt; }

	void SetViewport(int width, int height);

	double ExactScreenX(double ty) const;
	double ExactScreenY(double tx) const;
	int ScreenX(double ty) const;
	int ScreenY(double tx) const;
	double MapXAt(int sy) const;
	double MapYAt(int sx) const;

	void CentreOn(double tx, double ty);
	void GlideTo(double tx, double ty);
	void Drag(int dx, int dy);
	void Zoom(bool in);
	void ZoomAt(int sx, int sy, bool in);
	void Halt();
	void Update(uint delta_ms, std::optional<TilePoint> chase);

private:
	double BaseX() const;
	double BaseY() const;
	double TilesFor(double pixels_per_second, uint delta_ms) const;
	void Confine();
	void MoveToward(TilePoint target, double share);
	void Pan(uint delta_ms);
	void EdgeScroll(uint delta_ms);
	void Glide(uint delta_ms);
	void Settle(uint delta_ms);

	int width = 0;
	int height = 0;

	/* Position in tile units at the screen centre; ppt is pixels per tile. */
	double x = 0.0;
	double y = 0.0;
	double ppt = 16.0;
	double dest_ppt = 16.0;

	bool anchored = false;
	int anchor_sx = 0;
	int anchor_sy = 0;
	TilePoint anchor{};

	bool gliding = false;
	TilePoint glide{};

	double pan_vx = 0.0;
	double pan_vy = 0.0;
};

extern Camera _camera;

#endif /* MINI_CORE_CAMERA_H */
