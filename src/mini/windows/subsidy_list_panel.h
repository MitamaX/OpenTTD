/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file subsidy_list_panel.h Subsidies on offer and awarded; a row opens the route's source. */

#ifndef MINI_WINDOWS_SUBSIDY_LIST_PANEL_H
#define MINI_WINDOWS_SUBSIDY_LIST_PANEL_H

#include "../ui/ledger_panel.h"

class SubsidyListPanel final : public LedgerPanel {
public:
	SubsidyListPanel();

private:
	void Fill() override;
};

#endif /* MINI_WINDOWS_SUBSIDY_LIST_PANEL_H */
