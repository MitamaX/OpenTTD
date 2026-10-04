/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file scene_watch.cpp Notices when a stylesheet, document, icon or scene file is saved. */

#include "scene_watch.h"

#include <algorithm>

static constexpr std::chrono::milliseconds POLL_INTERVAL{400};

static std::filesystem::file_time_type WriteTime(const std::filesystem::path &file)
{
	std::error_code error;
	std::filesystem::file_time_type time = std::filesystem::last_write_time(file, error);
	return error ? std::filesystem::file_time_type::min() : time;
}

SceneWatch::SceneWatch(std::vector<std::filesystem::path> paths) : paths(std::move(paths)), seen(this->Latest())
{
}

bool SceneWatch::Changed()
{
	auto now = std::chrono::steady_clock::now();
	if (now < this->next_poll) return false;
	this->next_poll = now + POLL_INTERVAL;

	std::filesystem::file_time_type latest = this->Latest();
	if (latest == this->seen) return false;
	this->seen = latest;
	return true;
}

std::filesystem::file_time_type SceneWatch::Latest() const
{
	std::filesystem::file_time_type latest = std::filesystem::file_time_type::min();
	for (const std::filesystem::path &path : this->paths) {
		latest = std::max(latest, WriteTime(path));
		if (!std::filesystem::is_directory(path)) continue;

		std::error_code error;
		for (const auto &entry : std::filesystem::recursive_directory_iterator(path, error)) latest = std::max(latest, WriteTime(entry.path()));
	}
	return latest;
}
