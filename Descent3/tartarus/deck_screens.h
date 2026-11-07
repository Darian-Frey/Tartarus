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

// Deck versions of the small stock screens (F-069, D-016): message and edit
// boxes, the Keys help, the Guide-Bot menu, New Game, level warp, Pilots and
// the end-of-level review.
// Each hook asks here first; kDeckStock means run the stock screen, which is
// always the answer in Classic Mode and in multiplayer.

#pragma once

#include <filesystem>
#include <string>
#include <vector>

class newuiListBox;

namespace tartarus {

inline constexpr int kDeckStock = -2;

/// True if deck screens may replace stock ones here: not in Classic Mode or
/// multiplayer, and not before the renderer and settings are up.
bool Deck_Available();

/// DoMessageBox(): the stock return value (0 or 1), or kDeckStock.
int Deck_MessageBox(const char *title, const char *msg, int type);

/// DoEditDialog(): 1 for OK, 0 for cancelled, or kDeckStock.
int Deck_EditBox(const char *prompt, char *buffer, int buflen, bool showcancel);

/// HelpDisplay(): text ids in key, description pairs, ending in 0. False: run stock.
bool HelpDeck_Run(const int *text_ids);

/// BuddyDisplay(): the chosen command, -1 for none, or kDeckStock.
int BuddyDeck_Pick(const char *title, const std::vector<std::string> &commands);

/// MenuNewGame(): the chosen listbox row, -1 if cancelled, or kDeckStock.
int MissionDeck_Pick(newuiListBox *list, const std::vector<std::filesystem::path> &files);

/// DisplayLevelWarpDlg(): the chosen level (from 1), -1 if cancelled, or kDeckStock.
int LevelDeck_Pick(newuiListBox *list, int levels, const char *mission);

/// PilotSelect(): runs the whole screen. False: run stock.
bool PilotDeck_Run();

/// PostLevelResults() in single player: the Pilot Performance Review. False: run stock.
bool ResultsDeck_Run(bool success);

} // namespace tartarus
