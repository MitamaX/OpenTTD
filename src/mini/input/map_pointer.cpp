/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file map_pointer.cpp The map at the foot of the HUD: what a click, a drag and the wheel do there. */

#include "../../stdafx.h"
#include "map_pointer.h"

#include <RmlUi/Core.h>

#include "../../gfx_func.h"
#include "../core/camera.h"
#include "../tools/build_tool.h"
#include "input_mode.h"
#include "pointer_router.h"

#include "../../safeguards.h"

static constexpr int LEFT_BUTTON = 0;
static constexpr int RIGHT_BUTTON = 1;
static constexpr int MIDDLE_BUTTON = 2;

static Point EventPoint(const Rml::Event &event)
{
	return {event.GetParameter<int>("mouse_x", 0), event.GetParameter<int>("mouse_y", 0)};
}

/* RmlUi hands the map whatever lands on it, but a press that began elsewhere keeps the pointer there. */
static bool MapHoldsPointer()
{
	return _pointer.Current() == PointerLayer::Map;
}

MapPointer::MapPointer(Inspector inspect) : HudPart("map"), inspect(std::move(inspect))
{
}

void MapPointer::Bind(Rml::DataModelConstructor &model)
{
	model.BindEventCallback("press", &MapPointer::Press, this);
	model.BindEventCallback("release", &MapPointer::Release, this);
	model.BindEventCallback("move", &MapPointer::Move, this);
	model.BindEventCallback("zoom", &MapPointer::Zoom, this);
}

/* The map binds no values; it only listens. */
void MapPointer::Collect()
{
}

void MapPointer::Press(Rml::DataModelHandle, Rml::Event &event, const Rml::VariantList &)
{
	if (!MapHoldsPointer()) return;

	Point at = EventPoint(event);
	switch (event.GetParameter<int>("button", LEFT_BUTTON)) {
		case LEFT_BUTTON: this->Click(at, event.GetParameter<int>("ctrl_key", 0) != 0); break;
		case RIGHT_BUTTON: _mode.Unwind(); break;
		case MIDDLE_BUTTON: this->grab = at; break;
		default: break;
	}
}

/* A build drag ends wherever the button comes up; off the map RmlUi reports it as the end of the map's drag, which names no button. */
void MapPointer::Release(Rml::DataModelHandle, Rml::Event &event, const Rml::VariantList &)
{
	switch (event.GetParameter<int>("button", LEFT_BUTTON)) {
		case LEFT_BUTTON: if (MapHoldsPointer()) _tool.Release(); break;
		case MIDDLE_BUTTON: this->grab.reset(); break;
		default: break;
	}
}

/* The grabbed point follows the pointer. Over a panel the map waits, and catches up once the pointer is back. */
void MapPointer::Move(Rml::DataModelHandle, Rml::Event &event, const Rml::VariantList &)
{
	if (!this->grab.has_value()) return;
	if (!_middle_button_down) {
		this->grab.reset();
		return;
	}

	Point at = EventPoint(event);
	_camera.Drag(at.x - this->grab->x, at.y - this->grab->y);
	this->grab = at;
}

/* The wheel only zooms the map: RmlUi must neither scroll with it nor start its autoscroll on a middle press. */
void MapPointer::Zoom(Rml::DataModelHandle, Rml::Event &event, const Rml::VariantList &)
{
	event.StopPropagation();

	float turn = event.GetParameter<float>("wheel_delta_y", 0.0f);
	if (turn == 0.0f || !MapHoldsPointer()) return;

	bool in = turn < 0.0f;
	if (_mode.Following()) {
		/* While following, zooming keeps the vehicle centred instead of anchoring the cursor point. */
		_camera.Zoom(in);
	} else {
		Point at = EventPoint(event);
		_camera.ZoomAt(at.x, at.y, in);
	}
}

void MapPointer::Click(Point at, bool ctrl) const
{
	TilePoint tile = _camera.MapAt(at.x, at.y);
	if (_tool.Kind() != MiniTool::None) {
		_tool.Press(tile, ctrl);
	} else if (_mode.PickingOrders()) {
		_mode.PickOrderAt(tile);
	} else {
		this->inspect(at);
	}
}
