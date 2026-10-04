/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file ui_text.h Official language strings rendered as plain display text for the mini UI. */

#ifndef MINI_UI_UI_TEXT_H
#define MINI_UI_UI_TEXT_H

#include <string>

#include "../../economy_type.h"
#include "../../string_func.h"
#include "../../strings_func.h"

template <typename... Args>
std::string GameText(StringID str, Args &&... args)
{
	return StrMakeValid(GetString(str, std::forward<Args>(args)...), {});
}

std::string CashFlowText(Money amount);

#endif /* MINI_UI_UI_TEXT_H */
