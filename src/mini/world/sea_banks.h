/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file sea_banks.h Earth banked up out of the sea against tiles standing above the shore, rounding down from their tops into the seabed. */

#ifndef MINI_WORLD_SEA_BANKS_H
#define MINI_WORLD_SEA_BANKS_H

#include <array>
#include <optional>
#include <vector>

#include "../core/camera.h"
#include "seabed.h"

inline constexpr int SEA_BANK_REACH = 2;

struct Brink {
	MapVector from;
	MapVector to;
	MapVector outward;
	std::array<double, 2> top;
	std::array<double, 2> rise;
	std::array<double, 2> cap;

	double Lift(const MapVector &at, double ground) const;
};

struct Segment {
	MapVector from;
	MapVector to;

	double Distance(const MapVector &at) const { return SegmentDistance(at, this->from, this->to); }
};

class BankSite {
public:
	BankSite() = default;
	BankSite(std::vector<Brink> brinks, std::vector<Segment> pins);

	double Lift(double x, double y, double ground) const;
	bool Empty() const { return this->brinks.empty(); }

private:
	std::vector<Brink> brinks;
	std::vector<Segment> pins;
};

class SeaBanks {
public:
	explicit SeaBanks(const TileSpan &tiles) : tiles(tiles) {}

	BankSite Around(const Seabed &bed, int tx, int ty) const;

private:
	struct Gathered {
		std::vector<Brink> brinks;
		std::vector<Segment> pins;
	};

	const Gathered &Gather(const Seabed &bed) const;

	TileSpan tiles;
	mutable std::optional<Gathered> gathered;
};

#endif /* MINI_WORLD_SEA_BANKS_H */
