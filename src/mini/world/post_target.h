/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file post_target.h A colour texture with a framebuffer of its own, which one step of the world's finishing draws into and the next reads. */

#ifndef MINI_WORLD_POST_TARGET_H
#define MINI_WORLD_POST_TARGET_H

#include "../../core/geometry_type.hpp"
#include "world_target.h"

class PostTarget {
public:
	PostTarget(int format, TargetSampling filter) : format(format), filter(filter) {}

	bool Fit(Dimension size);
	void Bind() const;
	void BindTexture(uint unit) const;
	void Release();

	Dimension Size() const { return this->size; }

private:
	int format;
	TargetSampling filter;
	uint32_t framebuffer = 0;
	uint32_t texture = 0;
	Dimension size{};
};

#endif /* MINI_WORLD_POST_TARGET_H */
