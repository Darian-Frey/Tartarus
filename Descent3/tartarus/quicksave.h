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

// Quicksave, quickload and the level-start autosave (F-064, SPEC.md §11).
// g_quicksave_key (F9, stock's quicksave key) saves at once to a file of its
// own, savegame/quicksave, never into a numbered slot; g_quickload_key (F6)
// loads it at once. With g_checkpoints on, each level is saved to
// savegame/autosave a moment after it starts. Both appear at the top of the
// deck Load screen. Classic Mode keeps stock F9 (saves into the last slot
// used) and has no quickload or autosave; nothing in multiplayer.

#pragma once

namespace tartarus {

/// Registers the g_ cvars. Called from Tartarus_Init().
void Quicksave_Init();

/// The quicksave and quickload keys. True if one was used.
bool Quicksave_HandleKey(int key);

/// Per frame: takes the autosave once the level is under way.
void Quicksave_Frame();

/// A level has started: an autosave is due.
void Quicksave_LevelStart();

/// A save was loaded: no autosave over it.
void Quicksave_Loaded();

} // namespace tartarus
