/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file facade_painters.cpp Wall strips: a wall material stacked storey over storey with its openings, and the glazing laid over them. */

#include "../../stdafx.h"
#include "facade_painters.h"

#include "../core/seed.h"
#include "material_atlas.h"
#include "pattern_noise.h"

#include "../../safeguards.h"

namespace {

	enum class OpeningKind : uint8_t {
		None,
		Window,
		Shop,
		Door,
		GlassDoor,
		Shutter,
	};

	enum class Muntin : uint8_t {
		None,
		Cross,
		Sash,
		Mullion,
		Transom,
		Grid,
	};

	enum class Head : uint8_t {
		Square,
		Arch,
	};

	struct Opening {
		OpeningKind kind = OpeningKind::None;
		int width = 0;
		int height = 0;
		int sill = 0;
		Muntin muntin = Muntin::None;
		Head head = Head::Square;
	};

	struct FacadeLayout {
		int pitch;
		Opening upper{};
		Opening ground{};
		Opening entrance{};
		int fascia_rows = 0;
	};

	/* An opening set on a strip, its columns counted from the left and its rows from the ground. */
	struct Placement {
		Opening opening;
		int left;
		int bottom;
		int column;
		int storey;
	};

	struct Grey {
		float luma;
		float alpha;
	};

	enum GlazingSalt : uint32_t {
		BLIND_SALT = 1,
		SHEEN_SALT,
	};

}

static constexpr Opening COTTAGE_WINDOW = {.kind = OpeningKind::Window, .width = 7, .height = 4, .sill = 2, .muntin = Muntin::Cross};
static constexpr Opening COTTAGE_DOOR = {.kind = OpeningKind::Door, .width = 5, .height = 7};
static constexpr Opening TERRACE_WINDOW = {.kind = OpeningKind::Window, .width = 4, .height = 5, .sill = 1, .muntin = Muntin::Sash};
static constexpr Opening TERRACE_DOOR = {.kind = OpeningKind::Door, .width = 4, .height = 6};
static constexpr Opening FLAT_WINDOW = {.kind = OpeningKind::Window, .width = 6, .height = 4, .sill = 2, .muntin = Muntin::Transom};
static constexpr Opening FLAT_ENTRANCE = {.kind = OpeningKind::GlassDoor, .width = 6, .height = 7};
static constexpr Opening SHOP_UPPER_WINDOW = {.kind = OpeningKind::Window, .width = 5, .height = 4, .sill = 2, .muntin = Muntin::Mullion};
static constexpr Opening SHOP_DISPLAY = {.kind = OpeningKind::Shop, .width = 12, .height = 7, .sill = 2};
static constexpr Opening SHOP_DOOR = {.kind = OpeningKind::GlassDoor, .width = 4, .height = 9};
static constexpr Opening OFFICE_RIBBON = {.kind = OpeningKind::Window, .width = 7, .height = 3, .sill = 3};
static constexpr Opening OFFICE_LOBBY = {.kind = OpeningKind::Window, .width = 7, .height = 8, .sill = 2, .muntin = Muntin::Transom};
static constexpr Opening OFFICE_ENTRANCE = {.kind = OpeningKind::GlassDoor, .width = 7, .height = 10, .muntin = Muntin::Mullion};
static constexpr Opening CURTAIN_PANEL = {.kind = OpeningKind::Window, .width = 7, .height = 6, .sill = 1};
static constexpr Opening CURTAIN_LOBBY = {.kind = OpeningKind::Window, .width = 7, .height = 10, .sill = 1, .muntin = Muntin::Transom};
static constexpr Opening CURTAIN_ENTRANCE = {.kind = OpeningKind::GlassDoor, .width = 7, .height = 11, .muntin = Muntin::Mullion};
static constexpr Opening ARCHED_WINDOW = {.kind = OpeningKind::Window, .width = 6, .height = 10, .sill = 3, .muntin = Muntin::Sash, .head = Head::Arch};
static constexpr Opening ARCHED_DOOR = {.kind = OpeningKind::Door, .width = 8, .height = 13, .muntin = Muntin::Mullion, .head = Head::Arch};
static constexpr Opening FACTORY_WINDOW = {.kind = OpeningKind::Window, .width = 11, .height = 7, .sill = 5, .muntin = Muntin::Grid};
static constexpr Opening LOADING_SHUTTER = {.kind = OpeningKind::Shutter, .width = 20, .height = 13};
static constexpr Opening BAY_SHUTTER = {.kind = OpeningKind::Shutter, .width = 24, .height = 14};
static constexpr int SHOP_FASCIA_ROWS = 2;

static constexpr int FRAME_TEXELS = 1;
static constexpr int SILL_ROWS = 1;
static constexpr int ARCH_SAMPLES = 4;
static constexpr int TRANSOM_DROP = 2;
static constexpr int GRID_PANE_PITCH = 4;
static constexpr int GROUND_STOREY = 0;
static constexpr int ENTRANCE_COLUMN = -1;

static constexpr float SILL_LUMA = 1.0f;
static constexpr float LINTEL_LUMA = 0.97f;
static constexpr float BRICK_LINTEL_LUMA = 0.9f;
static constexpr float OPENING_LUMA = 0.40f;
static constexpr float REVEAL_LUMA = 0.30f;
static constexpr float DOOR_LUMA = 0.32f;
static constexpr float DOOR_SHADOW_LUMA = 0.26f;
static constexpr float SHUTTER_HOUSING_LUMA = 0.45f;
static constexpr float SHUTTER_SLAT_LUMA = 0.66f;
static constexpr float SHUTTER_GROOVE_LUMA = 0.58f;
static constexpr float FASCIA_LUMA = 0.62f;
static constexpr int PLINTH_ROWS = 2;
static constexpr float PLINTH_SHADE = 0.8f;
static constexpr int STREAK_ROWS = 6;
static constexpr float STREAK_DEPTH = 0.06f;

static constexpr uint32_t GLAZING_SEED = 0x676C617AU;
static constexpr float PANE_LUMA = 0.38f;
static constexpr float PANE_SPREAD = 0.14f;
static constexpr float BLIND_SHARE = 0.18f;
static constexpr float BLIND_LUMA = 0.62f;
static constexpr float HEAD_SHADE = 0.55f;

static constexpr FacadeLayout LayoutOf(WindowGrid grid)
{
	switch (grid) {
		case WindowGrid::None: return {.pitch = TEXELS_PER_TILE};
		case WindowGrid::Cottage: return {.pitch = 16, .upper = COTTAGE_WINDOW, .ground = COTTAGE_WINDOW, .entrance = COTTAGE_DOOR};
		case WindowGrid::Terrace: return {.pitch = 16, .upper = TERRACE_WINDOW, .ground = TERRACE_WINDOW, .entrance = TERRACE_DOOR};
		case WindowGrid::Apartment: return {.pitch = 16, .upper = FLAT_WINDOW, .ground = FLAT_WINDOW, .entrance = FLAT_ENTRANCE};
		case WindowGrid::Shopfront: return {.pitch = 16, .upper = SHOP_UPPER_WINDOW, .ground = SHOP_DISPLAY, .entrance = SHOP_DOOR, .fascia_rows = SHOP_FASCIA_ROWS};
		case WindowGrid::Office: return {.pitch = 8, .upper = OFFICE_RIBBON, .ground = OFFICE_LOBBY, .entrance = OFFICE_ENTRANCE};
		case WindowGrid::Curtain: return {.pitch = 8, .upper = CURTAIN_PANEL, .ground = CURTAIN_LOBBY, .entrance = CURTAIN_ENTRANCE};
		case WindowGrid::Arched: return {.pitch = 16, .upper = ARCHED_WINDOW, .ground = ARCHED_WINDOW, .entrance = ARCHED_DOOR};
		case WindowGrid::Industrial: return {.pitch = 32, .upper = FACTORY_WINDOW, .ground = FACTORY_WINDOW, .entrance = LOADING_SHUTTER};
		case WindowGrid::Doors: return {.pitch = 32, .ground = BAY_SHUTTER};
		case WindowGrid::End: break;
	}
	NOT_REACHED();
}

static constexpr bool HasSill(OpeningKind kind)
{
	return kind == OpeningKind::Window || kind == OpeningKind::Shop;
}

static constexpr bool IsGlazed(OpeningKind kind)
{
	return kind == OpeningKind::Window || kind == OpeningKind::Shop || kind == OpeningKind::GlassDoor;
}

static constexpr bool HasGlassFrame(OpeningKind kind)
{
	return kind == OpeningKind::Shop || kind == OpeningKind::GlassDoor;
}

static constexpr bool FitsBand(const Opening &opening, int band_rows)
{
	bool sill_inside = !HasSill(opening.kind) || opening.sill >= SILL_ROWS;
	return sill_inside && opening.sill + opening.height + FRAME_TEXELS <= band_rows;
}

static constexpr bool FitsFacade(WindowGrid grid)
{
	const FacadeLayout layout = LayoutOf(grid);
	const FacadeMetrics &metrics = FacadeMetricsOf(grid);
	bool repeats_with_bay = metrics.bay_texels % layout.pitch == 0;
	bool ground_fits = FitsBand(layout.ground, metrics.ground_texels) && FitsBand(layout.entrance, metrics.ground_texels) && layout.fascia_rows < metrics.ground_texels;
	return repeats_with_bay && ground_fits && FitsBand(layout.upper, metrics.storey_texels);
}

static constexpr bool EveryLayoutFits()
{
	for (uint8_t grid = 0; grid < to_underlying(WindowGrid::End); grid++) {
		if (!FitsFacade(static_cast<WindowGrid>(grid))) return false;
	}
	return true;
}

static_assert(EveryLayoutFits());

static int RowOf(int height)
{
	return STRIP_TEXELS - 1 - height;
}

static TexelRect RowsDown(int left, int width, int top_height, int rows)
{
	return {left, RowOf(top_height), width, rows};
}

static Placement Placed(const Opening &opening, int centre, int band_bottom, int column, int storey)
{
	return {opening, centre - opening.width / 2, band_bottom + opening.sill, column, storey};
}

/* The ground row comes first and the entrance over it, so an entrance may stand inside a ground opening or replace one. */
template <typename Visit>
static void ForEachOpening(WindowGrid grid, Visit &&visit)
{
	const FacadeLayout layout = LayoutOf(grid);
	const FacadeMetrics &metrics = FacadeMetricsOf(grid);
	auto visit_row = [&](const Opening &opening, int band_bottom, int storey) {
		if (opening.kind == OpeningKind::None) return;
		for (int column = 0; column < TEXELS_PER_TILE / layout.pitch; column++) {
			visit(Placed(opening, layout.pitch / 2 + column * layout.pitch, band_bottom, column, storey));
		}
	};

	visit_row(layout.ground, 0, GROUND_STOREY);
	if (layout.entrance.kind != OpeningKind::None) visit(Placed(layout.entrance, metrics.bay_texels / 2, 0, ENTRANCE_COLUMN, GROUND_STOREY));
	int storey = GROUND_STOREY + 1;
	for (int bottom = metrics.ground_texels; bottom + metrics.storey_texels <= STRIP_TEXELS; bottom += metrics.storey_texels) {
		visit_row(layout.upper, bottom, storey++);
	}
}

static float Radius(const Opening &opening)
{
	return opening.width / 2.0f;
}

static float Springline(const Placement &place)
{
	const Opening &opening = place.opening;
	float top = static_cast<float>(place.bottom + opening.height);
	return opening.head == Head::Arch ? top - Radius(opening) : top;
}

static float ArchCover(float centre_x, float springline, float radius, int x, int y)
{
	int inside = 0;
	for (int sy = 0; sy < ARCH_SAMPLES; sy++) {
		for (int sx = 0; sx < ARCH_SAMPLES; sx++) {
			float dx = x + (sx + TEXEL_CENTRE) / ARCH_SAMPLES - centre_x;
			float dy = y + (sy + TEXEL_CENTRE) / ARCH_SAMPLES - springline;
			if (dy < 0.0f || dx * dx + dy * dy <= radius * radius) inside++;
		}
	}
	return static_cast<float>(inside) / (ARCH_SAMPLES * ARCH_SAMPLES);
}

/* The share of the texel inside the opening once it is grown by this many texels at its sides and head. */
static float Cover(const Placement &place, int x, int y, int grow)
{
	const Opening &opening = place.opening;
	if (x < place.left - grow || x >= place.left + opening.width + grow) return 0.0f;
	if (y < place.bottom || y >= place.bottom + opening.height + grow) return 0.0f;
	if (opening.head == Head::Square) return 1.0f;
	return ArchCover(place.left + Radius(opening), Springline(place), Radius(opening) + grow, x, y);
}

/* Windows wear a lintel or arch above them only; doorways are framed up both sides as well. */
static float FrameCover(const Placement &place, int x, int y)
{
	if (HasSill(place.opening.kind) && y < Springline(place)) return 0.0f;
	return Cover(place, x, y, FRAME_TEXELS) - Cover(place, x, y, 0);
}

static bool IsFullyOpen(const Placement &place, int x, int y)
{
	return Cover(place, x, y, 0) >= 1.0f;
}

static bool UnderHead(const Placement &place, int x, int y)
{
	return !IsFullyOpen(place, x, y + 1);
}

static bool OnRim(const Placement &place, int x, int y)
{
	return UnderHead(place, x, y) || !IsFullyOpen(place, x, y - 1) || !IsFullyOpen(place, x - 1, y) || !IsFullyOpen(place, x + 1, y);
}

static bool IsMuntin(const Placement &place, int x, int y)
{
	const Opening &opening = place.opening;
	int lx = x - place.left;
	int ly = y - place.bottom;
	bool centre_column = lx == opening.width / 2;
	bool centre_row = ly == opening.height / 2;
	switch (opening.muntin) {
		case Muntin::None: return false;
		case Muntin::Cross: return centre_column || centre_row;
		case Muntin::Sash: return centre_row;
		case Muntin::Mullion: return centre_column;
		case Muntin::Transom: return ly == opening.height - TRANSOM_DROP;
		case Muntin::Grid: return (lx + 1) % GRID_PANE_PITCH == 0 || (ly + 1) % GRID_PANE_PITCH == 0;
	}
	NOT_REACHED();
}

template <typename Inside>
static void Stamp(LumaCell &cell, const Placement &place, const Grey &frame, Inside &&inside)
{
	const Opening &opening = place.opening;
	for (int y = place.bottom; y < place.bottom + opening.height + FRAME_TEXELS; y++) {
		for (int x = place.left - FRAME_TEXELS; x < place.left + opening.width + FRAME_TEXELS; x++) {
			int row = RowOf(y);
			cell.Blend(x, row, frame.luma, frame.alpha, FrameCover(place, x, y));
			float open = Cover(place, x, y, 0);
			if (open <= 0.0f) continue;
			Grey fill = inside(x, y);
			cell.Blend(x, row, fill.luma, fill.alpha, open);
		}
	}
}

static float ShutterLuma(const Placement &place, int y)
{
	int below_top = place.bottom + place.opening.height - 1 - y;
	if (below_top == 0) return SHUTTER_HOUSING_LUMA;
	return below_top % 2 == 0 ? SHUTTER_GROOVE_LUMA : SHUTTER_SLAT_LUMA;
}

static float OpeningLuma(const Placement &place, int x, int y)
{
	switch (place.opening.kind) {
		case OpeningKind::Door: return IsMuntin(place, x, y) || UnderHead(place, x, y) ? DOOR_SHADOW_LUMA : DOOR_LUMA;
		case OpeningKind::Shutter: return ShutterLuma(place, y);
		default: return UnderHead(place, x, y) ? REVEAL_LUMA : OPENING_LUMA;
	}
}

static void StampOpening(LumaCell &strip, const Placement &place, float lintel_luma)
{
	const Opening &opening = place.opening;
	if (HasSill(opening.kind)) {
		strip.Fill(RowsDown(place.left - FRAME_TEXELS, opening.width + 2 * FRAME_TEXELS, place.bottom - SILL_ROWS, SILL_ROWS), SILL_LUMA);
	}
	Stamp(strip, place, {lintel_luma, OPAQUE_ALPHA}, [&](int x, int y) { return Grey{OpeningLuma(place, x, y), OPAQUE_ALPHA}; });
}

static void StreakBelowSill(LumaCell &strip, const Placement &place)
{
	if (!HasSill(place.opening.kind)) return;
	int below_sill = place.bottom - SILL_ROWS - 1;
	for (int drop = 0; drop < STREAK_ROWS && below_sill - drop >= 0; drop++) {
		float fade = 1.0f - static_cast<float>(drop) / STREAK_ROWS;
		strip.Scale(RowsDown(place.left, place.opening.width, below_sill - drop, 1), 1.0f - STREAK_DEPTH * fade);
	}
}

static bool HasRainStreaks(Material wall)
{
	return wall == Material::Render || wall == Material::Concrete;
}

static void PaintFascia(LumaCell &strip, WindowGrid grid)
{
	int rows = LayoutOf(grid).fascia_rows;
	if (rows == 0) return;
	int cornice = FacadeMetricsOf(grid).ground_texels - 1;
	strip.Fill(RowsDown(0, TEXELS_PER_TILE, cornice, 1), SILL_LUMA);
	strip.Fill(RowsDown(0, TEXELS_PER_TILE, cornice - 1, rows), FASCIA_LUMA);
}

static float PaneLuma(const Placement &place, int y, uint32_t seed)
{
	const Opening &opening = place.opening;
	bool blind = opening.kind == OpeningKind::Window && Hash01(SubSeed(seed, BLIND_SALT), place.column, place.storey) < BLIND_SHARE;
	if (blind && y - place.bottom >= opening.height / 2) return BLIND_LUMA;
	return PANE_LUMA + PANE_SPREAD * Hash01(seed, place.column, place.storey);
}

static float GlassLuma(const Placement &place, int x, int y, uint32_t seed)
{
	bool framed = HasGlassFrame(place.opening.kind) && OnRim(place, x, y);
	if (framed || IsMuntin(place, x, y)) return GLASS_FRAME_LUMA;
	float luma = PaneLuma(place, y, seed);
	if (UnderHead(place, x, y)) luma *= HEAD_SHADE;
	return luma + GlassSheen(x, y, Hash01(SubSeed(seed, SHEEN_SALT), 0, 0));
}

static Grey GlassGrey(const Placement &place, int x, int y, uint32_t seed)
{
	if (!IsGlazed(place.opening.kind)) return {0.0f, CLEAR_ALPHA};
	return {GlassLuma(place, x, y, seed), OPAQUE_ALPHA};
}

static void GlazeOpening(LumaCell &glazing, const Placement &place, uint32_t seed)
{
	Stamp(glazing, place, {0.0f, CLEAR_ALPHA}, [&](int x, int y) { return GlassGrey(place, x, y, seed); });
}

LumaCell PaintFacadeStrip(const LumaCell &surface, Material wall, WindowGrid grid)
{
	LumaCell strip(TEXELS_PER_TILE, STRIP_TEXELS, 0.0f);
	strip.Paint([&](const CellTexel &texel) { return surface.Luma(texel.x, texel.y); });
	strip.Scale(RowsDown(0, TEXELS_PER_TILE, PLINTH_ROWS - 1, PLINTH_ROWS), PLINTH_SHADE);
	if (HasRainStreaks(wall)) ForEachOpening(grid, [&](const Placement &place) { StreakBelowSill(strip, place); });
	float lintel_luma = wall == Material::Brick ? BRICK_LINTEL_LUMA : LINTEL_LUMA;
	ForEachOpening(grid, [&](const Placement &place) { StampOpening(strip, place, lintel_luma); });
	PaintFascia(strip, grid);
	return strip;
}

LumaCell PaintGlazingStrip(WindowGrid grid)
{
	LumaCell glazing(TEXELS_PER_TILE, STRIP_TEXELS, 0.0f, CLEAR_ALPHA);
	uint32_t seed = SubSeed(GLAZING_SEED, to_underlying(grid));
	ForEachOpening(grid, [&](const Placement &place) { GlazeOpening(glazing, place, seed); });
	return glazing;
}

float StoreyMean(const LumaCell &strip, WindowGrid grid)
{
	const FacadeMetrics &metrics = FacadeMetricsOf(grid);
	int storey_top = metrics.ground_texels + metrics.storey_texels - 1;
	return strip.MeanLuma(RowsDown(0, TEXELS_PER_TILE, storey_top, metrics.storey_texels));
}
