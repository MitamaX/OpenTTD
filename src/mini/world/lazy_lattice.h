/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file lazy_lattice.h Values on a lattice of points, each worked out the first time it is asked for. */

#ifndef MINI_WORLD_LAZY_LATTICE_H
#define MINI_WORLD_LAZY_LATTICE_H

#include <optional>
#include <vector>

template <class Value>
class LazyLattice {
public:
	LazyLattice(int x0, int y0, int columns, int rows) : x0(x0), y0(y0), columns(columns), rows(rows), values(static_cast<size_t>(columns) * rows)
	{
	}

	template <class Compute>
	Value At(int x, int y, Compute compute) const
	{
		int i = x - this->x0;
		int j = y - this->y0;
		if (i < 0 || j < 0 || i >= this->columns || j >= this->rows) return compute(x, y);
		std::optional<Value> &value = this->values[static_cast<size_t>(j) * this->columns + i];
		if (!value.has_value()) value = compute(x, y);
		return *value;
	}

private:
	int x0;
	int y0;
	int columns;
	int rows;
	mutable std::vector<std::optional<Value>> values;
};

#endif /* MINI_WORLD_LAZY_LATTICE_H */
