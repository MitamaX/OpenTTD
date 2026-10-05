/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file finance_panel.h The company's money: standing, yearly ledger and the loan. */

#ifndef MINI_WINDOWS_FINANCE_PANEL_H
#define MINI_WINDOWS_FINANCE_PANEL_H

#include "../ui/ledger_panel.h"

struct Company;

class FinancePanel final : public LedgerPanel {
public:
	FinancePanel();

	bool IsAlive() const override;

private:
	void Collect() override;
	void CollectStanding(const Company &company);
	void CollectYear(const Company &company);
};

#endif /* MINI_WINDOWS_FINANCE_PANEL_H */
