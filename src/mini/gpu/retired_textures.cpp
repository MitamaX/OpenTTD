/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file retired_textures.cpp Textures let go of while the GPU may still be drawing frames that sample them, deleted once those frames are done. */

#include "../../stdafx.h"
#include "retired_textures.h"

#include "gl_api.h"
#include "gpu_frame.h"

#include "../../safeguards.h"

RetiredTextures _retired_textures;

void RetiredTextures::Retire(uint32_t name)
{
	if (name != 0) this->retired.push_back({name, this->frame});
}

void RetiredTextures::Advance()
{
	this->frame++;
	while (!this->retired.empty() && this->frame - this->retired.front().frame >= GPU_FRAMES_IN_FLIGHT) {
		glDeleteTextures(1, &this->retired.front().name);
		this->retired.pop_front();
	}
}

void RetiredTextures::Release()
{
	for (const Retired &texture : this->retired) glDeleteTextures(1, &texture.name);
	this->retired.clear();
}
