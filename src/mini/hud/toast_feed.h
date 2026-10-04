/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file toast_feed.h Short-lived cards for command failures and news. */

#ifndef MINI_HUD_TOAST_FEED_H
#define MINI_HUD_TOAST_FEED_H

#include <optional>
#include <string>
#include <vector>

#include "../../news_type.h"

struct Toast {
	std::string summary;
	std::string detail;
	uint repeat = 1;
	uint left_ms = 0;
	bool warn = false;
	NewsReference ref{};

	float Opacity() const;
};

/* Command failures land here instead of the stock modal, and news instead of
 * the newspaper: a stack of cards that ages out on its own. */
class ToastFeed {
public:
	const std::vector<Toast> &Items() const { return this->toasts; }

	void Report(std::string summary, std::string detail, bool warn);
	void Announce(std::string headline, std::string date, bool advice, NewsReference ref);
	void Age(uint delta_ms);
	std::optional<NewsReference> Dismiss(size_t i);
	void Clear() { this->toasts.clear(); }

private:
	void Push(Toast toast);

	std::vector<Toast> toasts;
};

extern ToastFeed _toast_feed;

#endif /* MINI_HUD_TOAST_FEED_H */
