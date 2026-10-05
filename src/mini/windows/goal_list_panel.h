/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file goal_list_panel.h The company's goals and the global ones; a row opens what the goal points at. */

#ifndef MINI_WINDOWS_GOAL_LIST_PANEL_H
#define MINI_WINDOWS_GOAL_LIST_PANEL_H

#include "../ui/ledger_panel.h"

class GoalListPanel final : public LedgerPanel {
public:
	GoalListPanel();

private:
	void Collect() override;
};

#endif /* MINI_WINDOWS_GOAL_LIST_PANEL_H */
