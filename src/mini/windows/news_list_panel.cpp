/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file news_list_panel.cpp The news history; a row leads to what the item is about. */

#include "../../stdafx.h"
#include "news_list_panel.h"

#include "../../news_gui.h"
#include "../../news_type.h"
#include "../ui/ui_text.h"
#include "window_links.h"

#include "../../table/strings.h"

#include "../../safeguards.h"

enum NewsListTab : int {
	NLT_ALL,
	NLT_ADVICE,
};

static LedgerLine NewsLine(const NewsItem &item)
{
	NewsReference ref = item.ref1;
	LedgerLine line(StrMakeValid(item.GetStatusText(), {}), GameText(STR_JUST_DATE_TINY, item.date));
	line.Tint(item.type == NewsType::Advice ? Tone::Warn : Tone::Plain).OnClick([ref] { FollowNews(ref); });
	return line;
}

NewsListPanel::NewsListPanel() : LedgerPanel("news", "소식", {"전체", "조언"})
{
}

void NewsListPanel::Collect()
{
	this->sections.clear();
	LedgerSection &list = this->Section();
	for (const NewsItem &item : GetNews()) {
		if (this->tab == NLT_ADVICE && item.type != NewsType::Advice) continue;
		list.Add(NewsLine(item));
	}
	if (list.lines.empty()) list.Add(LedgerLine::Text("소식 없음", Tone::Dim));
}
