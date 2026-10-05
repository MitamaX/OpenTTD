/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file engine_preview_panel.cpp The offer of an exclusive preview of a new engine. */

#include "../../stdafx.h"
#include "engine_preview_panel.h"

#include "../../command_func.h"
#include "../../company_func.h"
#include "../../core/format.hpp"
#include "../../engine_base.h"
#include "../../engine_cmd.h"
#include "../../engine_gui.h"
#include "../ui/ui_text.h"

#include "../../table/strings.h"

#include "../../safeguards.h"

EnginePreviewPanel::EnginePreviewPanel(EngineID engine) : LedgerPanel(fmt::format("preview{}", engine.base()), "신형 차량", {}), engine(engine)
{
}

bool EnginePreviewPanel::IsAlive() const
{
	const Engine *e = Engine::GetIfValid(this->engine);
	return e != nullptr && e->preview_company == _local_company;
}

void EnginePreviewPanel::Collect()
{
	this->sections.clear();
	this->Section().Add(LedgerLine::Text(GameText(STR_ENGINE_PREVIEW_MESSAGE, GetEngineCategoryName(this->engine))));
	this->Section(GameText(STR_ENGINE_NAME, PackEngineNameDParam(this->engine, EngineNameContext::PreviewNews)))
		.Add(LedgerLine::Text(StrMakeValid(GetEngineInfoString(this->engine), {})));

	EngineID engine = this->engine;
	this->commands = {
		{"수락", true, [this, engine] { Command<CMD_WANT_ENGINE_PREVIEW>::Post(engine); this->Close(); }},
		{"거절", true, [this] { this->Close(); }},
	};
}
