/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file rml_layer.cpp The RmlUi context the mini UI draws in, from the map up. */

#include "../../stdafx.h"
#include "rml_layer.h"

#include <RmlUi/Core.h>

#include "../gpu/gl_api.h"
#include "fonts.h"
#include "map_view.h"
#include "native_slot.h"
#include "raster_image.h"
#include "rml_renderer.h"

#include "../../safeguards.h"

template <class TElement>
static void RegisterElement(const char *tag)
{
	static Rml::ElementInstancerGeneric<TElement> instancer;
	Rml::Factory::RegisterElementInstancer(tag, &instancer);
}

RmlLayer::RmlLayer() = default;
RmlLayer::~RmlLayer() = default;

Rml::Context *RmlLayer::Acquire()
{
	if (this->context == nullptr && !this->unavailable) this->Start();
	return this->context;
}

void RmlLayer::Start()
{
	this->unavailable = !LoadGl();
	if (this->unavailable) return;

	auto renderer = std::make_unique<RmlRenderer>();
	this->unavailable = !*renderer;
	if (this->unavailable) return;
	this->renderer = std::move(renderer);

	Rml::SetFileInterface(&this->files);
	Rml::SetSystemInterface(&this->system);
	Rml::SetRenderInterface(this->renderer.get());
	Rml::Initialise();
	RegisterElement<MapView>(MapView::TAG);
	RegisterElement<NativeSlot>(NativeSlot::TAG);
	RegisterElement<RasterImage>(RasterImage::TAG);
	for (const std::string &font : MiniFontFiles()) Rml::LoadFontFace(font);

	this->context = Rml::CreateContext("mini", Rml::Vector2i(1, 1), nullptr, &this->text_input);
	_gpu.Attach(this);
}

void RmlLayer::Update(int width, int height, float dp_ratio)
{
	this->context->SetDimensions(Rml::Vector2i(width, height));
	this->context->SetDensityIndependentPixelRatio(dp_ratio);
	this->context->Update();
}

void RmlLayer::Render(Dimension screen)
{
	this->renderer->SyncLentTextures();
	this->renderer->SetViewport(static_cast<int>(screen.width), static_cast<int>(screen.height));
	this->renderer->BeginFrame();
	this->context->Render();
	this->renderer->EndFrame();
}

void RmlLayer::Detach()
{
	Rml::Shutdown();
	this->context = nullptr;
	this->renderer.reset();
	this->held = {};
}

/* Only an element that takes the pointer counts, never the bare context root. */
const Rml::Element *RmlLayer::ElementAt(int x, int y) const
{
	if (this->context == nullptr) return nullptr;
	const Rml::Element *element = this->context->GetElementAtPoint(Rml::Vector2f(static_cast<float>(x), static_cast<float>(y)));
	return element == this->context->GetRootElement() ? nullptr : element;
}

/* Each driver event arrives on its own, so a button that changed changed with this event. */
void RmlLayer::Feed(const RmlPointer &pointer)
{
	if (this->context == nullptr) return;

	this->context->ProcessMouseMove(pointer.x, pointer.y, pointer.modifiers);
	for (int button = 0; button < static_cast<int>(this->held.size()); button++) {
		if (this->held[button] == pointer.buttons[button]) continue;
		this->held[button] = pointer.buttons[button];
		if (pointer.buttons[button]) {
			this->context->ProcessMouseButtonDown(button, pointer.modifiers);
		} else {
			this->context->ProcessMouseButtonUp(button, pointer.modifiers);
		}
	}
	if (pointer.wheel != 0) this->context->ProcessMouseWheel(Rml::Vector2f(0.0f, static_cast<float>(pointer.wheel)), pointer.modifiers);
}

/* The pointer is on another layer: nothing here stays hovered, and a press there ends typing here. */
void RmlLayer::Leave(bool pressed)
{
	if (this->context == nullptr) return;
	if (this->context->GetHoverElement() != nullptr) this->context->ProcessMouseLeave();
	if (pressed && this->IsTyping()) this->ReleaseFocus();
}

void RmlLayer::ProcessKey(const RmlKey &key)
{
	if (key.identifier == Rml::Input::KI_ESCAPE) {
		this->ReleaseFocus();
		return;
	}
	this->context->ProcessKeyDown(key.identifier, key.modifiers);
}

void RmlLayer::ProcessText(std::string_view text)
{
	this->context->ProcessTextInput(Rml::String(text));
}

void RmlLayer::Compose(std::string_view text)
{
	this->text_input.Compose(text);
}

void RmlLayer::ReleaseFocus()
{
	if (Rml::Element *focus = this->context->GetFocusElement(); focus != nullptr) focus->Blur();
}
