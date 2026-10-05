/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file graph_panel.cpp Panels whose every tab is an official window: the company graphs and the performance league. */

#include "../../stdafx.h"
#include "graph_panel.h"

#include "../../graph_gui.h"
#include "../../league_gui.h"
#include "../../window_type.h"

#include "../../safeguards.h"

static void OpenOperatingProfit(WindowNumber) { ShowOperatingProfitGraph(); }
static void OpenIncome(WindowNumber) { ShowIncomeGraph(); }
static void OpenCompanyValue(WindowNumber) { ShowCompanyValueGraph(); }
static void OpenPerformance(WindowNumber) { ShowPerformanceHistoryGraph(); }
static void OpenDeliveredCargo(WindowNumber) { ShowDeliveredCargoGraph(); }
static void OpenPaymentRates(WindowNumber) { ShowCargoPaymentRates(); }
static void OpenLeagueTable(WindowNumber) { ShowPerformanceLeagueTable(); }
static void OpenRatingDetail(WindowNumber) { ShowPerformanceRatingDetail(); }

static constexpr GraphPage COMPANY_GRAPHS[] = {
	{"영업 이익", {WC_OPERATING_PROFIT, OpenOperatingProfit}},
	{"수입", {WC_INCOME_GRAPH, OpenIncome}},
	{"가치", {WC_COMPANY_VALUE, OpenCompanyValue}},
	{"성능", {WC_PERFORMANCE_HISTORY, OpenPerformance}},
	{"화물", {WC_DELIVERED_CARGO, OpenDeliveredCargo}},
	{"지급률", {WC_PAYMENT_RATES, OpenPaymentRates}},
};

static constexpr GraphPage LEAGUE_PAGES[] = {
	{"순위", {WC_COMPANY_LEAGUE, OpenLeagueTable}},
	{"상세", {WC_PERFORMANCE_DETAIL, OpenRatingDetail}},
};

static Rml::Vector<Rml::String> PageNames(std::span<const GraphPage> pages)
{
	Rml::Vector<Rml::String> names;
	for (const GraphPage &page : pages) names.emplace_back(page.name);
	return names;
}

/* The plotted lines need the width to stay apart, so the company graphs open wide. */
std::unique_ptr<GraphPanel> GraphPanel::CompanyGraphs()
{
	auto panel = std::unique_ptr<GraphPanel>(new GraphPanel("graph", "그래프", COMPANY_GRAPHS));
	panel->wide = true;
	return panel;
}

std::unique_ptr<GraphPanel> GraphPanel::League()
{
	return std::unique_ptr<GraphPanel>(new GraphPanel("league", "순위", LEAGUE_PAGES));
}

GraphPanel::GraphPanel(std::string key, Rml::String title, std::span<const GraphPage> pages) :
	WindowPanel(std::move(key), std::move(title), PageNames(pages)), pages(pages)
{
}

std::optional<EmbedTarget> GraphPanel::Embed() const
{
	if (static_cast<size_t>(this->tab) >= this->pages.size()) return std::nullopt;
	return EmbedTarget{this->pages[this->tab].spec};
}

void GraphPanel::Fill()
{
}
