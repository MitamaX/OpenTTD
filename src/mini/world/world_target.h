/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file world_target.h The framebuffer the 3D world is drawn into: high range colour and depth, and a snapshot of both that surface passes read. */

#ifndef MINI_WORLD_WORLD_TARGET_H
#define MINI_WORLD_WORLD_TARGET_H

#include "../../core/geometry_type.hpp"

class WorldTarget {
public:
	bool Bind(Dimension size);
	void Snapshot() const;
	void Release();

	uint32_t Colour() const { return this->drawn.colour; }
	uint32_t Depth() const { return this->drawn.depth; }
	Dimension Size() const { return this->size; }

private:
	/* A framebuffer with its colour and depth textures. */
	struct Layer {
		uint32_t framebuffer = 0;
		uint32_t colour = 0;
		uint32_t depth = 0;

		bool Build(Dimension size);
		void Release();
	};

	bool Build(Dimension size);

	Layer drawn;
	Layer snapshot;
	Dimension size{};
};

#endif /* MINI_WORLD_WORLD_TARGET_H */
