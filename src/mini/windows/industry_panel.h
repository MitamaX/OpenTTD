/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file industry_panel.h An industry: its output, the stations serving it, its needs and the official production graph. */

#ifndef MINI_WINDOWS_INDUSTRY_PANEL_H
#define MINI_WINDOWS_INDUSTRY_PANEL_H

#include "../../industry_type.h"
#include "window_panel.h"

struct Industry;

class IndustryPanel final : public WindowPanel {
public:
	explicit IndustryPanel(IndustryID industry);

	bool IsAlive() const override;

private:
	std::optional<CameraShot> Camera() const override;
	std::optional<EmbedTarget> Embed() const override;
	void Fill() override;

	void FillOutput(const Industry &industry);
	void FillStations(const Industry &industry);
	void FillNeeds(const Industry &industry);
	void FillCommands(const Industry &industry);

	const IndustryID industry;
};

#endif /* MINI_WINDOWS_INDUSTRY_PANEL_H */
