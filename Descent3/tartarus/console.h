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

// Drop-down developer console (F-010): engine glue around ConsoleCore. Keys come
// from ProcessKeys() in GameLoop.cpp, drawing from GameRenderFrame(). Available
// in Classic Mode too (non-cheat commands only, SPEC.md §12).

#pragma once

#include "console_core.h"

namespace tartarus {

/// Registers con_* cvars and engine commands, mirrors plog into the scrollback,
/// loads console_history.txt. Called from Tartarus_Init().
void Console_Init();

/// The game's console (for modules that add commands).
ConsoleCore &Console();

/// Offer a key from the game's key queue. Returns true if the console used it
/// (the toggle key always; everything while open). Must run before any other
/// key handler (AV-008).
bool Console_HandleKey(int key);

/// Draws the console if it is open or animating. Call once per game frame,
/// after the HUD.
void Console_Render();

bool Console_IsOpen();
void Console_Open();
void Console_Close();

} // namespace tartarus
