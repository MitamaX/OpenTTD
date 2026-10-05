/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file fonts.h Font files every mini UI layer renders with. */

#ifndef MINI_UI_FONTS_H
#define MINI_UI_FONTS_H

#include <cstdlib>
#include <filesystem>
#include <string>
#include <vector>

using FontCandidates = std::vector<std::filesystem::path>;

/* Hangul needs a CJK face, and every platform keeps one somewhere else: the
 * regular face first, then the bold one. */
inline std::vector<FontCandidates> MiniFontFaces()
{
#if defined(_WIN32)
	const char *windows = std::getenv("WINDIR");
	if (windows == nullptr) return {};
	std::filesystem::path fonts = std::filesystem::path(windows) / "Fonts";
	return {{fonts / "malgun.ttf"}, {fonts / "malgunbd.ttf"}};
#elif defined(__APPLE__)
	return {{"/System/Library/Fonts/AppleSDGothicNeo.ttc"}};
#else
	return {
		{"/usr/share/fonts/opentype/noto/NotoSansCJK-Regular.ttc", "/usr/share/fonts/noto-cjk/NotoSansCJK-Regular.ttc", "/usr/share/fonts/google-noto-cjk/NotoSansCJK-Regular.ttc"},
		{"/usr/share/fonts/opentype/noto/NotoSansCJK-Bold.ttc", "/usr/share/fonts/noto-cjk/NotoSansCJK-Bold.ttc", "/usr/share/fonts/google-noto-cjk/NotoSansCJK-Bold.ttc"},
	};
#endif
}

/* Each face is the first of its candidates that is installed here. */
inline std::vector<std::string> MiniFontFiles()
{
	std::vector<std::string> files;
	for (const FontCandidates &face : MiniFontFaces()) {
		for (const std::filesystem::path &file : face) {
			std::error_code error;
			if (!std::filesystem::exists(file, error)) continue;
			files.push_back(file.string());
			break;
		}
	}
	return files;
}

#endif /* MINI_UI_FONTS_H */
