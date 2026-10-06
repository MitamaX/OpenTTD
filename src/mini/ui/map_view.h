/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file map_view.h The map as an RmlUi element: the frame's map draw list, drawn as RmlUi meshes. */

#ifndef MINI_UI_MAP_VIEW_H
#define MINI_UI_MAP_VIEW_H

#include <RmlUi/Core/Element.h>
#include <RmlUi/Core/Geometry.h>

#include <vector>

#include "../gpu/draw_list.h"

namespace Rml { class RenderManager; }

class MapView final : public Rml::Element {
public:
	static constexpr const char TAG[] = "map-view";

	explicit MapView(const Rml::String &tag);

protected:
	void OnRender() override;

private:
	void DrawBatches(Rml::RenderManager &render_manager, const DrawList &list);
	void Draw(Rml::RenderManager &render_manager, Rml::Geometry &geometry, const DrawList &list, const DrawBatch &batch) const;

	std::vector<Rml::Geometry> batches;
};

#endif /* MINI_UI_MAP_VIEW_H */
