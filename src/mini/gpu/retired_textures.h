/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file retired_textures.h Textures let go of while the GPU may still be drawing frames that sample them, deleted once those frames are done. */

#ifndef MINI_GPU_RETIRED_TEXTURES_H
#define MINI_GPU_RETIRED_TEXTURES_H

#include <cstdint>
#include <deque>

/* Deleting a texture a frame in flight still samples makes the driver wait for the GPU to draw every frame queued. */
class RetiredTextures {
public:
	void Retire(uint32_t name);
	void Advance();
	void Release();

private:
	struct Retired {
		uint32_t name;
		uint64_t frame;
	};

	std::deque<Retired> retired;
	uint64_t frame = 0;
};

extern RetiredTextures _retired_textures;

#endif /* MINI_GPU_RETIRED_TEXTURES_H */
