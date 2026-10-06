/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file system_pointer.h The game's cursor handed to the operating system, so the pointer keeps pace with the hand however slowly frames come. */

#ifndef MINI_CORE_SYSTEM_POINTER_H
#define MINI_CORE_SYSTEM_POINTER_H

#include <atomic>
#include <vector>

#include "../../gfx_type.h"

struct CursorPicture {
	std::vector<Colour> pixels;
	int width = 0;
	int height = 0;
	Point hotspot{};

	Colour &At(int x, int y) { return this->pixels[static_cast<size_t>(y) * this->width + x]; }
	const Colour &At(int x, int y) const { return this->pixels[static_cast<size_t>(y) * this->width + x]; }
};

class SystemPointer {
public:
	virtual ~SystemPointer() = default;

	void Refresh();
	void Forget();
	void Reset();
	bool Draws() const;

protected:
	virtual bool Adopt(const CursorPicture &picture) = 0;
	virtual void Release() = 0;
	virtual void Show(bool shown) = 0;

private:
	std::vector<CursorSprite> sprites;
	std::atomic<bool> stale = true;
	bool adopted = false;
	bool shown = false;

	void Rebuild();
};

#endif /* MINI_CORE_SYSTEM_POINTER_H */
