/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file car_model.cpp The car parked about town, at a kerb or in a yard, built of the parts road vehicles are. */

#include "../../stdafx.h"
#include "car_model.h"

#include <algorithm>
#include <array>
#include <cmath>

#include "../map/car_look.h"
#include "vehicle_parts.h"

#include "../../safeguards.h"

static constexpr Vec3 X_AXIS = {1.0, 0.0, 0.0};
static constexpr double HALF_LENGTH = CAR_LENGTH * 0.5;
static constexpr double HALF_WIDTH = CAR_WIDTH * 0.5;
static constexpr double BODY_TOP = CAR_CLEARANCE + CAR_BODY_HEIGHT;
static constexpr HullProfile BODY = {HALF_WIDTH, CAR_CLEARANCE, BODY_TOP, 0.01};
static constexpr BodyEnd NOSE = RoundedEnd(0.022, 0.8, 0.7);
static constexpr BodyEnd TAIL = RoundedEnd(0.012, 0.86, 0.9);
static constexpr HullProfile CABIN = {HALF_WIDTH - CAB_INSET, BODY_TOP, BODY_TOP + CAB_HEIGHT, 0.008};
static constexpr double CABIN_LENGTH = CAR_LENGTH * CAB_SHARE;
static constexpr double CABIN_SHIFT = 0.014;
static constexpr double CABIN_BACK = -CABIN_LENGTH * 0.5 - CABIN_SHIFT;
static constexpr double CABIN_FRONT = CABIN_LENGTH * 0.5 - CABIN_SHIFT;
static constexpr BodyEnd WINDSCREEN = RoundedEnd(0.028, 0.88, 0.12);
static constexpr BodyEnd BACKLIGHT = RoundedEnd(0.018, 0.9, 0.35);
static constexpr Band WINDOWS = {BODY_TOP + 0.003, CABIN.high - CABIN.bevel};
static constexpr double WHEEL_RADIUS = 0.0125;
static constexpr double WHEEL_WIDTH = 0.008;
static constexpr double HALF_TRACK = HalfTrackUnder(HALF_WIDTH, WHEEL_WIDTH);
static constexpr double AXLE_SPAN = (HALF_TRACK + WHEEL_WIDTH * 0.5) * 2.0;
static constexpr std::array<double, 2> AXLES = {-0.052, 0.054};

/* A low bonnet and boot either side of a glazed cabin over a drum of tyres at each axle, its paintwork keyed for each copy to take a paint of its own. */
static ModelMesh Car()
{
	VehicleKit kit(VehicleDetail::Full);
	BodyRun body(BODY, -HALF_LENGTH, HALF_LENGTH, TAIL, NOSE, SIMPLE_SHELL_STEPS);
	BodyRun cabin(CABIN, CABIN_BACK, CABIN_FRONT, BACKLIGHT, WINDSCREEN, SIMPLE_SHELL_STEPS);
	for (const BodyRun *shell : {&body, &cabin}) kit.Body(shell->Shell().Paint(PRIMARY).Gloss(PAINT_GLOSS));
	kit.Body(cabin.Stripe(WINDOWS, GLASS).Gloss(GLASS_GLOSS));
	for (double axle : AXLES) kit.Body(Wheel(axle, 0.0, WHEEL_RADIUS, AXLE_SPAN));
	ModelMesh car = kit.Done();
	return car.Occlude(GROUND_OPENNESS);
}

ModelMesh CarModel(uint32_t paint, double kerb)
{
	static const ModelMesh CAR = Car();
	double lean = std::asin(std::clamp(kerb / (2.0 * HALF_TRACK), -1.0, 1.0));
	ModelMesh car = CAR;
	return car.Repaint(PRIMARY, paint).Transform(Mat4::Translation({0.0, 0.0, HALF_TRACK * std::sin(lean)}) * Mat4::Turn(X_AXIS, lean));
}
