/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file lattice_noise.h Value noise, smooth between the hashed values at the points of a lattice. */

#ifndef MINI_CORE_LATTICE_NOISE_H
#define MINI_CORE_LATTICE_NOISE_H

#include <cmath>
#include <cstdint>

#include "seed.h"

/* The values the hash gives the four lattice points about a point, blended along a smooth curve between them in the precision given. */
template <class Real, class Hash>
Real LatticeBlend(Real u, Real v, Hash hash)
{
	Real cell_u = std::floor(u);
	Real cell_v = std::floor(v);
	Real fu = u - cell_u;
	Real fv = v - cell_v;
	Real su = fu * fu * (Real(3) - Real(2) * fu);
	Real sv = fv * fv * (Real(3) - Real(2) * fv);
	int cu = static_cast<int>(cell_u);
	int cv = static_cast<int>(cell_v);
	Real north = std::lerp(hash(cu, cv), hash(cu + 1, cv), su);
	Real south = std::lerp(hash(cu, cv + 1), hash(cu + 1, cv + 1), su);
	return std::lerp(north, south, sv);
}

/* Value noise over the map, smooth between the hashed points of a lattice this many to a tile. */
inline double LatticeNoise(double x, double y, double frequency, uint32_t salt)
{
	return LatticeBlend(x * frequency, y * frequency, [salt](int cu, int cv) {
		return SeedShare(Hash32(salt ^ Hash32(static_cast<uint32_t>(cu) ^ Hash32(static_cast<uint32_t>(cv)))), 0, SeedDice::SHARE_BITS);
	});
}

#endif /* MINI_CORE_LATTICE_NOISE_H */
