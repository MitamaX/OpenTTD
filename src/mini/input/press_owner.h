/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file press_owner.h Which layer a mouse press belongs to. */

#ifndef MINI_INPUT_PRESS_OWNER_H
#define MINI_INPUT_PRESS_OWNER_H

enum class PressSide : uint8_t {
	None,
	Native,
	Mini,
	Map,
};

/* Whoever the press started on keeps the mouse until every button is up.
 * Re-deciding each frame would press the widgets a dragged mini window is
 * carried across. */
class PressOwner {
public:
	PressSide Held();
	void Claim(PressSide side);

private:
	PressSide side = PressSide::None;
};

#endif /* MINI_INPUT_PRESS_OWNER_H */
