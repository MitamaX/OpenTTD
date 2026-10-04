/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file window_catalog.cpp The status windows as the window bar lists them, and what each one is called. */

#include "../../stdafx.h"
#include "window_catalog.h"

#include "../../table/strings.h"

#include "../../safeguards.h"

static const MiniWinItem _win_company_items[] = {
	{INVALID_STRING_ID, "FINANCES", MiniWin::Finances, "finances"},
	{INVALID_STRING_ID, "INFO", MiniWin::CompanyInfo, "company"},
	{INVALID_STRING_ID, "GOALS", MiniWin::Goals, "goals"},
	{STR_GRAPH_MENU_COMPANY_LEAGUE_TABLE, "LEAGUE", MiniWin::League, "league"},
	{STR_GRAPH_MENU_OPERATING_PROFIT_GRAPH, "GRAPH", MiniWin::Graph, "graph"},
};

static const MiniWinItem _win_vehicle_items[] = {
	{INVALID_STRING_ID, "DEPOT", MiniWin::Buy, "vehicle-shop"},
	{INVALID_STRING_ID, "GROUPS", MiniWin::Groups, "groups"},
	{INVALID_STRING_ID, "STATIONS", MiniWin::Stations, "station"},
	{STR_REPLACE_VEHICLE_TRAIN, "TRAIN", MiniWin::Trains, "trains"},
	{STR_REPLACE_VEHICLE_ROAD_VEHICLE, "ROAD", MiniWin::RoadVehicles, "road-vehicles"},
	{STR_REPLACE_VEHICLE_SHIP, "SHIP", MiniWin::Ships, "ships"},
	{STR_REPLACE_VEHICLE_AIRCRAFT, "AIRCRAFT", MiniWin::Aircraft, "aircraft"},
};

static const MiniWinItem _win_world_items[] = {
	{INVALID_STRING_ID, "MAP", MiniWin::Map, "map"},
	{STR_NEWS_MENU_MESSAGE_HISTORY_MENU, "NEWS", MiniWin::News, "news"},
	{STR_TOWN_MENU_TOWN_DIRECTORY, "TOWNS", MiniWin::Towns, "towns"},
	{STR_INDUSTRY_MENU_INDUSTRY_DIRECTORY, "INDUSTRY", MiniWin::Industries, "industry"},
	{STR_SUBSIDIES_MENU_SUBSIDIES, "SUBSIDY", MiniWin::Subsidies, "subsidies"},
	{INVALID_STRING_ID, "SIGN", MiniWin::Signs, "sign"},
};

/* The game's own windows have no mini counterpart and none is wanted: they are
 * opened straight and the shell wraps whatever comes up. */
static const MiniWinItem _win_system_items[] = {
	{STR_FILE_MENU_SAVE_GAME, "SAVE", MiniWin::Save, "save"},
	{STR_FILE_MENU_LOAD_GAME, "LOAD", MiniWin::Load, "load"},
	{STR_SETTINGS_MENU_GAME_OPTIONS, "OPTIONS", MiniWin::Options, "options"},
	{STR_TOOLBAR_SOUND_MUSIC, "MUSIC", MiniWin::Music, "music"},
	{STR_FILE_MENU_QUIT_GAME, "ABANDON", MiniWin::Abandon, "abandon"},
	{STR_FILE_MENU_EXIT, "EXIT", MiniWin::Quit, "quit"},
};

static const MiniWinCategory _win_cats[] = {
	{STR_CONFIG_SETTING_COMPANY, "COMPANY", "finances", _win_company_items},
	{STR_CONFIG_SETTING_VEHICLES, "VEHICLES", "trains", _win_vehicle_items},
	{STR_CONFIG_SETTING_ENVIRONMENT, "WORLD", "map", _win_world_items},
	{INVALID_STRING_ID, "SYSTEM", "options", _win_system_items},
};

std::span<const MiniWinCategory> WindowCategories()
{
	return _win_cats;
}
