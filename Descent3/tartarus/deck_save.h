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

// Deck-style save and load screens (F-069, D-016). Upstream's SaveGameDialog()
// and LoadGameDialog() call these first; the file format and the save itself
// stay upstream's. Classic Mode keeps the stock dialogs.

#pragma once

#include <filesystem>

namespace tartarus {

/// SaveDeck_PickLoad() result: Classic Mode, run the stock dialog.
inline constexpr int kSaveDeckStock = -2;

/// Runs the whole save screen. False in Classic Mode.
bool SaveDeck_Run();

/// Runs the load screen: the chosen slot, -1 if cancelled, or kSaveDeckStock.
int SaveDeck_PickLoad();

std::filesystem::path SaveDeck_SlotPath(int slot);

} // namespace tartarus
