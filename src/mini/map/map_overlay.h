/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file map_overlay.h The info layer the map highlights over a greyed base. */

#ifndef MINI_MAP_MAP_OVERLAY_H
#define MINI_MAP_MAP_OVERLAY_H

enum class MiniLayer : uint8_t {
	None,
	Rail,
	Road,
};

/* ONI-style overlay: an explicit toggle that swaps the info layer without
 * leaving the screen. Picking a build tool auto-engages its layer; that
 * auto choice reverts when the tool is dropped, a manual toggle sticks. */
class MapOverlay {
public:
	MiniLayer Shown() const { return this->shown; }
	MiniLayer Filter() const;

	void Toggle(MiniLayer layer);
	void FollowTool(MiniLayer tool_layer);
	void Reset();

private:
	MiniLayer shown = MiniLayer::None;
	bool automatic = false;
	MiniLayer last_tool_layer = MiniLayer::None;
};

extern MapOverlay _overlay;

#endif /* MINI_MAP_MAP_OVERLAY_H */
