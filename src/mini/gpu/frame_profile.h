/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file frame_profile.h Time the mini UI's frames spend in each named section, on the GPU and the CPU, logged when the environment asks for it. */

#ifndef MINI_GPU_FRAME_PROFILE_H
#define MINI_GPU_FRAME_PROFILE_H

#include <array>
#include <chrono>
#include <cstdint>
#include <fstream>
#include <string>
#include <string_view>
#include <vector>

/** Whether a section is timed on the GPU as well as the CPU; sections run outside the GL context are timed on the CPU only. */
enum class ProfileClock : uint8_t {
	Cpu,
	Both,
};

class FrameProfile {
public:
	static constexpr const char VARIABLE[] = "OTTD_MINI_PROFILE";
	static constexpr const char LOG_NAME[] = "profile.txt";

	FrameProfile();

	bool Active() const { return this->active; }
	size_t Open(std::string_view group, std::string_view section, ProfileClock clock);
	void Close(size_t record);
	void EndFrame();
	void Report(std::string_view label);
	void Release();

private:
	using Clock = std::chrono::steady_clock;

	struct Record {
		std::string_view group;
		std::string_view section;
		uint32_t begin_query;
		uint32_t end_query;
		Clock::time_point begin;
		double cpu_ms;
	};

	struct Frame {
		std::vector<Record> records;
		std::vector<uint32_t> queries;
		size_t used_queries = 0;
	};

	/** A section's time over one frame, summed over every time it ran in it. */
	struct Tally {
		double gpu_ms = 0.0;
		double cpu_ms = 0.0;
		bool on_gpu = false;
		bool ran = false;
	};

	struct Samples {
		std::string section;
		std::vector<double> gpu_ms;
		std::vector<double> cpu_ms;
		Tally frame;
	};

	static constexpr size_t FRAMES_IN_FLIGHT = 4;
	static constexpr size_t UNLABELLED_REPORT_FRAMES = 240;

	uint32_t TakeQuery(Frame &frame);
	void Collect(Frame &frame);
	Samples &SamplesOf(std::string_view group, std::string_view section);
	void Write(std::string_view label);

	std::array<Frame, FRAMES_IN_FLIGHT> frames;
	std::vector<Samples> samples;
	std::ofstream log;
	size_t current = 0;
	size_t frame_count = 0;
	size_t reported_frames = 0;
	Clock::time_point last_frame_end;
	bool active = false;
};

/** Times what runs while it lives as one section of the frame. */
class ProfileScope {
public:
	explicit ProfileScope(std::string_view section, ProfileClock clock = ProfileClock::Both);
	ProfileScope(std::string_view group, std::string_view section, ProfileClock clock = ProfileClock::Both);
	~ProfileScope();

	ProfileScope(const ProfileScope &) = delete;
	ProfileScope &operator=(const ProfileScope &) = delete;

private:
	size_t record;
};

extern FrameProfile _frame_profile;

#endif /* MINI_GPU_FRAME_PROFILE_H */
