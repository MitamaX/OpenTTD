/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file note_layer.cpp Text floating over the map: the pause banner and the note beside the cursor. */

#include "../../stdafx.h"
#include "note_layer.h"

#include <iterator>

#include <RmlUi/Core.h>

#include "../../bridge.h"
#include "../../company_base.h"
#include "../../company_func.h"
#include "../../core/format.hpp"
#include "../../gfx_func.h"
#include "../../industrytype.h"
#include "../../newgrf_airport.h"
#include "../../openttd.h"
#include "../../rail.h"
#include "../../road.h"
#include "../tools/build_tool.h"
#include "../tools/clear_filter.h"
#include "../tools/tool_choices.h"
#include "../tools/tool_estimate.h"
#include "../ui/tone.h"
#include "../ui/ui_text.h"
#include "build_catalog.h"

#include "../../table/strings.h"

#include "../../safeguards.h"

static constexpr const char TOOL_NOTE_ID[] = "tool-note";
static constexpr float CURSOR_GAP_X_DP = 9.0f;
static constexpr float CURSOR_GAP_Y_DP = 11.0f;
static constexpr float EDGE_GAP_DP = 4.0f;

static std::string_view ToolHint(MiniTool kind)
{
	switch (kind) {
		case MiniTool::Rail: return "DRAG PATH / CTRL REMOVE / RMB CANCEL";
		case MiniTool::Convert:
		case MiniTool::RoadConvert: return "DRAG AREA / RMB CANCEL";
		case MiniTool::Road: return "DRAG LINE / CTRL REMOVE / RMB CANCEL";
		case MiniTool::Station: return "DRAG AREA / Q E TURN / CTRL REMOVE / RMB CANCEL";
		case MiniTool::BusStop:
		case MiniTool::TruckStop: return "CLICK ROAD / Q E TURN / CTRL REMOVE / RMB CANCEL";
		case MiniTool::RailWaypoint: return "CLICK TRACK / CTRL REMOVE / RMB CANCEL";
		case MiniTool::RoadWaypoint: return "CLICK ROAD / CTRL REMOVE / RMB CANCEL";
		case MiniTool::TrainDepot:
		case MiniTool::RoadDepot: return "Q E ROTATE EXIT / CTRL REMOVE / RMB CANCEL";
		case MiniTool::ShipDepot: return "Q E ROTATE / CTRL REMOVE / RMB CANCEL";
		case MiniTool::Dock: return "CLICK SHORE SLOPE / CTRL REMOVE / RMB CANCEL";
		case MiniTool::Buoy: return "CLICK WATER / CTRL REMOVE / RMB CANCEL";
		case MiniTool::Airport: return "CLICK SITE / CTRL REMOVE / RMB CANCEL";
		case MiniTool::Canal: return "DRAG AREA / CTRL REMOVE / RMB CANCEL";
		case MiniTool::Lock: return "CLICK SLOPE / CTRL REMOVE / RMB CANCEL";
		case MiniTool::Demolish: return "DRAG AREA / RMB CANCEL";
		case MiniTool::Signal: return "DRAG TRACK / CTRL REMOVE / RMB CANCEL";
		case MiniTool::RailTunnel:
		case MiniTool::RoadTunnel: return "CLICK SLOPE / CTRL REMOVE / RMB CANCEL";
		case MiniTool::RailBridge:
		case MiniTool::RoadBridge: return "DRAG SPAN / RMB CANCEL";
		case MiniTool::Terraform: return "DRAG LEVEL / CLICK RAISE / CTRL LOWER / RMB CANCEL";
		case MiniTool::Headquarters: return "CLICK 2x2 SPOT / CTRL REMOVE / RMB CANCEL";
		case MiniTool::Trees: return "DRAG AREA / CTRL CLEAR / RMB CANCEL";
		case MiniTool::BuyLand: return "DRAG AREA / CTRL SELL / RMB CANCEL";
		case MiniTool::Industry: return "CLICK SITE / CTRL REMOVE / RMB CANCEL";
		case MiniTool::Sign: return "CLICK SPOT / CTRL REMOVE / RMB CANCEL";
		default: return {};
	}
}

class TitleBuilder {
public:
	explicit TitleBuilder(std::string title) : title(std::move(title)) {}

	template <typename... Args>
	void Add(fmt::format_string<Args...> format, Args &&... args)
	{
		this->title += "  ";
		fmt::format_to(std::back_inserter(this->title), format, std::forward<Args>(args)...);
	}

	std::string Take() { return std::move(this->title); }

private:
	std::string title;
};

static void AddChoices(TitleBuilder &title, MiniTool kind)
{
	if (kind == MiniTool::Airport) {
		const AirportSpec *as = AirportSpec::Get(_choices.Airport());
		if (as->IsAvailable()) title.Add("{}", GameText(as->name));
	}
	if (kind == MiniTool::Rail || kind == MiniTool::Convert) title.Add("{}", GameText(GetRailTypeInfo(_choices.Rail())->strings.name));
	if (IsRoadTool(kind)) title.Add("{}", GameText(GetRoadTypeInfo(_choices.Road())->strings.name));
	if (IsRoadStopTool(kind)) title.Add("{}", _choices.StopThrough() ? "통과" : "만입");
	if (kind == MiniTool::Signal) title.Add("{}", SignalTypeLabel(_choices.Signal()));
	if (kind == MiniTool::Demolish) title.Add("{}", _clear_filter.Label());
	if (_estimate.TunnelEnd() != INVALID_TILE) title.Add("{}칸", _estimate.TunnelLength());
	if (IsBridgeTool(kind)) title.Add("{}", GameText(GetBridgeSpec(_choices.Bridge(std::max(_tool.Plans().line.BridgeLength(), 1U)))->material));
	if (kind == MiniTool::Industry) {
		IndustryType it = _choices.Industry();
		if (it != IT_INVALID) {
			const IndustrySpec *indsp = GetIndustrySpec(it);
			title.Add("{}  {}", GameText(indsp->name), GameText(STR_JUST_CURRENCY_LONG, indsp->GetConstructionCost()));
		}
	}
}

static void AddDragSize(TitleBuilder &title, MiniTool kind)
{
	if (!_tool.Dragging()) return;

	const ToolPlans &plans = _tool.Plans();
	if (kind == MiniTool::Rail && !plans.rail.pieces.empty()) title.Add("{}", plans.rail.pieces.size());
	if (kind == MiniTool::Road && !plans.line.tiles.empty()) title.Add("{}", plans.line.tiles.size());
	if (IsBridgeTool(kind)) title.Add("{}", plans.line.BridgeLength());
	if (kind == MiniTool::Signal && !plans.signal.tiles.empty()) title.Add("{}", plans.signal.tiles.size());
	if (IsRectTool(kind) && plans.area.valid) title.Add("{}x{}", plans.area.Width(), plans.area.Height());
	if (kind == MiniTool::Station && plans.area.valid && !_tool.Removing()) {
		int w = plans.area.Width();
		int h = plans.area.Height();
		bool along_x = _choices.StationAxis() == AXIS_X;
		title.Add("{}선 {}칸", along_x ? h : w, along_x ? w : h);
	}
}

NoteLayer::NoteLayer() : HudPart("notes")
{
}

void NoteLayer::Bind(Rml::DataModelConstructor &model)
{
	this->Expose(model, "paused", &this->paused);
	this->Expose(model, "pause_text", &this->pause_text);
	this->Expose(model, "tool_shown", &this->tool_shown);
	this->Expose(model, "title", &this->title);
	this->Expose(model, "hint", &this->hint);
	this->Expose(model, "cost", &this->cost);
	this->Expose(model, "cost_tone", &this->cost_tone);
}

void NoteLayer::Collect()
{
	this->paused = _pause_mode.Any();
	this->pause_text = GameText(STR_STATUSBAR_PAUSED);

	MiniTool kind = _tool.Kind();
	this->tool_shown = kind != MiniTool::None && _cursor.in_window;
	if (!this->tool_shown) return;

	TitleBuilder title(ToolLabel(kind));
	AddChoices(title, kind);
	AddDragSize(title, kind);
	this->title = title.Take();
	this->hint = ToolHint(kind);
	this->CollectCost();
	this->FollowCursor();
}

/* What the plan under the cursor would charge, red once the balance cannot
 * cover it. */
void NoteLayer::CollectCost()
{
	this->cost.clear();
	this->cost_tone = ToneName(Tone::Plain);
	if (!_estimate.Priced()) return;

	Money price = _estimate.Cost();
	bool income = price < 0;
	this->cost = GameText(income ? STR_MESSAGE_ESTIMATED_INCOME : STR_MESSAGE_ESTIMATED_COST, income ? -price : price);
	const Company *c = Company::GetIfValid(_local_company);
	if (!income && c != nullptr && c->money < price) this->cost_tone = ToneName(Tone::Loss);
}

/* The note keeps clear of the cursor and is pushed back inside the screen
 * by its size from the last layout. */
void NoteLayer::FollowCursor()
{
	Rml::Element *root = this->Root();
	Rml::Element *note = root == nullptr ? nullptr : root->GetElementById(TOOL_NOTE_ID);
	if (note == nullptr) return;

	float dp = root->GetContext()->GetDensityIndependentPixelRatio();
	Rml::Vector2f bounds = root->GetBox().GetSize(Rml::BoxArea::Border);
	Rml::Vector2f size = note->GetBox().GetSize(Rml::BoxArea::Border);
	float x = std::min(_cursor.pos.x + CURSOR_GAP_X_DP * dp, bounds.x - size.x - EDGE_GAP_DP * dp);
	float y = std::min(_cursor.pos.y + CURSOR_GAP_Y_DP * dp, bounds.y - size.y - EDGE_GAP_DP * dp);
	note->SetProperty(Rml::PropertyId::Left, Rml::Property(x, Rml::Unit::PX));
	note->SetProperty(Rml::PropertyId::Top, Rml::Property(y, Rml::Unit::PX));
}
