/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file chunk_keep.h How much of what was built for its blocks a field keeps once they are out of every view, and which blocks give theirs back first. */

#ifndef MINI_WORLD_CHUNK_KEEP_H
#define MINI_WORLD_CHUNK_KEEP_H

#include <algorithm>
#include <cstddef>
#include <functional>
#include <utility>
#include <vector>

/* A block out of every view keeps what was built for it, so a view coming back finds it built, while all its field holds stays within the bytes it may keep;
 * past that, the blocks longest out of view give theirs back first. A block some view wanted in the frame before is always kept. */
class ChunkKeep {
public:
	/* Bytes says how much a block holds, seen the last frame some view wanted it, and drop has it give back all it holds. */
	template <class Chunk, class Bytes, class Seen, class Drop>
	void Trim(std::vector<Chunk> &chunks, uint64_t frame, Bytes bytes, Seen seen, Drop drop)
	{
		size_t held = 0;
		for (const Chunk &chunk : chunks) held += std::invoke(bytes, chunk);
		if (held <= MOST_KEPT_BYTES) return;

		this->idle.clear();
		for (size_t index = 0; index < chunks.size(); index++) {
			const Chunk &chunk = chunks[index];
			if (std::invoke(bytes, chunk) > 0 && std::invoke(seen, chunk) + 1 < frame) this->idle.emplace_back(std::invoke(seen, chunk), index);
		}
		std::ranges::sort(this->idle);
		for (const auto &[since, index] : this->idle) {
			if (held <= MOST_KEPT_BYTES) return;
			held -= std::invoke(bytes, chunks[index]);
			std::invoke(drop, chunks[index]);
		}
	}

private:
	static constexpr size_t MOST_KEPT_BYTES = 96 << 20;

	std::vector<std::pair<uint64_t, size_t>> idle;
};

#endif /* MINI_WORLD_CHUNK_KEEP_H */
