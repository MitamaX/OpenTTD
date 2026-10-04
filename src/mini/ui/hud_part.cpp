/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file hud_part.cpp A region of the HUD document, bound to a data model named after it. */

#include "../../stdafx.h"
#include "hud_part.h"

#include "../../safeguards.h"

HudPart::HudPart(Rml::String region) : region(std::move(region))
{
}

bool HudPart::Attach(Rml::Context &context)
{
	return this->CreateModel(context, this->region);
}
