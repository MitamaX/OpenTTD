/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file rml_layer.cpp The RmlUi context the raylib driver composites over the mini UI. */

#include "../../stdafx.h"
#include "rml_layer.h"

#include <RmlUi/Core.h>

#include "fonts.h"
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
	this->unavailable = !RmlGL3::Initialize();
	if (this->unavailable) return;

	auto renderer = std::make_unique<RmlRenderer>();
	this->unavailable = !*renderer;
	if (this->unavailable) return;
	this->renderer = std::move(renderer);

	Rml::SetFileInterface(&this->files);
	Rml::SetSystemInterface(&this->system);
	Rml::SetRenderInterface(this->renderer.get());
	Rml::Initialise();
	RegisterElement<NativeSlot>(NativeSlot::TAG);
	RegisterElement<RasterImage>(RasterImage::TAG);
	for (const char *font : MINI_FONTS) Rml::LoadFontFace(font);

	this->context = Rml::CreateContext("mini", Rml::Vector2i(1, 1), nullptr, &this->text_input);
	RlwAttachLayer(this);
}

void RmlLayer::Update(int width, int height, float dp_ratio)
{
	this->context->SetDimensions(Rml::Vector2i(width, height));
	this->context->SetDensityIndependentPixelRatio(dp_ratio);
	this->context->Update();
}

void RmlLayer::Render(int width, int height)
{
	this->renderer->SyncScreenTexture();
	this->renderer->SetViewport(width, height);
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
	RmlGL3::Shutdown();
}

void RmlLayer::TrackPointer(const RmlPointer &pointer)
{
	if (this->context == nullptr) return;

	this->context->ProcessMouseMove(pointer.x, pointer.y, pointer.modifiers);
	for (int button = 0; button < static_cast<int>(this->held.size()); button++) {
		if (!this->held[button] || pointer.buttons[button]) continue;
		this->held[button] = false;
		this->context->ProcessMouseButtonUp(button, pointer.modifiers);
	}

	bool pressed_elsewhere = !this->context->IsMouseInteracting() && std::ranges::any_of(pointer.buttons, std::identity{});
	if (this->IsTyping() && pressed_elsewhere) this->ReleaseFocus();
}

bool RmlLayer::CapturePointer(const RmlPointer &pointer)
{
	if (this->context == nullptr || !this->context->IsMouseInteracting()) return false;

	for (int button = 0; button < static_cast<int>(this->held.size()); button++) {
		if (this->held[button] || !pointer.buttons[button]) continue;
		this->held[button] = true;
		this->context->ProcessMouseButtonDown(button, pointer.modifiers);
	}
	if (pointer.wheel != 0) this->context->ProcessMouseWheel(Rml::Vector2f(0.0f, static_cast<float>(pointer.wheel)), pointer.modifiers);
	return true;
}

/* The pointer counts as over the layer only on an element that takes it, never on the bare context root. */
const Rml::Element *RmlLayer::Hovered() const
{
	if (this->context == nullptr || !this->context->IsMouseInteracting()) return nullptr;
	return this->context->GetHoverElement();
}

void RmlLayer::ProcessKey(const RmlKey &key)
{
	if (key.identifier == Rml::Input::KI_ESCAPE) {
		this->ReleaseFocus();
		return;
	}
	this->context->ProcessKeyDown(key.identifier, key.modifiers);
}

void RmlLayer::ProcessText(char32_t character)
{
	this->context->ProcessTextInput(static_cast<Rml::Character>(character));
}

void RmlLayer::ReleaseFocus()
{
	if (Rml::Element *focus = this->context->GetFocusElement(); focus != nullptr) focus->Blur();
}
