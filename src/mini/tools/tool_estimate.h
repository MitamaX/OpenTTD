/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file tool_estimate.h What the plan in hand would cost, and which of its tiles the game would refuse. */

#ifndef MINI_TOOLS_TOOL_ESTIMATE_H
#define MINI_TOOLS_TOOL_ESTIMATE_H

#include <vector>

#include "../../economy_type.h"
#include "../../tile_type.h"
#include "tool_kind.h"

/* The tool's own commands run a second time as a probe, so the blueprint
 * shows the price and the refusals before anything is built. */
class ToolEstimate {
public:
	void Update();

	bool Priced() const { return this->probed && this->ok; }
	Money Cost() const { return this->cost; }
	TileIndex TunnelEnd() const { return this->tunnel_end; }
	uint TunnelLength() const { return this->tunnel_len; }
	bool FitsPerTile() const { return !this->fit.empty(); }
	bool Fits(size_t i) const;
	uint32_t PlanColour(uint32_t c) const;
	uint32_t PlanColour(uint32_t c, size_t i) const;

private:
	void Clear();
	uint64_t Key() const;
	void ProbeCost();
	void ProbeClick(MiniTool kind);
	void ProbeFit();

	bool probed = false;
	bool ok = false;
	/* The probe declined to ask, so the plan carries no verdict either way. */
	bool unknown = false;
	Money cost = 0;
	uint64_t key = 0;
	/* Per-tile verdicts for the tools whose drag the game fills in tile by
	 * tile; empty when the plan stands or falls as a whole. */
	std::vector<bool> fit;
	/* Where the tunnel command said it would surface, and how far that is. */
	TileIndex tunnel_end = INVALID_TILE;
	uint tunnel_len = 0;
};

extern ToolEstimate _estimate;

#endif /* MINI_TOOLS_TOOL_ESTIMATE_H */
