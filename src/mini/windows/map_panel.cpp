/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file map_panel.cpp The map overview: the whole map in one of its modes, town names, the camera frame, and a click to go there. */

#include "../../stdafx.h"
#include "map_panel.h"

#include <RmlUi/Core.h>

#include "../../core/math_func.hpp"
#include "../../map_func.h"
#include "../../town.h"
#include "../core/camera.h"
#include "../ui/pixel_style.h"
#include "../ui/raster_image.h"
#include "../ui/ui_text.h"
#include "window_links.h"

#include "../../table/strings.h"

#include "../../safeguards.h"

static constexpr const char MAP_DOCUMENT[] = "mini_ui/map.rml";
static constexpr const char TOWN_CLASS[] = "map-town";
static constexpr const char HIDDEN_CLASS[] = "hidden";
static constexpr int REPAINT_SLICES = 30;
static constexpr float TOWN_LIFT = 2.0f;

static constexpr StringID MAP_MODE_NAMES[] = {
	STR_SMALLMAP_TYPE_CONTOURS, STR_SMALLMAP_TYPE_VEHICLES, STR_SMALLMAP_TYPE_INDUSTRIES, STR_SMALLMAP_TYPE_ROUTES,
	STR_SMALLMAP_TYPE_ROUTEMAP, STR_SMALLMAP_TYPE_VEGETATION, STR_SMALLMAP_TYPE_OWNERS,
};

static Rml::Vector<Rml::String> MapTabs()
{
	Rml::Vector<Rml::String> tabs;
	for (StringID name : MAP_MODE_NAMES) tabs.push_back(GameText(name));
	return tabs;
}

static bool Overlaps(const Rml::Rectanglef &a, const Rml::Rectanglef &b)
{
	return a.Left() < b.Right() && b.Left() < a.Right() && a.Top() < b.Bottom() && b.Top() < a.Bottom();
}

struct Footprint {
	ExactPoint centre;
	double width;
	double height;
	double turn;
};

static ExactPoint Midpoint(ExactPoint a, ExactPoint b)
{
	return {(a.x + b.x) / 2, (a.y + b.y) / 2};
}

/* The sides are measured between the midpoints of opposite edges, so ground that bends the corners still gives a rectangle. */
static Footprint FootprintOf(const std::array<ExactPoint, 4> &corners)
{
	auto [top_left, top_right, bottom_right, bottom_left] = corners;
	ExactPoint left = Midpoint(top_left, bottom_left);
	ExactPoint right = Midpoint(top_right, bottom_right);
	ExactPoint top = Midpoint(top_left, top_right);
	ExactPoint bottom = Midpoint(bottom_left, bottom_right);
	double across_x = right.x - left.x;
	double across_y = right.y - left.y;
	return {Midpoint(left, right), std::hypot(across_x, across_y), std::hypot(bottom.x - top.x, bottom.y - top.y), std::atan2(across_y, across_x)};
}

MapPanel::MapPanel() : Panel("map", MAP_DOCUMENT, "지도", MapTabs())
{
	this->wide = true;
}

void MapPanel::BindSheet(Rml::DataModelConstructor &model)
{
	this->Expose(model, "towns", &this->towns);
	model.BindEventCallback("seek", &MapPanel::Seek, this);
}

/* Town names are placed largest first, so a crowded map keeps the names that matter. */
void MapPanel::Collect()
{
	this->towns.clear();
	this->town_points.clear();
	if (this->Mode() != OverviewMode::Contour || this->overview.Width() == 0) return;

	std::vector<const Town *> by_size;
	for (const Town *t : Town::Iterate()) by_size.push_back(t);
	std::ranges::sort(by_size, std::greater{}, [](const Town *t) { return t->cache.population; });
	for (const Town *t : by_size) {
		this->towns.push_back(GameText(STR_TOWN_NAME, t->index));
		this->town_points.push_back(this->overview.PixelOf(TileX(t->xy), TileY(t->xy)));
	}
}

Rml::Element *MapPanel::Frame() const
{
	return this->Document()->GetElementById("map-frame");
}

/* The frame keeps the map's proportions inside the room the panel gives it. */
std::optional<Rml::Vector2i> MapPanel::FrameSize() const
{
	Rml::Element *area = this->Document()->GetElementById("map");
	if (area == nullptr) return std::nullopt;

	Rml::Vector2f room = area->GetBox().GetSize(Rml::BoxArea::Content);
	float scale = std::min(room.x / Map::SizeY(), room.y / Map::SizeX());
	return Rml::Vector2i(std::max(1, static_cast<int>(Map::SizeY() * scale)), std::max(1, static_cast<int>(Map::SizeX() * scale)));
}

bool MapPanel::Shape()
{
	Rml::Element *frame = this->Frame();
	std::optional<Rml::Vector2i> size = this->FrameSize();
	if (frame == nullptr || !size.has_value()) return false;

	bool resized = SetPixels(*frame, Rml::PropertyId::Width, static_cast<float>(size->x));
	resized |= SetPixels(*frame, Rml::PropertyId::Height, static_cast<float>(size->y));
	return resized;
}

/* Scanning every tile costs too much to do in one frame, so the picture is repainted a slice of rows each frame, and at once only for a new mode or size. */
void MapPanel::AfterLayout()
{
	Rml::Element *frame = this->Frame();
	std::optional<Rml::Vector2i> size = this->FrameSize();
	if (frame == nullptr || !size.has_value()) return;

	if (this->Mode() != this->painted_mode || *size != Rml::Vector2i(this->overview.Width(), this->overview.Height())) {
		this->overview.Paint(size->x, size->y, this->Mode());
		this->painted_mode = this->Mode();
		this->ShowPicture(*frame);
	} else if (this->overview.PaintSlice(static_cast<int>(CeilDiv(size->y, REPAINT_SLICES)), this->Mode())) {
		this->ShowPicture(*frame);
	}
	this->PlaceView(*frame);
	this->PlaceTowns(*frame);
}

void MapPanel::ShowPicture(Rml::Element &frame)
{
	if (auto *image = dynamic_cast<RasterImage *>(frame.GetElementById("map-image")); image != nullptr) {
		image->Show(this->overview.Pixels(), Rml::Vector2i(this->overview.Width(), this->overview.Height()));
	}
}

/* The camera sees a turned rectangle of the map, so its box is laid out upright around the centre and turned onto it. */
void MapPanel::PlaceView(Rml::Element &frame)
{
	Rml::Element *view = frame.GetElementById("map-view");
	if (view == nullptr) return;

	std::array<TilePoint, 4> seen = _camera.ViewCorners();
	std::array<ExactPoint, 4> corners;
	std::ranges::transform(seen, corners.begin(), [this](const TilePoint &corner) { return this->overview.ExactPixelOf(corner.first, corner.second); });
	Footprint footprint = FootprintOf(corners);
	SetPixels(*view, Rml::PropertyId::Left, static_cast<float>(footprint.centre.x - footprint.width / 2));
	SetPixels(*view, Rml::PropertyId::Top, static_cast<float>(footprint.centre.y - footprint.height / 2));
	SetPixels(*view, Rml::PropertyId::Width, static_cast<float>(footprint.width));
	SetPixels(*view, Rml::PropertyId::Height, static_cast<float>(footprint.height));
	view->SetProperty(Rml::PropertyId::Transform, Rml::Transform::MakeProperty({Rml::Transforms::Rotate2D(static_cast<float>(footprint.turn), Rml::Unit::RAD)}));
}

/* A name sits centred above its town; one that would leave the map or land on a name already placed is hidden. */
void MapPanel::PlaceTowns(Rml::Element &frame)
{
	Rml::ElementList labels;
	frame.GetElementsByClassName(labels, TOWN_CLASS);
	Rml::Vector2f bounds = frame.GetBox().GetSize();

	std::vector<Rml::Rectanglef> placed;
	for (size_t i = 0; i < labels.size() && i < this->town_points.size(); i++) {
		Rml::Element &label = *labels[i];
		Rml::Vector2f size = label.GetBox().GetSize(Rml::BoxArea::Border);
		Rml::Vector2f at(this->town_points[i].x - size.x / 2, this->town_points[i].y - TOWN_LIFT - size.y);
		Rml::Rectanglef box = Rml::Rectanglef::FromPositionSize(at, size);

		bool fits = box.Left() >= 0 && box.Top() >= 0 && box.Right() <= bounds.x;
		bool clear = std::ranges::none_of(placed, [&box](const Rml::Rectanglef &other) { return Overlaps(box, other); });
		label.SetClass(HIDDEN_CLASS, !(fits && clear));
		if (fits && clear) placed.push_back(box);
		SetPixels(label, Rml::PropertyId::Left, at.x);
		SetPixels(label, Rml::PropertyId::Top, at.y);
	}
}

void MapPanel::Seek(Rml::DataModelHandle, Rml::Event &event, const Rml::VariantList &)
{
	Rml::Vector2f origin = event.GetCurrentElement()->GetAbsoluteOffset(Rml::BoxArea::Content);
	int x = static_cast<int>(event.GetParameter<float>("mouse_x", 0.0f) - origin.x);
	int y = static_cast<int>(event.GetParameter<float>("mouse_y", 0.0f) - origin.y);
	ScrollToTile(this->overview.TileAt(x, y));
}
