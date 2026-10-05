/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file map_pointer.h The map at the foot of the HUD: what a click, a drag and the wheel do there. */

#ifndef MINI_INPUT_MAP_POINTER_H
#define MINI_INPUT_MAP_POINTER_H

#include <functional>
#include <optional>

#include "../../core/geometry_type.hpp"
#include "../ui/hud_part.h"

class MapPointer final : public HudPart {
public:
	/* Opens whatever lies under a click that no tool or order pick took. */
	using Inspector = std::function<void(Point at)>;

	explicit MapPointer(Inspector inspect);

private:
	void Bind(Rml::DataModelConstructor &model) override;
	void Collect() override;

	void Press(Rml::DataModelHandle model, Rml::Event &event, const Rml::VariantList &arguments);
	void Release(Rml::DataModelHandle model, Rml::Event &event, const Rml::VariantList &arguments);
	void Move(Rml::DataModelHandle model, Rml::Event &event, const Rml::VariantList &arguments);
	void Zoom(Rml::DataModelHandle model, Rml::Event &event, const Rml::VariantList &arguments);

	void Click(Point at, bool ctrl) const;

	const Inspector inspect;
	std::optional<Point> grab;
};

#endif /* MINI_INPUT_MAP_POINTER_H */
