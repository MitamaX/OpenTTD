/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file map_panel.h The map overview: the whole map in one of its modes, town names, the camera frame, and a click to go there. */

#ifndef MINI_WINDOWS_MAP_PANEL_H
#define MINI_WINDOWS_MAP_PANEL_H

#include <optional>
#include <vector>

#include "../map/overview.h"
#include "../ui/panel.h"

namespace Rml { class Element; }

class MapPanel final : public Panel {
public:
	MapPanel();

private:
	void BindSheet(Rml::DataModelConstructor &model) override;
	void Collect() override;
	bool Shape() override;
	void AfterLayout() override;

	OverviewMode Mode() const { return static_cast<OverviewMode>(this->tab); }
	Rml::Element *Frame() const;
	std::optional<Rml::Vector2i> FrameSize() const;
	void ShowPicture(Rml::Element &frame);
	void PlaceView(Rml::Element &frame);
	void PlaceTowns(Rml::Element &frame);
	void Seek(Rml::DataModelHandle model, Rml::Event &event, const Rml::VariantList &arguments);

	Overview overview;
	OverviewMode painted_mode = OverviewMode::Contour;
	Rml::Vector<Rml::String> towns;
	std::vector<Point> town_points;
};

#endif /* MINI_WINDOWS_MAP_PANEL_H */
