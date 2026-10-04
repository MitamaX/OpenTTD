/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file tone.cpp The emphasis a piece of text carries, named the way the stylesheets match it. */

#include "../../stdafx.h"
#include "tone.h"

#include "../../safeguards.h"

Rml::String ToneName(Tone tone)
{
	switch (tone) {
		case Tone::Plain: return "plain";
		case Tone::Accent: return "accent";
		case Tone::Warn: return "warn";
		case Tone::Loss: return "loss";
		case Tone::Dim: return "dim";
	}
	NOT_REACHED();
}
