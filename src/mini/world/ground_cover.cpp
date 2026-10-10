/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file ground_cover.cpp Tufts of grass scattered over open meadow and rough land and scrub over the desert, laid out block by block from the map's ground and shown only near the eye. */

#include "../../stdafx.h"
#include "ground_cover.h"

#include <algorithm>
#include <cmath>

#include "../../map_func.h"
#include "../../settings_type.h"
#include "../core/camera.h"
#include "../core/seed.h"
#include "../core/tones.h"
#include "../map/tile_shapes.h"
#include "hill_relief.h"

#include "../../safeguards.h"

static constexpr int CLEAR_REACH = 1;
static constexpr int MEADOW_TUFTS = 10;
static constexpr int ROUGH_TUFTS = 22;
static constexpr int MOST_DESERT_TUFTS = 7;
static constexpr int MOST_SHRUBS = 5;
static constexpr SeedRange TUFT_SCALE = {1.0, 1.8};
static constexpr SeedRange SHRUB_SCALE = {0.6, 2.4};
static constexpr std::array<double, 2> SCRUB_CLUMPS_PER_TILE = {0.13, 0.41};
static constexpr std::array<double, 2> SCRUB_CLUMP_WEIGHTS = {0.65, 0.35};
static constexpr SeedRange SCRUB_GATHERING = {0.42, 0.72};
static constexpr double LONE_SHRUB_SCALE = 0.6;
static constexpr SeedRange DESERT_STRAW = {0.6, 1.0};
static constexpr double SINK = 0.003;
static constexpr uint32_t TUFT_SALT = 0x7F1A2B3CU;
static constexpr uint32_t SCRUB_SALT = 0x5C2B1D0EU;
static constexpr std::array<uint32_t, 8> SCRUB_GREENS = {0x5F6A3E, 0x7C8259, 0x8F8C63, 0x86704A, 0x4E6B2C, 0x8E8A4A, 0x87907A, 0x6B5838};

/* The dark and the light greens of grass in each climate, and of lush grass, as the ground paints them. */
struct GrassGreens {
	uint32_t dark;
	uint32_t light;
};

static constexpr std::array<GrassGreens, 4> CLIMATE_GREENS = {{
	{0x546E36, 0x808F4D},
	{0x61785A, 0x879970},
	{0x858C47, 0xADA85C},
	{0x6BB861, 0x94D97A},
}};
static constexpr GrassGreens LUSH_GREENS = {0x33592B, 0x4F783B};
static constexpr uint32_t ROUGH_STRAW = 0x8C804C;

static std::array<uint8_t, 4> TuftGreen(const GrassGreens &greens, double share, double straw)
{
	return InstanceColour(Mix(Mix(greens.dark, greens.light, static_cast<uint>(share * CHANNEL_MAX)), ROUGH_STRAW, static_cast<uint>(straw * CHANNEL_MAX)));
}

static VehicleInstance TuftOn(const LaidGround &ground, double x, double y, double turn, double scale, const std::array<uint8_t, 4> &colour)
{
	double z = ground.Level(x, y) * LevelRise() - SINK;
	return VehicleInstance{
		static_cast<float>(x), static_cast<float>(y), static_cast<float>(z), static_cast<float>(turn), 0.0f, 0.0f,
		static_cast<float>(scale), colour, colour, {},
	};
}

/* Value noise over the map, smooth between the hashed points of a lattice this many to a tile. */
static double LatticeNoise(double x, double y, double frequency, uint32_t salt)
{
	double u = x * frequency;
	double v = y * frequency;
	int cx = static_cast<int>(std::floor(u));
	int cy = static_cast<int>(std::floor(v));
	auto corner = [&](int dx, int dy) {
		return SeedShare(Hash32(salt ^ Hash32(static_cast<uint32_t>(cx + dx) ^ Hash32(static_cast<uint32_t>(cy + dy)))), 0, SeedDice::SHARE_BITS);
	};
	double fx = SmoothStep(0.0, 1.0, u - cx);
	double fy = SmoothStep(0.0, 1.0, v - cy);
	return std::lerp(std::lerp(corner(0, 0), corner(1, 0), fx), std::lerp(corner(0, 1), corner(1, 1), fx), fy);
}

/* How thickly scrub gathers about a point: in clumps across the desert with open sand between, and in smaller knots within them. */
static double ScrubGathering(double x, double y)
{
	double noise = 0.0;
	for (size_t octave = 0; octave < SCRUB_CLUMPS_PER_TILE.size(); octave++) {
		noise += LatticeNoise(x, y, SCRUB_CLUMPS_PER_TILE[octave], SubSeed(SCRUB_SALT, static_cast<uint32_t>(octave))) * SCRUB_CLUMP_WEIGHTS[octave];
	}
	return SmoothStep(SCRUB_GATHERING.low, SCRUB_GATHERING.high, noise);
}

/* Each tuft and shrub a tile might bear stands only as often as scrub gathers where it falls, the shrubs larger where it gathers thick. */
static void StrewScrub(const LaidGround &ground, int tx, int ty, const GrassGreens &greens, ScatterCopies &tufts)
{
	SeedDice dice(Hash32(SCRUB_SALT ^ TileXY(tx, ty).base()));
	for (int tuft = 0; tuft < MOST_DESERT_TUFTS + MOST_SHRUBS; tuft++) {
		bool shrub = tuft >= MOST_DESERT_TUFTS;
		double x = tx + dice.Share();
		double y = ty + dice.Share();
		double gathering = ScrubGathering(x, y);
		if (dice.Share() >= gathering) continue;
		std::array<uint8_t, 4> colour = shrub ? InstanceColour(SCRUB_GREENS[dice.Below(SCRUB_GREENS.size())]) : TuftGreen(greens, dice.Share(), dice.Between(DESERT_STRAW));
		size_t shape = shrub ? BLADE_TUFTS + dice.Below(SHRUB_TUFTS) : dice.Below(BLADE_TUFTS);
		double turn = dice.Between(0.0, 2.0 * M_PI);
		double scale = shrub ? dice.Between(SHRUB_SCALE) * std::lerp(LONE_SHRUB_SCALE, 1.0, gathering) : dice.Between(TUFT_SCALE);
		tufts[shape].push_back(TuftOn(ground, x, y, turn, scale, colour));
	}
}

GroundCover::GroundCover() : BlockScatter(TUFT_SHAPES, std::max(CLEAR_REACH, LANDFORM_REACH), OPEN_LAND_READS | ChangeKind::Relief)
{
}

/* Tufts keep a tile clear of ways, buildings and water about it, whose banks and walls the ground may rise or fall over.
 * Meadow at full growth bears a scatter of tufts, rough land many more and some of them dry; each tuft stands on the ground where it falls, turned and sized its own way. */
void GroundCover::Strew(const TileSpan &tiles, ScatterCopies &copies) const
{
	const GrassGreens &climate = CLIMATE_GREENS[to_underlying(_settings_game.game_creation.landscape) % CLIMATE_GREENS.size()];
	Landform land(tiles);
	for (int ty = tiles.ty0; ty <= tiles.ty1; ty++) {
		for (int tx = tiles.tx0; tx <= tiles.tx1; tx++) {
			GroundTexel ground = _world_tiles.GroundAt(TileXY(tx, ty));
			bool rough = ground.material == GroundMaterial::Rough;
			bool meadow = ground.material == GroundMaterial::Grass && (ground.detail & GROUND_DENSITY_MASK) == GROUND_DENSITY_MASK;
			bool desert = ground.material == GroundMaterial::Desert;
			if ((!rough && !meadow && !desert) || !ClearAround(tx, ty, CLEAR_REACH)) continue;
			LaidGround laid(land, tx, ty);
			if (desert) {
				StrewScrub(laid, tx, ty, climate, copies);
				continue;
			}
			const GrassGreens &greens = (ground.detail & GROUND_LUSH_BIT) != 0 ? LUSH_GREENS : climate;
			SeedDice dice(Hash32(TUFT_SALT ^ TileXY(tx, ty).base()));
			int count = rough ? ROUGH_TUFTS : MEADOW_TUFTS;
			for (int tuft = 0; tuft < count; tuft++) {
				double x = tx + dice.Share();
				double y = ty + dice.Share();
				double straw = rough ? dice.Between(0.0, 0.8) : dice.Between(0.0, 0.25);
				std::array<uint8_t, 4> green = TuftGreen(greens, dice.Share(), straw);
				uint32_t shape = dice.Below(BLADE_TUFTS);
				double turn = dice.Between(0.0, 2.0 * M_PI);
				copies[shape].push_back(TuftOn(laid, x, y, turn, dice.Between(TUFT_SCALE), green));
			}
		}
	}
}
