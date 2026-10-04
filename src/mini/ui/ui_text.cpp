/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file ui_text.cpp Official language strings rendered as plain display text for the mini UI. */

#include "../../stdafx.h"
#include "ui_text.h"

#include "../../table/strings.h"

#include "../../safeguards.h"

/* Stock finance sign convention: positive table values are outgo, negative are income and show with a plus sign. */
std::string CashFlowText(Money amount)
{
	if (amount == 0) return GameText(STR_FINANCES_ZERO_INCOME, amount);
	if (amount < 0) return GameText(STR_FINANCES_POSITIVE_INCOME, -amount);
	return GameText(STR_FINANCES_NEGATIVE_INCOME, amount);
}
