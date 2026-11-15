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

// Guide-Bot commands (F-072): Next key, Door I can now open and Unexplored
// area, added to the deck Guide-Bot menu. Tartarus picks the target with a
// shortest-route search over rooms and terrain from the player's room (doors
// the player cannot open block it, as in AIFindAltPath()), then hands it to
// the Guide-Bot script as GBC_TARTARUS_GO_TO_OBJ or _ROOM (scripts/AIGame.cpp),
// which goes there as for its own Find commands. Deck only, so Classic Mode and
// multiplayer keep the stock menu and never send these.

#pragma once

struct gb_menu;

namespace tartarus {

/// Registers the guide console command. Called from Tartarus_Init().
void GuideBot_Init();

/// BuddyDisplay(): the deck Guide-Bot menu, with the Tartarus commands when the
/// bot is out of the ship. Runs a Tartarus command itself; otherwise returns
/// the chosen stock command, -1 for none, or kDeckStock to run the stock menu.
int GuideBot_RunMenu(const gb_menu &menu);

} // namespace tartarus
