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

// Hold and snap to upright, and stronger auto-level (F-043, SPEC.md §9). These
// add bank input in the controls stage, as if the pilot pressed the bank keys,
// so stock physics and its auto-level are untouched: al_strength 0 with no key
// held adds nothing (AV-011). Any roll input cancels. Off in Classic Mode.

#pragma once

struct game_controls;

namespace tartarus {

/// Registers the al_* cvars. Called from Tartarus_Init().
void Level_Init();

/// Takes the snap key from the key queue. True if it was that key.
bool Level_HandleKey(int key);

/// Adds the bank input. Called from the controls stage, after all other input.
void Level_Apply(game_controls *controls);

} // namespace tartarus
