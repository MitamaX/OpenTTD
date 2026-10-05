/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file takeover_panel.cpp The offer to buy another company, bankrupt or taken over by force. */

#include "../../stdafx.h"
#include "takeover_panel.h"

#include "../../command_func.h"
#include "../../company_base.h"
#include "../../company_func.h"
#include "../../core/format.hpp"
#include "../../economy_cmd.h"
#include "../ui/ui_text.h"

#include "../../table/strings.h"

#include "../../safeguards.h"

TakeoverPanel::TakeoverPanel(CompanyID company, bool hostile) :
	LedgerPanel(fmt::format("takeover{}{}", company.base(), hostile ? "hostile" : ""), GameText(STR_COMPANY_NAME, company), {}),
	company(company), hostile(hostile)
{
}

bool TakeoverPanel::IsAlive() const
{
	return Company::IsValidID(this->company) && Company::IsValidID(_local_company);
}

Money TakeoverPanel::Price() const
{
	const Company *c = Company::Get(this->company);
	return this->hostile ? CalculateHostileTakeoverValue(c) : c->bankrupt_value;
}

void TakeoverPanel::Collect()
{
	const Company &target = *Company::Get(this->company);
	Money price = this->Price();
	StringID offer = this->hostile ? STR_BUY_COMPANY_HOSTILE_TAKEOVER : STR_BUY_COMPANY_MESSAGE;

	this->title = GameText(STR_COMPANY_NAME, this->company);
	this->sections.clear();
	this->Section().Add(LedgerLine::Text(GameText(offer, this->company, price)));
	this->Section().lines = {
		{"성능 지수", fmt::format("{}/1000", target.old_economy[0].performance_history)},
		{"보유 현금", GameText(STR_JUST_CURRENCY_LONG, target.money)},
		{"대출", GameText(STR_JUST_CURRENCY_LONG, target.current_loan), target.current_loan > 0 ? Tone::Warn : Tone::Plain},
	};

	CompanyID company = this->company;
	bool hostile = this->hostile;
	this->commands = {
		{"매수", Company::Get(_local_company)->money >= price, [this, company, hostile] { Command<CMD_BUY_COMPANY>::Post(STR_ERROR_CAN_T_BUY_COMPANY, company, hostile); this->Close(); }},
		{"거절", true, [this] { this->Close(); }},
	};
}
