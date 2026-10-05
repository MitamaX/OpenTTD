/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file pointer_router.h Which layer a mouse event belongs to. */

#ifndef MINI_INPUT_POINTER_ROUTER_H
#define MINI_INPUT_POINTER_ROUTER_H

#include <optional>

/* What RmlUi finds under the pointer: a native slot passes it on to the
 * official window it shows, the map lies at the foot of the HUD, and every
 * other element belongs to a panel or the HUD. */
enum class PointerLayer : uint8_t {
	Native,
	Panel,
	Map,
};

/* Whoever a press started on keeps the mouse until every button is up, so a
 * native drag never reaches RmlUi and the map acts on no press it did not start. */
class PointerRouter {
public:
	PointerLayer Route(PointerLayer under);
	PointerLayer Current() const { return this->current; }
	bool OnMap() const;

private:
	std::optional<PointerLayer> owner;
	PointerLayer current = PointerLayer::Map;
};

extern PointerRouter _pointer;

#endif /* MINI_INPUT_POINTER_ROUTER_H */
