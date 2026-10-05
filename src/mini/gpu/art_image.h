/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file art_image.h Picture files from the player's art folder, decoded to RGBA bytes at the size a slot asks for. */

#ifndef MINI_GPU_ART_IMAGE_H
#define MINI_GPU_ART_IMAGE_H

#include <span>
#include <string>

bool LoadArtImage(const std::string &path, std::span<uint32_t> rgba, int width, int height);

#endif /* MINI_GPU_ART_IMAGE_H */
