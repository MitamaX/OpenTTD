/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file status_stream.cpp The problem rows hanging below the colony panel. */

#include "../../stdafx.h"
#include "status_stream.h"

#include <RmlUi/Core.h>

#include "../../core/format.hpp"
#include "../../mini_ui.h"
#include "../../vehicle_base.h"
#include "../../vehicle_gui.h"
#include "../ui/tone.h"
#include "../ui/ui_text.h"
#include "status_board.h"

#include "../../table/strings.h"

#include "../../safeguards.h"

static constexpr size_t NAMES_SHOWN = 8;
static constexpr std::chrono::milliseconds SCAN_INTERVAL{250};

static Rml::Vector<Rml::String> VehicleNames(const std::vector<VehicleID> &list)
{
	Rml::Vector<Rml::String> names;
	for (size_t i = 0; i < list.size() && i < NAMES_SHOWN; i++) {
		if (Vehicle::IsValidID(list[i])) names.push_back(GameText(STR_VEHICLE_NAME, list[i]));
	}
	if (list.size() > names.size()) names.push_back(fmt::format("+{}", list.size() - names.size()));
	return names;
}

StatusStream::StatusStream() : HudPart("status"), scan_beat(SCAN_INTERVAL)
{
}

void StatusStream::Bind(Rml::DataModelConstructor &model)
{
	if (Rml::StructHandle<StatusRow> row = model.RegisterStruct<StatusRow>()) {
		row.RegisterMember("status", &StatusRow::status);
		row.RegisterMember("text", &StatusRow::text);
		row.RegisterMember("tone", &StatusRow::tone);
		row.RegisterMember("names", &StatusRow::names);
	}
	model.RegisterArray<Rml::Vector<StatusRow>>();
	this->Expose(model, "rows", &this->rows);
	model.BindEventCallback("cycle", &StatusStream::Cycle, this);
}

void StatusStream::Collect()
{
	if (!this->scan_beat.Due()) return;
	_status_board.Scan();
	this->rows.clear();
	for (int i = 0; i < to_underlying(VehicleStatus::End); i++) {
		VehicleStatus status = static_cast<VehicleStatus>(i);
		const std::vector<VehicleID> &list = _status_board.Vehicles(status);
		if (list.empty()) continue;
		this->rows.push_back({i, fmt::format("{} ({})", StatusLabel(status), list.size()), ToneName(StatusIsCritical(status) ? Tone::Loss : Tone::Warn), VehicleNames(list)});
	}
}

void StatusStream::Cycle(Rml::DataModelHandle, Rml::Event &, const Rml::VariantList &arguments)
{
	int index = ArgumentIndex(arguments);
	if (index < 0 || index >= to_underlying(VehicleStatus::End)) return;

	const Vehicle *v = _status_board.Next(static_cast<VehicleStatus>(index));
	if (v == nullptr) return;
	ShowVehicleViewWindow(v->First());
	MiniUiScrollTo(v->x_pos, v->y_pos);
}
