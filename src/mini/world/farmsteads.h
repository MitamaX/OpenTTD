/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file farmsteads.h The countryside's own life away from the towns: farmsteads in their meadows, hay bales on the stubble, fences and stone walls along the pastures. */

#ifndef MINI_WORLD_FARMSTEADS_H
#define MINI_WORLD_FARMSTEADS_H

#include "block_scatter.h"
#include "farm_models.h"

/* Only open land with nothing else on it is dressed, each tile the same way every time, and only here and there. */
class Farmsteads final : public BlockScatter {
public:
	Farmsteads();

protected:
	void Strew(const TileSpan &tiles, ScatterCopies &copies) const override;
};

/* Fences and walls along the pastures, apart from the farmsteads as they are thin enough to show only from nearer. */
class FieldBoundaries final : public BlockScatter {
public:
	FieldBoundaries();

protected:
	void Strew(const TileSpan &tiles, ScatterCopies &copies) const override;
};

#endif /* MINI_WORLD_FARMSTEADS_H */
