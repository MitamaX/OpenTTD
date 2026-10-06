/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file tunnel_models.cpp A tunnel's portal in 3D: a stone face with an arched mouth, set into the hillside over the cut its way runs into. */

#include "../../stdafx.h"
#include "tunnel_models.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <numbers>
#include <vector>

#include "../../map_func.h"
#include "../../tunnelbridge_map.h"
#include "../map/tile_shapes.h"
#include "way_shapes.h"

#include "../../safeguards.h"

static constexpr double FACE_HALF = 0.42;
static constexpr double RAIL_MOUTH_HALF = 0.22;
static constexpr double ROAD_MOUTH_HALF = 0.31;
static constexpr double SPRING_SHARE = 0.45;
static constexpr double ARCH_SHARE = 0.35;
static constexpr double FACE_PROUD = 0.05;
static constexpr double MOUTH_PROUD = 0.006;
static constexpr double CORNICE_HEIGHT = 0.03;
static constexpr double CORNICE_PROUD = 0.07;
static constexpr double CORNICE_OVERHANG = 0.02;
static constexpr int ARCH_STEPS = 10;
static constexpr uint32_t PORTAL_STONE = 0x8E887C;
static constexpr uint32_t REVEAL_STONE = 0x6F6A60;
static constexpr uint32_t CORNICE_STONE = 0xA59F92;
static constexpr uint32_t MOUTH = 0x141618;

/* Points on the portal as distances right of the way's centre line, heights above the tunnel floor and how far they stand out of the hill's face. */
class PortalFrame {
public:
	PortalFrame(const MapVector &edge, const MapVector &inward) : edge(edge), inward(inward), right(RightOf(inward)) {}

	Vec3 At(double across, double height, double proud) const
	{
		MapVector at = this->edge + this->right * across - this->inward * proud;
		return {at.x, at.y, height};
	}

	/* A direction in the plane of the face: across to the right and up. */
	Vec3 Facing(double across, double up) const
	{
		return {this->right.x * across, this->right.y * across, up};
	}

	Vec3 Out() const { return {-this->inward.x, -this->inward.y, 0.0}; }

	void Quad(ModelMesh &mesh, std::array<Vec3, 4> corners, const Vec3 &facing) const
	{
		std::array<uint32_t, 4> points;
		std::ranges::transform(corners, points.begin(), [&](const Vec3 &corner) { return mesh.Point(corner, facing); });
		mesh.Quad(points[0], points[1], points[2], points[3]);
	}

private:
	MapVector edge;
	MapVector inward;
	MapVector right;
};

/* The mouth's edge, up one side, over the arch and down the other, as distances across and heights. */
static std::vector<MapVector> MouthOutline(double mouth, double spring, double arch)
{
	std::vector<MapVector> outline = {{-mouth, 0.0}};
	for (int step = 0; step <= ARCH_STEPS; step++) {
		double angle = std::numbers::pi * (1.0 - static_cast<double>(step) / ARCH_STEPS);
		outline.push_back({mouth * std::cos(angle), spring + arch * std::sin(angle)});
	}
	outline.push_back({mouth, 0.0});
	return outline;
}

/* The face stands proud of the step the hill shows over the cut and as high as it; the arch springs from the mouth's sides, its reveal running back to a dark mouth. */
void LayTunnelPortal(ModelMesh &mesh, TileIndex entrance)
{
	int tx = TileX(entrance);
	int ty = TileY(entrance);
	MapVector inward = Outward(GetTunnelBridgeDirection(entrance));
	MapVector centre = {tx + HALF_TILE, ty + HALF_TILE};
	MapVector edge = centre + inward * HALF_TILE;
	double floor = TileGround(tx, ty).Level(centre.x, centre.y);
	int nx = tx + static_cast<int>(inward.x);
	int ny = ty + static_cast<int>(inward.y);
	double hill = OnMap(nx, ny) ? TileGround(nx, ny).Level(edge.x, edge.y) : floor + 1.0;
	double height = std::max(hill - floor, 1.0) * LevelRise();
	double mouth = GetTunnelBridgeTransportType(entrance) == TRANSPORT_RAIL ? RAIL_MOUTH_HALF : ROAD_MOUTH_HALF;
	double spring = height * SPRING_SHARE;
	double arch = std::min(height * ARCH_SHARE, height - spring - CORNICE_HEIGHT);
	std::vector<MapVector> outline = MouthOutline(mouth, spring, arch);

	PortalFrame frame(edge, inward);
	Vec3 out = frame.Out();
	ModelMesh stone;
	frame.Quad(stone, {frame.At(-FACE_HALF, 0.0, FACE_PROUD), frame.At(-mouth, 0.0, FACE_PROUD), frame.At(-mouth, height, FACE_PROUD), frame.At(-FACE_HALF, height, FACE_PROUD)}, out);
	frame.Quad(stone, {frame.At(mouth, 0.0, FACE_PROUD), frame.At(FACE_HALF, 0.0, FACE_PROUD), frame.At(FACE_HALF, height, FACE_PROUD), frame.At(mouth, height, FACE_PROUD)}, out);
	for (double side : {-1.0, 1.0}) {
		double across = side * FACE_HALF;
		frame.Quad(stone, {frame.At(across, 0.0, 0.0), frame.At(across, 0.0, FACE_PROUD), frame.At(across, height, FACE_PROUD), frame.At(across, height, 0.0)}, frame.Facing(side, 0.0));
	}
	ModelMesh reveal;
	ModelMesh opening;
	uint32_t middle = opening.Point(frame.At(0.0, spring, MOUTH_PROUD), out);
	for (size_t point = 0; point + 1 < outline.size(); point++) {
		const MapVector &from = outline[point];
		const MapVector &to = outline[point + 1];
		if (point > 0 && point + 2 < outline.size()) {
			frame.Quad(stone, {frame.At(from.x, from.y, FACE_PROUD), frame.At(to.x, to.y, FACE_PROUD), frame.At(to.x, height, FACE_PROUD), frame.At(from.x, height, FACE_PROUD)}, out);
		}
		Vec3 inside = frame.Facing(-(from.x + to.x) * 0.5, spring - (from.y + to.y) * 0.5);
		frame.Quad(reveal, {frame.At(from.x, from.y, FACE_PROUD), frame.At(from.x, from.y, MOUTH_PROUD), frame.At(to.x, to.y, MOUTH_PROUD), frame.At(to.x, to.y, FACE_PROUD)}, inside);
		opening.Triangle(middle, opening.Point(frame.At(from.x, from.y, MOUTH_PROUD), out), opening.Point(frame.At(to.x, to.y, MOUTH_PROUD), out));
	}

	MapVector cornice_middle = edge - inward * (CORNICE_PROUD * 0.5);
	ModelMesh cornice = Block(cornice_middle, RightOf(inward), {-FACE_HALF - CORNICE_OVERHANG, -CORNICE_PROUD * 0.5, height}, {FACE_HALF + CORNICE_OVERHANG, CORNICE_PROUD * 0.5, height + CORNICE_HEIGHT});

	ModelMesh portal = stone.Paint(PORTAL_STONE);
	portal.Append(reveal.Paint(REVEAL_STONE));
	portal.Append(opening.Paint(MOUTH));
	portal.Append(cornice.Paint(CORNICE_STONE));
	mesh.Append(Drape(portal, [floor](double, double) { return floor; }));
}
