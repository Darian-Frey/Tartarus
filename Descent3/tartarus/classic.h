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

// Classic Mode (F-002, D-004): the one gate every Tartarus module checks.
// Every per-frame Tartarus entry point begins with `if (IsClassic()) return;`
// (AV-001).

#pragma once

namespace tartarus {

/// Registers the `classic` cvar and applies -classic / -tartarus. Call once,
/// after the config has been loaded.
void Classic_Init(bool force_classic, bool force_tartarus);

/// True when Tartarus gameplay, HUD and AI changes must be inactive.
bool IsClassic();

/// True when the mode for this session comes from -classic / -tartarus rather
/// than the saved `classic` setting. Cleared as soon as the user changes `classic`.
bool IsClassicForcedByCommandLine();

} // namespace tartarus
