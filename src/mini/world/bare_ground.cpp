/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file bare_ground.cpp Where the land wears through to bare stone, read alike by the ground's shader and by the stones standing on the ground. */

#include "../../stdafx.h"
#include "bare_ground.h"

#include <algorithm>

#include "../../settings_type.h"

#include "../../safeguards.h"

double SteepShare(double slope)
{
	return SmoothStep(STANDARD_SLOPE, STEEPEST_SLOPE, slope);
}

double AltitudeShare(double level)
{
	return std::clamp(level / std::max(static_cast<double>(_world_tiles.Peak()), MIN_RELIEF_SPAN), 0.0, 1.0);
}

double BlanketDepth(const GroundTexel &ground)
{
	LandscapeType landscape = _settings_game.game_creation.landscape;
	bool snowed = landscape == LandscapeType::Arctic && ground.material == GroundMaterial::Snow;
	bool sanded = landscape == LandscapeType::Tropic && ground.material == GroundMaterial::Desert;
	return snowed || sanded ? ((ground.detail & GROUND_DENSITY_MASK) + 1.0) / (GROUND_DENSITY_MASK + 1.0) : 0.0;
}

double BareShare(double steep, double altitude, double blanket)
{
	return steep * (STEEP_BARING + blanket * SHED_BARING) + SmoothStep(ALPINE_ALTITUDE, 1.0, altitude) * ALPINE_BARING;
}

double ExposedShare(double bare, double threshold)
{
	return SmoothStep(threshold - EXPOSURE_BLEND, threshold + EXPOSURE_BLEND, bare);
}

double BeddingOf(const MapVector &at, double level)
{
	return level * STRATA_PER_LEVEL + Dot(at, STRATA_DIP);
}
