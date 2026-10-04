/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file toast_feed.cpp Short-lived cards for command failures and news. */

#include "../../stdafx.h"
#include "toast_feed.h"

#include "../../settings_type.h"

#include "../../safeguards.h"

static constexpr size_t TOAST_MAX = 4;
static constexpr uint TOAST_FADE_MS = 400;
static constexpr uint MS_PER_SECOND = 1000;
static constexpr uint NEWS_LIFE_FACTOR = 2;

ToastFeed _toast_feed;

static uint ErrorLife()
{
	return std::max<uint>(_settings_client.gui.errmsg_duration, 1) * MS_PER_SECOND;
}

float Toast::Opacity() const
{
	return this->left_ms >= TOAST_FADE_MS ? 1.0f : static_cast<float>(this->left_ms) / TOAST_FADE_MS;
}

/* The same failure again restarts its card and counts up instead of stacking
 * a copy. */
void ToastFeed::Report(std::string summary, std::string detail, bool warn)
{
	if (!this->toasts.empty()) {
		Toast &last = this->toasts.back();
		if (last.summary == summary && last.detail == detail) {
			last.repeat++;
			last.left_ms = ErrorLife();
			last.warn = last.warn || warn;
			return;
		}
	}
	this->Push({std::move(summary), std::move(detail), 1, ErrorLife(), warn, {}});
}

/* A news card keeps the item's reference, so a click lands on what the
 * message is about. */
void ToastFeed::Announce(std::string headline, std::string date, bool advice, NewsReference ref)
{
	this->Push({std::move(headline), std::move(date), 1, ErrorLife() * NEWS_LIFE_FACTOR, advice, ref});
}

void ToastFeed::Age(uint delta_ms)
{
	for (Toast &t : this->toasts) t.left_ms = t.left_ms > delta_ms ? t.left_ms - delta_ms : 0;
	std::erase_if(this->toasts, [](const Toast &t) { return t.left_ms == 0; });
}

std::optional<NewsReference> ToastFeed::Dismiss(size_t i)
{
	if (i >= this->toasts.size()) return std::nullopt;
	NewsReference ref = this->toasts[i].ref;
	this->toasts.erase(this->toasts.begin() + i);
	return ref;
}

void ToastFeed::Push(Toast toast)
{
	this->toasts.push_back(std::move(toast));
	if (this->toasts.size() > TOAST_MAX) this->toasts.erase(this->toasts.begin());
}
