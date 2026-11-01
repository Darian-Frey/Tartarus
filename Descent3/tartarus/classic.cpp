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

// Classic Mode (F-002). See classic.h.

#include "classic.h"

#include <optional>

#include "cvar.h"

#ifndef TARTARUS_DEFAULT_CLASSIC
#define TARTARUS_DEFAULT_CLASSIC 1
#endif

namespace tartarus {

namespace {
Cvar *g_classic = nullptr;
// Kept outside the cvar so a command-line choice is never written to tartarus.cfg.
std::optional<bool> g_session_override;
// Cached for per-frame callers; refreshed whenever either source changes.
bool g_is_classic = TARTARUS_DEFAULT_CLASSIC != 0;

void Refresh() { g_is_classic = g_session_override.value_or(g_classic == nullptr || g_classic->GetBool()); }
} // namespace

void Classic_Init(bool force_classic, bool force_tartarus) {
  g_classic = Cvars().RegisterBool("classic", TARTARUS_DEFAULT_CLASSIC != 0,
                                   "Classic Mode: 1 = stock Descent 3 gameplay, HUD and AI; 0 = Tartarus features on",
                                   CVAR_ARCHIVE);
  g_classic->AddChangeCallback([](const Cvar &) {
    g_session_override.reset(); // an explicit change beats the command line
    Refresh();
  });

  if (force_classic) {
    g_session_override = true; // -classic wins if both are given
  } else if (force_tartarus) {
    g_session_override = false;
  }
  Refresh();
}

bool IsClassic() { return g_is_classic; }

bool IsClassicForcedByCommandLine() { return g_session_override.has_value(); }

} // namespace tartarus
