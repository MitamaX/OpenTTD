/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file system_pointer.cpp The game's cursor handed to the operating system, so the pointer keeps pace with the hand however slowly frames come. */

#include "../../stdafx.h"
#include "system_pointer.h"

#include <algorithm>
#include <iterator>
#include <span>

#include "../../core/bitmath_func.hpp"
#include "../../gfx_func.h"
#include "../../palette_func.h"
#include "../../spritecache.h"
#include "../../spriteloader/spriteloader.hpp"
#include "../../zoom_type.h"
#include "../../table/sprites.h"

#include "../../safeguards.h"

/* Colours resolve the way the OpenGL sprite shader resolves them, so both pointers look alike. */
static Colour ResolvePixel(const SpriteLoader::CommonPixel &pixel, bool has_rgba, const uint8_t *remap)
{
	if (pixel.m == 0) return has_rgba ? Colour(pixel.r, pixel.g, pixel.b, pixel.a) : Colour{};

	uint8_t index = remap == nullptr ? pixel.m : remap[pixel.m];
	Colour colour = _cur_palette.palette[index];
	if (!has_rgba) return colour;

	uint8_t brightness = std::max({pixel.r, pixel.g, pixel.b});
	if (brightness != 0) colour = AdjustBrightness(colour, brightness);
	if (index != 0) colour.a = pixel.a;
	return colour;
}

static Colour Over(Colour top, Colour under)
{
	if (top.a == UINT8_MAX || under.a == 0) return top;
	if (top.a == 0) return under;

	uint under_weight = under.a * (UINT8_MAX - top.a) / UINT8_MAX;
	uint alpha = top.a + under_weight;
	auto mix = [&](uint top_channel, uint under_channel) {
		return static_cast<uint8_t>((top_channel * top.a + under_channel * under_weight) / alpha);
	};
	return Colour(mix(top.r, under.r), mix(top.g, under.g), mix(top.b, under.b), static_cast<uint8_t>(alpha));
}

class CursorSpriteReader : public SpriteEncoder {
public:
	CursorPicture picture;

	explicit CursorSpriteReader(PaletteID pal) : pal(pal) {}

	bool Is32BppSupported() override { return true; }

	Sprite *Encode(SpriteType, const SpriteLoader::SpriteCollection &sprite, SpriteAllocator &) override
	{
		const SpriteLoader::Sprite &level = sprite[_gui_zoom];
		bool has_rgba = level.colours != SpriteComponent::Palette;
		const uint8_t *remap = this->pal == PAL_NONE ? nullptr : GetNonSprite(GB(this->pal, 0, PALETTE_WIDTH), SpriteType::Recolour) + 1;

		this->picture = {{}, level.width, level.height, {-level.x_offs, -level.y_offs}};
		std::span<const SpriteLoader::CommonPixel> source(level.data, static_cast<size_t>(level.width) * level.height);
		std::ranges::transform(source, std::back_inserter(this->picture.pixels), [&](const SpriteLoader::CommonPixel &pixel) {
			return ResolvePixel(pixel, has_rgba, remap);
		});
		return nullptr;
	}

private:
	PaletteID pal;
};

static CursorPicture ReadCursorSprite(const CursorSprite &cursor_sprite)
{
	CursorSpriteReader reader(cursor_sprite.image.pal);
	UniquePtrSpriteAllocator allocator;
	GetRawSprite(cursor_sprite.image.sprite, SpriteType::Normal, &allocator, &reader);

	reader.picture.hotspot.x -= cursor_sprite.pos.x;
	reader.picture.hotspot.y -= cursor_sprite.pos.y;
	return std::move(reader.picture);
}

/* Every platform wants the hotspot inside the picture, so the pointer's own pixel always belongs to it. */
static CursorPicture Flatten(std::span<const CursorSprite> sprites)
{
	if (sprites.empty()) return {};

	std::vector<CursorPicture> layers;
	std::ranges::transform(sprites, std::back_inserter(layers), ReadCursorSprite);

	int left = 0;
	int top = 0;
	int right = 1;
	int bottom = 1;
	for (const CursorPicture &layer : layers) {
		left = std::min(left, -layer.hotspot.x);
		top = std::min(top, -layer.hotspot.y);
		right = std::max(right, layer.width - layer.hotspot.x);
		bottom = std::max(bottom, layer.height - layer.hotspot.y);
	}

	CursorPicture picture{std::vector<Colour>(static_cast<size_t>(right - left) * (bottom - top)), right - left, bottom - top, {-left, -top}};
	for (const CursorPicture &layer : layers) {
		int shift_x = picture.hotspot.x - layer.hotspot.x;
		int shift_y = picture.hotspot.y - layer.hotspot.y;
		for (int y = 0; y < layer.height; y++) {
			for (int x = 0; x < layer.width; x++) {
				Colour &under = picture.At(shift_x + x, shift_y + y);
				under = Over(layer.At(x, y), under);
			}
		}
	}
	return picture;
}

static bool SameSprites(std::span<const CursorSprite> a, std::span<const CursorSprite> b)
{
	return std::ranges::equal(a, b, [](const CursorSprite &x, const CursorSprite &y) {
		return x.image == y.image && x.pos.x == y.pos.x && x.pos.y == y.pos.y;
	});
}

void SystemPointer::Refresh()
{
	bool rebuilt = this->stale.exchange(false) || !SameSprites(this->sprites, _cursor.sprites);
	if (rebuilt) this->Rebuild();

	bool wanted = this->Draws();
	if (wanted == this->shown && !(wanted && rebuilt)) return;

	this->shown = wanted;
	this->Show(wanted);
}

void SystemPointer::Forget()
{
	this->stale = true;
}

void SystemPointer::Reset()
{
	this->Release();
	this->adopted = false;
	this->shown = false;
	this->stale = true;
}

/* A pinned pointer would twitch between the system's move and the game's warp back, so the game draws it then. */
bool SystemPointer::Draws() const
{
	return this->adopted && !_cursor.fix_at;
}

void SystemPointer::Rebuild()
{
	this->sprites = _cursor.sprites;
	CursorPicture picture = Flatten(this->sprites);
	this->adopted = !picture.pixels.empty() && this->Adopt(picture);
	if (!this->adopted) this->Release();
}
