/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file rockfall.h Stones strewn over rocky ground, laid out block by block from the map's ground and shown only near the eye. */

#ifndef MINI_WORLD_ROCKFALL_H
#define MINI_WORLD_ROCKFALL_H

#include "block_scatter.h"

inline constexpr double ROCK_SHOWN_PIXELS = 28.0;
inline constexpr double ROCK_SOLID_PIXELS = 38.0;
inline constexpr double ROCK_CAST_PIXELS = 48.0;
inline constexpr double ROCK_CASTER_WIDTH = 0.08;
inline constexpr int ROCK_BLOCK_MARGIN = 1;

class Rockfall final : public BlockScatter {
public:
	Rockfall();

protected:
	void Strew(const TileSpan &tiles, ScatterCopies &copies) const override;
};

#endif /* MINI_WORLD_ROCKFALL_H */
