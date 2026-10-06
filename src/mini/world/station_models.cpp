/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file station_models.cpp Stations in 3D: rail platforms with their buildings and roofs as each tile's layout stands them, waypoint gantries, and bus shelters. */

#include "../../stdafx.h"
#include "station_models.h"

#include <algorithm>
#include <array>

#include "../../direction_func.h"
#include "../../map_func.h"
#include "../../station_map.h"
#include "../map/tile_shapes.h"
#include "road_models.h"

#include "../../safeguards.h"

/** What a rail station tile's layout stands on its platforms, after the game's own layouts. */
enum class StopLayout : uint8_t {
	Platforms,
	Building,
	RearRoof,
	FrontRoof,
};

static constexpr int LAYOUTS = 4;
static constexpr double EDGE = 0.5;
static constexpr double PLATFORM_INNER = 0.19;
static constexpr double PLATFORM_TOP = 0.11;
static constexpr double PLATFORM_FOOT = -0.012;
static constexpr double COPING_WIDTH = 0.03;
static constexpr double COPING_RISE = 0.004;
static constexpr double SAFETY_FROM = 0.24;
static constexpr double SAFETY_TO = 0.255;
static constexpr double SAFETY_RISE = 0.002;

static constexpr double BUILDING_HEIGHT = 0.34;
static constexpr double WINDOWS_LOW = 0.15;
static constexpr double WINDOWS_HIGH = 0.27;
static constexpr double WINDOWS_PROUD = 0.003;
static constexpr double AWNING_REACH = 0.09;
static constexpr double AWNING_LOW = 0.255;
static constexpr double AWNING_HIGH = 0.268;

static constexpr double ROOF_EAVES = 0.43;
static constexpr double ROOF_RIDGE = 0.5;
static constexpr double ROOF_HALF = 0.52;
static constexpr double ROOF_THICKNESS = 0.012;
static constexpr double PILLAR_LATERAL = 0.37;
static constexpr double PILLAR_HALF = 0.012;
static constexpr std::array<double, 2> PILLAR_SHARES = {0.25, 0.75};
static constexpr double RIB_HALF = 0.007;

static constexpr double GANTRY_LATERAL = 0.33;
static constexpr double GANTRY_HEIGHT = 0.42;
static constexpr double GANTRY_HALF = 0.012;
static constexpr Vec3 SIGN_LOW = {-0.006, -0.08, 0.36};
static constexpr Vec3 SIGN_HIGH = {0.006, 0.08, 0.41};

static constexpr double SHELTER_FROM = 0.3;
static constexpr double SHELTER_TO = 0.7;
static constexpr double SHELTER_FRONT = 0.34;
static constexpr double SHELTER_BACK = 0.47;
static constexpr double SHELTER_PANEL = 0.008;
static constexpr double SHELTER_HEIGHT = 0.21;
static constexpr double SHELTER_ROOF = 0.014;

static constexpr uint32_t PLATFORM = 0xB4AFA5;
static constexpr uint32_t PLATFORM_SIDE = 0x8C887F;
static constexpr uint32_t COPING = 0xDDD8CC;
static constexpr uint32_t SAFETY_LINE = 0xE3BF3A;
static constexpr uint32_t WALL = 0xD6C8AA;
static constexpr uint32_t BUILDING_ROOF = 0x5E5853;
static constexpr uint32_t GLASS = 0x5A7C8E;
static constexpr uint32_t AWNING = 0x7B2E2A;
static constexpr uint32_t ROOF_GLASS = 0x7FA2B2;
static constexpr uint32_t FRAME = 0x4E555C;
static constexpr uint32_t SIGN = 0xE8C440;
static constexpr double GLASS_GLOSS = 0.8;
static constexpr double FRAME_GLOSS = 0.4;

static Footing GroundOf(TileIndex tile)
{
	return [ground = TileGround(tile)](double x, double y) { return ground.Level(x, y); };
}

/* A run along an axis through the middle of a tile, edge to edge. */
static Stretch MiddleRun(TileIndex tile, Axis axis)
{
	MapVector middle = {TileX(tile) + EDGE, TileY(tile) + EDGE};
	MapVector along = axis == AXIS_X ? MapVector{EDGE, 0.0} : MapVector{0.0, EDGE};
	return {middle - along, middle + along};
}

static StopLayout LayoutOf(TileIndex tile)
{
	return static_cast<StopLayout>((GetStationGfx(tile) / AXIS_END) % LAYOUTS);
}

/* One rail station tile: the game lays out its rear platform on the side of lower map coordinates. */
class StopBuilder {
public:
	StopBuilder(TileIndex tile, WayDetail detail) :
		tile(tile), axis(GetRailStationAxis(tile)), run(MiddleRun(tile, this->axis)), rear(this->axis == AXIS_X ? 1.0 : -1.0), detail(detail)
	{
	}

	ModelMesh Build()
	{
		if (IsRailWaypoint(this->tile)) {
			this->Gantry();
			return std::move(this->parts);
		}
		StopLayout layout = LayoutOf(this->tile);
		this->Platform(-1.0);
		this->Platform(1.0);
		if (layout == StopLayout::Building) this->Building();
		if (layout == StopLayout::RearRoof) this->Roof(this->rear);
		if (layout == StopLayout::FrontRoof) this->Roof(-this->rear);
		return std::move(this->parts);
	}

private:
	/* A platform ends where the station does along its track. */
	bool Ends(bool last) const
	{
		TileIndexDiffC step = TileIndexDiffCByDiagDir(AxisToDiagDir(this->axis));
		int sign = last ? 1 : -1;
		int nx = static_cast<int>(TileX(this->tile)) + sign * step.x;
		int ny = static_cast<int>(TileY(this->tile)) + sign * step.y;
		if (!OnMap(nx, ny)) return true;
		TileIndex next = TileXY(nx, ny);
		return !IsRailStationTile(next) || GetRailStationAxis(next) != this->axis;
	}

	Vec3 Point(double share, double lateral, double height) const
	{
		MapVector at = this->run.At(share, lateral);
		return {at.x, at.y, height};
	}

	void Box(double from, double to, double low, double high, uint32_t top, uint32_t sides, double gloss = 0.0, bool capped = false)
	{
		std::array<SectionPoint, 5> section = BoxSection(from, to, low, high, top, sides, gloss);
		this->parts.Append(Laid(this->run, section, 1));
		for (bool last : {false, true}) {
			if (capped || this->Ends(last)) this->parts.Append(EndCap(this->run, section, last));
		}
	}

	void Platform(double side)
	{
		this->Box(side * PLATFORM_INNER, side * EDGE, PLATFORM_FOOT, PLATFORM_TOP, PLATFORM, PLATFORM_SIDE);
		if (this->detail == WayDetail::Simple) return;
		this->Box(side * PLATFORM_INNER, side * (PLATFORM_INNER + COPING_WIDTH), PLATFORM_TOP, PLATFORM_TOP + COPING_RISE, COPING, COPING);
		this->Box(side * SAFETY_FROM, side * SAFETY_TO, PLATFORM_TOP, PLATFORM_TOP + SAFETY_RISE, SAFETY_LINE, SAFETY_LINE);
	}

	/* The station building stands on the rear platform, glazed and sheltered by an awning toward the track. */
	void Building()
	{
		double side = this->rear;
		this->Box(side * PLATFORM_INNER, side * EDGE, PLATFORM_TOP, BUILDING_HEIGHT, BUILDING_ROOF, WALL, 0.0, true);
		if (this->detail == WayDetail::Simple) return;
		this->Box(side * (PLATFORM_INNER - WINDOWS_PROUD), side * PLATFORM_INNER, WINDOWS_LOW, WINDOWS_HIGH, GLASS, GLASS, GLASS_GLOSS, true);
		this->Box(side * (PLATFORM_INNER - AWNING_REACH), side * PLATFORM_INNER, AWNING_LOW, AWNING_HIGH, AWNING, AWNING, 0.0, true);
	}

	/* A glazed gable over the whole tile on pillars along one platform, running on into the roofs of the tiles beside it. */
	void Roof(double pillar_side)
	{
		std::array<SectionPoint, 6> gable = {{
			{-ROOF_HALF, ROOF_EAVES - ROOF_THICKNESS, FRAME, FRAME_GLOSS}, {-ROOF_HALF, ROOF_EAVES, ROOF_GLASS, GLASS_GLOSS}, {0.0, ROOF_RIDGE, ROOF_GLASS, GLASS_GLOSS},
			{ROOF_HALF, ROOF_EAVES, FRAME, FRAME_GLOSS}, {ROOF_HALF, ROOF_EAVES - ROOF_THICKNESS, FRAME, FRAME_GLOSS}, {0.0, ROOF_RIDGE - ROOF_THICKNESS, FRAME, FRAME_GLOSS},
		}};
		std::array<SectionPoint, 7> closed;
		std::copy(gable.begin(), gable.end(), closed.begin());
		closed.back() = gable.front();
		this->parts.Append(Laid(this->run, closed, 1));
		for (double share : PILLAR_SHARES) {
			MapVector at = this->run.At(share, pillar_side * PILLAR_LATERAL);
			this->parts.Append(Block(at, this->run.Along(), {-PILLAR_HALF, -PILLAR_HALF, PLATFORM_TOP}, {PILLAR_HALF, PILLAR_HALF, ROOF_EAVES}).Paint(FRAME).Gloss(FRAME_GLOSS));
			Vec3 ridge = this->Point(share, 0.0, ROOF_RIDGE);
			for (double side : {-1.0, 1.0}) this->parts.Append(Strut(this->Point(share, side * ROOF_HALF, ROOF_EAVES), ridge, RIB_HALF).Paint(FRAME).Gloss(FRAME_GLOSS));
		}
	}

	/* A waypoint is marked by a gantry over its track bearing a sign. */
	void Gantry()
	{
		for (double side : {-1.0, 1.0}) {
			MapVector post = this->run.At(EDGE, side * GANTRY_LATERAL);
			this->parts.Append(Block(post, this->run.Along(), {-GANTRY_HALF, -GANTRY_HALF, 0.0}, {GANTRY_HALF, GANTRY_HALF, GANTRY_HEIGHT}).Paint(FRAME).Gloss(FRAME_GLOSS));
		}
		double beam = GANTRY_HEIGHT - GANTRY_HALF;
		this->parts.Append(Strut(this->Point(EDGE, -GANTRY_LATERAL, beam), this->Point(EDGE, GANTRY_LATERAL, beam), GANTRY_HALF).Paint(FRAME).Gloss(FRAME_GLOSS));
		this->parts.Append(Block(this->run.At(EDGE, 0.0), this->run.Along(), SIGN_LOW, SIGN_HIGH).Paint(SIGN));
	}

	TileIndex tile;
	Axis axis;
	Stretch run;
	double rear;
	WayDetail detail;
	ModelMesh parts;
};

void LayRailStop(ModelMesh &mesh, TileIndex tile, WayDetail detail)
{
	ModelMesh stop = StopBuilder(tile, detail).Build();
	mesh.Append(Drape(stop, GroundOf(tile)));
}

/* A glass shelter stands on the pavement at one side of a drive through bus stop. */
void LayBusShelter(ModelMesh &mesh, TileIndex tile, WayDetail detail)
{
	if (detail == WayDetail::Simple) return;
	Stretch whole = MiddleRun(tile, GetDriveThroughStopAxis(tile));
	Stretch run = {whole.At(SHELTER_FROM, 0.0), whole.At(SHELTER_TO, 0.0)};
	ModelMesh shelter = Laid(run, BoxSection(SHELTER_BACK - SHELTER_PANEL, SHELTER_BACK, PAVEMENT_TOP, SHELTER_HEIGHT, GLASS, GLASS, GLASS_GLOSS), 1);
	std::array<SectionPoint, 5> roof = BoxSection(SHELTER_FRONT, SHELTER_BACK + SHELTER_PANEL, SHELTER_HEIGHT, SHELTER_HEIGHT + SHELTER_ROOF, FRAME, FRAME, FRAME_GLOSS);
	shelter.Append(Laid(run, roof, 1));
	shelter.Append(EndCap(run, roof, false));
	shelter.Append(EndCap(run, roof, true));
	mesh.Append(Drape(shelter, GroundOf(tile)));
}
