/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file takeover_panel.h The offer to buy another company, bankrupt or taken over by force. */

#ifndef MINI_WINDOWS_TAKEOVER_PANEL_H
#define MINI_WINDOWS_TAKEOVER_PANEL_H

#include "../../company_type.h"
#include "../../economy_type.h"
#include "../ui/ledger_panel.h"

class TakeoverPanel final : public LedgerPanel {
public:
	TakeoverPanel(CompanyID company, bool hostile);

	bool IsAlive() const override;

private:
	void Fill() override;
	Money Price() const;

	const CompanyID company;
	const bool hostile;
};

#endif /* MINI_WINDOWS_TAKEOVER_PANEL_H */
