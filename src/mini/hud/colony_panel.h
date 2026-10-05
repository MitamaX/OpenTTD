/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file colony_panel.h The top-left corner: year gauge, company, money and the time controls. */

#ifndef MINI_HUD_COLONY_PANEL_H
#define MINI_HUD_COLONY_PANEL_H

#include "../ui/hud_part.h"

struct Company;

struct GaugeSegment {
	Rml::String angle;
	bool done = false;

	bool operator==(const GaugeSegment &) const = default;
};

struct SpeedButton {
	Rml::String icon;
	bool active = false;
	bool off = false;

	bool operator==(const SpeedButton &) const = default;
};

class ColonyPanel final : public HudPart {
public:
	ColonyPanel();

private:
	void Bind(Rml::DataModelConstructor &model) override;
	void Collect() override;
	void CollectFunds(const Company &company);
	void CollectGauge();
	void CollectSpeeds();
	void HoldWidth();
	void SetSpeed(Rml::DataModelHandle model, Rml::Event &event, const Rml::VariantList &arguments);

	Rml::String name;
	Rml::String date;
	Rml::String money;
	Rml::String vehicles;
	Rml::String warning;
	Rml::String funds_tone;
	Rml::Vector<GaugeSegment> gauge;
	Rml::Vector<SpeedButton> speeds;
	float held_width = 0.0f;
};

#endif /* MINI_HUD_COLONY_PANEL_H */
