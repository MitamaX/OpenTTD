/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file grades.cpp How a share or a rating reads at a glance. */

#include "../../stdafx.h"
#include "grades.h"

#include "../../town_type.h"

#include "../../table/strings.h"

#include "../../safeguards.h"

static constexpr uint SHARE_POOR = 25;
static constexpr uint SHARE_FAIR = 50;

Tone ShareTone(uint percent)
{
	if (percent < SHARE_POOR) return Tone::Loss;
	if (percent < SHARE_FAIR) return Tone::Warn;
	return Tone::Plain;
}

Tone TownRatingTone(int rating)
{
	if (rating <= RATING_VERYPOOR) return Tone::Loss;
	if (rating <= RATING_MEDIOCRE) return Tone::Warn;
	return Tone::Plain;
}

StringID TownRatingString(int rating)
{
	if (rating > RATING_EXCELLENT) return STR_CARGO_RATING_OUTSTANDING;
	if (rating > RATING_VERYGOOD) return STR_CARGO_RATING_EXCELLENT;
	if (rating > RATING_GOOD) return STR_CARGO_RATING_VERY_GOOD;
	if (rating > RATING_MEDIOCRE) return STR_CARGO_RATING_GOOD;
	if (rating > RATING_POOR) return STR_CARGO_RATING_MEDIOCRE;
	if (rating > RATING_VERYPOOR) return STR_CARGO_RATING_POOR;
	if (rating > RATING_APPALLING) return STR_CARGO_RATING_VERY_POOR;
	return STR_CARGO_RATING_APPALLING;
}
