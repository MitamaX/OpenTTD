/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file surface_painters.cpp One repeating grey pattern per building material, shared by the walls and roofs made of it. */

#include "../../stdafx.h"
#include "surface_painters.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <numbers>
#include <vector>

#include "../core/camera.h"
#include "../core/seed.h"
#include "material_atlas.h"
#include "pattern_noise.h"

#include "../../safeguards.h"

namespace {

	enum NoiseSalt : uint32_t {
		PHASE_SALT = 1,
		DRIFT_SALT,
		GRAIN_SALT,
		SPECKLE_SALT,
		FIBRE_SALT,
		STREAK_SALT,
		STAIN_SALT,
		STRAW_SALT,
	};

	enum class Lap : uint8_t {
		Aligned,
		Half,
		Random,
	};

	struct Bond {
		int course_rows;
		int blocks;
		float joint_drift = 0.0f;
		Lap lap = Lap::Aligned;
	};

	struct Block {
		int lx;
		int ly;
		int width;
		float shade;
	};

	struct Mottle {
		float luma;
		float grain = 0.0f;
		int cycles = 1;
		int octaves = 1;
		float speckle = 0.0f;
	};

	struct Coursework {
		Bond bond;
		float joint;
		float spread;
		Mottle surface;
		float fibre = 0.0f;
	};

}

static constexpr uint32_t SURFACE_SEED = 0x6D617465U;
static constexpr float TWO_PI = 2.0f * std::numbers::pi_v<float>;

static constexpr int FIBRE_CYCLES_ALONG = 4;
static constexpr int FIBRE_CYCLES_ACROSS = 32;
static constexpr int STREAK_CYCLES_ACROSS = 16;
static constexpr int STREAK_CYCLES_DOWN = 2;
static constexpr int STAIN_CYCLES = 2;
static constexpr int STAIN_OCTAVES = 2;

static constexpr Coursework BRICK_WORK = {
	.bond = {.course_rows = 4, .blocks = 8, .lap = Lap::Half},
	.joint = 0.96f,
	.spread = 0.10f,
	.surface = {.luma = 0.80f, .grain = 0.04f, .cycles = 8, .octaves = 3},
};

static constexpr Mottle RENDER_MOTTLE = {.luma = 0.93f, .grain = 0.04f, .cycles = 4, .octaves = 3, .speckle = 0.015f};

static constexpr std::array<float, 4> CLAPBOARD_ROWS = {0.72f, 0.90f, 0.95f, 0.97f};
static constexpr Bond CLAPBOARD_BOND = {.course_rows = static_cast<int>(CLAPBOARD_ROWS.size()), .blocks = 2, .joint_drift = 0.4f, .lap = Lap::Random};
static constexpr float CLAPBOARD_SPREAD = 0.06f;
static constexpr float CLAPBOARD_FIBRE = 0.05f;
static constexpr float BUTT_JOINT_SHADE = 0.8f;

static constexpr Bond ASHLAR_BOND = {.course_rows = 8, .blocks = 4, .joint_drift = 0.25f, .lap = Lap::Random};
static constexpr Coursework STONE_WORK = {
	.bond = ASHLAR_BOND,
	.joint = 0.55f,
	.spread = 0.12f,
	.surface = {.luma = 0.84f, .grain = 0.06f, .cycles = 8, .octaves = 3},
};
static constexpr Coursework FOUNDATION_WORK = {.bond = ASHLAR_BOND, .joint = 0.55f, .spread = 0.12f, .surface = {.luma = 0.84f}};

static constexpr Coursework CONCRETE_WORK = {
	.bond = {.course_rows = 16, .blocks = 2},
	.joint = 0.72f,
	.spread = 0.06f,
	.surface = {.luma = 0.88f, .grain = 0.06f, .cycles = 8, .octaves = 3},
};
static constexpr int FORM_TIE_PITCH_X = 16;
static constexpr int FORM_TIE_PITCH_Y = 8;
static constexpr float FORM_TIE_SHADE = 0.8f;

static constexpr int CURTAIN_BAY = 8;
static constexpr float CURTAIN_HEAD_LUMA = 0.50f;
static constexpr float CURTAIN_FOOT_LUMA = 0.35f;
static constexpr float CURTAIN_PANE_SPREAD = 0.04f;

static constexpr int CORRUGATION_PITCH = 4;
static constexpr float CORRUGATION_LUMA = 0.82f;
static constexpr float CORRUGATION_DEPTH = 0.13f;
static constexpr int SHEET_ROWS = 32;
static constexpr float SHEET_LAP_LUMA = 0.70f;
static constexpr float SHEET_STREAK = 0.10f;

static constexpr Bond PLATE_BOND = {.course_rows = 16, .blocks = 2, .lap = Lap::Half};
static constexpr float PLATE_LUMA = 0.93f;
static constexpr float WELD_LUMA = 0.78f;
static constexpr float PLATE_SEAM_LUMA = 0.85f;
static constexpr float PLATE_STREAK = 0.08f;

static constexpr Coursework PLANK_WORK = {
	.bond = {.course_rows = 8, .blocks = 2, .joint_drift = 0.4f, .lap = Lap::Random},
	.joint = 0.55f,
	.spread = 0.15f,
	.surface = {.luma = 0.825f},
	.fibre = 0.06f,
};

static constexpr float LATTICE_LUMA = 0.9f;
static constexpr float LATTICE_HALF_WIDTH = 1.0f;
static constexpr int LATTICE_PANEL_ROWS = 16;

static constexpr std::array<float, 8> PANTILE_ROW_SHADE = {0.62f, 0.82f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 0.95f};
static constexpr Bond PANTILE_BOND = {.course_rows = static_cast<int>(PANTILE_ROW_SHADE.size()), .blocks = 8};
static constexpr float PANTILE_TROUGH_LUMA = 0.78f;
static constexpr float PANTILE_CREST_RISE = 0.22f;
static constexpr float PANTILE_ROUNDNESS = 0.6f;
static constexpr float PANTILE_SPREAD = 0.10f;
static constexpr Mottle PANTILE_FIRING = {.luma = 1.0f, .grain = 0.05f, .cycles = 4, .octaves = 2};

static constexpr Coursework SLATE_WORK = {
	.bond = {.course_rows = 8, .blocks = 7, .joint_drift = 0.23f, .lap = Lap::Random},
	.joint = 0.55f,
	.spread = 0.14f,
	.surface = {.luma = 0.80f, .grain = 0.03f, .cycles = 8, .octaves = 2},
};
static constexpr float SLATE_LAP_LUMA = 0.6f;

static constexpr Coursework SHINGLE_WORK = {
	.bond = {.course_rows = 8, .blocks = 10, .joint_drift = 0.375f, .lap = Lap::Random},
	.joint = 0.5f,
	.spread = 0.16f,
	.surface = {.luma = 0.82f, .speckle = 0.12f},
};
static constexpr float SHINGLE_LAP_LUMA = 0.65f;

static constexpr float THATCH_LUMA = 0.66f;
static constexpr float THATCH_STRAW = 0.26f;
static constexpr float THATCH_SPECKLE = 0.08f;
static constexpr int STRAW_CYCLES_ACROSS = 16;
static constexpr int STRAW_CYCLES_DOWN = 2;
static constexpr float FINE_STRAW_SHARE = 0.3f;
static constexpr int THATCH_LAYER_ROWS = 16;
static constexpr double THATCH_LAYER_FADE_ROWS = 4.0;
static constexpr float THATCH_LAYER_SHADE = 0.8f;

static constexpr int SEAM_PITCH = 16;
static constexpr float SEAM_RIDGE_LUMA = 1.0f;
static constexpr float SEAM_FLANK_LUMA = 0.7f;
static constexpr Mottle SEAM_PANEL = {.luma = 0.86f, .grain = 0.04f, .cycles = 4, .octaves = 2};

static constexpr Mottle GRAVEL_MOTTLE = {.luma = 0.86f, .grain = 0.08f, .cycles = 16, .octaves = 3, .speckle = 0.05f};
static constexpr int PEBBLE_CYCLES = 16;
static constexpr float PEBBLE_DEPTH = 0.15f;
static constexpr float GRAVEL_STAIN = 0.05f;

static constexpr Mottle MEMBRANE_MOTTLE = {.luma = 0.94f, .grain = 0.03f, .cycles = 4, .octaves = 3};
static constexpr int MEMBRANE_SEAM_PITCH = 16;
static constexpr float MEMBRANE_SEAM_SHADE = 0.92f;
static constexpr int PUDDLE_CYCLES = 2;
static constexpr int PUDDLE_OCTAVES = 3;
static constexpr double PUDDLE_SHORE = 0.66;
static constexpr double PUDDLE_DEEP = 0.74;
static constexpr float PUDDLE_SHADE = 0.1f;

static constexpr Mottle DECK_MOTTLE = {.luma = 0.85f, .grain = 0.07f, .cycles = 8, .octaves = 3, .speckle = 0.03f};
static constexpr int DECK_SLAB = 32;
static constexpr float DECK_JOINT_SHADE = 0.82f;

static constexpr int ROOF_PANE_WIDTH = 8;
static constexpr int ROOF_PANE_LENGTH = 16;
static constexpr float ROOF_GLASS_LUMA = 0.40f;

static constexpr Mottle ASPHALT_MOTTLE = {.luma = 0.80f, .grain = 0.04f, .cycles = 16, .octaves = 2, .speckle = 0.10f};

static constexpr bool FillsCell(const Bond &bond)
{
	return TEXELS_PER_TILE % bond.course_rows == 0;
}

static_assert(FillsCell(BRICK_WORK.bond) && FillsCell(CLAPBOARD_BOND) && FillsCell(ASHLAR_BOND) && FillsCell(CONCRETE_WORK.bond) && FillsCell(PLATE_BOND));
static_assert(FillsCell(PLANK_WORK.bond) && FillsCell(PANTILE_BOND) && FillsCell(SLATE_WORK.bond) && FillsCell(SHINGLE_WORK.bond));

static float Grain(uint32_t seed, const CellTexel &texel, int cycles, int octaves)
{
	return Centred(TileFbm(SubSeed(seed, GRAIN_SALT), texel.u, texel.v, cycles, octaves));
}

static float Speckle(uint32_t seed, const CellTexel &texel)
{
	return Centred(Hash01(SubSeed(seed, SPECKLE_SALT), texel.x, texel.y));
}

static float Mottled(const Mottle &mottle, uint32_t seed, const CellTexel &texel)
{
	return mottle.luma + mottle.grain * Grain(seed, texel, mottle.cycles, mottle.octaves) + mottle.speckle * Speckle(seed, texel);
}

static float WoodGrain(uint32_t seed, const CellTexel &texel)
{
	return Centred(TileNoise(SubSeed(seed, FIBRE_SALT), texel.u, texel.v, FIBRE_CYCLES_ALONG, FIBRE_CYCLES_ACROSS));
}

static float Streaked(uint32_t seed, const CellTexel &texel, float depth)
{
	return 1.0f - depth + depth * TileNoise(SubSeed(seed, STREAK_SALT), texel.u, texel.v, STREAK_CYCLES_ACROSS, STREAK_CYCLES_DOWN);
}

static float Stained(uint32_t seed, const CellTexel &texel, float depth)
{
	return 1.0f - depth + depth * TileFbm(SubSeed(seed, STAIN_SALT), texel.u, texel.v, STAIN_CYCLES, STAIN_OCTAVES);
}

static float BlockSpacing(const Bond &bond)
{
	return static_cast<float>(TEXELS_PER_TILE) / bond.blocks;
}

static float CoursePhase(const Bond &bond, uint32_t seed, int course)
{
	switch (bond.lap) {
		case Lap::Aligned: return 0.0f;
		case Lap::Half: return course % 2 == 0 ? 0.0f : BlockSpacing(bond) / 2;
		case Lap::Random: return Hash01(SubSeed(seed, PHASE_SALT), course, 0) * TEXELS_PER_TILE;
	}
	NOT_REACHED();
}

/* Each joint strays from its even spot by up to half the drift, so neighbouring blocks differ by up to the drift in width. */
static std::vector<int> CourseJoints(const Bond &bond, uint32_t seed, int course)
{
	float phase = CoursePhase(bond, seed, course);
	std::vector<int> joints(bond.blocks + 1);
	for (int block = 0; block < bond.blocks; block++) {
		float drift = bond.joint_drift * Centred(Hash01(SubSeed(seed, DRIFT_SALT), course, block));
		joints[block] = static_cast<int>(std::lround(phase + BlockSpacing(bond) * (block + drift)));
	}
	joints[bond.blocks] = joints[0] + TEXELS_PER_TILE;
	return joints;
}

static std::vector<Block> LayBlocks(const Bond &bond, uint32_t seed)
{
	std::vector<Block> blocks(TEXELS_PER_TILE * TEXELS_PER_TILE);
	for (int course = 0; course < TEXELS_PER_TILE / bond.course_rows; course++) {
		std::vector<int> joints = CourseJoints(bond, seed, course);
		for (int block = 0; block < bond.blocks; block++) {
			int width = joints[block + 1] - joints[block];
			float shade = Hash01(seed, course, block);
			for (int ly = 0; ly < bond.course_rows; ly++) {
				int y = course * bond.course_rows + ly;
				for (int lx = 0; lx < width; lx++) blocks[y * TEXELS_PER_TILE + Wrapped(joints[block] + lx, TEXELS_PER_TILE)] = {lx, ly, width, shade};
			}
		}
	}
	return blocks;
}

template <typename Shade>
static void PaintBlocks(LumaCell &cell, const Bond &bond, uint32_t seed, Shade &&shade)
{
	std::vector<Block> blocks = LayBlocks(bond, seed);
	cell.Paint([&](const CellTexel &texel) { return shade(texel, blocks[texel.y * TEXELS_PER_TILE + texel.x]); });
}

static bool IsHeadJoint(const Block &block)
{
	return block.lx == 0;
}

static bool IsBedJoint(const Block &block, const Bond &bond)
{
	return block.ly == bond.course_rows - 1;
}

static bool IsLapShadow(const Block &block)
{
	return block.ly == 0;
}

static float BlockLuma(const Coursework &work, uint32_t seed, const CellTexel &texel, const Block &block)
{
	return Mottled(work.surface, seed, texel) + work.spread * Centred(block.shade) + work.fibre * WoodGrain(seed, texel);
}

static float MasonryLuma(const Coursework &work, uint32_t seed, const CellTexel &texel, const Block &block)
{
	return IsHeadJoint(block) || IsBedJoint(block, work.bond) ? work.joint : BlockLuma(work, seed, texel, block);
}

static void PaintMasonry(LumaCell &cell, uint32_t seed, const Coursework &work)
{
	PaintBlocks(cell, work.bond, seed, [&](const CellTexel &texel, const Block &block) { return MasonryLuma(work, seed, texel, block); });
}

/* Overlapping courses show no bed joint; the course above shades the top of the one below instead. */
static void PaintLapped(LumaCell &cell, uint32_t seed, const Coursework &work, float lap_luma)
{
	PaintBlocks(cell, work.bond, seed, [&](const CellTexel &texel, const Block &block) {
		if (IsHeadJoint(block)) return work.joint;
		return IsLapShadow(block) ? lap_luma : BlockLuma(work, seed, texel, block);
	});
}

static void PaintPlain(LumaCell &cell, uint32_t)
{
	cell.Fill(cell.Bounds(), SURFACE_MEAN);
}

static void PaintBrick(LumaCell &cell, uint32_t seed)
{
	PaintMasonry(cell, seed, BRICK_WORK);
}

static void PaintRender(LumaCell &cell, uint32_t seed)
{
	cell.Paint([&](const CellTexel &texel) { return Mottled(RENDER_MOTTLE, seed, texel); });
}

static void PaintTimber(LumaCell &cell, uint32_t seed)
{
	PaintBlocks(cell, CLAPBOARD_BOND, seed, [&](const CellTexel &texel, const Block &block) {
		float luma = CLAPBOARD_ROWS[block.ly] * (1.0f + CLAPBOARD_SPREAD * Centred(block.shade) + CLAPBOARD_FIBRE * WoodGrain(seed, texel));
		return IsHeadJoint(block) ? luma * BUTT_JOINT_SHADE : luma;
	});
}

static void PaintStone(LumaCell &cell, uint32_t seed)
{
	PaintMasonry(cell, seed, STONE_WORK);
}

static bool IsFormTie(const CellTexel &texel)
{
	return texel.x % FORM_TIE_PITCH_X == FORM_TIE_PITCH_X / 2 && texel.y % FORM_TIE_PITCH_Y == FORM_TIE_PITCH_Y / 2;
}

static void PaintConcrete(LumaCell &cell, uint32_t seed)
{
	PaintBlocks(cell, CONCRETE_WORK.bond, seed, [&](const CellTexel &texel, const Block &block) {
		float luma = MasonryLuma(CONCRETE_WORK, seed, texel, block);
		return IsFormTie(texel) ? luma * FORM_TIE_SHADE : luma;
	});
}

static void PaintGlass(LumaCell &cell, uint32_t seed)
{
	cell.Paint([&](const CellTexel &texel) {
		if (texel.x % CURTAIN_BAY == 0) return GLASS_FRAME_LUMA;
		float drop = static_cast<float>(texel.y % CURTAIN_BAY) / (CURTAIN_BAY - 1);
		float pane = CURTAIN_PANE_SPREAD * Centred(Hash01(seed, texel.x / CURTAIN_BAY, texel.y / CURTAIN_BAY));
		return std::lerp(CURTAIN_HEAD_LUMA, CURTAIN_FOOT_LUMA, drop) + pane;
	});
}

static void PaintCorrugated(LumaCell &cell, uint32_t seed)
{
	cell.Paint([&](const CellTexel &texel) {
		if (texel.y % SHEET_ROWS == SHEET_ROWS - 1) return SHEET_LAP_LUMA;
		float ridge = std::sin(TWO_PI * (texel.x + TEXEL_CENTRE) / CORRUGATION_PITCH);
		return (CORRUGATION_LUMA + CORRUGATION_DEPTH * ridge) * Streaked(seed, texel, SHEET_STREAK);
	});
}

static void PaintMetal(LumaCell &cell, uint32_t seed)
{
	PaintBlocks(cell, PLATE_BOND, seed, [&](const CellTexel &texel, const Block &block) {
		if (IsBedJoint(block, PLATE_BOND)) return WELD_LUMA;
		return IsHeadJoint(block) ? PLATE_SEAM_LUMA : PLATE_LUMA * Streaked(seed, texel, PLATE_STREAK);
	});
}

static void PaintPlanks(LumaCell &cell, uint32_t seed)
{
	PaintMasonry(cell, seed, PLANK_WORK);
}

static float PeriodicGap(float offset, float period)
{
	float rest = offset - period * std::floor(offset / period);
	return std::min(rest, period - rest);
}

static float StrutCover(float distance)
{
	return std::clamp(LATTICE_HALF_WIDTH + TEXEL_CENTRE - distance, 0.0f, 1.0f);
}

/* Posts stand on the cell's side edges and the braces are two families of slanted lines, so the frame repeats seamlessly. */
static void PaintLattice(LumaCell &cell, uint32_t)
{
	constexpr float slope = static_cast<float>(LATTICE_PANEL_ROWS) / TEXELS_PER_TILE;
	const float across_brace = 1.0f / std::hypot(1.0f, slope);
	cell.ForEachTexel([&](const CellTexel &texel) {
		float x = texel.x + TEXEL_CENTRE;
		float y = texel.y + TEXEL_CENTRE;
		float post = PeriodicGap(x, TEXELS_PER_TILE);
		float rising = PeriodicGap(y - slope * x, LATTICE_PANEL_ROWS) * across_brace;
		float falling = PeriodicGap(y + slope * x, LATTICE_PANEL_ROWS) * across_brace;
		cell.Set(texel.x, texel.y, LATTICE_LUMA, StrutCover(std::min({post, rising, falling})));
	});
}

static float BarrelProfile(const Block &block)
{
	float across = std::sin(std::numbers::pi_v<float> * (block.lx + TEXEL_CENTRE) / block.width);
	return PANTILE_TROUGH_LUMA + PANTILE_CREST_RISE * std::pow(across, PANTILE_ROUNDNESS);
}

static void PaintClayTile(LumaCell &cell, uint32_t seed)
{
	PaintBlocks(cell, PANTILE_BOND, seed, [&](const CellTexel &texel, const Block &block) {
		float firing = Mottled(PANTILE_FIRING, seed, texel) + PANTILE_SPREAD * Centred(block.shade);
		return BarrelProfile(block) * PANTILE_ROW_SHADE[block.ly] * firing;
	});
}

static void PaintSlate(LumaCell &cell, uint32_t seed)
{
	PaintLapped(cell, seed, SLATE_WORK, SLATE_LAP_LUMA);
}

static void PaintShingle(LumaCell &cell, uint32_t seed)
{
	PaintLapped(cell, seed, SHINGLE_WORK, SHINGLE_LAP_LUMA);
}

static float Straw(uint32_t seed, const CellTexel &texel)
{
	float coarse = TileNoise(seed, texel.u, texel.v, STRAW_CYCLES_ACROSS, STRAW_CYCLES_DOWN);
	float fine = TileNoise(SubSeed(seed, STRAW_SALT), texel.u, texel.v, 2 * STRAW_CYCLES_ACROSS, 2 * STRAW_CYCLES_DOWN);
	return std::lerp(coarse, fine, FINE_STRAW_SHARE);
}

static void PaintThatch(LumaCell &cell, uint32_t seed)
{
	cell.Paint([&](const CellTexel &texel) {
		float luma = THATCH_LUMA + THATCH_STRAW * Straw(seed, texel) + THATCH_SPECKLE * Speckle(seed, texel);
		float layer = static_cast<float>(SmoothStep(0.0, THATCH_LAYER_FADE_ROWS, texel.y % THATCH_LAYER_ROWS));
		return luma * std::lerp(THATCH_LAYER_SHADE, 1.0f, layer);
	});
}

static void PaintMetalSeam(LumaCell &cell, uint32_t seed)
{
	cell.Paint([&](const CellTexel &texel) {
		int offset = texel.x % SEAM_PITCH;
		if (offset == 0) return SEAM_RIDGE_LUMA;
		if (offset == 1 || offset == SEAM_PITCH - 1) return SEAM_FLANK_LUMA;
		return Mottled(SEAM_PANEL, seed, texel);
	});
}

static void PaintGravel(LumaCell &cell, uint32_t seed)
{
	cell.Paint([&](const CellTexel &texel) {
		float pebble = 1.0f - PEBBLE_DEPTH * TileCells(seed, texel.u, texel.v, PEBBLE_CYCLES);
		return Mottled(GRAVEL_MOTTLE, seed, texel) * pebble * Stained(seed, texel, GRAVEL_STAIN);
	});
}

static void PaintMembrane(LumaCell &cell, uint32_t seed)
{
	cell.Paint([&](const CellTexel &texel) {
		float luma = Mottled(MEMBRANE_MOTTLE, seed, texel);
		if (texel.x % MEMBRANE_SEAM_PITCH == 0) luma *= MEMBRANE_SEAM_SHADE;
		float wet = TileFbm(SubSeed(seed, STAIN_SALT), texel.u, texel.v, PUDDLE_CYCLES, PUDDLE_OCTAVES);
		return luma * (1.0f - PUDDLE_SHADE * static_cast<float>(SmoothStep(PUDDLE_SHORE, PUDDLE_DEEP, wet)));
	});
}

static void PaintRoofDeck(LumaCell &cell, uint32_t seed)
{
	cell.Paint([&](const CellTexel &texel) {
		float luma = Mottled(DECK_MOTTLE, seed, texel);
		bool joint = texel.x % DECK_SLAB == 0 || texel.y % DECK_SLAB == 0;
		return joint ? luma * DECK_JOINT_SHADE : luma;
	});
}

static void PaintGlassRoof(LumaCell &cell, uint32_t seed)
{
	float sheen_phase = Hash01(seed, 0, 0);
	cell.Paint([&](const CellTexel &texel) {
		bool frame = texel.x % ROOF_PANE_WIDTH == 0 || texel.y % ROOF_PANE_LENGTH == 0;
		return frame ? GLASS_FRAME_LUMA : ROOF_GLASS_LUMA + GlassSheen(texel.x, texel.y, sheen_phase);
	});
}

static void PaintFoundation(LumaCell &cell, uint32_t seed)
{
	PaintMasonry(cell, seed, FOUNDATION_WORK);
}

static void PaintAsphalt(LumaCell &cell, uint32_t seed)
{
	cell.Paint([&](const CellTexel &texel) { return Mottled(ASPHALT_MOTTLE, seed, texel); });
}

static SurfacePainter PainterOf(Material material)
{
	switch (material) {
		case Material::Plain: return PaintPlain;
		case Material::Brick: return PaintBrick;
		case Material::Render: return PaintRender;
		case Material::Timber: return PaintTimber;
		case Material::Stone: return PaintStone;
		case Material::Concrete: return PaintConcrete;
		case Material::Glass: return PaintGlass;
		case Material::Corrugated: return PaintCorrugated;
		case Material::Metal: return PaintMetal;
		case Material::Planks: return PaintPlanks;
		case Material::Lattice: return PaintLattice;
		case Material::ClayTile: return PaintClayTile;
		case Material::Slate: return PaintSlate;
		case Material::Shingle: return PaintShingle;
		case Material::Thatch: return PaintThatch;
		case Material::MetalSeam: return PaintMetalSeam;
		case Material::Gravel: return PaintGravel;
		case Material::Membrane: return PaintMembrane;
		case Material::RoofDeck: return PaintRoofDeck;
		case Material::GlassRoof: return PaintGlassRoof;
		case Material::Foundation: return PaintFoundation;
		case Material::Asphalt: return PaintAsphalt;
		case Material::End: break;
	}
	NOT_REACHED();
}

LumaCell PaintSurface(Material material)
{
	LumaCell cell(TEXELS_PER_TILE, TEXELS_PER_TILE, SURFACE_MEAN);
	PainterOf(material)(cell, SubSeed(SURFACE_SEED, to_underlying(material)));
	cell.NormalizeMean(SURFACE_MEAN);
	return cell;
}
