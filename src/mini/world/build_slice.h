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

/* A frame's building, from when the frame opens: the interval it is drawn in and how long the slices of every field have run in it so far. */
class BuildFrame {
public:
	using Clock = std::chrono::steady_clock;

	void Open(Clock::duration interval)
	{
		this->opened = Clock::now();
		this->interval = interval;
		this->spent = {};
	}

private:
	friend class BuildSlice;

	Clock::time_point opened = Clock::now();
	Clock::duration interval{};
	Clock::duration spent{};
};

extern BuildFrame _build_frame;

/* The wall-clock time from its start a frame spends building, past which a build left unfinished goes on in the next frame.
 * A field with blocks in sight still waiting on their builds hurries: its slice runs longer, as long as the frame's building and the frame itself leave room. */
class BuildSlice {
public:
	explicit BuildSlice(std::chrono::microseconds length = LENGTH) : start(Clock::now()), deadline(this->start + length) {}
	explicit BuildSlice(bool hurried) : BuildSlice(hurried ? HurriedLength() : LENGTH) {}
	~BuildSlice() { _build_frame.spent += Clock::now() - this->start; }
	BuildSlice(const BuildSlice &) = delete;
	BuildSlice &operator=(const BuildSlice &) = delete;

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
	using Clock = BuildFrame::Clock;
	static constexpr std::chrono::microseconds LENGTH{1500};
	static constexpr std::chrono::microseconds HURRIED_LENGTH{4000};
	static constexpr std::chrono::microseconds HURRIED_FRAME_BUILDING{6000};
	static constexpr int HURRIED_FRAME_SHARE_PERCENT = 50;

	static std::chrono::microseconds HurriedLength()
	{
		using std::chrono::duration_cast;
		const BuildFrame &frame = _build_frame;
		Clock::duration building_left = HURRIED_FRAME_BUILDING - frame.spent;
		Clock::duration frame_left = frame.opened + frame.interval * HURRIED_FRAME_SHARE_PERCENT / 100 - Clock::now();
		return std::clamp(duration_cast<std::chrono::microseconds>(std::min(building_left, frame_left)), LENGTH, HURRIED_LENGTH);
	}

	Clock::time_point start;
	Clock::time_point deadline;
};

/* A build under way to replace a block's meshes, and whether the world changed under it since it began. */
template <class Build>
struct Rebuild {
	Build build;
	bool outdated = false;
};

/* A block waits on its build from the frame it was found out of date in, counted afresh as it comes into sight or goes out of it,
 * so the blocks a cut or a turn brings into sight are built nearest first instead of after those that waited out of sight. */
template <class Chunk>
void MarkDue(Chunk &chunk, uint64_t frame, bool in_sight)
{
	if (chunk.due_since != 0 && chunk.due_in_sight == in_sight) return;
	chunk.due_since = frame;
	chunk.due_in_sight = in_sight;
}

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
