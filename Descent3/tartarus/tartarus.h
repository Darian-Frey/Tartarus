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

// Tartarus start-up: the single entry point init.cpp calls.

#pragma once

namespace tartarus {

/// Called from InitD3Systems1() right after InitIOSystems(): command-line args
/// are parsed and the writable base directory is known. Sets up the cvar
/// registry (tartarus.cfg), then Classic Mode, then each module's cvars.
void Tartarus_Init();

/// True when started with -skipcinematics: in-game cinematics (level intros)
/// end as soon as upstream allows a key press to skip them. For testing.
bool SkipCinematics();

/// True when started with -skipbriefing: the pre-level briefing (and ship
/// selection, which shares its screen) is skipped. For testing.
bool SkipBriefing();

/// Offer a key from ProcessKeys() before any upstream handler (AV-008): the
/// console first, then the cheat menu (F-012). True if Tartarus used it.
bool HandleKey(int key);

/// Per-frame game logic, after DoPlayerFrame(): the cheat hold loop (F-012).
void Frame();

/// Multiplier applied to Frametime right after CalcFrameTime() (TORPOR).
float TimeScale();

/// Overlays drawn after the HUD: cheat menu and tag, then the console on top.
void RenderOverlays();

} // namespace tartarus
