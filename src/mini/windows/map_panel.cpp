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

#include "../../gfx_func.h"
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
static constexpr std::chrono::milliseconds REPAINT_INTERVAL{500};
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

/* Scanning every tile costs too much to repeat per frame, so the picture is repainted on a slow beat. */
void MapPanel::AfterLayout()
{
	Rml::ElementDocument &document = *this->Document();
	Rml::Element *area = document.GetElementById("map");
	Rml::Element *frame = document.GetElementById("map-frame");
	if (area == nullptr || frame == nullptr) return;

	Rml::Vector2f room = area->GetBox().GetSize(Rml::BoxArea::Content);
	float scale = std::min(room.x / Map::SizeY(), room.y / Map::SizeX());
	Rml::Vector2i size(std::max(1, static_cast<int>(Map::SizeY() * scale)), std::max(1, static_cast<int>(Map::SizeX() * scale)));
	SetPixels(*frame, Rml::PropertyId::Width, static_cast<float>(size.x));
	SetPixels(*frame, Rml::PropertyId::Height, static_cast<float>(size.y));

	bool stale = std::chrono::steady_clock::now() - this->painted_at >= REPAINT_INTERVAL;
	if (stale || this->Mode() != this->painted_mode || size != Rml::Vector2i(this->overview.Width(), this->overview.Height())) this->Repaint(*frame, size);
	this->PlaceView(*frame);
	this->PlaceTowns(*frame);
}

void MapPanel::Repaint(Rml::Element &frame, Rml::Vector2i size)
{
	this->overview.Paint(size.x, size.y, this->Mode());
	this->painted_mode = this->Mode();
	this->painted_at = std::chrono::steady_clock::now();
	if (auto *image = dynamic_cast<RasterImage *>(frame.GetElementById("map-image")); image != nullptr) image->Show(this->overview.Pixels(), size);
}

void MapPanel::PlaceView(Rml::Element &frame)
{
	Rml::Element *view = frame.GetElementById("map-view");
	if (view == nullptr) return;

	double half_y = _screen.width * 0.5 / _camera.Ppt();
	double half_x = _screen.height * 0.5 / _camera.Ppt();
	Point top_left = this->overview.PixelOf(_camera.X() - half_x, _camera.Y() - half_y);
	Point bottom_right = this->overview.PixelOf(_camera.X() + half_x, _camera.Y() + half_y);
	SetPixels(*view, Rml::PropertyId::Left, static_cast<float>(top_left.x));
	SetPixels(*view, Rml::PropertyId::Top, static_cast<float>(top_left.y));
	SetPixels(*view, Rml::PropertyId::Width, static_cast<float>(bottom_right.x - top_left.x));
	SetPixels(*view, Rml::PropertyId::Height, static_cast<float>(bottom_right.y - top_left.y));
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
