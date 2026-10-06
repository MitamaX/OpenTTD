/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file volume_geometry.cpp The faces of a building's solids, the order the solids stand in, and the cuts that fit faces to tiles and texture cells. */

#include "../../stdafx.h"
#include "volume_geometry.h"

#include <algorithm>
#include <functional>
#include <initializer_list>
#include <numbers>
#include <ranges>

#include "../../core/bitmath_func.hpp"
#include "../art/material_atlas.h"
#include "../core/tuning.h"
#include "tile_shapes.h"

#include "../../safeguards.h"

static constexpr double PARAPET_INSET = 0.04;
static constexpr double PARAPET_INNER_SHADE = 0.9;
static constexpr double EAVE_OVERHANG = 0.03;
static constexpr double HIP_SQUARE_EPS = 0.02;
static constexpr double SAWTOOTH_PITCH = 0.33;
static constexpr double SAWTOOTH_GLASS_SHARE = 0.15;
static constexpr int DOME_RINGS = 2;
static constexpr double WALL_FOOT_SHADE = 0.82;
static constexpr double UV_EPS = 1e-6;
static constexpr double STRIP_RISE = static_cast<double>(TEXELS_PER_TILE) / STRIP_TEXELS;
static constexpr double FULL_TURN = 2.0 * std::numbers::pi;
static constexpr double HALF_TURN = std::numbers::pi;
static constexpr double QUARTER_TURN = std::numbers::pi / 2.0;
static constexpr double MIDWAY = 0.5;
static constexpr Vec3 NO_NORMAL = {0.0, 0.0, 0.0};
static constexpr Vec3 DOWN = {0.0, 0.0, -1.0};
static constexpr UvMap PLAN_UV = {{0.0, 1.0, 0.0}, 0.0, {1.0, 0.0, 0.0}, 0.0, true};

static_assert(MAX_SOLIDS <= 16, "a solid's predecessors fit one 16 bit mask");

/* Model heights stand raised by the height scale like the ground, which flattens how steep a face turns. */
Vec3 RenderNormal(const Vec3 &model_normal)
{
	return Normalised({model_normal.x, model_normal.y, model_normal.z / _tuning.height_scale});
}

DiagDirection FacingSide(const Vec3 &normal)
{
	if (std::abs(normal.x) >= std::abs(normal.y)) return normal.x > 0.0 ? DIAGDIR_SW : DIAGDIR_NE;
	return normal.y > 0.0 ? DIAGDIR_SE : DIAGDIR_NW;
}

double GroundHeight(const PlanPoint &plan, const SolidPlacement &placement)
{
	const PlanRect &footprint = placement.footprint;
	int tx = static_cast<int>(std::clamp(std::floor(plan.x), footprint.x0, footprint.x1 - 1.0));
	int ty = static_cast<int>(std::clamp(std::floor(plan.y), footprint.y0, footprint.y1 - 1.0));
	return HeightOf(placement.floor, TileGround(tx, ty).Level(plan.x, plan.y));
}

std::array<PlanPoint, RECT_CORNERS> CornersOf(const PlanRect &rect)
{
	return {{{rect.x0, rect.y0}, {rect.x1, rect.y0}, {rect.x1, rect.y1}, {rect.x0, rect.y1}}};
}

static Vec3 Lerp(const Vec3 &from, const Vec3 &to, double share)
{
	return {std::lerp(from.x, to.x, share), std::lerp(from.y, to.y, share), std::lerp(from.z, to.z, share)};
}

static PlanPoint Lerp(const PlanPoint &from, const PlanPoint &to, double share)
{
	return {std::lerp(from.x, to.x, share), std::lerp(from.y, to.y, share)};
}

static PlanRect Grown(const PlanRect &rect, double dx, double dy)
{
	return {rect.x0 - dx, rect.y0 - dy, rect.x1 + dx, rect.y1 + dy};
}

static bool HasArea(const PlanRect &rect)
{
	return rect.x0 < rect.x1 && rect.y0 < rect.y1;
}

static PlanPoint CentreOf(const PlanRect &rect)
{
	return {(rect.x0 + rect.x1) / 2.0, (rect.y0 + rect.y1) / 2.0};
}

static double SquaredDistance(const PlanPoint &a, const PlanPoint &b)
{
	return (a.x - b.x) * (a.x - b.x) + (a.y - b.y) * (a.y - b.y);
}

/* Twice the polygon's area along each axis, pointing to the side its corners run counterclockwise around. */
static Vec3 AreaVector(std::span<const FacePoint> points)
{
	Vec3 sum = NO_NORMAL;
	for (size_t i = 0; i < points.size(); i++) {
		const FacePoint &a = points[i];
		const FacePoint &b = points[(i + 1) % points.size()];
		sum.x += (a.y - b.y) * (a.z + b.z);
		sum.y += (a.z - b.z) * (a.x + b.x);
		sum.z += (a.x - b.x) * (a.y + b.y);
	}
	return sum;
}

static double Highest(const FacePolygon &polygon)
{
	return std::ranges::max(polygon.Points() | std::views::transform(&FacePoint::z));
}

static double Lowest(const FacePolygon &polygon)
{
	return std::ranges::min(polygon.Points() | std::views::transform(&FacePoint::z));
}

static double HorizontalLength(const Vec3 &v)
{
	return std::hypot(v.x, v.y);
}

/* How far down its slope a face runs from its highest corner to its lowest. */
static double FallRun(const FacePolygon &polygon)
{
	Vec3 normal = Normalised(AreaVector(polygon.Points()));
	return (Highest(polygon) - Lowest(polygon)) / HorizontalLength(normal);
}

/* The way across a face from left to right, as seen from outside. */
static Vec3 Across(const Vec3 &normal)
{
	double length = HorizontalLength(normal);
	return {-normal.y / length, normal.x / length, 0.0};
}

static Vec3 Scaled(const Vec3 &v, double factor)
{
	return {v.x * factor, v.y * factor, v.z * factor};
}

UvMap WallUv(const Vec3 &normal)
{
	return {Across(normal), 0.0, DOWN, 0.0, true};
}

/* Whole bays fit the wall exactly from its left end, so no window is cut in half. */
static UvMap StripUv(const Face &face, const FacadeMetrics &metrics)
{
	Vec3 across = Across(face.normal);
	auto [left, right] = std::ranges::minmax(face.polygon.Points() | std::views::transform([&](const FacePoint &point) { return Dot(across, PositionOf(point)); }));
	double bay = static_cast<double>(metrics.bay_texels) / TEXELS_PER_TILE;
	double length = right - left;
	double scale = std::max(1.0, std::round(length / bay)) * bay / length;
	return {Scaled(across, scale), -left * scale, {0.0, 0.0, -STRIP_RISE}, 1.0, false};
}

static UvMap SlopeUv(const Face &face, double start)
{
	double fall = 1.0 / HorizontalLength(face.normal);
	return {Across(face.normal), 0.0, {0.0, 0.0, -fall}, start + Highest(face.polygon) * fall, true};
}

/* Around a drum the texture runs along each side's bottom edge, carrying on from where the last side ended. */
static UvMap ArcUv(const Face &face, double start)
{
	const FacePoint &from = face.polygon.Points()[0];
	const FacePoint &to = face.polygon.Points()[1];
	Vec3 chord = Normalised({to.x - from.x, to.y - from.y, 0.0});
	return {chord, start - Dot(chord, PositionOf(from)), DOWN, 0.0, true};
}

static bool IsPitched(RoofShape roof)
{
	return roof != RoofShape::Flat && roof != RoofShape::Parapet;
}

static bool Suits(RoofShape roof, SolidKind kind)
{
	switch (roof) {
		case RoofShape::Flat: return true;
		case RoofShape::Dome:
		case RoofShape::Cone: return kind == SolidKind::Cylinder;
		default: return kind == SolidKind::Box;
	}
}

static bool HasHipRidge(const Solid &solid)
{
	float width = solid.x1 - solid.x0;
	float depth = solid.y1 - solid.y0;
	float along = solid.ridge == AXIS_X ? width - depth : depth - width;
	return along * (1.0f - 2.0f * solid.taper) >= HIP_SQUARE_EPS;
}

static bool CarriesFacade(const Solid &solid)
{
	return solid.kind == SolidKind::Box && solid.windows != WindowGrid::None && HasFacade(solid.wall_material, solid.windows) && solid.taper == 0.0f;
}

/* At massing distance every pitched roof becomes flat halfway up its rise and every drum becomes a block. */
static void Flatten(Solid &shape)
{
	if (shape.kind == SolidKind::Cylinder) shape.kind = SolidKind::Box;
	if (IsPitched(shape.roof)) shape.wall += shape.rise / 2.0f;
	if (shape.roof == RoofShape::Parapet) shape.wall += PARAPET_HEIGHT;
	shape.roof = RoofShape::Flat;
	shape.rise = 0.0f;
}

static Solid Settled(const Solid &solid, const ShapeDetail &detail)
{
	Solid shape = solid;
	if (!Suits(shape.roof, shape.kind) || (IsPitched(shape.roof) && shape.rise <= 0.0f)) shape.roof = RoofShape::Flat;
	if (shape.roof == RoofShape::Hip && !HasHipRidge(shape)) shape.roof = RoofShape::Pyramid;
	if (detail.massing) Flatten(shape);
	return shape;
}

static PlanRect PlanOf(const Solid &solid, const PlanRect &footprint)
{
	return {footprint.x0 + solid.x0, footprint.y0 + solid.y0, footprint.x0 + solid.x1, footprint.y0 + solid.y1};
}

static PlanRect Tapered(const PlanRect &rect, float taper)
{
	return Grown(rect, -taper * (rect.x1 - rect.x0), -taper * (rect.y1 - rect.y0));
}

/* How far a point of the plan lies toward one of its sides, from 0 at the opposite edge to 1 at that side. */
static double TowardSide(const PlanPoint &point, const PlanRect &rect, DiagDirection side)
{
	switch (side) {
		case DIAGDIR_NE: return (rect.x1 - point.x) / (rect.x1 - rect.x0);
		case DIAGDIR_SW: return (point.x - rect.x0) / (rect.x1 - rect.x0);
		case DIAGDIR_NW: return (rect.y1 - point.y) / (rect.y1 - rect.y0);
		default: return (point.y - rect.y0) / (rect.y1 - rect.y0);
	}
}

/* The ring's side i runs from corner i to corner i + 1; even sides run along map X. */
static bool RunsAlong(size_t side, Axis axis)
{
	return (side % 2 == 0) == (axis == AXIS_X);
}

static FacePoint WithNormal(FacePoint point, const Vec3 &normal)
{
	point.normal = Normalised(normal);
	return point;
}

enum class UvMapping : uint8_t { Plan, Wall, Strip, Slope, Arc };

/* How a face is clad and how its texture runs; start is where a slope's or an arc's run begins. */
struct FaceSurface {
	Cladding cladding;
	UvMapping mapping;
	double start = 0.0;
	bool smooth = false;
	bool footed = false;
};

/* The eaves a pitched roof overhangs its walls with, the height they drop to, and the ridge its slopes climb to. */
struct Pitch {
	PlanRect eaves;
	double eave_height;
	PlanPoint ridge_from;
	PlanPoint ridge_to;

	const PlanPoint &RidgeNear(const PlanPoint &eave) const
	{
		return SquaredDistance(eave, this->ridge_from) <= SquaredDistance(eave, this->ridge_to) ? this->ridge_from : this->ridge_to;
	}
};

class SolidShaper {
public:
	SolidShaper(const Solid &solid, const SolidPlacement &placement, FaceSink &sink) :
		shape(Settled(solid, placement.detail)),
		placement(placement),
		sink(sink),
		base_plan(PlanOf(solid, placement.footprint)),
		top_plan(Tapered(this->base_plan, solid.taper)),
		facade(CarriesFacade(solid))
	{
	}

	void Build()
	{
		switch (this->shape.kind) {
			case SolidKind::Decal: this->BuildDecal(); break;
			case SolidKind::Cylinder: this->BuildCylinder(); break;
			default: this->BuildBox(); break;
		}
	}

private:
	double Eave() const { return this->shape.base + this->shape.wall; }
	double Crest() const { return this->Eave() + this->shape.rise; }
	double TopShare() const { return 1.0 - 2.0 * this->shape.taper; }
	int Segments() const { return this->placement.detail.segments; }
	double TurnOf(int step) const { return FULL_TURN * step / this->Segments(); }
	int ArcSteps() const { return this->Segments() / 2; }
	double ArcTurn(int line) const { return HALF_TURN * line / this->ArcSteps(); }
	double ArcShare(int line) const { return (1.0 - std::cos(this->ArcTurn(line))) / 2.0; }
	double ArcHeight(int line) const { return this->Eave() + this->shape.rise * std::sin(this->ArcTurn(line)); }

	FacePoint At(const PlanPoint &plan, double height, double shade = 1.0) const
	{
		return {plan.x, plan.y, height, NO_NORMAL, shade, false};
	}

	FacePoint Foot(const PlanPoint &plan) const
	{
		return this->At(plan, this->shape.base, WALL_FOOT_SHADE);
	}

	FacePoint OnGround(const PlanPoint &plan) const
	{
		return {plan.x, plan.y, GroundHeight(plan, this->placement), NO_NORMAL, 1.0, true};
	}

	double WallTop(const PlanPoint &corner) const
	{
		switch (this->shape.roof) {
			case RoofShape::Parapet: return this->Eave() + PARAPET_HEIGHT;
			case RoofShape::Shed: return this->Eave() + this->shape.rise * TowardSide(corner, this->top_plan, this->shape.high_side);
			default: return this->Eave();
		}
	}

	/* A wall across the ridge rises into the end of its roof on the way from one top corner over to the other. */
	void AddRoofEnd(FacePolygon &wall, const PlanPoint &from, const PlanPoint &to) const
	{
		switch (this->shape.roof) {
			case RoofShape::Gable:
				wall.Add(this->At(Lerp(from, to, MIDWAY), this->Crest()));
				break;

			case RoofShape::Vault:
				for (int line = 1; line < this->ArcSteps(); line++) wall.Add(this->At(Lerp(from, to, this->ArcShare(line)), this->ArcHeight(line)));
				break;

			default:
				break;
		}
	}

	PlanPoint RingPoint(double share, int step) const
	{
		PlanPoint centre = CentreOf(this->base_plan);
		double turn = this->TurnOf(step);
		return {centre.x + share * this->HalfWidth() * std::cos(turn), centre.y + share * this->HalfDepth() * std::sin(turn)};
	}

	double HalfWidth() const { return (this->base_plan.x1 - this->base_plan.x0) / 2.0; }
	double HalfDepth() const { return (this->base_plan.y1 - this->base_plan.y0) / 2.0; }

	void BuildDecal()
	{
		Face decal;
		for (const PlanPoint &corner : CornersOf(this->base_plan)) {
			decal.polygon.Add(this->shape.base > 0.0f ? this->At(corner, this->shape.base) : this->OnGround(corner));
		}
		this->Emit(decal, {Cladding::Decal, UvMapping::Plan});
	}

	void BuildBox()
	{
		this->BuildWalls();
		this->BuildRoof();
	}

	void BuildWalls()
	{
		std::array<PlanPoint, RECT_CORNERS> bottom = CornersOf(this->base_plan);
		std::array<PlanPoint, RECT_CORNERS> top = CornersOf(this->top_plan);
		FaceSurface surface = this->facade ? FaceSurface{Cladding::Facade, UvMapping::Strip} : FaceSurface{Cladding::Wall, UvMapping::Wall};
		surface.footed = true;
		for (size_t side = 0; side < RECT_CORNERS; side++) {
			size_t next = (side + 1) % RECT_CORNERS;
			Face wall;
			wall.polygon.Add(this->Foot(bottom[side]));
			wall.polygon.Add(this->Foot(bottom[next]));
			wall.polygon.Add(this->At(top[next], this->WallTop(top[next])));
			if (!RunsAlong(side, this->shape.ridge)) this->AddRoofEnd(wall.polygon, top[next], top[side]);
			wall.polygon.Add(this->At(top[side], this->WallTop(top[side])));
			this->Emit(wall, surface);
		}
	}

	void BuildRoof()
	{
		switch (this->shape.roof) {
			case RoofShape::Parapet: this->BuildParapet(); break;
			case RoofShape::Gable:
			case RoofShape::Hip:
			case RoofShape::Pyramid: this->BuildPitched(); break;
			case RoofShape::Shed: this->BuildShed(); break;
			case RoofShape::Sawtooth: this->BuildSawtooth(); break;
			case RoofShape::Vault: this->BuildVault(); break;
			default: this->BuildLid(this->top_plan, this->Eave(), Cladding::Roof); break;
		}
	}

	void BuildLid(const PlanRect &rect, double height, Cladding cladding, double shade = 1.0)
	{
		Face lid;
		for (const PlanPoint &corner : CornersOf(rect)) lid.polygon.Add(this->At(corner, height, shade));
		this->Emit(lid, {cladding, UvMapping::Plan});
	}

	void BuildParapet()
	{
		double rim = this->Eave() + PARAPET_HEIGHT;
		this->BuildLid(this->top_plan, rim, Cladding::Wall);
		PlanRect inner = Grown(this->top_plan, -PARAPET_INSET, -PARAPET_INSET);
		if (HasArea(inner)) this->BuildLid(inner, rim, Cladding::Roof, PARAPET_INNER_SHADE);
	}

	/* Every slope runs on past the walls by the overhang, so its eaves drop below the wall tops. */
	Pitch PitchOf() const
	{
		const PlanRect &top = this->top_plan;
		PlanPoint centre = CentreOf(top);
		double half_x = (top.x1 - top.x0) / 2.0;
		double half_y = (top.y1 - top.y0) / 2.0;
		bool along_x = this->shape.ridge == AXIS_X;
		auto ridge_at = [&](double along) { return along_x ? PlanPoint{along, centre.y} : PlanPoint{centre.x, along}; };
		auto eave_height_for = [&](double run) { return this->Eave() - EAVE_OVERHANG * this->shape.rise / run; };

		if (this->shape.roof == RoofShape::Pyramid) {
			double run = std::max(half_x, half_y);
			double reach = EAVE_OVERHANG / run;
			return {Grown(top, half_x * reach, half_y * reach), eave_height_for(run), centre, centre};
		}

		double run = along_x ? half_y : half_x;
		PlanRect eaves = Grown(top, EAVE_OVERHANG, EAVE_OVERHANG);
		double low = along_x ? top.x0 : top.y0;
		double high = along_x ? top.x1 : top.y1;
		if (this->shape.roof == RoofShape::Gable) return {eaves, eave_height_for(run), ridge_at(low - EAVE_OVERHANG), ridge_at(high + EAVE_OVERHANG)};
		return {eaves, eave_height_for(run), ridge_at(low + run), ridge_at(high - run)};
	}

	void BuildPitched()
	{
		Pitch pitch = this->PitchOf();
		std::array<PlanPoint, RECT_CORNERS> eaves = CornersOf(pitch.eaves);
		for (size_t side = 0; side < RECT_CORNERS; side++) {
			if (this->shape.roof == RoofShape::Gable && !RunsAlong(side, this->shape.ridge)) continue;
			const PlanPoint &from = eaves[side];
			const PlanPoint &to = eaves[(side + 1) % RECT_CORNERS];
			Face slope;
			slope.polygon.Add(this->At(from, pitch.eave_height));
			slope.polygon.Add(this->At(to, pitch.eave_height));
			slope.polygon.Add(this->At(pitch.RidgeNear(to), this->Crest()));
			slope.polygon.Add(this->At(pitch.RidgeNear(from), this->Crest()));
			ClipToPlan(slope.polygon, this->placement.footprint);
			this->Emit(slope, {Cladding::Roof, UvMapping::Slope});
		}
	}

	void BuildShed()
	{
		Face roof;
		for (const PlanPoint &corner : CornersOf(this->top_plan)) roof.polygon.Add(this->At(corner, this->WallTop(corner)));
		this->Emit(roof, {Cladding::Roof, UvMapping::Slope});
	}

	/* The teeth overlap on screen, so the ones farther from the viewer go first. */
	void BuildSawtooth()
	{
		double width = this->top_plan.x1 - this->top_plan.x0;
		int teeth = std::max(1, static_cast<int>(std::lround(width / SAWTOOTH_PITCH)));
		double pitch = width / teeth;
		bool ascending = this->placement.toward.x >= 0.0;
		for (int i = 0; i < teeth; i++) {
			int tooth = ascending ? i : teeth - 1 - i;
			this->BuildTooth(this->top_plan.x0 + tooth * pitch, pitch);
		}
	}

	void BuildTooth(double from, double pitch)
	{
		double to = from + pitch;
		double crest_x = to - SAWTOOTH_GLASS_SHARE * pitch;
		double eave = this->Eave();
		double crest = this->Crest();
		double near_y = this->top_plan.y0;
		double far_y = this->top_plan.y1;
		this->Emit({this->At({from, near_y}, eave), this->At({to, near_y}, eave), this->At({crest_x, near_y}, crest)}, {Cladding::Wall, UvMapping::Wall});
		this->Emit({this->At({to, far_y}, eave), this->At({from, far_y}, eave), this->At({crest_x, far_y}, crest)}, {Cladding::Wall, UvMapping::Wall});
		this->Emit({this->At({from, far_y}, eave), this->At({from, near_y}, eave), this->At({crest_x, near_y}, crest), this->At({crest_x, far_y}, crest)}, {Cladding::Roof, UvMapping::Slope});
		this->Emit({this->At({crest_x, near_y}, crest), this->At({to, near_y}, eave), this->At({to, far_y}, eave), this->At({crest_x, far_y}, crest)}, {Cladding::Skylight, UvMapping::Slope});
	}

	/* The two sides along the ridge the vault springs from, both run the same way, the low one across the ridge first. */
	std::array<std::array<PlanPoint, 2>, 2> SpringingSides() const
	{
		std::array<PlanPoint, RECT_CORNERS> top = CornersOf(this->top_plan);
		size_t low = this->shape.ridge == AXIS_X ? 0 : RECT_CORNERS - 1;
		return {{
			{top[low], top[(low + 1) % RECT_CORNERS]},
			{top[(low + 3) % RECT_CORNERS], top[(low + 2) % RECT_CORNERS]},
		}};
	}

	/* A line of the vault running the ridge's way; line 0 springs from the low side and the last from the high side. */
	std::array<FacePoint, 2> ArcLine(int line) const
	{
		auto [low, high] = this->SpringingSides();
		double share = this->ArcShare(line);
		double height = this->ArcHeight(line);
		return {this->At(Lerp(low[0], high[0], share), height), this->At(Lerp(low[1], high[1], share), height)};
	}

	Face VaultFacet(int line) const
	{
		auto [from, to] = this->ArcLine(line);
		auto [next_from, next_to] = this->ArcLine(line + 1);
		Face facet;
		for (const FacePoint &corner : {from, to, next_to, next_from}) facet.polygon.Add(corner);
		return facet;
	}

	/* Each half of the vault is built from the crown down, so its texture runs on from facet to facet. */
	void BuildVault()
	{
		int crown = this->ArcSteps() / 2;
		this->BuildVaultHalf(crown, 0);
		this->BuildVaultHalf(crown, this->ArcSteps());
	}

	void BuildVaultHalf(int crown, int springing)
	{
		int step = springing < crown ? -1 : 1;
		double start = 0.0;
		for (int line = crown; line != springing; line += step) {
			Face facet = this->VaultFacet(std::min(line, line + step));
			this->Emit(facet, {Cladding::Roof, UvMapping::Slope, start});
			if (facet.polygon.IsSurface()) start += FallRun(facet.polygon);
		}
	}

	void BuildCylinder()
	{
		if (this->shape.wall > 0.0f) this->BuildDrum();
		switch (this->shape.roof) {
			case RoofShape::Cone: this->BuildCone(); break;
			case RoofShape::Dome: this->BuildDome(); break;
			default: this->BuildCap(); break;
		}
	}

	/* The drum's sides lean in as its top narrows, so their normals tilt up by the narrowing over the height. */
	Vec3 DrumNormal(int step) const
	{
		double turn = this->TurnOf(step);
		return {std::cos(turn) / this->HalfWidth(), std::sin(turn) / this->HalfDepth(), (1.0 - this->TopShare()) / this->shape.wall};
	}

	void BuildDrum()
	{
		double around = 0.0;
		for (int step = 0; step < this->Segments(); step++) {
			PlanPoint from = this->RingPoint(1.0, step);
			PlanPoint to = this->RingPoint(1.0, step + 1);
			Face side;
			side.polygon.Add(WithNormal(this->Foot(from), this->DrumNormal(step)));
			side.polygon.Add(WithNormal(this->Foot(to), this->DrumNormal(step + 1)));
			side.polygon.Add(WithNormal(this->At(this->RingPoint(this->TopShare(), step + 1), this->Eave()), this->DrumNormal(step + 1)));
			side.polygon.Add(WithNormal(this->At(this->RingPoint(this->TopShare(), step), this->Eave()), this->DrumNormal(step)));
			this->Emit(side, {.cladding = Cladding::Wall, .mapping = UvMapping::Arc, .start = around, .smooth = true, .footed = true});
			around += std::sqrt(SquaredDistance(from, to));
		}
	}

	void BuildCap()
	{
		Face cap;
		for (int step = 0; step < this->Segments(); step++) cap.polygon.Add(this->At(this->RingPoint(this->TopShare(), step), this->Eave()));
		this->Emit(cap, {Cladding::Roof, UvMapping::Plan});
	}

	void BuildCone()
	{
		FacePoint apex = this->At(CentreOf(this->base_plan), this->Crest());
		for (int step = 0; step < this->Segments(); step++) {
			FacePoint from = this->At(this->RingPoint(this->TopShare(), step), this->Eave());
			FacePoint to = this->At(this->RingPoint(this->TopShare(), step + 1), this->Eave());
			this->Emit({from, to, apex}, {Cladding::Roof, UvMapping::Slope});
		}
	}

	/* Ring 0 is the drum's top and the ring past the last is the crown; each lies on the ellipsoid the dome is cut from. */
	FacePoint DomePoint(int ring, int step) const
	{
		double climb = QUARTER_TURN * ring / (DOME_RINGS + 1);
		double turn = this->TurnOf(step);
		double half_width = this->TopShare() * this->HalfWidth();
		double half_depth = this->TopShare() * this->HalfDepth();
		FacePoint point = this->At(this->RingPoint(this->TopShare() * std::cos(climb), step), this->Eave() + this->shape.rise * std::sin(climb));
		return WithNormal(point, {std::cos(climb) * std::cos(turn) / half_width, std::cos(climb) * std::sin(turn) / half_depth, std::sin(climb) / this->shape.rise});
	}

	/* Each column of the dome is built from the crown down, so its texture runs on from facet to facet. */
	void BuildDome()
	{
		for (int step = 0; step < this->Segments(); step++) {
			double start = 0.0;
			for (int ring = DOME_RINGS; ring >= 0; ring--) {
				Face facet;
				facet.polygon.Add(this->DomePoint(ring, step));
				facet.polygon.Add(this->DomePoint(ring, step + 1));
				facet.polygon.Add(this->DomePoint(ring + 1, step + 1));
				facet.polygon.Add(this->DomePoint(ring + 1, step));
				this->Emit(facet, {Cladding::Roof, UvMapping::Slope, start, true});
				if (facet.polygon.IsSurface()) start += FallRun(facet.polygon);
			}
		}
	}

	UvMap MapOf(const Face &face, const FaceSurface &surface) const
	{
		switch (surface.mapping) {
			case UvMapping::Plan: return PLAN_UV;
			case UvMapping::Wall: return WallUv(face.normal);
			case UvMapping::Strip: return StripUv(face, FacadeMetricsOf(this->shape.windows));
			case UvMapping::Slope: return SlopeUv(face, surface.start);
			default: return ArcUv(face, surface.start);
		}
	}

	void Emit(Face &face, const FaceSurface &surface)
	{
		if (!face.polygon.IsSurface()) return;
		Vec3 area = AreaVector(face.polygon.Points());
		if (Dot(area, area) <= 0.0) return;
		face.normal = Normalised(area);
		face.cladding = surface.cladding;
		face.smooth = surface.smooth;
		face.footed = surface.footed;
		face.uv = this->MapOf(face, surface);
		this->sink.Take(face);
	}

	void Emit(std::initializer_list<FacePoint> points, const FaceSurface &surface)
	{
		Face face;
		for (const FacePoint &point : points) face.polygon.Add(point);
		this->Emit(face, surface);
	}

	Solid shape;
	const SolidPlacement &placement;
	FaceSink &sink;
	PlanRect base_plan;
	PlanRect top_plan;
	bool facade;
};

void BuildFaces(const Solid &solid, const SolidPlacement &placement, FaceSink &sink)
{
	SolidShaper(solid, placement, sink).Build();
}

static bool Overlap(float low_a, float high_a, float low_b, float high_b)
{
	return low_a < high_b && low_b < high_a;
}

/* Stacked solids go up from the lowest; solids side by side go from the far one to the near one along the axis that parts them. */
static bool DrawsBefore(const Solid &a, const Solid &b, bool added_first, MapVector toward)
{
	bool overlap_x = Overlap(a.x0, a.x1, b.x0, b.x1);
	bool overlap_y = Overlap(a.y0, a.y1, b.y0, b.y1);
	if (overlap_x && overlap_y) {
		if (a.base != b.base) return a.base < b.base;
		bool a_decal = a.kind == SolidKind::Decal;
		bool b_decal = b.kind == SolidKind::Decal;
		return a_decal == b_decal ? added_first : a_decal;
	}

	bool x_says = (a.x0 < b.x0) == (toward.x >= 0.0);
	bool y_says = (a.y0 < b.y0) == (toward.y >= 0.0);
	if (!overlap_x && !overlap_y) return x_says && y_says;
	return overlap_x ? y_says : x_says;
}

/* The earliest added solid with nothing left to wait for goes next; a cycle gives way to the earliest added. */
static uint8_t NextReady(std::span<const uint16_t> waits_for, uint16_t placed)
{
	uint8_t earliest = static_cast<uint8_t>(waits_for.size());
	for (uint8_t i = 0; i < waits_for.size(); i++) {
		if (HasBit(placed, i)) continue;
		if ((waits_for[i] & ~placed) == 0) return i;
		earliest = std::min(earliest, i);
	}
	return earliest;
}

std::array<uint8_t, MAX_SOLIDS> PaintOrder(std::span<const Solid> solids, MapVector toward)
{
	uint8_t count = static_cast<uint8_t>(solids.size());
	std::array<uint16_t, MAX_SOLIDS> waits_for{};
	for (uint8_t a = 0; a < count; a++) {
		for (uint8_t b = 0; b < count; b++) {
			if (a != b && DrawsBefore(solids[a], solids[b], a < b, toward)) SetBit(waits_for[b], a);
		}
	}

	std::array<uint8_t, MAX_SOLIDS> order{};
	uint16_t placed = 0;
	for (uint8_t slot = 0; slot < count; slot++) {
		order[slot] = NextReady(std::span(waits_for).first(count), placed);
		SetBit(placed, order[slot]);
	}
	return order;
}

static bool LiesOn(std::span<const FacePoint> points, double FacePoint::*coordinate, double edge)
{
	return std::ranges::all_of(points, [&](const FacePoint &point) { return std::abs(point.*coordinate - edge) < PLACE_EPS; });
}

/* A wall lying on a cell's edge belongs to the cell its own solid stands in, which is the one its normal points away from. */
bool OwnedByNeighbour(const Face &face, const PlanRect &cell)
{
	std::span<const FacePoint> points = face.polygon.Points();
	return (face.normal.x > 0.0 && LiesOn(points, &FacePoint::x, cell.x0))
		|| (face.normal.x < 0.0 && LiesOn(points, &FacePoint::x, cell.x1))
		|| (face.normal.y > 0.0 && LiesOn(points, &FacePoint::y, cell.y0))
		|| (face.normal.y < 0.0 && LiesOn(points, &FacePoint::y, cell.y1));
}

static FacePoint Between(const FacePoint &from, const FacePoint &to, double share)
{
	return {
		std::lerp(from.x, to.x, share),
		std::lerp(from.y, to.y, share),
		std::lerp(from.z, to.z, share),
		Lerp(from.normal, to.normal, share),
		std::lerp(from.shade, to.shade, share),
		from.grounded && to.grounded,
	};
}

/* Keeps the part of the polygon where the measure is not negative; every point made on the cut passes through the landing. */
template <typename Measure, typename Landing = std::identity>
static void Keep(FacePolygon &polygon, Measure measure, Landing landing = {})
{
	std::span<const FacePoint> points = polygon.Points();
	if (std::ranges::all_of(points, [&](const FacePoint &point) { return measure(point) >= 0.0; })) return;

	FacePolygon kept;
	for (size_t i = 0; i < points.size(); i++) {
		const FacePoint &from = points[i];
		const FacePoint &to = points[(i + 1) % points.size()];
		double from_side = measure(from);
		double to_side = measure(to);
		if (from_side >= 0.0) kept.Add(from);
		if ((from_side >= 0.0) != (to_side >= 0.0)) kept.Add(landing(Between(from, to, from_side / (from_side - to_side))));
	}
	polygon = kept;
}

void ClipToPlan(FacePolygon &polygon, const PlanRect &rect)
{
	Keep(polygon, [&](const FacePoint &point) { return point.x - rect.x0; });
	Keep(polygon, [&](const FacePoint &point) { return rect.x1 - point.x; });
	Keep(polygon, [&](const FacePoint &point) { return point.y - rect.y0; });
	Keep(polygon, [&](const FacePoint &point) { return rect.y1 - point.y; });
}

void ClipToHeights(FacePolygon &polygon, double low, double high)
{
	Keep(polygon, [&](const FacePoint &point) { return point.z - low; });
	Keep(polygon, [&](const FacePoint &point) { return high - point.z; });
}

static FacePoint AtFoot(FacePoint point)
{
	point.shade = WALL_FOOT_SHADE;
	return point;
}

void ClipToGround(FacePolygon &polygon, const SolidPlacement &placement)
{
	Keep(polygon, [&](const FacePoint &point) { return point.z - GroundHeight({point.x, point.y}, placement); }, AtFoot);
}

template <typename Coordinate>
static std::pair<int, int> CellRange(const FacePolygon &polygon, Coordinate coordinate)
{
	auto [low, high] = std::ranges::minmax(polygon.Points() | std::views::transform(coordinate));
	int first = static_cast<int>(std::floor(low + UV_EPS));
	return {first, std::max(first + 1, static_cast<int>(std::ceil(high - UV_EPS)))};
}

UvCells CellsOf(const FacePolygon &polygon, const UvMap &map)
{
	auto [u0, u1] = CellRange(polygon, [&](const FacePoint &point) { return map.U(point); });
	if (!map.repeats_v) return {u0, u1, 0, 1};
	auto [v0, v1] = CellRange(polygon, [&](const FacePoint &point) { return map.V(point); });
	return {u0, u1, v0, v1};
}

void ClipToUvCell(FacePolygon &polygon, const UvMap &map, UvCell cell)
{
	Keep(polygon, [&](const FacePoint &point) { return map.U(point) - cell.u; });
	Keep(polygon, [&](const FacePoint &point) { return cell.u + 1 - map.U(point); });
	if (!map.repeats_v) return;
	Keep(polygon, [&](const FacePoint &point) { return map.V(point) - cell.v; });
	Keep(polygon, [&](const FacePoint &point) { return cell.v + 1 - map.V(point); });
}
