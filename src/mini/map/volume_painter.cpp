/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file volume_painter.cpp Buildings and the steps between tiles drawn as lit, textured solids standing on the map. */

#include "../../stdafx.h"
#include "volume_painter.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <numbers>
#include <ranges>
#include <span>

#include "../../landscape.h"
#include "../../map_func.h"
#include "../../mini_atlas.h"
#include "../../settings_type.h"
#include "../art/material_atlas.h"
#include "../core/canvas.h"
#include "../core/tones.h"

#include "../../safeguards.h"

static constexpr double VISIBLE_EPS = 1e-4;
static constexpr int CYLINDER_SEGMENTS_SHAPED = 8;
static constexpr int CYLINDER_SEGMENTS_TEXTURED = 12;
static constexpr double SNOW_MIN_NZ = 0.5;
static constexpr uint SNOW_ROOF_SHARE = 210;
static constexpr double BUILDING_SHAPED_PPT = 12.0;
static constexpr double BUILDING_TEXTURED_PPT = 24.0;
static constexpr double BUILDING_DETAIL_PPT = 32.0;
static constexpr double MIN_FACE_PX = 0.75;
static constexpr double MIN_WALL_PX = 2.0;
static constexpr double HALF_DIAGONAL = std::numbers::sqrt2 / 2.0;
static constexpr int FOOTPRINT_REACH = MAX_FOOTPRINT_TILES - 1;
static constexpr double LOWEST = std::numeric_limits<double>::lowest();
static constexpr double HIGHEST = std::numeric_limits<double>::max();

static constexpr std::array<ShapeDetail, to_underlying(BuildingTier::Textured) + 1> TIER_SHAPES = {{
	{true, CYLINDER_SEGMENTS_SHAPED},
	{false, CYLINDER_SEGMENTS_SHAPED},
	{false, CYLINDER_SEGMENTS_TEXTURED},
}};

VolumePainter _volume_painter;

static BuildingTier TierFor(double ppt)
{
	if (ppt < BUILDING_SHAPED_PPT) return BuildingTier::Massing;
	if (ppt < BUILDING_TEXTURED_PPT) return BuildingTier::Shaped;
	return BuildingTier::Textured;
}

static std::optional<uint> SnowLine()
{
	if (_settings_game.game_creation.landscape != LandscapeType::Arctic) return std::nullopt;
	return GetSnowLine();
}

static Rect BoundsOf(std::span<const ScreenPoint> outline)
{
	auto [left, right] = std::ranges::minmax(outline | std::views::transform(&ScreenPoint::x));
	auto [top, bottom] = std::ranges::minmax(outline | std::views::transform(&ScreenPoint::y));
	return {static_cast<int>(std::floor(left)), static_cast<int>(std::floor(top)), static_cast<int>(std::ceil(right)), static_cast<int>(std::ceil(bottom))};
}

static double LongestSide(std::span<const ScreenPoint> outline)
{
	double longest = 0.0;
	for (size_t i = 0; i < outline.size(); i++) {
		const ScreenPoint &from = outline[i];
		const ScreenPoint &to = outline[(i + 1) % outline.size()];
		longest = std::max(longest, static_cast<double>(std::hypot(to.x - from.x, to.y - from.y)));
	}
	return longest;
}

/* How wide a face shows across its longest side, so a face seen edge on drops out however long it is. */
static double Thickness(std::span<const ScreenPoint> outline)
{
	double longest = LongestSide(outline);
	return longest > 0.0 ? std::abs(Winding(outline)) / 2.0 / longest : 0.0;
}

static float CellShare(double coordinate)
{
	return static_cast<float>(std::clamp(coordinate, 0.0, 1.0));
}

static PlanRect CellPlan(TileIndex cell)
{
	double x = TileX(cell);
	double y = TileY(cell);
	return {x, y, x + 1.0, y + 1.0};
}

static float TallestTop(const BuildingForm &form)
{
	return std::ranges::max(form.Solids() | std::views::transform(&Solid::Top));
}

static bool IsTop(Cladding cladding)
{
	return cladding == Cladding::Roof || cladding == Cladding::Skylight || cladding == Cladding::Decal;
}

[[maybe_unused]] static bool FitsContract(const BuildingForm &form)
{
	auto fits = [&](const Solid &solid) {
		return 0.0f <= solid.x0 && solid.x0 < solid.x1 && solid.x1 <= form.size_x
			&& 0.0f <= solid.y0 && solid.y0 < solid.y1 && solid.y1 <= form.size_y
			&& solid.Top() <= MAX_STRUCTURE_TILES
			&& (solid.role != SolidRole::Body || solid.Top() <= TALLEST_BUILDING_TILES);
	};
	return form.size_x <= MAX_FOOTPRINT_TILES && form.size_y <= MAX_FOOTPRINT_TILES && std::ranges::all_of(form.Solids(), fits);
}

TileSpan StructureSurvey(const TileSpan &visible)
{
	return {
		std::max(visible.tx0 - FOOTPRINT_REACH, 0),
		std::max(visible.ty0 - FOOTPRINT_REACH, 0),
		std::min(visible.tx1 + FOOTPRINT_REACH, static_cast<int>(Map::MaxX())),
		std::min(visible.ty1 + FOOTPRINT_REACH, static_cast<int>(Map::MaxY())),
	};
}

/* One layer of paint on a face: its colour before light, the light of a face lit as one plane, and the atlas cell it samples. */
struct Coat {
	uint32_t tint;
	double mean;
	double light;
	bool smooth;
	bool glassy;
	UvRect texture;
};

/* Lays coats of paint over the pieces of a face, projected from its placement's floor and cut into texture cells when the tier carries texture. */
class VolumePainter::CoatPainter {
public:
	CoatPainter(const VolumePainter &painter, const SolidPlacement &placement, bool textured) :
		painter(painter),
		placement(placement),
		textured(textured)
	{
	}

	/* Without texture every corner samples the plain white texel, so the cell's corners collapse onto it. */
	UvRect Sampled(const UvRect &texture) const
	{
		if (this->textured) return texture;
		const SolidTexel &white = this->painter.atlas;
		return {white.u, white.v, white.u, white.v};
	}

	void Lay(const FacePolygon &polygon, const UvMap &map, std::span<const Coat> coats) const
	{
		if (!this->textured) {
			for (const Coat &coat : coats) this->Paint(polygon, map, {}, coat);
			return;
		}

		UvCells cells = CellsOf(polygon, map);
		for (int u = cells.u0; u < cells.u1; u++) {
			for (int v = cells.v0; v < cells.v1; v++) {
				FacePolygon piece = polygon;
				ClipToUvCell(piece, map, {u, v});
				if (!piece.IsSurface()) continue;
				for (const Coat &coat : coats) this->Paint(piece, map, {u, v}, coat);
			}
		}
	}

	ScreenPoint Project(const FacePoint &point) const
	{
		return ScreenPointOf({point.x, point.y, LevelOf(this->placement.floor, point.z)});
	}

protected:
	const VolumePainter &painter;
	const SolidPlacement &placement;
	bool textured;

private:
	void Paint(const FacePolygon &piece, const UvMap &map, UvCell cell, const Coat &coat) const
	{
		std::span<const FacePoint> points = piece.Points();
		std::array<TexturedCorner, MAX_POLYGON_CORNERS> corners;
		std::ranges::transform(points, corners.begin(), [&](const FacePoint &point) { return this->CornerOf(point, map, cell, coat); });
		_map_draw.Polygon(this->painter.atlas.texture, std::span(corners).first(points.size()));
	}

	TexturedCorner CornerOf(const FacePoint &point, const UvMap &map, UvCell cell, const Coat &coat) const
	{
		FacePoint settled = this->Settled(point);
		return {
			this->Project(settled),
			std::lerp(coat.texture.left, coat.texture.right, CellShare(map.U(settled) - cell.u)),
			std::lerp(coat.texture.top, coat.texture.bottom, CellShare(map.V(settled) - cell.v)),
			_canvas.Tone(ScaledRgb(coat.tint, this->LightAt(point, coat) * coat.mean)),
		};
	}

	double LightAt(const FacePoint &point, const Coat &coat) const
	{
		const Daylight &daylight = this->painter.daylight;
		double light = (coat.smooth ? daylight.Light(RenderNormal(point.normal)) : coat.light) * point.shade;
		return coat.glassy ? daylight.Glass(light) : light;
	}

	/* A point cut out of a grounded edge lands on the ground beneath it, not on the straight line between the edge's ends. */
	FacePoint Settled(const FacePoint &point) const
	{
		if (!point.grounded) return point;
		FacePoint settled = point;
		settled.z = GroundHeight({point.x, point.y}, this->placement);
		return settled;
	}
};

/* Paints the faces of one solid that show in one cell of its form. */
class VolumePainter::SolidPainter final : public FaceSink, private CoatPainter {
public:
	SolidPainter(const VolumePainter &painter, const BuildingForm &form, const Solid &solid, const SolidPlacement &placement, const PlanRect &cell, const VolumeStyle &style) :
		CoatPainter(painter, placement, painter.tier == BuildingTier::Textured && !style.accent.has_value()),
		form(form),
		solid(solid),
		cell(cell),
		style(style),
		clips(form.size_x > 1 || form.size_y > 1)
	{
	}

	void Take(const Face &face) override
	{
		Vec3 normal = RenderNormal(face.normal);
		if (!this->Shows(face, normal)) return;

		FacePolygon piece = face.polygon;
		if (this->clips) ClipToPlan(piece, this->cell);
		if (face.footed) ClipToGround(piece, this->placement);
		if (piece.IsSurface()) this->Cover(face, piece, this->painter.daylight.Light(normal));
	}

private:
	bool Shows(const Face &face, const Vec3 &normal) const
	{
		std::span<const FacePoint> points = face.polygon.Points();
		const FacePoint &corner = points.front();
		if (!this->painter.FacesViewer(normal, {corner.x, corner.y, LevelOf(this->placement.floor, corner.z)})) return false;
		if (this->clips && OwnedByNeighbour(face, this->cell)) return false;

		std::array<ScreenPoint, MAX_POLYGON_CORNERS> outline;
		std::ranges::transform(points, outline.begin(), [&](const FacePoint &point) { return this->Project(point); });
		return Thickness(std::span(outline).first(points.size())) >= (IsUpright(normal) ? MIN_WALL_PX : MIN_FACE_PX);
	}

	void Cover(const Face &face, const FacePolygon &piece, double light) const
	{
		if (this->textured && face.cladding == Cladding::Facade) {
			this->CoverFacade(face, piece, light);
			return;
		}

		Coat coat = this->CoatOf(face.cladding, face, light);
		this->Lay(piece, face.uv, {&coat, 1});
	}

	/* Windows run from the ground band up to the eaves; below and above them the wall is plain. */
	void CoverFacade(const Face &face, const FacePolygon &piece, double light) const
	{
		UvMap plain = WallUv(face.normal);
		Coat wall = this->CoatOf(Cladding::Wall, face, light);
		std::array<Coat, 2> facade = {this->CoatOf(Cladding::Facade, face, light), this->GlazingOf(face, light)};
		size_t layers = this->painter.glazing > 0.0 ? facade.size() : 1;
		double windows_from = this->PlainBelow(face);
		double eave = this->solid.base + this->solid.wall;
		this->CoverBand(piece, LOWEST, windows_from, plain, {&wall, 1});
		this->CoverBand(piece, windows_from, eave, face.uv, std::span(facade).first(layers));
		this->CoverBand(piece, eave, HIGHEST, plain, {&wall, 1});
	}

	/* A wall off the fronts keeps its ground storey plain, unless it stands above the ground band already. */
	double PlainBelow(const Face &face) const
	{
		double ground_band = GroundStoreyTiles(this->solid.windows);
		bool front = this->solid.fronts.Test(FacingSide(face.normal));
		return front || this->solid.base >= ground_band ? LOWEST : ground_band;
	}

	void CoverBand(const FacePolygon &piece, double low, double high, const UvMap &map, std::span<const Coat> coats) const
	{
		FacePolygon band = piece;
		ClipToHeights(band, low, high);
		if (band.IsSurface()) this->Lay(band, map, coats);
	}

	Coat CoatOf(Cladding cladding, const Face &face, double light) const
	{
		return {this->TintOf(cladding, face), this->MeanOf(cladding), light, face.smooth, cladding == Cladding::Skylight, this->Sampled(this->TextureOf(cladding))};
	}

	Coat GlazingOf(const Face &face, double light) const
	{
		uint32_t glass = this->solid.glass_tint;
		return {WithAlpha(glass, FadedAlpha(this->painter.glazing, Alpha(glass))), 1.0, light, face.smooth, true, GlazingUv(this->solid.windows)};
	}

	uint32_t TintOf(Cladding cladding, const Face &face) const
	{
		uint32_t natural = this->NaturalTint(cladding, face);
		if (!this->style.accent.has_value()) return natural;
		uint32_t accent = *this->style.accent;
		return WithAlpha(IsTop(cladding) ? accent : Darken(accent), Alpha(natural));
	}

	uint32_t NaturalTint(Cladding cladding, const Face &face) const
	{
		switch (cladding) {
			case Cladding::Wall:
			case Cladding::Facade: return this->solid.wall_tint;
			case Cladding::Skylight: return this->solid.glass_tint;
			case Cladding::Roof: return this->Snowy(face) ? Mix(this->solid.roof_tint, COL_SNOW, SNOW_ROOF_SHARE) : this->solid.roof_tint;
			default: return this->solid.roof_tint;
		}
	}

	bool Snowy(const Face &face) const
	{
		const std::optional<uint> &snow_line = this->painter.snow_line;
		return snow_line.has_value() && this->form.floor > *snow_line && face.normal.z >= SNOW_MIN_NZ;
	}

	UvRect TextureOf(Cladding cladding) const
	{
		switch (cladding) {
			case Cladding::Wall: return MaterialUv(this->solid.wall_material);
			case Cladding::Facade: return FacadeUv(this->solid.wall_material, this->solid.windows);
			case Cladding::Skylight: return MaterialUv(Material::GlassRoof);
			default: return MaterialUv(this->solid.roof_material);
		}
	}

	/* Untextured faces stand in for their texture's mean luminance, so nothing brightens where texture takes over. */
	double MeanOf(Cladding cladding) const
	{
		if (this->textured || this->style.accent.has_value()) return 1.0;
		return cladding == Cladding::Facade ? FacadeMean(this->solid.wall_material, this->solid.windows) : SURFACE_MEAN;
	}

	const BuildingForm &form;
	const Solid &solid;
	const PlanRect &cell;
	const VolumeStyle &style;
	bool clips;
};

void VolumePainter::BeginFrame()
{
	this->picks.clear();
	this->ppt = _camera.Ppt();
	this->tier = TierFor(this->ppt);
	this->daylight = Daylight::Now();
	this->glazing = SmoothStep(BUILDING_TEXTURED_PPT, BUILDING_DETAIL_PPT, this->ppt);
	this->snow_line = SnowLine();
	this->atlas = MiniAtlasSolid();
}

/* The tile's whole form, raised to the tallest structure, has to reach the screen. */
bool VolumePainter::MayShow(TileIndex tile) const
{
	auto [x, y] = TileCentre(TileX(tile), TileY(tile));
	double reach = MAX_FOOTPRINT_TILES * HALF_DIAGONAL;
	double low = TileGround(tile).Lowest();
	PlanRect around = {x - reach, y - reach, x + reach, y + reach};
	std::array<PlanPoint, RECT_CORNERS> corners = CornersOf(around);
	std::array<ScreenPoint, 2 * RECT_CORNERS> outline;
	for (size_t i = 0; i < RECT_CORNERS; i++) {
		outline[2 * i] = ScreenPointOf({corners[i].x, corners[i].y, low});
		outline[2 * i + 1] = ScreenPointOf({corners[i].x, corners[i].y, low + STRUCTURE_RISE_LEVELS});
	}
	return OnScreen(outline);
}

void VolumePainter::Draw(const BuildingForm &form, TileIndex cell, const VolumeStyle &style)
{
	assert(FitsContract(form));
	if (form.count == 0) return;

	PlanRect plan = CellPlan(cell);
	std::optional<Rect> bounds = this->PieceBounds(form, plan);
	if (!bounds.has_value()) return;

	SolidPlacement placement = this->PlacementOf(form);
	std::array<uint8_t, MAX_SOLIDS> order = PaintOrder(form.Solids(), placement.toward);
	for (uint8_t index : std::span(order).first(form.count)) this->DrawSolid(form, form.solids[index], placement, plan, style);
	if (form.pickable) this->picks.push_back({*bounds, cell});
}

std::optional<TileIndex> VolumePainter::PickAt(Point screen) const
{
	for (const Pick &pick : this->picks | std::views::reverse) {
		if (pick.bounds.Contains(screen)) return pick.tile;
	}
	return std::nullopt;
}

bool VolumePainter::FacesViewer(const Vec3 &unit_normal, const WorldPoint &at) const
{
	return Dot(unit_normal, Normalised(_camera.Eye() - RenderPoint(at))) > VISIBLE_EPS;
}

/* The solids of a form stand in the order the view meets them from its eye. */
SolidPlacement VolumePainter::PlacementOf(const BuildingForm &form) const
{
	PlanRect footprint = {static_cast<double>(form.tx), static_cast<double>(form.ty), static_cast<double>(form.tx + form.size_x), static_cast<double>(form.ty + form.size_y)};
	TilePoint middle = {(footprint.x0 + footprint.x1) / 2.0, (footprint.y0 + footprint.y1) / 2.0};
	return {footprint, static_cast<double>(form.floor), _camera.Toward(middle), TIER_SHAPES[to_underlying(this->tier)]};
}

/* The cell's column from the form's floor, which every tile of a form stands on, up to the form's tallest top. */
std::optional<Rect> VolumePainter::PieceBounds(const BuildingForm &form, const PlanRect &cell) const
{
	std::array<PlanPoint, RECT_CORNERS> corners = CornersOf(cell);
	double low = form.floor;
	double high = LevelOf(form.floor, TallestTop(form));
	std::array<ScreenPoint, 2 * RECT_CORNERS> outline;
	for (size_t i = 0; i < RECT_CORNERS; i++) {
		outline[2 * i] = ScreenPointOf({corners[i].x, corners[i].y, low});
		outline[2 * i + 1] = ScreenPointOf({corners[i].x, corners[i].y, high});
	}
	if (!OnScreen(outline)) return std::nullopt;
	return BoundsOf(outline);
}

bool VolumePainter::ShowsDetail(const VolumeStyle &style) const
{
	return this->ppt >= BUILDING_DETAIL_PPT && !style.accent.has_value();
}

void VolumePainter::DrawSolid(const BuildingForm &form, const Solid &solid, const SolidPlacement &placement, const PlanRect &cell, const VolumeStyle &style) const
{
	if (solid.role == SolidRole::Detail && !this->ShowsDetail(style)) return;
	SolidPainter painter(*this, form, solid, placement, cell, style);
	BuildFaces(solid, placement, painter);
}
