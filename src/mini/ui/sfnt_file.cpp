/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file sfnt_file.cpp A standalone font file put together from the tables of one face. */

#include "../../stdafx.h"
#include "sfnt_file.h"

#include "../../core/math_func.hpp"

#include "../../safeguards.h"

static constexpr size_t TABLE_COUNT_OFFSET = 4;
static constexpr size_t TABLE_RECORD_SIZE = 16;
static constexpr uint TABLE_ALIGNMENT = 4;
static constexpr size_t WORD = 4;
static constexpr size_t HALF_WORD = 2;
static constexpr uint32_t TRUETYPE_OUTLINES = 0x00010000;
static constexpr uint32_t CFF_OUTLINES = 0x4F54544F;
static constexpr uint32_t CFF_TAG = 0x43464620;
static constexpr uint32_t CFF2_TAG = 0x43464632;

static uint32_t ReadBigEndian(std::span<const uint8_t> bytes, size_t offset, size_t width)
{
	uint32_t value = 0;
	for (uint8_t byte : bytes.subspan(offset, width)) value = value << 8 | byte;
	return value;
}

static void WriteBigEndian(std::vector<uint8_t> &out, size_t value, size_t width)
{
	for (size_t shift = width * 8; shift > 0; shift -= 8) out.push_back(static_cast<uint8_t>(value >> (shift - 8)));
}

/* A table sums as big-endian words, its last word padded with zeros. */
static uint32_t Checksum(std::span<const uint8_t> data)
{
	uint32_t sum = 0;
	for (size_t offset = 0; offset < data.size(); offset += WORD) {
		size_t width = std::min(WORD, data.size() - offset);
		sum += ReadBigEndian(data, offset, width) << (8 * (WORD - width));
	}
	return sum;
}

size_t SfntDirectorySize(std::span<const uint8_t> header)
{
	if (header.size() < SFNT_HEADER_SIZE) return 0;
	return SFNT_HEADER_SIZE + ReadBigEndian(header, TABLE_COUNT_OFFSET, HALF_WORD) * TABLE_RECORD_SIZE;
}

/* Each record of the directory starts with its table's tag. */
std::vector<uint32_t> SfntDirectoryTags(std::span<const uint8_t> directory)
{
	size_t size = SfntDirectorySize(directory);
	if (size == 0 || directory.size() < size) return {};

	std::vector<uint32_t> tags;
	for (size_t record = SFNT_HEADER_SIZE; record < size; record += TABLE_RECORD_SIZE) tags.push_back(ReadBigEndian(directory, record, WORD));
	return tags;
}

/* The directory lists the tables by tag, and each table starts on a word boundary after it. */
std::vector<uint8_t> BuildSfnt(std::vector<SfntTable> tables)
{
	if (tables.empty()) return {};
	std::ranges::sort(tables, {}, &SfntTable::tag);

	bool cff = std::ranges::any_of(tables, [](const SfntTable &table) { return table.tag == CFF_TAG || table.tag == CFF2_TAG; });
	size_t count = tables.size();
	size_t selector = std::bit_width(count) - 1;
	size_t search_range = TABLE_RECORD_SIZE << selector;

	std::vector<uint8_t> file;
	WriteBigEndian(file, cff ? CFF_OUTLINES : TRUETYPE_OUTLINES, WORD);
	WriteBigEndian(file, count, HALF_WORD);
	WriteBigEndian(file, search_range, HALF_WORD);
	WriteBigEndian(file, selector, HALF_WORD);
	WriteBigEndian(file, count * TABLE_RECORD_SIZE - search_range, HALF_WORD);

	size_t offset = SFNT_HEADER_SIZE + count * TABLE_RECORD_SIZE;
	for (const SfntTable &table : tables) {
		WriteBigEndian(file, table.tag, WORD);
		WriteBigEndian(file, Checksum(table.data), WORD);
		WriteBigEndian(file, offset, WORD);
		WriteBigEndian(file, table.data.size(), WORD);
		offset += Align(table.data.size(), TABLE_ALIGNMENT);
	}
	for (const SfntTable &table : tables) {
		file.insert(file.end(), table.data.begin(), table.data.end());
		file.resize(Align(file.size(), TABLE_ALIGNMENT));
	}
	return file;
}
