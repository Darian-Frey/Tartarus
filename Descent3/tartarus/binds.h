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

// Console key bindings (F-010, SPEC.md §3): `bind <key> "<line>"`,
// `unbind <key>`, saved to binds.cfg in the writable base directory. A bound
// key runs its line in a level when the console and cheat menu are closed;
// the key's stock action is then skipped (held flight controls are read
// separately and keep working). Cheat lines are policed as when typed.

#pragma once

namespace tartarus {

/// Registers bind/unbind and runs binds.cfg. Called from Tartarus_Init()
/// after the console exists.
void Binds_Init();

/// Offer a key after the console and cheat menu. True if a binding ran.
bool Binds_HandleKey(int key);

} // namespace tartarus
