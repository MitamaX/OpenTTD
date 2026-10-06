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
static constexpr double FACE_OFFSET = 0.012;
static constexpr double MOUTH_DEPTH = 0.04;
static constexpr double CORNICE_HEIGHT = 0.03;
static constexpr double CORNICE_REACH = 0.05;
static constexpr double CORNICE_OVERHANG = 0.02;
static constexpr int ARCH_STEPS = 10;
static constexpr uint32_t PORTAL_STONE = 0x8E887C;
static constexpr uint32_t CORNICE_STONE = 0xA59F92;
static constexpr uint32_t MOUTH = 0x141618;

/* Points on the portal's face as distances right of the way's centre line and heights above the tunnel floor. */
class PortalFace {
public:
	PortalFace(const MapVector &edge, const MapVector &inward) : edge(edge), right(RightOf(inward)), facing({-inward.x, -inward.y, 0.0}) {}

	uint32_t Point(ModelMesh &mesh, double across, double height, double back = 0.0) const
	{
		MapVector at = this->edge + this->right * across - MapVector{this->facing.x, this->facing.y} * back;
		return mesh.Point({at.x, at.y, height}, this->facing);
	}

	void Quad(ModelMesh &mesh, double left, double right, double low, double high) const
	{
		mesh.Quad(this->Point(mesh, left, low), this->Point(mesh, right, low), this->Point(mesh, right, high), this->Point(mesh, left, high));
	}

private:
	MapVector edge;
	MapVector right;
	Vec3 facing;
};

/* The face stands as high as the step the hill shows over the cut, its arch springing from the mouth's sides and rising clear of the top. */
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
	double rise = LevelRise();
	double height = std::max(hill - floor, 1.0) * rise;
	double mouth = GetTunnelBridgeTransportType(entrance) == TRANSPORT_RAIL ? RAIL_MOUTH_HALF : ROAD_MOUTH_HALF;
	double spring = height * SPRING_SHARE;
	double arch = std::min(height * ARCH_SHARE, height - spring - CORNICE_HEIGHT);

	std::vector<MapVector> outline = {{-mouth, 0.0}};
	for (int step = 0; step <= ARCH_STEPS; step++) {
		double angle = std::numbers::pi * (1.0 - static_cast<double>(step) / ARCH_STEPS);
		outline.push_back({mouth * std::cos(angle), spring + arch * std::sin(angle)});
	}
	outline.push_back({mouth, 0.0});

	PortalFace face(edge - inward * FACE_OFFSET, inward);
	ModelMesh stone;
	face.Quad(stone, -FACE_HALF, -mouth, 0.0, height);
	face.Quad(stone, mouth, FACE_HALF, 0.0, height);
	for (size_t point = 1; point + 2 < outline.size(); point++) {
		const MapVector &from = outline[point];
		const MapVector &to = outline[point + 1];
		stone.Quad(face.Point(stone, from.x, from.y), face.Point(stone, to.x, to.y), face.Point(stone, to.x, height), face.Point(stone, from.x, height));
	}
	ModelMesh opening;
	uint32_t middle = face.Point(opening, 0.0, spring, MOUTH_DEPTH);
	for (size_t point = 0; point < outline.size(); point++) {
		const MapVector &from = outline[point];
		const MapVector &to = outline[(point + 1) % outline.size()];
		opening.Triangle(middle, face.Point(opening, from.x, from.y, MOUTH_DEPTH), face.Point(opening, to.x, to.y, MOUTH_DEPTH));
	}

	MapVector along = RightOf(inward);
	MapVector cornice_middle = edge - inward * (FACE_OFFSET + CORNICE_REACH * 0.5);
	ModelMesh cornice = Block(cornice_middle, along, {-FACE_HALF - CORNICE_OVERHANG, -CORNICE_REACH * 0.5, height}, {FACE_HALF + CORNICE_OVERHANG, CORNICE_REACH * 0.5, height + CORNICE_HEIGHT});

	ModelMesh portal = stone.Paint(PORTAL_STONE);
	portal.Append(opening.Paint(MOUTH));
	portal.Append(cornice.Paint(CORNICE_STONE));
	mesh.Append(Drape(portal, [floor](double, double) { return floor; }));
}
