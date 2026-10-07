/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file ground_cover.h Tufts of grass scattered over open meadow and rough land and scrub over the desert, laid out block by block from the map's ground and shown only near the eye. */

#ifndef MINI_WORLD_GROUND_COVER_H
#define MINI_WORLD_GROUND_COVER_H

#include "block_scatter.h"
#include "tuft_models.h"

/* Each tuft is gathered in the bucket of its shape. */
class GroundCover final : public BlockScatter {
public:
	GroundCover();

protected:
	void Strew(const TileSpan &tiles, ScatterCopies &copies) const override;
};

#endif /* MINI_WORLD_GROUND_COVER_H */
