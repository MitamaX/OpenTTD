/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file space.cpp Points, directions and transforms of the mini map's 3D world. */

#include "../../stdafx.h"
#include "space.h"

#include "../../safeguards.h"

static constexpr int ROWS = 4;
static constexpr int CLIP_W = 3;

std::array<float, 16> Mat4::Floats() const
{
	std::array<float, 16> floats;
	for (size_t i = 0; i < floats.size(); i++) floats[i] = static_cast<float>(this->m[i]);
	return floats;
}

Mat4 Mat4::operator*(const Mat4 &other) const
{
	Mat4 product;
	for (int row = 0; row < ROWS; row++) {
		for (int column = 0; column < ROWS; column++) {
			double sum = 0.0;
			for (int k = 0; k < ROWS; k++) sum += this->At(row, k) * other.At(k, column);
			product.At(row, column) = sum;
		}
	}
	return product;
}

Mat4 Mat4::View(const Vec3 &eye, const Vec3 &right, const Vec3 &up, const Vec3 &back)
{
	Mat4 view;
	const std::array<Vec3, 3> axes = {right, up, back};
	for (int row = 0; row < static_cast<int>(axes.size()); row++) {
		view.At(row, 0) = axes[row].x;
		view.At(row, 1) = axes[row].y;
		view.At(row, 2) = axes[row].z;
		view.At(row, 3) = -Dot(axes[row], eye);
	}
	view.At(CLIP_W, CLIP_W) = 1.0;
	return view;
}

/* The focal lengths are in clip units: half the screen's width or height spans one. */
Mat4 Mat4::Perspective(double focal_x, double focal_y, double near, double far)
{
	Mat4 projection;
	projection.At(0, 0) = focal_x;
	projection.At(1, 1) = focal_y;
	projection.At(2, 2) = -(far + near) / (far - near);
	projection.At(2, 3) = -2.0 * far * near / (far - near);
	projection.At(CLIP_W, 2) = -1.0;
	return projection;
}

static Plane PlaneOf(const Mat4 &clip, int row, double sign)
{
	Vec3 normal = {clip.At(CLIP_W, 0) + sign * clip.At(row, 0), clip.At(CLIP_W, 1) + sign * clip.At(row, 1), clip.At(CLIP_W, 2) + sign * clip.At(row, 2)};
	double offset = clip.At(CLIP_W, 3) + sign * clip.At(row, 3);
	double length = Length(normal);
	return {normal * (1.0 / length), offset / length};
}

Frustum FrustumOf(const Mat4 &view_projection)
{
	Frustum frustum;
	for (int axis = 0; axis < 3; axis++) {
		frustum[2 * axis] = PlaneOf(view_projection, axis, 1.0);
		frustum[2 * axis + 1] = PlaneOf(view_projection, axis, -1.0);
	}
	return frustum;
}
