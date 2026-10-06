/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file space.h Points, directions and transforms of the mini map's 3D world. */

#ifndef MINI_CORE_SPACE_H
#define MINI_CORE_SPACE_H

#include <array>
#include <cmath>

struct Vec3 {
	double x;
	double y;
	double z;
};

constexpr Vec3 operator+(const Vec3 &a, const Vec3 &b)
{
	return {a.x + b.x, a.y + b.y, a.z + b.z};
}

constexpr Vec3 operator-(const Vec3 &a, const Vec3 &b)
{
	return {a.x - b.x, a.y - b.y, a.z - b.z};
}

constexpr Vec3 operator*(const Vec3 &v, double scale)
{
	return {v.x * scale, v.y * scale, v.z * scale};
}

constexpr double Dot(const Vec3 &a, const Vec3 &b)
{
	return a.x * b.x + a.y * b.y + a.z * b.z;
}

constexpr Vec3 Cross(const Vec3 &a, const Vec3 &b)
{
	return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}

inline double Length(const Vec3 &v)
{
	return std::sqrt(Dot(v, v));
}

inline Vec3 Normalised(const Vec3 &v)
{
	double length = Length(v);
	return length > 0.0 ? v * (1.0 / length) : v;
}

/* Column major, as GL takes it. */
struct Mat4 {
	std::array<double, 16> m{};

	double &At(int row, int column) { return this->m[column * 4 + row]; }
	double At(int row, int column) const { return this->m[column * 4 + row]; }

	std::array<float, 16> Floats() const;
	Mat4 operator*(const Mat4 &other) const;

	static Mat4 Identity();
	static Mat4 Translation(const Vec3 &offset);
	static Mat4 Scaling(const Vec3 &scale);
	static Mat4 Turn(const Vec3 &axis, double radians);
	static Mat4 View(const Vec3 &eye, const Vec3 &right, const Vec3 &up, const Vec3 &back);
	static Mat4 Perspective(double focal_x, double focal_y, double near, double far);
	static Mat4 Orthographic(const Vec3 &low, const Vec3 &high);
};

Vec3 Transformed(const Mat4 &transform, const Vec3 &point);
Vec3 TransformedNormal(const Mat4 &transform, const Vec3 &normal);

/* A plane as its unit normal and offset; points with a positive distance lie on its inner side. */
struct Plane {
	Vec3 normal;
	double offset;

	double Distance(const Vec3 &point) const { return Dot(this->normal, point) + this->offset; }
};

using Frustum = std::array<Plane, 6>;

Frustum FrustumOf(const Mat4 &view_projection);
bool BoxMeets(const Frustum &frustum, const Vec3 &low, const Vec3 &high);

#endif /* MINI_CORE_SPACE_H */
