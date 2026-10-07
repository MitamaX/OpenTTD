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
#include "../core/seed.h"
#include "../core/tones.h"
#include "../map/tile_shapes.h"

#include "../../safeguards.h"

static constexpr int CLEAR_REACH = 1;
static constexpr int MEADOW_TUFTS = 10;
static constexpr int ROUGH_TUFTS = 22;
static constexpr int DESERT_TUFTS = 3;
static constexpr int MOST_SHRUBS = 3;
static constexpr SeedRange TUFT_SCALE = {1.0, 1.8};
static constexpr SeedRange SHRUB_SCALE = {0.8, 1.8};
static constexpr SeedRange DESERT_STRAW = {0.6, 1.0};
static constexpr double SINK = 0.003;
static constexpr uint32_t TUFT_SALT = 0x7F1A2B3CU;
static constexpr uint32_t SCRUB_SALT = 0x5C2B1D0EU;
static constexpr std::array<uint32_t, 4> SCRUB_GREENS = {0x5F6A3E, 0x7C8259, 0x8F8C63, 0x86704A};

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

static VehicleInstance TuftOn(double x, double y, double turn, double scale, const std::array<uint8_t, 4> &colour)
{
	double z = GroundLevel(x, y) * LevelRise() - SINK;
	return VehicleInstance{
		static_cast<float>(x), static_cast<float>(y), static_cast<float>(z), static_cast<float>(turn), 0.0f, 0.0f,
		static_cast<float>(scale), colour, colour, {},
	};
}

static void StrewScrub(int tx, int ty, const GrassGreens &greens, ScatterCopies &tufts)
{
	SeedDice dice(Hash32(SCRUB_SALT ^ TileXY(tx, ty).base()));
	int shrubs = std::max(static_cast<int>(dice.Below(MOST_SHRUBS + 2)) - 1, 0);
	for (int tuft = 0; tuft < DESERT_TUFTS + shrubs; tuft++) {
		bool shrub = tuft >= DESERT_TUFTS;
		double x = tx + dice.Share();
		double y = ty + dice.Share();
		std::array<uint8_t, 4> colour = shrub ? InstanceColour(SCRUB_GREENS[dice.Below(SCRUB_GREENS.size())]) : TuftGreen(greens, dice.Share(), dice.Between(DESERT_STRAW));
		size_t shape = shrub ? BLADE_TUFTS + dice.Below(SHRUB_TUFTS) : dice.Below(BLADE_TUFTS);
		double turn = dice.Between(0.0, 2.0 * M_PI);
		tufts[shape].push_back(TuftOn(x, y, turn, dice.Between(shrub ? SHRUB_SCALE : TUFT_SCALE), colour));
	}
}

GroundCover::GroundCover() : BlockScatter(TUFT_SHAPES, CLEAR_REACH)
{
}

/* Tufts keep a tile clear of ways, buildings and water about it, whose banks and walls the ground may rise or fall over.
 * Meadow at full growth bears a scatter of tufts, rough land many more and some of them dry; each tuft stands on the ground where it falls, turned and sized its own way. */
void GroundCover::Strew(const TileSpan &tiles, ScatterCopies &copies) const
{
	const GrassGreens &climate = CLIMATE_GREENS[to_underlying(_settings_game.game_creation.landscape) % CLIMATE_GREENS.size()];
	for (int ty = tiles.ty0; ty <= tiles.ty1; ty++) {
		for (int tx = tiles.tx0; tx <= tiles.tx1; tx++) {
			GroundTexel ground = _world_tiles.GroundAt(TileXY(tx, ty));
			bool rough = ground.material == GroundMaterial::Rough;
			bool meadow = ground.material == GroundMaterial::Grass && (ground.detail & GROUND_DENSITY_MASK) == GROUND_DENSITY_MASK;
			bool desert = ground.material == GroundMaterial::Desert;
			if ((!rough && !meadow && !desert) || !ClearAround(tx, ty, CLEAR_REACH)) continue;
			if (desert) {
				StrewScrub(tx, ty, climate, copies);
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
				copies[shape].push_back(TuftOn(x, y, turn, dice.Between(TUFT_SCALE), green));
			}
		}
	}
}
