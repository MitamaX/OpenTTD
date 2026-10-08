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
#include "frame_units.h"

#include "../../safeguards.h"

static constexpr int DETAIL_SIZE = 512;
static constexpr float SLOPE_ENCODING = 6.0f;
static constexpr int HOLLOW_REACH = 4;
static constexpr float HOLLOW_ENCODING = 4.0f;
static constexpr float STONE_COVER_GAIN = 4.0f;
static constexpr float STONE_RIM = 0.4f;
static constexpr float STONE_LUMPINESS = 0.15f;
static constexpr double NOISE_DEVIATION = 0.2;

/* One size of stone: how many cells it is scattered over across the texture, how often a cell holds one, its radius in cells, how high it stands and how deep it may sink. */
struct StoneLayout {
	int cells;
	float presence;
	SeedRange radius;
	float rise;
	float deepest_sink;
	uint32_t salt;
};

static constexpr std::array<StoneLayout, 3> STONE_LAYOUTS = {{
	{12, 0.3f, {0.25, 0.45}, 1.0f, 0.5f, 101},
	{28, 0.4f, {0.15, 0.42}, 0.7f, 0.5f, 202},
	{84, 0.55f, {0.2, 0.45}, 0.4f, 0.4f, 303},
}};

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

	static int Wrapped(int i) { return ((i % DETAIL_SIZE) + DETAIL_SIZE) % DETAIL_SIZE; }

private:
	std::vector<float> values;
};

/* One octave of noise: a lattice of this many cells across the texture, read along a turn given as a whole number vector, so it still wraps with the texture but lines up with neither of its edges. */
struct Lattice {
	int cells;
	int turn_x;
	int turn_y;
	float weight;
};

static constexpr std::array<Lattice, 2> FINE_LATTICES = {{{12, 1, 2, 1.0f}, {25, 2, -1, 0.55f}}};
static constexpr std::array<Lattice, 3> MICRO_LATTICES = {{{30, 2, 1, 1.0f}, {60, 1, -2, 0.55f}, {90, 1, 1, 0.3f}}};

static float Quintic(float f)
{
	return f * f * f * (f * (f * 6.0f - 15.0f) + 10.0f);
}

/* The gradient at a lattice point, wrapping with the lattice. */
static std::pair<float, float> LatticeGradient(int x, int y, int period, uint32_t salt)
{
	uint32_t wx = static_cast<uint32_t>(((x % period) + period) % period);
	uint32_t wy = static_cast<uint32_t>(((y % period) + period) % period);
	float angle = Hash32(wx * 0x8DA6B343U ^ Hash32(wy * 0xD8163841U + salt)) / 4294967296.0f * 2.0f * static_cast<float>(M_PI);
	return {std::cos(angle), std::sin(angle)};
}

/* Gradient noise, which unlike value noise shows no ridges along its lattice. */
static float GradientNoise(float u, float v, int period, uint32_t salt)
{
	int cx = static_cast<int>(std::floor(u));
	int cy = static_cast<int>(std::floor(v));
	float fx = u - cx;
	float fy = v - cy;
	auto corner = [&](int dx, int dy) {
		auto [gx, gy] = LatticeGradient(cx + dx, cy + dy, period, salt);
		return gx * (fx - dx) + gy * (fy - dy);
	};
	float sx = Quintic(fx);
	float sy = Quintic(fy);
	return std::lerp(std::lerp(corner(0, 0), corner(1, 0), sx), std::lerp(corner(0, 1), corner(1, 1), sx), sy);
}

template <size_t N>
static float Octaves(int x, int y, const std::array<Lattice, N> &lattices, uint32_t salt)
{
	float sum = 0.0f;
	for (const Lattice &lattice : lattices) {
		float scale = static_cast<float>(lattice.cells) / DETAIL_SIZE;
		float u = (lattice.turn_x * x - lattice.turn_y * y) * scale;
		float v = (lattice.turn_y * x + lattice.turn_x * y) * scale;
		sum += GradientNoise(u, v, lattice.cells, salt++) * lattice.weight;
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
	uint32_t wx = static_cast<uint32_t>(((x % cells) + cells) % cells);
	uint32_t wy = static_cast<uint32_t>(((y % cells) + cells) % cells);
	return Hash32(wx * 0x9E3779B1U ^ Hash32(wy + salt));
}

/* Stones lie one at most to a cell, rounded and half sunk into the ground, the highest wherever two meet. */
static float StoneHeight(int x, int y, const StoneLayout &layout)
{
	float cell_texels = static_cast<float>(DETAIL_SIZE) / layout.cells;
	int cx = static_cast<int>(std::floor(x / cell_texels));
	int cy = static_cast<int>(std::floor(y / cell_texels));
	float height = 0.0f;
	for (int dy = -1; dy <= 1; dy++) {
		for (int dx = -1; dx <= 1; dx++) {
			SeedDice dice(CellSeed(cx + dx, cy + dy, layout.cells, layout.salt));
			if (dice.Share() > layout.presence) continue;
			float px = (cx + dx + static_cast<float>(dice.Between(0.25, 0.75))) * cell_texels;
			float py = (cy + dy + static_cast<float>(dice.Between(0.25, 0.75))) * cell_texels;
			float radius = static_cast<float>(dice.Between(layout.radius)) * cell_texels;
			float stretch = static_cast<float>(dice.Between(0.6, 1.0));
			float sink = static_cast<float>(dice.Between(0.0, layout.deepest_sink));
			float turn = static_cast<float>(dice.Between(0.0, 2.0 * M_PI));
			float lobes = static_cast<float>(dice.Between(0.0, 2.0 * M_PI));
			float along = (x - px) * std::cos(turn) + (y - py) * std::sin(turn);
			float across = ((y - py) * std::cos(turn) - (x - px) * std::sin(turn)) / stretch;
			float lumpy = radius * (1.0f + STONE_LUMPINESS * std::sin(3.0f * std::atan2(across, along) + lobes));
			float reach = std::hypot(along, across) / lumpy;
			if (reach < 1.0f) height = std::max(height, StoneProfile(reach) * (1.0f - sink) * layout.rise);
		}
	}
	return height;
}

static float StonesAt(int x, int y)
{
	float height = 0.0f;
	for (const StoneLayout &layout : STONE_LAYOUTS) height = std::max(height, StoneHeight(x, y, layout));
	return height;
}

static uint8_t Byte(float share)
{
	return static_cast<uint8_t>(std::clamp(share, 0.0f, 1.0f) * 255.0f + 0.5f);
}

static uint32_t Texel(float r, float g, float b, float a)
{
	return Byte(r) | Byte(g) << 8 | Byte(b) << 16 | static_cast<uint32_t>(Byte(a)) << 24;
}

/* How far a point sinks below the ground about it, which shades the hollows between stones. */
static float Hollow(const DetailPlane &height, int x, int y)
{
	float sum = 0.0f;
	for (int dy = -HOLLOW_REACH; dy <= HOLLOW_REACH; dy++) {
		for (int dx = -HOLLOW_REACH; dx <= HOLLOW_REACH; dx++) sum += height.At(x + dx, y + dy);
	}
	constexpr int side = 2 * HOLLOW_REACH + 1;
	return sum / (side * side) - height.At(x, y);
}

static std::pair<float, float> EncodedSlope(const DetailPlane &height, int x, int y)
{
	float sx = (height.At(x + 1, y) - height.At(x - 1, y)) * 0.5f * SLOPE_ENCODING;
	float sy = (height.At(x, y + 1) - height.At(x, y - 1)) * 0.5f * SLOPE_ENCODING;
	return {0.5f + 0.5f * sx, 0.5f + 0.5f * sy};
}

struct DetailTexels {
	std::vector<uint32_t> grain;
	std::vector<uint32_t> relief;
};

/* The grain holds the fine and the finest ground noise, the stones' cover and the hollows between them; the relief the slopes of the fine bumps and of the stones apart. */
static DetailTexels BuildDetail()
{
	DetailPlane fine, micro, stones, bumps;
	fine.Fill([](int x, int y) { return Octaves(x, y, FINE_LATTICES, 11); });
	micro.Fill([](int x, int y) { return Octaves(x, y, MICRO_LATTICES, 23); });
	fine.Normalise();
	micro.Normalise();
	stones.Fill(StonesAt);
	bumps.Fill([&](int x, int y) { return 0.4f * fine.At(x, y) + 0.6f * micro.At(x, y); });

	DetailTexels texels;
	texels.grain.reserve(DETAIL_SIZE * DETAIL_SIZE);
	texels.relief.reserve(DETAIL_SIZE * DETAIL_SIZE);
	for (int y = 0; y < DETAIL_SIZE; y++) {
		for (int x = 0; x < DETAIL_SIZE; x++) {
			auto [bump_x, bump_y] = EncodedSlope(bumps, x, y);
			auto [stone_x, stone_y] = EncodedSlope(stones, x, y);
			float hollow = Hollow(stones, x, y) * HOLLOW_ENCODING;
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
