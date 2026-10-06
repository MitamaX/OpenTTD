/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file seed.h Stateless hashes that turn one seed into the same choices every time. */

#ifndef MINI_CORE_SEED_H
#define MINI_CORE_SEED_H

#include <array>
#include <cstdint>

constexpr uint32_t Hash32(uint32_t v)
{
	v ^= v >> 16;
	v *= 0x7FEB352DU;
	v ^= v >> 15;
	v *= 0x846CA68BU;
	v ^= v >> 16;
	return v;
}

constexpr uint32_t SubSeed(uint32_t seed, uint32_t salt)
{
	return Hash32(seed + salt);
}

constexpr uint32_t SeedBits(uint32_t seed, uint first, uint count)
{
	return (seed >> first) & ((1U << count) - 1U);
}

constexpr float SeedShare(uint32_t seed, uint first, uint count)
{
	return SeedBits(seed, first, count) / static_cast<float>((1U << count) - 1U);
}

template <typename T, size_t N>
constexpr const T &SeedPick(const std::array<T, N> &options, uint32_t bits)
{
	return options[bits % N];
}

struct SeedRange {
	double low;
	double high;
};

/* A run of choices drawn one after another from one seed, the same run every time. */
class SeedDice {
public:
	static constexpr uint SHARE_BITS = 16;

	explicit constexpr SeedDice(uint32_t seed) : seed(seed) {}

	constexpr uint32_t Next() { return SubSeed(this->seed, this->rolls++); }
	constexpr double Share() { return SeedShare(this->Next(), 0, SHARE_BITS); }
	constexpr double Between(double low, double high) { return low + (high - low) * this->Share(); }
	constexpr double Between(SeedRange range) { return this->Between(range.low, range.high); }
	constexpr uint32_t Below(uint32_t count) { return this->Next() % count; }

private:
	uint32_t seed;
	uint32_t rolls = 0;
};

#endif /* MINI_CORE_SEED_H */
