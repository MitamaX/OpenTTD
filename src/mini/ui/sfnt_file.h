/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file sfnt_file.h A standalone font file put together from the tables of one face. */

#ifndef MINI_UI_SFNT_FILE_H
#define MINI_UI_SFNT_FILE_H

#include <span>
#include <vector>

/* Tags are the four tag bytes read as one big-endian number. */
struct SfntTable {
	uint32_t tag;
	std::vector<uint8_t> data;
};

static constexpr size_t SFNT_HEADER_SIZE = 12;

size_t SfntDirectorySize(std::span<const uint8_t> header);
std::vector<uint32_t> SfntDirectoryTags(std::span<const uint8_t> directory);
std::vector<uint8_t> BuildSfnt(std::vector<SfntTable> tables);

#endif /* MINI_UI_SFNT_FILE_H */
