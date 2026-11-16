/*
 * Tartarus, a fork of Descent 3
 * Copyright (C) 2026 Tartarus contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "frame_clock.h"

#include <thread>

#include "chrono_timer.h"
#include "classic.h"
#include "cvar.h"
#include "frame_clock_core.h"

namespace tartarus {

namespace {

Cvar *r_precise_timer = nullptr;
int64_t g_last_us = -1; // when the last frame was measured

bool Enabled() { return r_precise_timer != nullptr && r_precise_timer->GetBool() && !IsClassic(); }

} // namespace

void FrameClock_Init() {
  r_precise_timer = Cvars().RegisterBool("r_precise_timer", true,
                                         "Measure frames in microseconds and cap them exactly (F-061)",
                                         CVAR_ARCHIVE | CVAR_CLASSIC_LOCKED);
}

void FrameClock_Refine(float *frametime) {
  const int64_t now = D3::ChronoTimer::GetTimeUS();
  const int64_t last = g_last_us;
  g_last_us = now;
  if (!Enabled() || last < 0 || frametime == nullptr)
    return;
  *frametime = RefineFrametime(*frametime, static_cast<float>(now - last) * 1e-6f);
}

void FrameClock_Limit() {
  if (!Enabled() || g_last_us < 0)
    return;
  Cvar *maxfps = Cvars().Find("r_maxfps");
  const float fps = maxfps != nullptr ? maxfps->GetFloat() : 0.0f;
  int64_t wait = FrameWaitUs(g_last_us, D3::ChronoTimer::GetTimeUS(), fps);
  if (wait <= 0)
    return;
  if (wait > 1500)
    D3::ChronoTimer::SleepMS(static_cast<int>((wait - 1000) / 1000)); // sleep most of it, spin the last ms
  while (FrameWaitUs(g_last_us, D3::ChronoTimer::GetTimeUS(), fps) > 0)
    std::this_thread::yield();
}

} // namespace tartarus
