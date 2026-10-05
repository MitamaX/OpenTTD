/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file graph_panel.h Panels whose every tab is an official window: the company graphs and the performance league. */

#ifndef MINI_WINDOWS_GRAPH_PANEL_H
#define MINI_WINDOWS_GRAPH_PANEL_H

#include <memory>
#include <span>

#include "window_panel.h"

struct GraphPage {
	const char *name;
	DockSpec spec;
};

class GraphPanel final : public WindowPanel {
public:
	static std::unique_ptr<GraphPanel> CompanyGraphs();
	static std::unique_ptr<GraphPanel> League();

private:
	GraphPanel(std::string key, Rml::String title, std::span<const GraphPage> pages);

	std::optional<EmbedTarget> Embed() const override;
	void Fill() override;

	const std::span<const GraphPage> pages;
};

#endif /* MINI_WINDOWS_GRAPH_PANEL_H */
