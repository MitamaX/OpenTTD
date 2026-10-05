/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file game_fonts.cpp The font the game's own font search settled on, read out for RmlUi. */

#include "../../stdafx.h"
#include "game_fonts.h"

#include "../../fileio_func.h"
#include "../../fontcache.h"
#include "sfnt_file.h"

#if defined(_WIN32)
#	include "../../core/bitmath_func.hpp"
#	include <windows.h>
#elif defined(WITH_COCOA)
#	include "../../os/macosx/macos.h"
#elif defined(WITH_FREETYPE)
#	include "../../core/format.hpp"
#	include "../../os/unix/font_unix.h"
#	include <ft2build.h>
#	include FT_FREETYPE_H
#	include FT_TRUETYPE_TABLES_H
#endif

#include "../../safeguards.h"

using Rml::Style::FontWeight;

static constexpr size_t MAX_FONT_FILE = 64 * 1024 * 1024;

#if defined(_WIN32)

/* GDI names a table by its tag bytes in file order; tag zero reads the selected face's own directory, even inside a collection. */
static std::vector<uint8_t> GdiRead(HDC dc, uint32_t tag, size_t length)
{
	std::vector<uint8_t> data(length);
	if (length == 0 || GetFontData(dc, std::byteswap(tag), 0, data.data(), static_cast<DWORD>(length)) != length) data.clear();
	return data;
}

static size_t GdiTableSize(HDC dc, uint32_t tag)
{
	DWORD size = GetFontData(dc, std::byteswap(tag), 0, nullptr, 0);
	return size == GDI_ERROR ? 0 : size;
}

static std::vector<SfntTable> GdiTables(HDC dc)
{
	std::vector<uint8_t> directory = GdiRead(dc, 0, SfntDirectorySize(GdiRead(dc, 0, SFNT_HEADER_SIZE)));
	std::vector<SfntTable> tables;
	for (uint32_t tag : SfntDirectoryTags(directory)) tables.push_back({tag, GdiRead(dc, tag, GdiTableSize(dc, tag))});
	return tables;
}

static std::vector<uint8_t> GdiFontFile(const LOGFONT &logfont)
{
	HDC dc = CreateCompatibleDC(nullptr);
	HFONT font = CreateFontIndirect(&logfont);
	HGDIOBJ previous = SelectObject(dc, font);
	std::vector<uint8_t> file = BuildSfnt(GdiTables(dc));
	SelectObject(dc, previous);
	DeleteObject(font);
	DeleteDC(dc);
	return file;
}

/* The game's font cache holds a GDI logical font; asking for it bold picks the family's bold face. */
static std::vector<FontFace> PlatformFaces(const void *handle)
{
	LOGFONT regular = *static_cast<const LOGFONT *>(handle);
	LOGFONT bold = regular;
	bold.lfWeight = FW_BOLD;
	return {{GdiFontFile(regular), FontWeight::Normal}, {GdiFontFile(bold), FontWeight::Bold}};
}

#elif defined(WITH_COCOA)

static std::vector<SfntTable> CoreTextTables(CTFontRef font)
{
	std::vector<SfntTable> tables;
	CFAutoRelease<CFArrayRef> tags(CTFontCopyAvailableTables(font, kCTFontTableOptionNoOptions));
	if (!tags) return tables;

	for (CFIndex i = 0; i < CFArrayGetCount(tags.get()); i++) {
		/* The array holds the tags themselves, not objects wrapping them. */
		CTFontTableTag tag = static_cast<CTFontTableTag>(reinterpret_cast<uintptr_t>(CFArrayGetValueAtIndex(tags.get(), i)));
		CFAutoRelease<CFDataRef> data(CTFontCopyTable(font, tag, kCTFontTableOptionNoOptions));
		if (!data) continue;
		const UInt8 *bytes = CFDataGetBytePtr(data.get());
		tables.push_back({tag, {bytes, bytes + CFDataGetLength(data.get())}});
	}
	return tables;
}

/* The game's font cache holds a CoreText font; the family's bold face is a trait away, when there is one. */
static std::vector<FontFace> PlatformFaces(const void *handle)
{
	CTFontRef regular = static_cast<CTFontRef>(handle);
	std::vector<FontFace> faces{{BuildSfnt(CoreTextTables(regular)), FontWeight::Normal}};
	CFAutoRelease<CTFontRef> bold(CTFontCreateCopyWithSymbolicTraits(regular, 0.0, nullptr, kCTFontTraitBold, kCTFontTraitBold));
	if (bold) faces.push_back({BuildSfnt(CoreTextTables(bold.get())), FontWeight::Bold});
	return faces;
}

#elif defined(WITH_FREETYPE)

static std::vector<SfntTable> FreeTypeTables(FT_Face face)
{
	std::vector<SfntTable> tables;
	FT_ULong tag;
	FT_ULong length;
	for (FT_UInt index = 0; FT_Sfnt_Table_Info(face, index, &tag, &length) == FT_Err_Ok; index++) {
		std::vector<uint8_t> data(length);
		if (FT_Load_Sfnt_Table(face, tag, 0, data.data(), &length) == FT_Err_Ok) tables.push_back({static_cast<uint32_t>(tag), std::move(data)});
	}
	return tables;
}

/* The game's font cache holds a FreeType face; fontconfig finds the family's bold face beside it. */
static std::vector<FontFace> PlatformFaces(const void *handle)
{
	FT_Face regular = *static_cast<const FT_Face *>(handle);
	std::vector<FontFace> faces{{BuildSfnt(FreeTypeTables(regular)), FontWeight::Normal}};
#ifdef WITH_FONTCONFIG
	FT_Face bold = nullptr;
	if (GetFontByFaceName(fmt::format("{}, Bold", regular->family_name), &bold) == FT_Err_Ok) {
		faces.push_back({BuildSfnt(FreeTypeTables(bold)), FontWeight::Bold});
		FT_Done_Face(bold);
	}
#endif /* WITH_FONTCONFIG */
	return faces;
}

#else

static std::vector<FontFace> PlatformFaces(const void *)
{
	return {};
}

#endif

/* A game that keeps to its sprite font has no TrueType face; the one it would have loaded instead stands in. */
static std::vector<FontFace> DefaultFaces()
{
	size_t length = 0;
	std::unique_ptr<char[]> file = ReadFileToMem(GetDefaultTruetypeFontFile(FS_NORMAL), length, MAX_FONT_FILE);
	if (file == nullptr) return {};

	const uint8_t *bytes = reinterpret_cast<const uint8_t *>(file.get());
	return {{{bytes, bytes + length}, FontWeight::Normal}};
}

/* The normal size font is whatever the player configured, or what the game found to cover its language. */
std::vector<FontFace> GameFontFaces()
{
	FontCache *font = FontCache::Get(FS_NORMAL);
	std::vector<FontFace> faces;
	if (font != nullptr && !font->IsBuiltInFont()) faces = PlatformFaces(font->GetOSHandle());
	std::erase_if(faces, [](const FontFace &face) { return face.data.empty(); });
	return faces.empty() ? DefaultFaces() : faces;
}
