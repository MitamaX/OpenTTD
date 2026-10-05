/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file sign_list_panel.cpp Every sign; a row moves the camera there and renames the sign in place. */

#include "../../stdafx.h"
#include "sign_list_panel.h"

#include "../../command_func.h"
#include "../../core/format.hpp"
#include "../../mini_ui.h"
#include "../../signs_base.h"
#include "../../signs_cmd.h"

#include "../../safeguards.h"

/* An empty name removes the sign, the same way the stock editor deletes one. */
static void RenameSign(SignID sign, std::string name)
{
	Command<CMD_RENAME_SIGN>::Post(sign, std::move(name));
}

static LedgerLine SignLine(const Sign &sign)
{
	SignID id = sign.index;
	int x = sign.x;
	int y = sign.y;
	LedgerLine line(sign.name);
	line.Tint(Tone::Plain)
		.OnClick([x, y] { MiniUiScrollTo(x, y); })
		.Renames(SignListPanel::EditKey(id), [id](std::string name) { RenameSign(id, std::move(name)); });
	return line;
}

SignListPanel::SignListPanel() : LedgerPanel("signs", "표지판", {})
{
}

Rml::String SignListPanel::EditKey(SignID sign)
{
	return fmt::format("sign{}", sign.base());
}

void SignListPanel::Fill()
{
	std::vector<const Sign *> signs;
	for (const Sign *sign : Sign::Iterate()) signs.push_back(sign);
	std::ranges::sort(signs, std::less{}, &Sign::name);

	LedgerSection &list = this->Section();
	if (signs.empty()) {
		list.Add(LedgerLine::Text("표지판 없음. 토지 메뉴의 표지판 도구로 놓습니다", Tone::Dim));
		return;
	}
	for (const Sign *sign : signs) list.Add(SignLine(*sign));
	list.Add(LedgerLine::Text("두 번 클릭으로 이름 변경. 빈 이름은 삭제", Tone::Dim));
}
