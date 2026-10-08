/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file build_slice.h Blocks' meshes rebuilt a little at a time: the slice of a frame spent on them, and a build under way to replace a block's meshes. */

#ifndef MINI_WORLD_BUILD_SLICE_H
#define MINI_WORLD_BUILD_SLICE_H

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <vector>

/* The wall-clock time from its start a frame spends building, past which a build left unfinished goes on in the next frame. */
class BuildSlice {
public:
	explicit BuildSlice(std::chrono::microseconds length = LENGTH) : deadline(Clock::now() + length) {}

	bool Spent() const { return Clock::now() >= this->deadline; }

	/* Takes a build on as far as the slice allows, saying whether it is done. */
	template <class Build>
	bool Carry(Build &build) const
	{
		while (!build.Done()) {
			if (this->Spent()) return false;
			build.Advance();
		}
		return true;
	}

private:
	using Clock = std::chrono::steady_clock;
	static constexpr std::chrono::microseconds LENGTH{1500};

	Clock::time_point deadline;
};

/* A build under way to replace a block's meshes, and whether the world changed under it since it began. */
template <class Build>
struct Rebuild {
	Build build;
	bool outdated = false;
};

/* The blocks of a field with a build under way, so those no view has wanted for long drop their builds without every block being looked over. */
class BuildsUnderWay {
public:
	void Clear() { this->blocks.clear(); }

	void Begin(size_t index)
	{
		if (std::ranges::find(this->blocks, index) == this->blocks.end()) this->blocks.push_back(index);
	}

	/* A block's build is dropped once the block has gone unwanted for the given frames. */
	template <class Chunk>
	void Abandon(std::vector<Chunk> &chunks, uint64_t frame, uint64_t unwanted_frames)
	{
		std::erase_if(this->blocks, [&](size_t index) {
			Chunk &chunk = chunks[index];
			if (!chunk.rebuild.has_value()) return true;
			if (frame - chunk.wanted < unwanted_frames) return false;
			chunk.rebuild.reset();
			chunk.due_since = 0;
			return true;
		});
	}

private:
	std::vector<size_t> blocks;
};

#endif /* MINI_WORLD_BUILD_SLICE_H */
