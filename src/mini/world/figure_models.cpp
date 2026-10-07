/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file figure_models.cpp Tiny low poly people in the poses of a walk, their shirts and trousers painted like a vehicle's paintwork. */

#include "../../stdafx.h"
#include "figure_models.h"

#include "../model/model_shapes.h"
#include "vehicle_parts.h"

#include "../../safeguards.h"

static constexpr Vec3 SIDEWAYS = {0.0, 1.0, 0.0};

static constexpr double HIP_HEIGHT = 0.05;
static constexpr double LEG_HALF = 0.0055;
static constexpr double LEG_SPREAD = 0.0075;
static constexpr double SHOULDER_HEIGHT = 0.086;
static constexpr double ARM_HALF = 0.0035;
static constexpr double ARM_LENGTH = 0.032;
static constexpr double ARM_SPREAD = 0.0175;
static constexpr Vec3 TORSO_LOW = {-0.0085, -0.014, HIP_HEIGHT - 0.004};
static constexpr Vec3 TORSO_HIGH = {0.0085, 0.014, SHOULDER_HEIGHT + 0.003};
static constexpr double NECK_HEIGHT = 0.004;
static constexpr double HEAD_HALF = 0.0075;
static constexpr double STRIDE_TURN = 0.38;
static constexpr double SWING_TURN = 0.45;

static constexpr uint32_t SHIRT = PaintworkTone(Paintwork::Primary, 1.0);
static constexpr uint32_t TROUSERS = PaintworkTone(Paintwork::Secondary, 1.0);
static constexpr uint32_t SKIN = 0xD9A57E;
static constexpr uint32_t HAIR = 0x3B2A20;

/* A limb hangs from its joint, swung forward by a turn about the axis across the body. */
static ModelMesh Limb(double half, double length, double joint_height, double spread, double swing, uint32_t tone)
{
	ModelMesh limb = Box({-half, -half, -length}, {half, half, 0.0}).Paint(tone);
	return limb.Transform(Mat4::Translation({0.0, spread, joint_height}) * Mat4::Turn(SIDEWAYS, swing));
}

/* Each leg swings one way and the arm on its side the other, so a stride shows from any side. */
static ModelMesh Figure(double stride)
{
	ModelMesh figure = Box(TORSO_LOW, TORSO_HIGH).Paint(SHIRT);
	double head_low = TORSO_HIGH.z + NECK_HEIGHT;
	figure.Append(Box({-HEAD_HALF, -HEAD_HALF, head_low}, {HEAD_HALF, HEAD_HALF, head_low + 2.0 * HEAD_HALF}).Paint(SKIN));
	figure.Append(Box({-HEAD_HALF - 0.001, -HEAD_HALF - 0.001, head_low + 1.4 * HEAD_HALF}, {HEAD_HALF * 0.4, HEAD_HALF + 0.001, head_low + 2.0 * HEAD_HALF + 0.002}).Paint(HAIR));
	for (double side : {-1.0, 1.0}) {
		figure.Append(Limb(LEG_HALF, HIP_HEIGHT, HIP_HEIGHT, side * LEG_SPREAD, side * stride * STRIDE_TURN, TROUSERS));
		figure.Append(Limb(ARM_HALF, ARM_LENGTH, SHOULDER_HEIGHT, side * ARM_SPREAD, -side * stride * SWING_TURN, SHIRT));
	}
	return figure.Facet();
}

std::vector<ModelMesh> BuildFigureModels()
{
	return {Figure(1.0), Figure(0.0), Figure(-1.0)};
}
