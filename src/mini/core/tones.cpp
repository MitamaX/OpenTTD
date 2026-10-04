/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file tones.cpp The colours the mini UI paints the map with, as 0xAARRGGBB. */

#include "../../stdafx.h"
#include "tones.h"

#include "../../cargotype.h"
#include "../../gfx_func.h"

#include "../../safeguards.h"

uint32_t PaletteRgb(PixelColour p)
{
	Colour c = _cur_palette.palette[p.p];
	return 0xFF000000U | (static_cast<uint32_t>(c.r) << 16) | (static_cast<uint32_t>(c.g) << 8) | c.b;
}

uint32_t CargoRgb(CargoType ct)
{
	return PaletteRgb(CargoSpec::Get(ct)->legend_colour);
}
