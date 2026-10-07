/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file frame_profile.cpp Time the mini UI's frames spend in each named section, on the GPU and the CPU, logged when the environment asks for it. */

#include "../../stdafx.h"
#include "frame_profile.h"

#include <algorithm>
#include <filesystem>

#include "../../3rdparty/fmt/format.h"
#include "../../string_func.h"
#include "frame_capture.h"
#include "gl_api.h"

#include "../../safeguards.h"

static constexpr std::string_view INTERVAL_SECTION = "interval";
static constexpr uint32_t NO_QUERY = 0;
static constexpr double NANOSECONDS_PER_MILLISECOND = 1e6;
static constexpr double MEDIAN_RANK = 0.5;
static constexpr double HIGH_RANK = 0.9;

FrameProfile _frame_profile;

static double Milliseconds(std::chrono::steady_clock::duration duration)
{
	return std::chrono::duration<double, std::milli>(duration).count();
}

static double Ranked(std::vector<double> values, double rank)
{
	if (values.empty()) return 0.0;
	size_t index = std::min(values.size() - 1, static_cast<size_t>(rank * static_cast<double>(values.size())));
	std::ranges::nth_element(values, values.begin() + static_cast<ptrdiff_t>(index));
	return values[index];
}

static double Highest(const std::vector<double> &values)
{
	return values.empty() ? 0.0 : std::ranges::max(values);
}

/* The log lies beside the shot list when frames are captured, else in the working folder. */
static std::filesystem::path LogPath()
{
	std::filesystem::path name = FrameProfile::LOG_NAME;
	std::optional<std::string_view> list = GetEnv(FrameCapture::SHOT_LIST_VARIABLE);
	if (!list.has_value()) return name;
	return std::filesystem::path(OTTD2FS(*list)).parent_path() / name;
}

FrameProfile::FrameProfile()
{
	this->active = GetEnv(VARIABLE) == "1";
}

uint32_t FrameProfile::TakeQuery(Frame &frame)
{
	if (frame.used_queries == frame.queries.size()) {
		uint32_t query = NO_QUERY;
		glGenQueries(1, &query);
		frame.queries.push_back(query);
	}
	uint32_t query = frame.queries[frame.used_queries++];
	glQueryCounter(query, GL_TIMESTAMP);
	return query;
}

size_t FrameProfile::Open(std::string_view group, std::string_view section, ProfileClock clock)
{
	Frame &frame = this->frames[this->current];
	uint32_t query = clock == ProfileClock::Both ? this->TakeQuery(frame) : NO_QUERY;
	frame.records.push_back({group, section, query, NO_QUERY, Clock::now(), 0.0});
	return frame.records.size() - 1;
}

void FrameProfile::Close(size_t record)
{
	Frame &frame = this->frames[this->current];
	Record &timed = frame.records[record];
	timed.cpu_ms = Milliseconds(Clock::now() - timed.begin);
	if (timed.begin_query != NO_QUERY) timed.end_query = this->TakeQuery(frame);
}

/* A frame's queries are read back only once it has had the frames in flight to finish on the GPU. */
void FrameProfile::EndFrame()
{
	Clock::time_point now = Clock::now();
	if (this->frame_count > 0) this->SamplesOf({}, INTERVAL_SECTION).cpu_ms.push_back(Milliseconds(now - this->last_frame_end));
	this->last_frame_end = now;
	this->frame_count++;

	this->current = (this->current + 1) % FRAMES_IN_FLIGHT;
	this->Collect(this->frames[this->current]);

	if (!_frame_capture.Active() && this->frame_count - this->reported_frames >= UNLABELLED_REPORT_FRAMES) {
		this->Report(fmt::format("frames {}-{}", this->reported_frames, this->frame_count));
	}
}

void FrameProfile::Collect(Frame &frame)
{
	for (const Record &record : frame.records) {
		Tally &tally = this->SamplesOf(record.group, record.section).frame;
		tally.ran = true;
		tally.cpu_ms += record.cpu_ms;
		if (record.end_query == NO_QUERY) continue;

		GLuint64 begin = 0;
		GLuint64 end = 0;
		glGetQueryObjectui64v(record.begin_query, GL_QUERY_RESULT, &begin);
		glGetQueryObjectui64v(record.end_query, GL_QUERY_RESULT, &end);
		tally.gpu_ms += static_cast<double>(end - begin) / NANOSECONDS_PER_MILLISECOND;
		tally.on_gpu = true;
	}
	for (Samples &samples : this->samples) {
		if (!samples.frame.ran) continue;
		samples.cpu_ms.push_back(samples.frame.cpu_ms);
		if (samples.frame.on_gpu) samples.gpu_ms.push_back(samples.frame.gpu_ms);
		samples.frame = {};
	}
	frame.records.clear();
	frame.used_queries = 0;
}

FrameProfile::Samples &FrameProfile::SamplesOf(std::string_view group, std::string_view section)
{
	std::string name = group.empty() ? std::string(section) : fmt::format("{}.{}", group, section);
	auto it = std::ranges::find(this->samples, name, &Samples::section);
	if (it != this->samples.end()) return *it;
	return this->samples.emplace_back(std::move(name));
}

/* Each section's samples since the last report: their median, the slowest tenth's floor and the slowest, in milliseconds. */
void FrameProfile::Report(std::string_view label)
{
	if (!this->active) return;
	this->Write(label);
	for (Samples &section : this->samples) {
		section.gpu_ms.clear();
		section.cpu_ms.clear();
	}
	this->reported_frames = this->frame_count;
}

void FrameProfile::Write(std::string_view label)
{
	if (!this->log.is_open()) this->log.open(LogPath(), std::ios::out | std::ios::trunc);

	this->log << fmt::format("# {}\n{:<20}{:>9}{:>9}{:>9}{:>9}{:>9}{:>9}\n", label, "section", "gpu_med", "gpu_p90", "gpu_max", "cpu_med", "cpu_p90", "cpu_max");
	for (const Samples &section : this->samples) {
		if (section.cpu_ms.empty()) continue;
		this->log << fmt::format("{:<20}{:>9.2f}{:>9.2f}{:>9.2f}{:>9.2f}{:>9.2f}{:>9.2f}\n", section.section,
			Ranked(section.gpu_ms, MEDIAN_RANK), Ranked(section.gpu_ms, HIGH_RANK), Highest(section.gpu_ms),
			Ranked(section.cpu_ms, MEDIAN_RANK), Ranked(section.cpu_ms, HIGH_RANK), Highest(section.cpu_ms));
	}
	this->log.flush();
}

void FrameProfile::Release()
{
	for (Frame &frame : this->frames) {
		if (!frame.queries.empty()) glDeleteQueries(static_cast<GLsizei>(frame.queries.size()), frame.queries.data());
		frame = {};
	}
}

ProfileScope::ProfileScope(std::string_view section, ProfileClock clock) : ProfileScope({}, section, clock)
{
}

ProfileScope::ProfileScope(std::string_view group, std::string_view section, ProfileClock clock) :
	record(_frame_profile.Active() ? _frame_profile.Open(group, section, clock) : 0)
{
}

ProfileScope::~ProfileScope()
{
	if (_frame_profile.Active()) _frame_profile.Close(this->record);
}
