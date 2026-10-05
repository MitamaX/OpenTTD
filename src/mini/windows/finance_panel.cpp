/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file finance_panel.cpp The company's money: standing, yearly ledger and the loan. */

#include "../../stdafx.h"
#include "finance_panel.h"

#include "../../command_func.h"
#include "../../company_base.h"
#include "../../company_func.h"
#include "../../economy_func.h"
#include "../../misc_cmd.h"
#include "../ui/ui_text.h"

#include "../../table/strings.h"

#include "../../safeguards.h"

enum FinanceTab : int {
	FT_STANDING,
	FT_YEAR,
};

struct ExpenseGroup {
	StringID title;
	std::span<const ExpensesType> types;
};

static constexpr ExpensesType REVENUE_TYPES[] = {EXPENSES_TRAIN_REVENUE, EXPENSES_ROADVEH_REVENUE, EXPENSES_AIRCRAFT_REVENUE, EXPENSES_SHIP_REVENUE};
static constexpr ExpensesType OPERATING_TYPES[] = {EXPENSES_TRAIN_RUN, EXPENSES_ROADVEH_RUN, EXPENSES_AIRCRAFT_RUN, EXPENSES_SHIP_RUN, EXPENSES_PROPERTY, EXPENSES_LOAN_INTEREST};
static constexpr ExpensesType CAPITAL_TYPES[] = {EXPENSES_CONSTRUCTION, EXPENSES_NEW_VEHICLES, EXPENSES_OTHER};

static constexpr ExpenseGroup EXPENSE_GROUPS[] = {
	{STR_FINANCES_REVENUE_TITLE, REVENUE_TYPES},
	{STR_FINANCES_OPERATING_EXPENSES_TITLE, OPERATING_TYPES},
	{STR_FINANCES_CAPITAL_EXPENSES_TITLE, CAPITAL_TYPES},
};

static Tone OutgoTone(Money amount)
{
	return amount > 0 ? Tone::Loss : Tone::Plain;
}

static Rml::String Currency(Money amount)
{
	return GameText(STR_JUST_CURRENCY_LONG, amount);
}

FinancePanel::FinancePanel() : LedgerPanel("finance", "재정", {"개요", "손익"})
{
}

bool FinancePanel::IsAlive() const
{
	return Company::IsValidID(_local_company);
}

void FinancePanel::Fill()
{
	const Company &company = *Company::Get(_local_company);

	if (this->tab == FT_YEAR) {
		this->FillYear(company);
	} else {
		this->FillStanding(company);
	}

	this->commands = {
		{"빌리기", company.current_loan < company.GetMaxLoan(), [] { Command<CMD_INCREASE_LOAN>::Post(STR_ERROR_CAN_T_BORROW_ANY_MORE_MONEY, LoanCommand::Interval, 0); }},
		{"갚기", company.current_loan > 0, [] { Command<CMD_DECREASE_LOAN>::Post(STR_ERROR_CAN_T_REPAY_LOAN, LoanCommand::Interval, 0); }},
	};
}

void FinancePanel::FillStanding(const Company &company)
{
	this->Section().lines = {
		{GameText(STR_FINANCES_BANK_BALANCE_TITLE), Currency(company.money), Tone::Accent},
		{GameText(STR_FINANCES_OWN_FUNDS_TITLE), Currency(company.money - company.current_loan)},
		{GameText(STR_FINANCES_LOAN_TITLE), Currency(company.current_loan), company.current_loan > 0 ? Tone::Warn : Tone::Plain},
		{GameText(STR_FINANCES_MAX_LOAN, company.GetMaxLoan())},
		{GameText(STR_FINANCES_INTEREST_RATE, _economy.interest_rate)},
		{"회사 가치", Currency(CalculateCompanyValue(&company))},
	};
}

void FinancePanel::FillYear(const Company &company)
{
	const Expenses &expenses = company.yearly_expenses[0];
	Money year = 0;
	for (const ExpenseGroup &group : EXPENSE_GROUPS) {
		LedgerSection &section = this->Section(GameText(group.title));
		Money sum = 0;
		for (ExpensesType type : group.types) {
			Money amount = expenses[type];
			sum += amount;
			if (amount != 0) section.lines.emplace_back(GameText(STR_FINANCES_SECTION_CONSTRUCTION + type), CashFlowText(amount), OutgoTone(amount));
		}
		section.lines.push_back(LedgerLine::Total("합계", CashFlowText(sum), OutgoTone(sum)));
		year += sum;
	}

	this->Section(GameText(STR_FINANCES_TOTAL_CAPTION)).Add({"올해 손익", CashFlowText(year), year > 0 ? Tone::Loss : Tone::Accent});
}
