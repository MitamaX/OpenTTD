/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file rml_layer.h The RmlUi context the raylib driver composites over the mini UI. */

#ifndef MINI_UI_RML_LAYER_H
#define MINI_UI_RML_LAYER_H

#include <array>
#include <memory>

#include "../../video/raylib_wrap.h"
#include "rml_input.h"
#include "rml_interfaces.h"

namespace Rml { class Context; class Element; }
class RmlRenderer;

class RmlLayer final : public RlwLayer {
public:
	RmlLayer();
	~RmlLayer() override;

	Rml::Context *Acquire();
	Rml::Context *Current() const { return this->context; }
	void Update(int width, int height, float dp_ratio);
	void Render(int width, int height) override;
	void Detach() override;

	void TrackPointer(const RmlPointer &pointer);
	bool CapturePointer(const RmlPointer &pointer);
	const Rml::Element *Hovered() const;

	bool IsTyping() const { return this->text_input.IsActive(); }
	void ProcessKey(const RmlKey &key);
	void ProcessText(char32_t character);

private:
	void Start();
	void ReleaseFocus();

	RmlFileInterface files;
	RmlSystemInterface system;
	RmlTextInputHandler text_input;
	std::unique_ptr<RmlRenderer> renderer;
	Rml::Context *context = nullptr;
	bool unavailable = false;
	std::array<bool, 3> held{};
};

#endif /* MINI_UI_RML_LAYER_H */
