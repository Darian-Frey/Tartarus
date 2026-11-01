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

// Password-gated cheat menu (F-012, SPEC.md §4, D-006): engine glue around
// CheatState. Refused in Classic Mode and multiplayer (AV-001, AV-004).
// Upstream's typed-sequence cheats in GameCheat.cpp are untouched.

#pragma once

#include "cheat_table.h"

namespace tartarus {

/// Registers the cheat console commands. Called from Tartarus_Init() after
/// Console_Init().
void Cheats_Init();

/// The session's unlock state (for the console's cheat policy).
CheatState &Cheats();

/// Policy for CMD_CHEAT console commands: true if the cheat the command
/// belongs to is unlocked (Debug builds: always).
bool Cheats_AllowsCommand(const char *command);

/// Offer a key (after the console). Shift+F12 opens/closes the menu; while
/// open the menu takes every key.
bool Cheats_HandleKey(int key);

/// Hold-at-max loop and toggle bookkeeping. Call once per frame after
/// DoPlayerFrame(). Early-out in Classic Mode (AV-001).
void Cheats_Frame();

/// Multiplier for Frametime (TORPOR / `timescale`). 1 in Classic Mode,
/// multiplayer, or when no time-scale cheat is active.
float Cheats_TimeScale();

/// Draws the menu if open and the "CHEATS" HUD tag. Call after the HUD.
void Cheats_Render();

bool Cheats_MenuIsOpen();
void Cheats_OpenMenu();
void Cheats_CloseMenu();

} // namespace tartarus
