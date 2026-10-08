/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file video_driver.cpp Common code between video driver implementations. */

#include "../stdafx.h"
#include "../core/random_func.hpp"
#include "../network/network.h"
#include "../blitter/factory.hpp"
#include "../debug.h"
#include "../driver.h"
#include "../fontcache.h"
#include "../gfx_func.h"
#include "../gfxinit.h"
#include "../mini_ui.h"
#include "../mini/core/tick_clock.h"
#include "../mini/gpu/frame_profile.h"
#include "../progress.h"
#include "../rev.h"
#include "../thread.h"
#include "../timer/timer_game_tick.h"
#include "../window_func.h"
#include "video_driver.hpp"

#include "../safeguards.h"

bool _video_hw_accel; ///< Whether to consider hardware accelerated video drivers on startup.
bool _video_vsync; ///< Whether we should use vsync (only if active video driver supports HW acceleration).

static constexpr int REPORTED_REFRESH_RATE_SLACK = 1; ///< Displays report rates such as 59.94 Hz rounded down.

void VideoDriver::ApplyVsync()
{
	if (_video_vsync || !MiniUiActive()) {
		this->presents_on_vsync = this->SetSwapInterval(_video_vsync ? 1 : 0);
		return;
	}
	int refreshes_per_frame = std::max(1, (this->GetDisplayRefreshRate() + REPORTED_REFRESH_RATE_SLACK) / _settings_client.gui.refresh_rate);
	this->presents_on_vsync = this->SetSwapInterval(-refreshes_per_frame) || this->SetSwapInterval(refreshes_per_frame);
}

void VideoDriver::GameLoop()
{
	auto interval = this->GetGameInterval();
	auto due = this->next_game_tick;
	this->next_game_tick += interval;

	/* Avoid next_game_tick getting behind more and more if it cannot keep up. */
	auto now = std::chrono::steady_clock::now();
	if (this->next_game_tick < now - ALLOWED_DRIFT * interval) this->next_game_tick = now;

	{
		std::lock_guard<std::mutex> lock(this->game_state_mutex);

		::GameLoop();
		_tick_clock.Stamp(TimerGameTick::counter, due, interval);
	}
}

void VideoDriver::GameThread()
{
	while (!_exit_game) {
		this->GameLoop();

		auto now = std::chrono::steady_clock::now();
		if (this->next_game_tick > now) {
			std::this_thread::sleep_for(this->next_game_tick - now);
		} else {
			/* Ensure we yield this thread if drawings wants to take a lock on
			 * the game state. This is mainly because most OSes have an
			 * optimization that if you unlock/lock a mutex in the same thread
			 * quickly, it will never context switch even if there is another
			 * thread waiting to take the lock on the same mutex. */
			std::lock_guard<std::mutex> lock(this->game_thread_wait_mutex);
		}
	}
}

/**
 * Pause the game-loop for a bit, releasing the game-state lock. This allows,
 * if the draw-tick requested this, the drawing to happen.
 */
void VideoDriver::GameLoopPause()
{
	/* If we are not called from the game-thread, ignore this request. */
	if (std::this_thread::get_id() != this->game_thread.get_id()) return;

	this->game_state_mutex.unlock();

	{
		/* See GameThread() for more details on this lock. */
		std::lock_guard<std::mutex> lock(this->game_thread_wait_mutex);
	}

	this->game_state_mutex.lock();
}

/* static */ void VideoDriver::GameThreadThunk(VideoDriver *drv)
{
	drv->GameThread();
}

void VideoDriver::StartGameThread()
{
	if (this->is_game_threaded) {
		this->is_game_threaded = StartNewThread(&this->game_thread, "ottd:game", &VideoDriver::GameThreadThunk, this);
	}

	Debug(driver, 1, "using {}thread for game-loop", this->is_game_threaded ? "" : "no ");
}

void VideoDriver::StopGameThread()
{
	if (!this->is_game_threaded) return;

	this->game_thread.join();
}

void VideoDriver::Tick()
{
	if (!this->is_game_threaded && std::chrono::steady_clock::now() >= this->next_game_tick) {
		this->GameLoop();

		/* For things like dedicated server, don't run a separate draw-tick. */
		if (!this->HasGUI()) {
			::InputLoop();
			::UpdateWindows();
			this->next_draw_tick = this->next_game_tick;
		}
	}

	auto now = std::chrono::steady_clock::now();
	if (this->HasGUI() && now >= this->next_draw_tick) {
		auto draw_interval = this->GetDrawInterval();
		this->next_draw_tick += draw_interval;
		/* A late frame is not caught up on by drawing the next ones back to back. */
		if (this->next_draw_tick < now) this->next_draw_tick = now + draw_interval;

		/* Locking video buffer can block (especially with vsync enabled), do it before taking game state lock. */
		{
			ProfileScope profile("tick", "vidlock", ProfileClock::Cpu);
			this->LockVideoBuffer();
		}
		_tick_clock.BeginFrame(std::chrono::steady_clock::now());

		{
			/* Tell the game-thread to stop so we can have a go. */
			std::unique_lock<std::mutex> lock_wait(this->game_thread_wait_mutex, std::defer_lock);
			std::unique_lock<std::mutex> lock_state(this->game_state_mutex, std::defer_lock);
			{
				ProfileScope profile("tick", "lock", ProfileClock::Cpu);
				lock_wait.lock();
				lock_state.lock();
			}
			ProfileScope locked_profile("tick", "locked", ProfileClock::Cpu);

			/* Keep the interactive randomizer a bit more random by requesting
			 * new values when-ever we can. */
			InteractiveRandom();

			this->DrainCommandQueue();

			while (this->PollEvent()) {}
			this->InputLoop();

			/* Check if the fast-forward button is still pressed. */
			if (fast_forward_key_pressed && !_networking && _game_mode != GM_MENU) {
				ChangeGameSpeed(true);
				this->fast_forward_via_key = true;
			} else if (this->fast_forward_via_key) {
				ChangeGameSpeed(false);
				this->fast_forward_via_key = false;
			}

			::InputLoop();

			/* Prevent drawing when switching mode, as windows can be removed when they should still appear. */
			if (_game_mode == GM_BOOTSTRAP || _switch_mode == SM_NONE || HasModalProgress()) {
				::UpdateWindows();
			}

			this->PopulateSystemSprites();
		}

		this->CheckPaletteAnim();

		auto unlock_video_buffer = [this]() {
			ProfileScope profile("tick", "vidunlock", ProfileClock::Cpu);
			this->UnlockVideoBuffer();
		};
		/* The mini UI samples the screen while it paints, so this tick's drawing goes to the GPU first. */
		bool unlock_before_paint = MiniUiActive();
		if (unlock_before_paint) unlock_video_buffer();
		this->Paint();
		if (!unlock_before_paint) unlock_video_buffer();

		/* Wait till the first successful drawing tick before marking the driver as operational. */
		static bool first_draw_tick = true;
		if (first_draw_tick) {
			first_draw_tick = false;
			DriverFactoryBase::MarkVideoDriverOperational();
		}
	}
}

void VideoDriver::SleepTillNextTick()
{
	auto next_tick = this->next_draw_tick;
	auto now = std::chrono::steady_clock::now();

	if (!this->is_game_threaded) {
		next_tick = min(next_tick, this->next_game_tick);
	}

	if (next_tick > now) {
		std::this_thread::sleep_for(next_tick - now);
	}
}

/**
 * Get the caption to use for the game's title bar.
 * @return The caption.
 */
/* static */ std::string VideoDriver::GetCaption()
{
	return fmt::format("OpenTTD {}", _openttd_revision);
}
