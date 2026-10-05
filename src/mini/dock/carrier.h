/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file carrier.h Frameless native viewports that paint under a mini window's view slot. */

#ifndef MINI_DOCK_CARRIER_H
#define MINI_DOCK_CARRIER_H

#include <variant>

#include "../../tile_type.h"
#include "../../vehicle_type.h"
#include "../../window_type.h"

struct Window;

using CarrierFocus = std::variant<TileIndex, VehicleID>;

enum class CarrierSubject : uint8_t {
	Vehicle,
	Station,
	Town,
	Industry,
};

WindowNumber CarrierNumber(int kind, int id);
bool IsCarrier(const Window *w);
Window *FindCarrier(WindowNumber num);
Window *OpenCarrier(WindowNumber num, CarrierFocus focus);
void FitCarrier(Window *w, int x, int y, int width, int height);
void CloseCarrier(WindowNumber num);

#endif /* MINI_DOCK_CARRIER_H */
