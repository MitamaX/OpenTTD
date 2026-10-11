/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file figure_models.cpp Tiny low poly people in the poses of a walk, their shirts and trousers painted like a vehicle's paintwork. */

#include "../../stdafx.h"
#include "figure_models.h"

#include <array>

#include "../model/model_shapes.h"
#include "vehicle_parts.h"

#include "../../safeguards.h"

static constexpr int BODY_SIDES = 6;
static constexpr int LIMB_SIDES = 4;

static constexpr double HIP_HEIGHT = 0.05;
static constexpr double HIP_RADIUS = 0.0062;
static constexpr double ANKLE_RADIUS = 0.0042;
static constexpr double LEG_SPREAD = 0.0075;
static constexpr double SHOULDER_HEIGHT = 0.086;
static constexpr double SHOULDER_RADIUS = 0.0038;
static constexpr double WRIST_RADIUS = 0.0027;
static constexpr double ARM_LENGTH = 0.032;
static constexpr double ARM_SPREAD = 0.0175;
static constexpr double NECK_TOP = 0.093;
static constexpr std::array<LatheRing, 4> TORSO_PROFILE = {{
	{0.0086, HIP_HEIGHT - 0.004}, {0.0094, 0.078}, {0.0084, SHOULDER_HEIGHT + 0.002}, {0.0034, NECK_TOP},
}};
static constexpr double HEAD_RADIUS = 0.0078;
static constexpr std::array<LatheRing, 4> HEAD_PROFILE = {{
	{HEAD_RADIUS * 0.6, NECK_TOP}, {HEAD_RADIUS, NECK_TOP + HEAD_RADIUS * 0.9}, {HEAD_RADIUS * 0.75, NECK_TOP + HEAD_RADIUS * 1.75}, {0.0, NECK_TOP + HEAD_RADIUS * 2.1},
}};
static constexpr double HAIR_LINE = NECK_TOP + HEAD_RADIUS * 1.3;
static constexpr double HAIR_BACK = -0.5 * HEAD_RADIUS;
static constexpr double STRIDE_TURN = 0.38;
static constexpr double SWING_TURN = 0.45;

static constexpr uint32_t SHIRT = PaintworkTone(Paintwork::Primary, 1.0);
static constexpr uint32_t TROUSERS = PaintworkTone(Paintwork::Secondary, 1.0);
static constexpr uint32_t SKIN = 0xD9A57E;
static constexpr uint32_t HAIR = 0x3B2A20;

static ModelMesh Head()
{
	return Lathe(HEAD_PROFILE, BODY_SIDES, Capped::Top).PaintBy([](const Vec3 &at) { return at.z > HAIR_LINE || at.x < HAIR_BACK ? HAIR : SKIN; });
}

/* Each leg swings one way and the arm on its side the other, so a stride shows from any side. */
static ModelMesh Figure(double stride)
{
	ModelMesh figure = Torso(TORSO_PROFILE, BODY_SIDES, Capped::Neither).Paint(SHIRT);
	figure.Append(Head());
	for (double side : {-1.0, 1.0}) {
		figure.Append(Limb(LIMB_SIDES, HIP_RADIUS, ANKLE_RADIUS, HIP_HEIGHT, {0.0, side * LEG_SPREAD, HIP_HEIGHT}, side * stride * STRIDE_TURN).Paint(TROUSERS));
		figure.Append(Limb(LIMB_SIDES, SHOULDER_RADIUS, WRIST_RADIUS, ARM_LENGTH, {0.0, side * ARM_SPREAD, SHOULDER_HEIGHT}, -side * stride * SWING_TURN).Paint(SHIRT));
	}
	return figure;
}

std::vector<ModelMesh> BuildFigureModels()
{
	std::vector<ModelMesh> models;
	for (const FigureBuild &build : FIGURE_BUILDS) {
		for (double stride : {1.0, 0.0, -1.0}) models.push_back(Figure(stride).Transform(Mat4::Scaling({build.girth, build.girth, build.height})));
	}
	return models;
}
