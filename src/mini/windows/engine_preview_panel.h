/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file engine_preview_panel.h The offer of an exclusive preview of a new engine. */

#ifndef MINI_WINDOWS_ENGINE_PREVIEW_PANEL_H
#define MINI_WINDOWS_ENGINE_PREVIEW_PANEL_H

#include "../../engine_type.h"
#include "../ui/ledger_panel.h"

class EnginePreviewPanel final : public LedgerPanel {
public:
	explicit EnginePreviewPanel(EngineID engine);

	bool IsAlive() const override;

private:
	void Fill() override;

	const EngineID engine;
};

#endif /* MINI_WINDOWS_ENGINE_PREVIEW_PANEL_H */
