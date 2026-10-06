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

#endif /* MINI_CORE_SEED_H */
