/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file signal_models.cpp Railway signals as low poly models: colour light signals and semaphores, each showing stop or clear. */

#include "../../stdafx.h"
#include "signal_models.h"

#include <numbers>

#include "../model/model_shapes.h"

#include "../../safeguards.h"

static constexpr Vec3 ACROSS = {1.0, 0.0, 0.0};
static constexpr double LAMP_GLOW = 1.0;
static constexpr double PAINT_GLOSS = 0.35;

static constexpr uint32_t FOOTING = 0x9B988F;
static constexpr uint32_t MAST = 0x5A5E63;
static constexpr uint32_t HEAD = 0x1E2023;
static constexpr uint32_t STOP_LIT = 0xFF3A2E;
static constexpr uint32_t STOP_DARK = 0x4A1714;
static constexpr uint32_t CLEAR_LIT = 0x3CFF7A;
static constexpr uint32_t CLEAR_DARK = 0x123A1E;
static constexpr uint32_t SEMAPHORE_POST = 0xD9D6CE;
static constexpr uint32_t ARM_RED = 0xC8352B;
static constexpr uint32_t ARM_STRIPE = 0xF0EEE8;

static constexpr Vec3 FOOTING_LOW = {-0.025, -0.025, -0.02};
static constexpr Vec3 FOOTING_HIGH = {0.025, 0.025, 0.02};
static constexpr double MAST_RADIUS = 0.011;
static constexpr double LIGHT_MAST_HEIGHT = 0.3;
static constexpr Vec3 HEAD_LOW = {-0.012, -0.026, 0.2};
static constexpr Vec3 HEAD_HIGH = {0.02, 0.026, 0.33};
static constexpr double LAMP_FRONT = 0.021;
static constexpr double LAMP_HALF = 0.014;
static constexpr double STOP_LAMP_Z = 0.3;
static constexpr double CLEAR_LAMP_Z = 0.235;

static constexpr double SEMAPHORE_HEIGHT = 0.4;
static constexpr double PIVOT_Z = 0.36;
static constexpr double ARM_LENGTH = 0.12;
static constexpr double ARM_HALF_WIDTH = 0.011;
static constexpr double ARM_THICKNESS = 0.006;
static constexpr double STRIPE_START = 0.075;
static constexpr double STRIPE_END = 0.09;
static constexpr double CLEAR_ANGLE = std::numbers::pi / 4.0;
static constexpr double SEMAPHORE_LAMP_HALF = 0.009;

static ModelMesh Lamp(double z, double half, uint32_t tone, bool lit)
{
	ModelMesh lamp = Box({LAMP_FRONT - 0.004, -half, z - half}, {LAMP_FRONT, half, z + half}).Paint(tone);
	return lit ? lamp.Glow(LAMP_GLOW) : lamp.Gloss(PAINT_GLOSS);
}

static ModelMesh Footing()
{
	return Box(FOOTING_LOW, FOOTING_HIGH).Paint(FOOTING);
}

static ModelMesh LightSignal(SignalState state)
{
	bool clear = state == SIGNAL_STATE_GREEN;
	ModelMesh model = Footing();
	model.Append(Prism(6, MAST_RADIUS, LIGHT_MAST_HEIGHT).Paint(MAST).Gloss(PAINT_GLOSS));
	model.Append(Box(HEAD_LOW, HEAD_HIGH).Paint(HEAD).Gloss(PAINT_GLOSS));
	model.Append(Lamp(STOP_LAMP_Z, LAMP_HALF, clear ? STOP_DARK : STOP_LIT, !clear));
	model.Append(Lamp(CLEAR_LAMP_Z, LAMP_HALF, clear ? CLEAR_LIT : CLEAR_DARK, clear));
	return model;
}

/* The arm swings about its pivot: level for stop, raised for clear. */
static ModelMesh SemaphoreArm(SignalState state)
{
	ModelMesh arm = Box({0.0, -ARM_LENGTH, -ARM_HALF_WIDTH}, {ARM_THICKNESS, 0.0, ARM_HALF_WIDTH}).Paint(ARM_RED).Gloss(PAINT_GLOSS);
	arm.Append(Box({ARM_THICKNESS, -STRIPE_END, -ARM_HALF_WIDTH}, {ARM_THICKNESS + 0.001, -STRIPE_START, ARM_HALF_WIDTH}).Paint(ARM_STRIPE));
	double raised = state == SIGNAL_STATE_GREEN ? CLEAR_ANGLE : 0.0;
	return arm.Transform(Mat4::Translation({MAST_RADIUS, 0.0, PIVOT_Z}) * Mat4::Turn(ACROSS, -raised));
}

static ModelMesh Semaphore(SignalState state)
{
	bool clear = state == SIGNAL_STATE_GREEN;
	ModelMesh model = Footing();
	model.Append(Prism(6, MAST_RADIUS, SEMAPHORE_HEIGHT).Paint(SEMAPHORE_POST));
	model.Append(SemaphoreArm(state));
	model.Append(Lamp(PIVOT_Z, SEMAPHORE_LAMP_HALF, clear ? CLEAR_LIT : STOP_LIT, true));
	return model;
}

std::vector<ModelMesh> BuildSignalModels()
{
	std::vector<ModelMesh> models(SIGNAL_MODELS);
	for (SignalState state : {SIGNAL_STATE_RED, SIGNAL_STATE_GREEN}) {
		models[SignalModelIndex(SIG_ELECTRIC, state)] = LightSignal(state);
		models[SignalModelIndex(SIG_SEMAPHORE, state)] = Semaphore(state);
	}
	return models;
}
