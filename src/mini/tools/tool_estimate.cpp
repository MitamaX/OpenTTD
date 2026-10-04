/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file tool_estimate.cpp What the plan in hand would cost, and which of its tiles the game would refuse. */

#include "../../stdafx.h"
#include "tool_estimate.h"

#include <algorithm>

#include "../../company_base.h"
#include "../../company_func.h"
#include "../../gfx_func.h"
#include "../../map_func.h"
#include "../../tunnelbridge.h"
#include "../core/tones.h"
#include "build_tool.h"
#include "clear_filter.h"
#include "command_probe.h"
#include "tile_pick.h"
#include "tool_choices.h"
#include "tool_commit.h"

#include "../../safeguards.h"

/* Long drags are not worth a command per tile, so past this the plan only
 * carries its whole-plan verdict. */
static constexpr size_t MINI_FIT_MAX = 1024;

ToolEstimate _estimate;

/* The industry tool prices itself from the spec and signs are free, so neither
 * is worth a probe. */
static bool WantsEstimate()
{
	MiniTool kind = _tool.Kind();
	if (kind == MiniTool::None || kind == MiniTool::Industry || kind == MiniTool::Sign) return false;
	if (!IsClickTool(kind) && !_tool.Dragging()) return false;
	return Company::IsValidID(_local_company);
}

/* A filtered clear asks the game once per tile. That is fine for the one run
 * the player asked for, but a probe repeats it while the drag moves, so past
 * the fit limit the price is left unanswered. */
static bool ClearTooWide()
{
	const AreaPlan &area = _tool.Plans().area;
	return _tool.FiltersClear() && area.valid && area.Tiles() > MINI_FIT_MAX;
}

template <typename Plan>
static std::vector<bool> ProbeRunFit(const Plan &plan, bool remove)
{
	if (plan.Size() == 0 || plan.Size() > MINI_FIT_MAX) return {};

	std::vector<bool> fit(plan.Size(), false);
	CommandProbe probe;
	for (const PlanRun &run : plan.Runs(remove)) {
		if (run.bridge) {
			/* A bridge stands or falls in one piece, so its span shares one answer. */
			PostRun(plan, run, remove);
			std::fill(fit.begin() + run.a, fit.begin() + run.b + 1, probe.TakeVerdict());
			continue;
		}
		for (size_t k = run.a; k <= run.b; k++) {
			PostRun(plan, {k, k, false}, remove);
			fit[k] = probe.TakeVerdict();
		}
	}
	return fit;
}

/* Area commands build what they can and skip the rest, so asking once for the
 * whole drag hides exactly the holes the player wants to see coming. Each tile
 * gets its own question instead. */
static std::vector<bool> ProbeAreaFit(const AreaPlan &area, bool remove)
{
	if (!area.valid || area.Tiles() > MINI_FIT_MAX) return {};

	MiniTool kind = _tool.Kind();
	bool filtered = _tool.FiltersClear();
	std::vector<bool> fit(area.Tiles(), false);
	CommandProbe probe;
	area.ForEach([&](int tx, int ty) {
		TileIndex tile = TileXY(tx, ty);
		size_t i = area.Index(tx, ty);
		if (filtered) {
			fit[i] = _clear_filter.Post(tile) && probe.TakeVerdict();
			return;
		}
		PostArea(kind, tile, tile, remove);
		fit[i] = probe.TakeVerdict();
	});
	return fit;
}

void ToolEstimate::Update()
{
	if (!WantsEstimate() || !_cursor.in_window) {
		this->Clear();
		return;
	}

	uint64_t key = this->Key();
	if (key == this->key) return;
	this->key = key;
	this->ProbeCost();
	this->ProbeFit();
}

bool ToolEstimate::Fits(size_t i) const
{
	return i >= this->fit.size() || this->fit[i];
}

/* Nothing per tile to say, so the whole blueprint takes the verdict. */
uint32_t ToolEstimate::PlanColour(uint32_t c) const
{
	return (this->probed && !this->ok && !this->unknown && this->fit.empty()) ? COL_BP_NO : c;
}

uint32_t ToolEstimate::PlanColour(uint32_t c, size_t i) const
{
	return this->Fits(i) ? this->PlanColour(c) : COL_BP_NO;
}

void ToolEstimate::Clear()
{
	this->probed = false;
	this->ok = false;
	this->unknown = false;
	this->fit.clear();
	this->tunnel_end = INVALID_TILE;
	this->key = 0;
}

uint64_t ToolEstimate::Key() const
{
	const ToolPlans &plans = _tool.Plans();
	ProbeKey key;
	key.Add(static_cast<uint64_t>(_tool.Kind()));
	key.Add((_tool.Removing() ? 1 : 0) | (_ctrl_pressed ? 2 : 0) | (_tool.Dragging() ? 4 : 0));
	key.Add(_choices.Key());
	key.Add(static_cast<uint64_t>(_clear_filter.Mode()));

	for (const auto &[tile, track] : plans.rail.pieces) key.Add((static_cast<uint64_t>(tile.base()) << 4) | track);
	for (TileIndex tile : plans.line.tiles) key.Add(tile.base());
	key.Add((static_cast<uint64_t>(plans.line.axis) << 32) | plans.line.tiles.size());
	for (TileIndex tile : plans.signal.tiles) key.Add(tile.base());
	key.Add(plans.signal.track);

	const AreaPlan &area = plans.area;
	if (area.valid) key.Add((static_cast<uint64_t>(area.x0) << 48) | (static_cast<uint64_t>(area.y0) << 32) | (static_cast<uint64_t>(area.x1) << 16) | static_cast<uint64_t>(area.y1));
	if (IsPointTool(_tool.Kind())) key.Add(SiteTileAt(CursorPoint()).base());
	return key.Value();
}

void ToolEstimate::ProbeCost()
{
	CommandProbe probe;
	this->unknown = false;
	this->tunnel_end = INVALID_TILE;

	MiniTool kind = _tool.Kind();
	if (IsClickTool(kind)) {
		this->ProbeClick(kind);
	} else if (ClearTooWide()) {
		this->unknown = true;
	} else {
		CommitDrag(_tool);
	}

	this->probed = true;
	this->cost = probe.Cost();
	this->ok = probe.TakeVerdict();
}

void ToolEstimate::ProbeClick(MiniTool kind)
{
	TileIndex tile = SiteTileAt(CursorPoint());
	/* The tunnel command works out where it surfaces and leaves the tile
	 * behind for the interface; a stale one would draw a tunnel that is
	 * not being planned. */
	_build_tunnel_endtile = TileIndex{};
	CommitClick(kind, tile, _ctrl_pressed);
	if (!IsTunnelTool(kind) || _build_tunnel_endtile == 0) return;

	this->tunnel_end = _build_tunnel_endtile;
	this->tunnel_len = DistanceManhattan(tile, this->tunnel_end);
}

/* The cost probe asks the way the tool commits, which for a drag is one range
 * command. That answers for the run as a whole, so the tools whose run the game
 * fills in tile by tile get asked again, once per tile. */
void ToolEstimate::ProbeFit()
{
	const ToolPlans &plans = _tool.Plans();
	bool remove = _tool.Removing();
	switch (_tool.Kind()) {
		case MiniTool::Rail: this->fit = ProbeRunFit(plans.rail, remove); break;
		case MiniTool::Road: this->fit = ProbeRunFit(plans.line, remove); break;
		/* A signal run is laid at the density setting's spacing, so most tiles
		 * in the drag never get one and a per-tile answer would be about
		 * signals that are not being placed. The run keeps one verdict. */
		case MiniTool::Demolish:
		case MiniTool::Canal:
		case MiniTool::Trees:
		case MiniTool::BuyLand:
		case MiniTool::Convert:
		case MiniTool::RoadConvert:
			this->fit = ProbeAreaFit(plans.area, remove);
			break;
		default:
			this->fit.clear();
			break;
	}
}
