/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file window_panel.h A ledger panel that can carry a live camera on its first tab and show an official window in its body. */

#ifndef MINI_WINDOWS_WINDOW_PANEL_H
#define MINI_WINDOWS_WINDOW_PANEL_H

#include <optional>

#include "../dock/carrier.h"
#include "../dock/native_dock.h"
#include "../ui/ledger_panel.h"

struct CameraShot {
	CarrierSubject subject;
	int id;
	CarrierFocus focus;
};

struct EmbedTarget {
	DockSpec spec;
	WindowNumber number = 0;
};

class NativeSlot;

class WindowPanel : public LedgerPanel {
public:
	void ListNatives(std::vector<NativeKey> &natives) const override;

protected:
	using LedgerPanel::LedgerPanel;

	virtual std::optional<CameraShot> Camera() const;
	virtual std::optional<EmbedTarget> Embed() const;

private:
	struct EmbedSite {
		NativeSlot *slot;
		Window *window;
	};

	void Collect() final;
	bool Shape() final;
	void AfterLayout() final;

	std::optional<EmbedSite> Site() const;
	void ShowCamera(const CameraShot &shot) const;
	bool Fit(const NativeSizing &sizing, Rml::Vector2f chrome);
	bool Unfit();

	std::optional<CameraShot> shot;
	std::optional<EmbedTarget> target;
	bool fitted = false;
};

#endif /* MINI_WINDOWS_WINDOW_PANEL_H */
