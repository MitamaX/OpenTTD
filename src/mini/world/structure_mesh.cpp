/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file structure_mesh.cpp Building forms as meshes in render space, every face clad in a material the shader patterns and the windows it draws. */

#include "../../stdafx.h"
#include "structure_mesh.h"

#include <algorithm>
#include <limits>

#include "../core/seed.h"
#include "../core/tones.h"
#include "../map/tile_shapes.h"
#include "../map/volume_geometry.h"
#include "structure_dressing.h"

#include "../../safeguards.h"

static constexpr double FOOTING_DEPTH = 0.05;
static constexpr double DECAL_LIFT = 0.004;
static constexpr int FULL_SEGMENTS = 16;
static constexpr int SIMPLE_SEGMENTS = 8;
static constexpr double LOWEST = std::numeric_limits<double>::lowest();
static constexpr double HIGHEST = std::numeric_limits<double>::max();
static constexpr uint SEED_BITS = 8;
static constexpr uint32_t BARE_EARTH = 0xFF8A7458U;
static constexpr uint32_t COPING_STONE = 0xFFD8D2C4U;
static constexpr uint COPING_SHARE = 140;
static constexpr double VENT_SEED_STEPS = 64.0;
static constexpr double FASCIA_SHADE = 0.72;

void StructureMesh::Polygon(std::span<const StructureVertex> corners)
{
	uint32_t first = static_cast<uint32_t>(this->vertices.size());
	this->vertices.insert(this->vertices.end(), corners.begin(), corners.end());
	for (uint32_t corner = 1; corner + 1 < corners.size(); corner++) this->Triangle(first, first + corner, first + corner + 1);
}

/* The triangles of solid claddings are laid before those of open ones; how many indices the solid ones take. */
size_t StructureMesh::SolidFirst()
{
	std::vector<uint32_t> solid;
	std::vector<uint32_t> open;
	for (auto corner = this->indices.begin(); corner != this->indices.end(); corner += 3) {
		std::vector<uint32_t> &into = IsOpen(static_cast<Material>(this->vertices[*corner].surface[0])) ? open : solid;
		into.insert(into.end(), corner, corner + 3);
	}
	size_t solid_indices = solid.size();
	solid.insert(solid.end(), open.begin(), open.end());
	this->indices = std::move(solid);
	return solid_indices;
}

FormStyle::FormStyle(const BuildingForm &form, MiniLayer layer) :
	floor(form.floor * LevelRise()),
	layer(to_underlying(layer)),
	seed(Hash32(static_cast<uint32_t>(form.tx) ^ Hash32(static_cast<uint32_t>(form.ty))))
{
}

StructureVertex FormStyle::Vertex(const ModelVertex &model, double u, double v, const Coat &coat, uint32_t glass) const
{
	StructureVertex vertex = {
		model,
		{static_cast<uint8_t>(Red(glass)), static_cast<uint8_t>(Green(glass)), static_cast<uint8_t>(Blue(glass)), static_cast<uint8_t>(SeedBits(this->seed, 0, SEED_BITS))},
		static_cast<float>(u),
		static_cast<float>(v),
		{to_underlying(coat.material), to_underlying(coat.grid), coat.flags.base(), this->layer},
	};
	vertex.model.Paint(coat.tint);
	return vertex;
}

/* Decals are laid opaque: one seen through stands for a shade over bare earth, deepening with every layer laid over it. */
static uint32_t OpaqueDecalTint(uint32_t tint, uint layer)
{
	uint alpha = Alpha(tint);
	if (alpha == CHANNEL_MAX) return tint;
	return Mix(BARE_EARTH, WithAlpha(tint, CHANNEL_MAX), std::min(alpha * (layer + 1), CHANNEL_MAX));
}

Vec3 RenderNormalOf(const Vec3 &model_normal)
{
	return Normalised({model_normal.x, model_normal.y, model_normal.z / FORM_HEIGHT_SCALE});
}

/* The lowest ground under any tile of the form, a little below which every wall standing on the ground starts. */
static double FootingOf(const BuildingForm &form)
{
	double lowest = form.floor;
	for (int ty = form.ty; ty < form.ty + form.size_y; ty++) {
		for (int tx = form.tx; tx < form.tx + form.size_x; tx++) lowest = std::min(lowest, TileGround(tx, ty).Lowest());
	}
	return lowest * LevelRise() - FOOTING_DEPTH;
}

/* Lays the faces of one solid into the mesh, each clad by what it is: walls, windowed bands of a facade, roofs and decals. */
class SolidMesher final : public FaceSink {
public:
	SolidMesher(const FormStyle &style, const Solid &solid, double footing, uint decal_layer, StructureMesh &mesh) :
		style(style), solid(solid), footing(footing), decal_layer(decal_layer), mesh(mesh)
	{
	}

	void Take(const Face &face) override
	{
		if (face.cladding == Cladding::Facade) {
			this->CoverFacade(face);
			return;
		}
		this->Lay(face.polygon, face, this->CoatOf(face.cladding));
	}

private:
	Coat CoatOf(Cladding cladding) const
	{
		switch (cladding) {
			case Cladding::Wall:
			case Cladding::Facade: return {this->solid.wall_material, WindowGrid::None, {}, this->solid.wall_tint};
			case Cladding::Roof: return {this->solid.roof_material, WindowGrid::None, SurfaceFlag::Roof, this->solid.roof_tint};
			case Cladding::Skylight: return {Material::GlassRoof, WindowGrid::None, SurfaceFlag::Roof, this->solid.glass_tint};
			case Cladding::Fascia: return {Material::Plain, WindowGrid::None, {}, ScaledRgb(this->solid.roof_tint, FASCIA_SHADE)};
			case Cladding::Coping: return {Material::Concrete, WindowGrid::None, {}, Mix(this->solid.wall_tint, COPING_STONE, COPING_SHARE)};
			default: return {this->solid.roof_material, WindowGrid::None, SurfaceFlag::Decal, OpaqueDecalTint(this->solid.roof_tint, this->decal_layer)};
		}
	}

	/* Windows run from the ground band up to the eaves; below and above them the wall is plain. */
	void CoverFacade(const Face &face)
	{
		bool front = this->solid.fronts.Test(FacingSide(face.normal));
		double windows_from = front || this->solid.base >= GroundStoreyTiles(this->solid.windows) ? LOWEST : GroundStoreyTiles(this->solid.windows);
		double eave = this->solid.base + this->solid.wall;
		Coat wall = this->CoatOf(Cladding::Wall);
		Coat facade = {this->solid.wall_material, this->solid.windows, front ? SurfaceFlags{SurfaceFlag::Facade, SurfaceFlag::Front} : SurfaceFlags{SurfaceFlag::Facade}, this->solid.wall_tint};
		this->LayBand(face, LOWEST, windows_from, wall);
		this->LayBand(face, windows_from, eave, facade);
		this->LayBand(face, eave, HIGHEST, wall);
	}

	void LayBand(const Face &face, double low, double high, const Coat &coat)
	{
		FacePolygon band = face.polygon;
		ClipToHeights(band, low, high);
		if (band.IsSurface()) this->Lay(band, face, coat);
	}

	void Lay(const FacePolygon &polygon, const Face &face, const Coat &coat)
	{
		std::span<const FacePoint> points = polygon.Points();
		std::array<StructureVertex, MAX_POLYGON_CORNERS> corners;
		bool upright = IsUpright(face.normal);
		std::ranges::transform(points, corners.begin(), [&](const FacePoint &point) {
			ModelVertex model{};
			model.Place(this->Place(point, face, coat));
			model.Face(RenderNormalOf(face.smooth && Dot(point.normal, point.normal) > 0.0 ? point.normal : face.normal));
			model.Occlude(point.shade);
			double v = upright ? point.z - this->solid.base : face.uv.V(point);
			return this->style.Vertex(model, face.uv.U(point), v, coat, this->solid.glass_tint);
		});
		this->mesh.Polygon(std::span(corners).first(points.size()));
	}

	/* A wall standing on the ground reaches down below its lowest point, and a decal lies a little above what it covers. */
	Vec3 Place(const FacePoint &point, const Face &face, const Coat &coat) const
	{
		double z = this->style.Floor() + point.z * (point.grounded ? LevelRise() * MODEL_TILE_LEVELS : FORM_HEIGHT_SCALE);
		if (face.footed && this->solid.base <= 0.0f && point.z <= this->solid.base + PLACE_EPS) z = this->footing;
		if (coat.flags.Test(SurfaceFlag::Decal)) z += DECAL_LIFT * (this->decal_layer + 1);
		return {point.x, point.y, z};
	}

	const FormStyle &style;
	const Solid &solid;
	double footing;
	uint decal_layer;
	StructureMesh &mesh;
};

/* Smoke leaves a stack through the top of its last slice, as wide as the slice narrows to. */
static void AddVent(const BuildingForm &form, const Solid &solid, double floor, std::vector<SmokeVent> &vents)
{
	if (solid.fixture != Fixture::Smokestack) return;
	double top_share = 1.0 - 2.0 * solid.taper;
	Vec3 mouth = {form.tx + (solid.x0 + solid.x1) / 2.0, form.ty + (solid.y0 + solid.y1) / 2.0, floor + solid.Top() * FORM_HEIGHT_SCALE};
	uint32_t seed = Hash32(static_cast<uint32_t>(mouth.x * VENT_SEED_STEPS) ^ Hash32(static_cast<uint32_t>(mouth.y * VENT_SEED_STEPS)));
	vents.push_back({mouth, std::min(solid.x1 - solid.x0, solid.y1 - solid.y0) / 2.0 * top_share, {}, seed});
}

static void AddPick(const BuildingForm &form, const Solid &solid, double floor, std::vector<StructurePick> &picks)
{
	if (solid.kind == SolidKind::Decal) return;
	picks.push_back({
		{form.tx + solid.x0, form.ty + solid.y0, floor + solid.base * FORM_HEIGHT_SCALE},
		{form.tx + solid.x1, form.ty + solid.y1, floor + solid.Top() * FORM_HEIGHT_SCALE},
		{form.tx, form.ty, form.tx + form.size_x - 1, form.ty + form.size_y - 1},
	});
}

void BuildStructure(const BuildingForm &form, StructureDetail detail, MiniLayer layer, StructureParts &parts)
{
	FormStyle style(form, layer);
	PlanRect footprint = {static_cast<double>(form.tx), static_cast<double>(form.ty), static_cast<double>(form.tx + form.size_x), static_cast<double>(form.ty + form.size_y)};
	bool full = detail == StructureDetail::Full;
	SolidPlacement placement = {footprint, form.floor, full ? FULL_SEGMENTS : SIMPLE_SEGMENTS};
	double footing = FootingOf(form);
	uint decals = 0;
	for (const Solid &solid : form.Solids()) {
		AddPick(form, solid, style.Floor(), parts.picks);
		AddVent(form, solid, style.Floor(), parts.vents);
		if (solid.role == SolidRole::Detail && !full) continue;
		if (full) DressSolid(style, solid, footprint, parts.mesh);
		if (full && ReplacesSolid(solid)) continue;
		SolidMesher mesher(style, solid, footing, decals, parts.mesh);
		BuildFaces(solid, placement, mesher);
		if (solid.kind == SolidKind::Decal) decals++;
	}
}
