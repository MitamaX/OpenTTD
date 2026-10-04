/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file scene_watch.h Notices when a stylesheet, document, icon or scene file is saved. */

#ifndef MINI_PREVIEW_SCENE_WATCH_H
#define MINI_PREVIEW_SCENE_WATCH_H

#include <chrono>
#include <filesystem>
#include <vector>

class SceneWatch {
public:
	explicit SceneWatch(std::vector<std::filesystem::path> paths);

	bool Changed();

private:
	std::filesystem::file_time_type Latest() const;

	const std::vector<std::filesystem::path> paths;
	std::filesystem::file_time_type seen;
	std::chrono::steady_clock::time_point next_poll;
};

#endif /* MINI_PREVIEW_SCENE_WATCH_H */
