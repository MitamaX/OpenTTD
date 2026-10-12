/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file ground_detail.cpp The ground's finest grain and relief as tiling textures, mipmapped so the driver keeps them crisp at any angle. */

#include "../../stdafx.h"
#include "ground_detail.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <utility>
#include <vector>

#include "../core/seed.h"
#include "../core/space.h"
#include "frame_units.h"

#include "../../safeguards.h"

static constexpr int HOLLOW_REACH = 8;
static constexpr float HOLLOW_ENCODING = 4.0f;
static constexpr float STONE_COVER_GAIN = 4.0f;
static constexpr float STONE_RIM = 0.4f;
static constexpr float STONE_LUMPINESS = 0.15f;
static constexpr double NOISE_DEVIATION = 0.2;
static constexpr double BUMP_SLOPE_ENERGY = 103.0;
static constexpr double MIN_SLOPE_ENERGY = 1e-9;
static constexpr std::pair<float, float> BUMP_SHARES = {0.2f, 0.8f};

/* One size of stone: how many cells it is scattered over across the texture, how often a cell holds one, its radius in cells, how high it stands and how deep it may sink. */
struct StoneLayout {
	int cells;
	float presence;
	SeedRange radius;
	float rise;
	float deepest_sink;
	uint32_t salt;
};

static constexpr std::array<StoneLayout, 4> STONE_LAYOUTS = {{
	{12, 0.3f, {0.25, 0.45}, 1.0f, 0.5f, 101},
	{28, 0.4f, {0.15, 0.42}, 0.7f, 0.5f, 202},
	{84, 0.55f, {0.2, 0.45}, 0.4f, 0.4f, 303},
	{200, 0.6f, {0.2, 0.45}, 0.2f, 0.4f, 404},
}};

static_assert((DETAIL_SIZE & (DETAIL_SIZE - 1)) == 0, "the planes wrap by masking");

/* The detail's planes, a texel at a time and wrapping at their edges so the textures tile. */
class DetailPlane {
public:
	DetailPlane() : values(DETAIL_SIZE * DETAIL_SIZE, 0.0f) {}

	float &At(int x, int y) { return this->values[Wrapped(y) * DETAIL_SIZE + Wrapped(x)]; }
	float At(int x, int y) const { return this->values[Wrapped(y) * DETAIL_SIZE + Wrapped(x)]; }

	template <typename F>
	void Fill(F value)
	{
		for (int y = 0; y < DETAIL_SIZE; y++) {
			for (int x = 0; x < DETAIL_SIZE; x++) this->At(x, y) = value(x, y);
		}
	}

	/* Spreads the plane about the middle by a set deviation, so its noise varies the ground as much as the shader's own. */
	void Normalise()
	{
		double sum = 0.0;
		double squares = 0.0;
		for (float value : this->values) {
			sum += value;
			squares += value * value;
		}
		double mean = sum / this->values.size();
		double deviation = std::sqrt(std::max(squares / this->values.size() - mean * mean, 1e-9));
		for (float &value : this->values) value = static_cast<float>(0.5 + (value - mean) / deviation * NOISE_DEVIATION);
	}

	void Scale(float factor)
	{
		for (float &value : this->values) value *= factor;
	}

	static int Wrapped(int i) { return i & (DETAIL_SIZE - 1); }

private:
	std::vector<float> values;
};

static int WrappedBy(int i, int period)
{
	return ((i % period) + period) % period;
}

/* One octave of noise: a lattice of this many cells across the texture, read along a turn given as a whole number vector, so it still wraps with the texture but lines up with neither of its edges. */
struct Lattice {
	int cells;
	int turn_x;
	int turn_y;
	float weight;
};

template <typename Layouts>
static constexpr bool AllCellsPositive(const Layouts &layouts)
{
	for (const auto &layout : layouts) {
		if (layout.cells <= 0) return false;
	}
	return true;
}

static_assert(AllCellsPositive(STONE_LAYOUTS), "every stone layout needs cells to wrap its seeds by");

static constexpr std::array<Lattice, 2> FINE_LATTICES = {{{12, 1, 2, 1.0f}, {25, 2, -1, 0.55f}}};
static constexpr std::array<Lattice, 4> MICRO_LATTICES = {{{30, 2, 1, 1.0f}, {60, 1, -2, 0.3f}, {90, 1, 1, 0.2f}, {81, 3, 1, 1.0f}}};

static_assert(AllCellsPositive(FINE_LATTICES) && AllCellsPositive(MICRO_LATTICES), "every lattice needs cells to wrap its gradients by");

static float Quintic(float f)
{
	return f * f * f * (f * (f * 6.0f - 15.0f) + 10.0f);
}

/* The gradient at each point of a lattice, wrapping with it. */
class GradientTable {
public:
	GradientTable(int cells, uint32_t salt) : cells(cells)
	{
		this->gradients.reserve(cells * cells);
		for (int y = 0; y < cells; y++) {
			for (int x = 0; x < cells; x++) {
				float angle = Hash32(static_cast<uint32_t>(x) * 0x8DA6B343U ^ Hash32(static_cast<uint32_t>(y) * 0xD8163841U + salt)) / 4294967296.0f * static_cast<float>(FULL_TURN);
				this->gradients.emplace_back(std::cos(angle), std::sin(angle));
			}
		}
	}

	std::pair<float, float> At(int x, int y) const
	{
		return this->gradients[WrappedBy(y, this->cells) * this->cells + WrappedBy(x, this->cells)];
	}

private:
	int cells;
	std::vector<std::pair<float, float>> gradients;
};

/* Gradient noise, which unlike value noise shows no ridges along its lattice. */
static float GradientNoise(float u, float v, const GradientTable &table)
{
	int cx = static_cast<int>(std::floor(u));
	int cy = static_cast<int>(std::floor(v));
	float fx = u - cx;
	float fy = v - cy;
	auto corner = [&](int dx, int dy) {
		auto [gx, gy] = table.At(cx + dx, cy + dy);
		return gx * (fx - dx) + gy * (fy - dy);
	};
	float sx = Quintic(fx);
	float sy = Quintic(fy);
	return std::lerp(std::lerp(corner(0, 0), corner(1, 0), sx), std::lerp(corner(0, 1), corner(1, 1), sx), sy);
}

template <size_t N>
static DetailPlane Octaves(const std::array<Lattice, N> &lattices, uint32_t salt)
{
	DetailPlane sum;
	for (const Lattice &lattice : lattices) {
		GradientTable table(lattice.cells, salt++);
		float scale = static_cast<float>(lattice.cells) / DETAIL_SIZE;
		for (int y = 0; y < DETAIL_SIZE; y++) {
			for (int x = 0; x < DETAIL_SIZE; x++) {
				float u = (lattice.turn_x * x - lattice.turn_y * y) * scale;
				float v = (lattice.turn_y * x + lattice.turn_x * y) * scale;
				sum.At(x, y) += GradientNoise(u, v, table) * lattice.weight;
			}
		}
	}
	return sum;
}

/* A stone's height across its face, rounded on top and easing into the ground at its rim. */
static float StoneProfile(float reach)
{
	float rim = std::clamp((1.0f - reach) / STONE_RIM, 0.0f, 1.0f);
	return std::sqrt(1.0f - reach * reach) * rim * rim * (3.0f - 2.0f * rim);
}

static uint32_t CellSeed(int x, int y, int cells, uint32_t salt)
{
	uint32_t wx = static_cast<uint32_t>(WrappedBy(x, cells));
	uint32_t wy = static_cast<uint32_t>(WrappedBy(y, cells));
	return Hash32(wx * 0x9E3779B1U ^ Hash32(wy + salt));
}

/* Stones lie one at most to a cell, rounded and half sunk into the ground; each is stamped over the texels it covers, the highest standing wherever two meet. The cells just past the edges stamp the wrapped stones from outside, so they cross it. */
static void StampStones(DetailPlane &stones, const StoneLayout &layout)
{
	float cell_texels = static_cast<float>(DETAIL_SIZE) / layout.cells;
	for (int cy = -1; cy <= layout.cells; cy++) {
		for (int cx = -1; cx <= layout.cells; cx++) {
			SeedDice dice(CellSeed(cx, cy, layout.cells, layout.salt));
			if (dice.Share() > layout.presence) continue;
			float px = (cx + static_cast<float>(dice.Between(0.25, 0.75))) * cell_texels;
			float py = (cy + static_cast<float>(dice.Between(0.25, 0.75))) * cell_texels;
			float radius = static_cast<float>(dice.Between(layout.radius)) * cell_texels;
			float stretch = static_cast<float>(dice.Between(0.6, 1.0));
			float sink = static_cast<float>(dice.Between(0.0, layout.deepest_sink));
			float turn = static_cast<float>(dice.Between(0.0, FULL_TURN));
			float lobes = static_cast<float>(dice.Between(0.0, FULL_TURN));
			float cos_turn = std::cos(turn);
			float sin_turn = std::sin(turn);
			float reach_texels = radius * (1.0f + STONE_LUMPINESS);
			int first_x = std::max(0, static_cast<int>(std::floor(px - reach_texels)));
			int last_x = std::min(DETAIL_SIZE - 1, static_cast<int>(std::ceil(px + reach_texels)));
			int first_y = std::max(0, static_cast<int>(std::floor(py - reach_texels)));
			int last_y = std::min(DETAIL_SIZE - 1, static_cast<int>(std::ceil(py + reach_texels)));
			for (int y = first_y; y <= last_y; y++) {
				for (int x = first_x; x <= last_x; x++) {
					float along = (x - px) * cos_turn + (y - py) * sin_turn;
					float across = ((y - py) * cos_turn - (x - px) * sin_turn) / stretch;
					float lumpy = radius * (1.0f + STONE_LUMPINESS * std::sin(3.0f * std::atan2(across, along) + lobes));
					float reach = std::hypot(along, across) / lumpy;
					if (reach < 1.0f) stones.At(x, y) = std::max(stones.At(x, y), StoneProfile(reach) * (1.0f - sink) * layout.rise);
				}
			}
		}
	}
}

static uint8_t Byte(float share)
{
	return static_cast<uint8_t>(std::clamp(share, 0.0f, 1.0f) * 255.0f + 0.5f);
}

static uint32_t Texel(float r, float g, float b, float a)
{
	return Byte(r) | Byte(g) << 8 | Byte(b) << 16 | static_cast<uint32_t>(Byte(a)) << 24;
}

/* The plane averaged over a square about each texel, wrapping at the edges, in one pass along each axis. */
static DetailPlane BoxAverage(const DetailPlane &plane, int reach)
{
	const int side = 2 * reach + 1;
	DetailPlane along, average;
	for (int y = 0; y < DETAIL_SIZE; y++) {
		double window = 0.0;
		for (int d = -reach; d <= reach; d++) window += plane.At(d, y);
		for (int x = 0; x < DETAIL_SIZE; x++) {
			along.At(x, y) = static_cast<float>(window / side);
			window += plane.At(x + reach + 1, y) - plane.At(x - reach, y);
		}
	}
	for (int x = 0; x < DETAIL_SIZE; x++) {
		double window = 0.0;
		for (int d = -reach; d <= reach; d++) window += along.At(x, d);
		for (int y = 0; y < DETAIL_SIZE; y++) {
			average.At(x, y) = static_cast<float>(window / side);
			window += along.At(x, y + reach + 1) - along.At(x, y - reach);
		}
	}
	return average;
}

/* The height's change per texel along each axis, centred on the texel. */
static std::pair<float, float> CentralSlope(const DetailPlane &height, int x, int y)
{
	return {(height.At(x + 1, y) - height.At(x - 1, y)) * 0.5f, (height.At(x, y + 1) - height.At(x, y - 1)) * 0.5f};
}

static std::pair<float, float> EncodedSlope(const DetailPlane &height, int x, int y)
{
	auto [slope_x, slope_y] = CentralSlope(height, x, y);
	return {0.5f + 0.5f * slope_x * SLOPE_ENCODING, 0.5f + 0.5f * slope_y * SLOPE_ENCODING};
}

/* The mean squared slope of a plane across the ground, taken over a tile, however finely the texture is sampled. */
static double SlopeEnergy(const DetailPlane &height)
{
	constexpr double texels_per_tile = DETAIL_SIZE / DETAIL_REPEAT_TILES;
	double sum = 0.0;
	for (int y = 0; y < DETAIL_SIZE; y++) {
		for (int x = 0; x < DETAIL_SIZE; x++) {
			auto [slope_x, slope_y] = CentralSlope(height, x, y);
			sum += (slope_x * slope_x + slope_y * slope_y) * texels_per_tile * texels_per_tile;
		}
	}
	return std::max(sum / (DETAIL_SIZE * DETAIL_SIZE), MIN_SLOPE_ENERGY);
}

struct DetailTexels {
	std::vector<uint32_t> grain;
	std::vector<uint32_t> relief;
};

/* The grain holds the fine and the finest ground noise, the stones' cover and the hollows between them; the relief the slopes of the fine bumps and of the stones apart. */
static DetailTexels BuildDetail()
{
	DetailPlane fine = Octaves(FINE_LATTICES, 11);
	DetailPlane micro = Octaves(MICRO_LATTICES, 23);
	DetailPlane stones, bumps;
	fine.Normalise();
	micro.Normalise();
	for (const StoneLayout &layout : STONE_LAYOUTS) StampStones(stones, layout);
	DetailPlane stone_average = BoxAverage(stones, HOLLOW_REACH);
	bumps.Fill([&](int x, int y) { return BUMP_SHARES.first * fine.At(x, y) + BUMP_SHARES.second * micro.At(x, y); });
	bumps.Scale(static_cast<float>(std::sqrt(BUMP_SLOPE_ENERGY / SlopeEnergy(bumps))));

	DetailTexels texels;
	texels.grain.reserve(DETAIL_SIZE * DETAIL_SIZE);
	texels.relief.reserve(DETAIL_SIZE * DETAIL_SIZE);
	for (int y = 0; y < DETAIL_SIZE; y++) {
		for (int x = 0; x < DETAIL_SIZE; x++) {
			auto [bump_x, bump_y] = EncodedSlope(bumps, x, y);
			auto [stone_x, stone_y] = EncodedSlope(stones, x, y);
			float hollow = (stone_average.At(x, y) - stones.At(x, y)) * HOLLOW_ENCODING;
			texels.grain.push_back(Texel(fine.At(x, y), micro.At(x, y), stones.At(x, y) * STONE_COVER_GAIN, 0.5f + hollow));
			texels.relief.push_back(Texel(bump_x, bump_y, stone_x, stone_y));
		}
	}
	return texels;
}

static const DetailTexels &Detail()
{
	static const DetailTexels texels = BuildDetail();
	return texels;
}

void GroundDetail::Load()
{
	if (this->grain.Allocated()) return;
	Dimension size{DETAIL_SIZE, DETAIL_SIZE};
	this->grain.Allocate(TexelFormat::TiledRgba, size, Detail().grain.data());
	this->relief.Allocate(TexelFormat::TiledRgba, size, Detail().relief.data());
}

void GroundDetail::Bind() const
{
	this->grain.Bind(GRAIN_UNIT);
	this->relief.Bind(RELIEF_UNIT);
}

void GroundDetail::Release()
{
	this->grain.Release();
	this->relief.Release();
}

void GroundDetail::BindSamplers(const ShaderProgram &program)
{
	program.BindSampler("u_grain", GRAIN_UNIT);
	program.BindSampler("u_relief", RELIEF_UNIT);
}
