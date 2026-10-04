/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file native_slot.cpp A panel element that shows the native screen lying under it. */

#include "../../stdafx.h"
#include "native_slot.h"

#include <RmlUi/Core.h>

#include "rml_renderer.h"

#include "../../safeguards.h"

static constexpr const char NATIVE_SLOT_TAG[] = "native-slot";

void NativeSlot::Register()
{
	static Rml::ElementInstancerGeneric<NativeSlot> instancer;
	Rml::Factory::RegisterElementInstancer(NATIVE_SLOT_TAG, &instancer);
}

NativeSlot::NativeSlot(const Rml::String &tag) : Rml::Element(tag)
{
}

Rml::Rectanglei NativeSlot::ScreenRect()
{
	Rml::Vector2f origin = this->GetAbsoluteOffset(Rml::BoxArea::Content).Round();
	Rml::Vector2f size = this->GetBox().GetSize(Rml::BoxArea::Content).Round();
	return Rml::Rectanglei::FromPositionSize(Rml::Vector2i(origin), Rml::Vector2i(size));
}

void NativeSlot::OnRender()
{
	Rml::RenderManager *render_manager = this->GetRenderManager();
	if (render_manager == nullptr) return;
	if (!this->texture) this->texture = render_manager->LoadTexture(RmlRenderer::SCREEN_SOURCE);

	Rml::Rectanglei rect = this->ScreenRect();
	Rml::Vector2i screen = this->texture.GetDimensions();
	if (rect != this->sampled_rect || screen != this->sampled_screen) this->Sample(*render_manager, rect, screen);
	this->geometry.Render(Rml::Vector2f(rect.TopLeft()), this->texture);
}

void NativeSlot::Sample(Rml::RenderManager &render_manager, Rml::Rectanglei rect, Rml::Vector2i screen)
{
	Rml::Vector2f texels(Rml::Math::Max(screen, Rml::Vector2i(1)));
	Rml::Mesh mesh = this->geometry.Release(Rml::Geometry::ReleaseMode::ClearMesh);
	Rml::MeshUtilities::GenerateQuad(mesh, Rml::Vector2f(0.0f), Rml::Vector2f(rect.Size()), Rml::ColourbPremultiplied(255),
		Rml::Vector2f(rect.TopLeft()) / texels, Rml::Vector2f(rect.BottomRight()) / texels);
	this->geometry = render_manager.MakeGeometry(std::move(mesh));
	this->sampled_rect = rect;
	this->sampled_screen = screen;
}
