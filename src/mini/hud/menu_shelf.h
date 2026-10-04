/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file menu_shelf.h Which category of a tile bar has its shelf of tiles pulled out. */

#ifndef MINI_HUD_MENU_SHELF_H
#define MINI_HUD_MENU_SHELF_H

class MenuShelf {
public:
	static constexpr int CLOSED = -1;

	int Open() const { return this->open; }
	bool IsOpen() const { return this->open != CLOSED; }
	void Close() { this->open = CLOSED; }

	bool Toggle(int index)
	{
		this->open = this->open == index ? CLOSED : index;
		return this->IsOpen();
	}

private:
	int open = CLOSED;
};

#endif /* MINI_HUD_MENU_SHELF_H */
