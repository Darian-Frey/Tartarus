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

// Energy <-> shield converter (F-030, SPEC.md §8): hold conv_to_shields_key
// (B) or conv_to_energy_key (N) to turn one into the other at a loss.
// Tartarus mode, single player; nothing in Classic Mode (AV-001).

#pragma once

namespace tartarus {

/// Registers the conv_ cvars. Called from Tartarus_Init().
void Converter_Init();

/// Per frame, from Frame(): converts while a key is held.
void Converter_Frame();

/// For the flight HUD's status words: converting this frame.
bool Converter_Active();

} // namespace tartarus
