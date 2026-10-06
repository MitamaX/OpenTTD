/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file structure_dressing.cpp The small things a building wears up close: chimney pots, ridge caps, balconies, air conditioners, ladders and rims. */

#include "../../stdafx.h"
#include "structure_dressing.h"

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>

#include "../core/seed.h"
#include "../core/tones.h"
#include "../map/tile_shapes.h"
#include "../model/model_shapes.h"

#include "../../safeguards.h"

static constexpr int PROP_SIDES = 8;
static constexpr int DRUM_SIDES = 16;

static constexpr double CHIMNEY_CAP_REACH = 0.006;
static constexpr double CHIMNEY_CAP_HEIGHT = 0.008;
static constexpr double POT_RADIUS = 0.007;
static constexpr double POT_HEIGHT = 0.016;
static constexpr uint32_t POT_TINT = 0xFFB0603EU;
static constexpr uint32_t CAP_TINT = 0xFF9C9890U;

static constexpr double FAN_SHARE = 0.32;
static constexpr double FAN_HEIGHT = 0.006;
static constexpr double WIDE_UNIT_SIDE = 0.15;
static constexpr uint32_t FAN_TINT = 0xFF3C4044U;

static constexpr double TANK_BODY_SHARE = 0.65;
static constexpr double TANK_LEG_SIDE = 0.008;
static constexpr double TANK_ROOF_RISE = 0.03;
static constexpr uint32_t TANK_WOOD = 0xFF7E6146U;

static constexpr double RIDGE_MIN_RISE = 0.04;
static constexpr double RIDGE_HALF_WIDTH = 0.011;
static constexpr double RIDGE_HEIGHT = 0.008;

static constexpr double BALCONY_DEPTH = 0.032;
static constexpr double BALCONY_SHARE = 0.78;
static constexpr double BALCONY_SLAB = 0.006;
static constexpr double RAILING_HEIGHT = 0.022;
static constexpr double RAILING_THICKNESS = 0.003;
static constexpr uint32_t RAILING_TINT = 0xFFE4E6E8U;
static constexpr double UNIT_WIDTH = 0.022;
static constexpr double UNIT_DEPTH = 0.014;
static constexpr double UNIT_HEIGHT = 0.016;
static constexpr double UNIT_SHARE = 0.14;
static constexpr uint32_t UNIT_TINT = 0xFFC9CCCEU;
static constexpr double BOX_WIDTH_SHARE = 0.5;
static constexpr double BOX_DEPTH = 0.01;
static constexpr double BOX_HEIGHT = 0.008;
static constexpr double BOX_SHARE = 0.45;
static constexpr std::array<uint32_t, 4> BLOOM_TINTS = {0xFFC8423AU, 0xFFE8B23AU, 0xFF5E9A48U, 0xFFD86FA0U};
static constexpr double WINDOW_SILL = 2.0 / TEXELS_PER_TILE;
static constexpr uint BALCONY_BIT = 9;

static constexpr double MAST_MIN_TOP = 1.0;
static constexpr double MAST_RADIUS = 0.005;
static constexpr double MAST_HEIGHT = 0.16;
static constexpr double MAST_INSET = 0.06;
static constexpr double MAST_SHARE = 0.5;
static constexpr uint32_t MAST_TINT = 0xFF8E9298U;

static constexpr double DRUM_MIN_WALL = 0.1;
static constexpr double RIM_REACH = 1.05;
static constexpr double RIM_HEIGHT = 0.01;
static constexpr double LADDER_WIDTH = 0.012;
static constexpr double LADDER_DEPTH = 0.005;

static constexpr std::array<DiagDirection, DIAGDIR_END> WALL_SIDES = {DIAGDIR_NE, DIAGDIR_SE, DIAGDIR_SW, DIAGDIR_NW};

bool ReplacesSolid(const Solid &solid)
{
	return solid.fixture == Fixture::WaterTank;
}

static bool CarriesWindows(const Solid &solid)
{
	return solid.kind == SolidKind::Box && solid.windows != WindowGrid::None && solid.windows != WindowGrid::Doors && solid.taper == 0.0f && HasFacade(solid.wall_material, solid.windows);
}

static uint32_t SolidSeed(const FormStyle &style, const Solid &solid)
{
	return Hash32(style.Seed() ^ Hash32(std::bit_cast<uint32_t>(solid.x0) ^ Hash32(std::bit_cast<uint32_t>(solid.y0) ^ std::bit_cast<uint32_t>(solid.base))));
}

/* One wall of a solid seen from outside: where its foot runs from and to along the map, and which way it faces. */
struct FacadeWall {
	MapVector from;
	MapVector along;
	MapVector outward;
	double length;
	bool front;

	MapVector At(double distance) const { return this->from + this->along * distance; }
};

/* Parts are built in the form's model space, in tiles across the map and model tiles above its floor. */
class SolidDresser {
public:
	SolidDresser(const FormStyle &style, const Solid &solid, const PlanRect &footprint, StructureMesh &mesh) :
		style(style),
		solid(solid),
		plan({footprint.x0 + solid.x0, footprint.y0 + solid.y0, footprint.x0 + solid.x1, footprint.y0 + solid.y1}),
		mesh(mesh),
		dice(SolidSeed(style, solid))
	{
	}

	void Dress()
	{
		switch (this->solid.fixture) {
			case Fixture::Chimney: this->DressChimney(); break;
			case Fixture::RooftopUnit: this->DressRooftopUnit(); break;
			case Fixture::WaterTank: this->DressWaterTank(); break;
			default: break;
		}
		if (this->solid.role != SolidRole::Body) return;
		this->DressRidge();
		this->DressFacades();
		this->DressDrum();
		this->DressSkyline();
	}

private:
	double Eave() const { return this->solid.base + this->solid.wall; }
	double CentreX() const { return (this->plan.x0 + this->plan.x1) / 2.0; }
	double CentreY() const { return (this->plan.y0 + this->plan.y1) / 2.0; }
	double HalfX() const { return (this->plan.x1 - this->plan.x0) / 2.0; }
	double HalfY() const { return (this->plan.y1 - this->plan.y0) / 2.0; }

	/* A part's faces each keep their own normal; its pattern runs across upright faces and over the plan on the others. */
	void Add(ModelMesh part, const Coat &coat)
	{
		part.Facet();
		uint32_t first = static_cast<uint32_t>(this->mesh.vertices.size());
		for (const ModelVertex &corner : part.vertices) {
			Vec3 at = corner.Position();
			Vec3 normal = corner.Normal();
			bool upright = IsUpright(normal);
			double u = upright ? (normal.x * at.y - normal.y * at.x) : at.x;
			double v = upright ? at.z - this->solid.base : at.y;
			ModelVertex placed = corner;
			placed.Place({at.x, at.y, this->style.Floor() + at.z * FORM_HEIGHT_SCALE});
			placed.Face(RenderNormalOf(normal));
			this->mesh.vertices.push_back(this->style.Vertex(placed, u, v, coat, this->solid.glass_tint));
		}
		for (uint32_t index : part.indices) this->mesh.indices.push_back(first + index);
	}

	void AddBox(const Vec3 &a, const Vec3 &b, const Coat &coat)
	{
		this->Add(Box({std::min(a.x, b.x), std::min(a.y, b.y), std::min(a.z, b.z)}, {std::max(a.x, b.x), std::max(a.y, b.y), std::max(a.z, b.z)}), coat);
	}

	void AddUpright(ModelMesh part, double x, double y, double z, const Coat &coat)
	{
		part.Transform(Mat4::Translation({x, y, z}));
		this->Add(std::move(part), coat);
	}

	/* One spot at the middle of the top, or two spread along its longer side. */
	template <class Visit>
	void OnTop(bool pair, Visit visit) const
	{
		bool along_x = this->HalfX() >= this->HalfY();
		double spread = pair ? std::max(this->HalfX(), this->HalfY()) / 2.0 : 0.0;
		for (double offset : {-spread, spread}) {
			visit(this->CentreX() + (along_x ? offset : 0.0), this->CentreY() + (along_x ? 0.0 : offset));
			if (!pair) return;
		}
	}

	void DressChimney()
	{
		double top = this->solid.Top();
		Coat cap = {Material::Concrete, WindowGrid::None, {}, CAP_TINT};
		this->AddBox({this->plan.x0 - CHIMNEY_CAP_REACH, this->plan.y0 - CHIMNEY_CAP_REACH, top - CHIMNEY_CAP_HEIGHT}, {this->plan.x1 + CHIMNEY_CAP_REACH, this->plan.y1 + CHIMNEY_CAP_REACH, top}, cap);
		Coat pot = {Material::Plain, WindowGrid::None, {}, POT_TINT};
		this->OnTop(this->HalfX() + this->HalfY() > 4.0 * POT_RADIUS, [&](double x, double y) { this->AddUpright(Prism(PROP_SIDES, POT_RADIUS, POT_HEIGHT), x, y, top, pot); });
	}

	void DressRooftopUnit()
	{
		Coat fan = {Material::Metal, WindowGrid::None, {}, FAN_TINT};
		double radius = FAN_SHARE * std::min(this->HalfX(), this->HalfY()) * 2.0;
		bool wide = std::max(this->HalfX(), this->HalfY()) * 2.0 >= WIDE_UNIT_SIDE;
		this->OnTop(wide, [&](double x, double y) { this->AddUpright(Prism(PROP_SIDES * 2, radius, FAN_HEIGHT), x, y, this->solid.Top(), fan); });
	}

	/* A timber tank on legs with a pointed lid, as town roofs carry them. */
	void DressWaterTank()
	{
		double radius = std::min(this->HalfX(), this->HalfY());
		double legs = this->solid.wall * (1.0 - TANK_BODY_SHARE);
		Coat wood = {Material::Planks, WindowGrid::None, {}, TANK_WOOD};
		Coat steel = {Material::Metal, WindowGrid::None, {}, Darken(this->solid.wall_tint)};
		for (double dx : {-1.0, 1.0}) {
			for (double dy : {-1.0, 1.0}) {
				double x = this->CentreX() + dx * radius * 0.6;
				double y = this->CentreY() + dy * radius * 0.6;
				this->AddBox({x - TANK_LEG_SIDE / 2.0, y - TANK_LEG_SIDE / 2.0, this->solid.base}, {x + TANK_LEG_SIDE / 2.0, y + TANK_LEG_SIDE / 2.0, this->solid.base + legs}, steel);
			}
		}
		this->AddUpright(Prism(DRUM_SIDES, radius, this->solid.wall - legs), this->CentreX(), this->CentreY(), this->solid.base + legs, wood);
		this->AddUpright(Cone(DRUM_SIDES, radius * RIM_REACH, TANK_ROOF_RISE), this->CentreX(), this->CentreY(), this->Eave(), steel);
	}

	/* Gable and hip roofs wear a cap along their ridge, which a gable carries on over its overhang. */
	void DressRidge()
	{
		bool gable = this->solid.roof == RoofShape::Gable;
		if (this->solid.kind != SolidKind::Box || (!gable && this->solid.roof != RoofShape::Hip) || this->solid.rise < RIDGE_MIN_RISE) return;

		bool along_x = this->solid.ridge == AXIS_X;
		double run = along_x ? this->HalfY() : this->HalfX();
		double low = (along_x ? this->plan.x0 : this->plan.y0) + (gable ? -EAVE_OVERHANG : run);
		double high = (along_x ? this->plan.x1 : this->plan.y1) - (gable ? -EAVE_OVERHANG : run);
		if (high <= low) return;

		double crest = this->solid.Top();
		double across = along_x ? this->CentreY() : this->CentreX();
		Vec3 a = along_x ? Vec3{low, across - RIDGE_HALF_WIDTH, crest - RIDGE_HEIGHT} : Vec3{across - RIDGE_HALF_WIDTH, low, crest - RIDGE_HEIGHT};
		Vec3 b = along_x ? Vec3{high, across + RIDGE_HALF_WIDTH, crest + RIDGE_HEIGHT / 2.0} : Vec3{across + RIDGE_HALF_WIDTH, high, crest + RIDGE_HEIGHT / 2.0};
		this->AddBox(a, b, {this->solid.roof_material, WindowGrid::None, SurfaceFlag::Roof, Darken(this->solid.roof_tint)});
	}

	FacadeWall WallOf(DiagDirection side) const
	{
		MapVector outward = Outward(side);
		MapVector along = {-outward.y, outward.x};
		MapVector centre = {this->CentreX() + outward.x * this->HalfX(), this->CentreY() + outward.y * this->HalfY()};
		double length = 2.0 * (outward.x != 0.0 ? this->HalfY() : this->HalfX());
		return {centre - along * (length / 2.0), along, outward, length, this->solid.fronts.Test(side)};
	}

	void DressFacades()
	{
		if (!CarriesWindows(this->solid)) return;
		for (DiagDirection side : WALL_SIDES) this->DressFacade(this->WallOf(side));
	}

	/* Whole bays fit a wall the way its windows are drawn, so each prop stands under a window. */
	void DressFacade(const FacadeWall &wall)
	{
		const FacadeMetrics &metrics = FacadeMetricsOf(this->solid.windows);
		double bay = static_cast<double>(metrics.bay_texels) / TEXELS_PER_TILE;
		int columns = std::max(1, static_cast<int>(std::lround(wall.length / bay))) * metrics.bay_texels / metrics.pitch_texels;
		double column_width = wall.length / columns;
		double ground = static_cast<double>(metrics.ground_texels) / TEXELS_PER_TILE;
		double storey = static_cast<double>(metrics.storey_texels) / TEXELS_PER_TILE;
		bool balconies = wall.front && this->solid.windows == WindowGrid::Apartment && SeedBits(this->dice.Next(), BALCONY_BIT, 1) != 0;
		for (double bottom = this->solid.base + ground; bottom + storey <= this->Eave() + PLACE_EPS; bottom += storey) {
			for (int column = 0; column < columns; column++) {
				MapVector centre = wall.At((column + 0.5) * column_width);
				if (balconies) {
					if (column % 2 == 0) this->AddBalcony(wall, centre, column_width, bottom);
				} else if (wall.front) {
					if (this->WearsWindowBoxes() && this->dice.Share() < BOX_SHARE) this->AddWindowBox(wall, centre, column_width, bottom);
				} else if (this->dice.Share() < UNIT_SHARE) {
					this->AddUnit(wall, centre, bottom);
				}
			}
		}
	}

	bool WearsWindowBoxes() const
	{
		return this->solid.windows == WindowGrid::Cottage || this->solid.windows == WindowGrid::Terrace || this->solid.windows == WindowGrid::Shopfront;
	}

	Vec3 OffWall(const FacadeWall &wall, MapVector at, double across, double out, double z) const
	{
		MapVector point = at + wall.along * across + wall.outward * out;
		return {point.x, point.y, z};
	}

	void AddBalcony(const FacadeWall &wall, MapVector centre, double column_width, double floor)
	{
		double half = column_width * BALCONY_SHARE / 2.0;
		Coat slab = {Material::Concrete, WindowGrid::None, {}, ScaledRgb(this->solid.wall_tint, 1.08)};
		Coat railing = {Material::Metal, WindowGrid::None, {}, RAILING_TINT};
		this->AddBox(this->OffWall(wall, centre, -half, 0.0, floor - BALCONY_SLAB), this->OffWall(wall, centre, half, BALCONY_DEPTH, floor), slab);
		this->AddBox(this->OffWall(wall, centre, -half, BALCONY_DEPTH - RAILING_THICKNESS, floor), this->OffWall(wall, centre, half, BALCONY_DEPTH, floor + RAILING_HEIGHT), railing);
		for (double end : {-half, half - RAILING_THICKNESS}) {
			this->AddBox(this->OffWall(wall, centre, end, 0.0, floor), this->OffWall(wall, centre, end + RAILING_THICKNESS, BALCONY_DEPTH, floor + RAILING_HEIGHT), railing);
		}
	}

	void AddUnit(const FacadeWall &wall, MapVector centre, double floor)
	{
		Coat unit = {Material::Metal, WindowGrid::None, {}, UNIT_TINT};
		this->AddBox(this->OffWall(wall, centre, -UNIT_WIDTH / 2.0, 0.0, floor), this->OffWall(wall, centre, UNIT_WIDTH / 2.0, UNIT_DEPTH, floor + UNIT_HEIGHT), unit);
	}

	void AddWindowBox(const FacadeWall &wall, MapVector centre, double column_width, double floor)
	{
		double half = column_width * BOX_WIDTH_SHARE / 2.0;
		Coat bloom = {Material::Plain, WindowGrid::None, {}, BLOOM_TINTS[this->dice.Below(static_cast<uint32_t>(BLOOM_TINTS.size()))]};
		double sill = floor + WINDOW_SILL;
		this->AddBox(this->OffWall(wall, centre, -half, 0.0, sill - BOX_HEIGHT), this->OffWall(wall, centre, half, BOX_DEPTH, sill), bloom);
	}

	/* Some tall flat roofs carry a radio mast at one corner. */
	void DressSkyline()
	{
		bool flat = this->solid.roof == RoofShape::Parapet || this->solid.roof == RoofShape::Flat;
		if (this->solid.kind != SolidKind::Box || !flat || this->solid.Top() < MAST_MIN_TOP || this->dice.Share() >= MAST_SHARE) return;
		double x = this->dice.Share() < 0.5 ? this->plan.x0 + MAST_INSET : this->plan.x1 - MAST_INSET;
		double y = this->dice.Share() < 0.5 ? this->plan.y0 + MAST_INSET : this->plan.y1 - MAST_INSET;
		this->AddUpright(Column(PROP_SIDES / 2, MAST_RADIUS * 2.0, MAST_RADIUS, MAST_HEIGHT), x, y, this->solid.Top(), {Material::Metal, WindowGrid::None, {}, MAST_TINT});
	}

	/* Tall metal drums wear a rim at their eaves and a ladder up their side. */
	void DressDrum()
	{
		if (this->solid.kind != SolidKind::Cylinder || this->solid.wall_material != Material::Metal || this->solid.wall < DRUM_MIN_WALL || this->solid.taper != 0.0f) return;
		double radius = std::min(this->HalfX(), this->HalfY());
		Coat rim = {Material::Metal, WindowGrid::None, {}, ScaledRgb(this->solid.wall_tint, 0.85)};
		this->AddUpright(Prism(DRUM_SIDES, radius * RIM_REACH, RIM_HEIGHT), this->CentreX(), this->CentreY(), this->Eave() - RIM_HEIGHT, rim);
		double x = this->CentreX() + radius;
		this->AddBox({x, this->CentreY() - LADDER_WIDTH / 2.0, this->solid.base}, {x + LADDER_DEPTH, this->CentreY() + LADDER_WIDTH / 2.0, this->Eave()}, {Material::Metal, WindowGrid::None, {}, Darken(this->solid.wall_tint)});
	}

	const FormStyle &style;
	const Solid &solid;
	PlanRect plan;
	StructureMesh &mesh;
	SeedDice dice;
};

void DressSolid(const FormStyle &style, const Solid &solid, const PlanRect &footprint, StructureMesh &mesh)
{
	SolidDresser(style, solid, footprint, mesh).Dress();
}
