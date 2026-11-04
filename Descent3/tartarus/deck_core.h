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

// Options deck model (F-069, D-016): pages and rows generated from cvar
// metadata. A curated table gives order, labels and choice names; any other
// saved cvar with a page's prefix appears on that page automatically, so a new
// setting needs no UI code. Pure standard C++, unit-tested; deck.cpp draws it.

#pragma once

#include <string>
#include <utility>
#include <vector>

#include "cvar.h"

namespace tartarus {

enum class DeckKind { Toggle, Slider, Choice, Action };
enum class DeckAction { None, StockOptions, KeyBindings, Done };

struct DeckRow {
  std::string label;
  std::string cvar; // empty for actions
  DeckKind kind = DeckKind::Slider;
  double min = 0, max = 1, step = 0.1;
  std::vector<std::pair<std::string, std::string>> choices; // shown text, cvar value
  std::string help;
  bool needs_restart = false; // takes effect when the game frame is rebuilt
  DeckAction action = DeckAction::None;
};

struct DeckPage {
  std::string name;
  std::vector<DeckRow> rows;
};

/// Curated entry. step 0 = derive from the cvar's range.
struct DeckRowDef {
  const char *cvar;
  const char *label;
  double step = 0;
  std::vector<std::pair<std::string, std::string>> choices = {};
  bool needs_restart = false;
};

struct DeckPageDef {
  const char *name;
  std::vector<DeckRowDef> rows;
  std::vector<const char *> auto_prefixes; // saved cvars with these prefixes are added after the curated rows
  std::vector<std::pair<const char *, DeckAction>> actions = {};
};

const std::vector<DeckPageDef> &DefaultDeckPages();

/// Curated cvars that are not registered are skipped.
std::vector<DeckPage> BuildDeckPages(const CvarRegistry &reg, const std::vector<DeckPageDef> &defs);

/// Gives a row its choices at run time (lists the table cannot know).
void DeckSetChoices(std::vector<DeckPage> &pages, const std::string &cvar,
                    std::vector<std::pair<std::string, std::string>> choices);

/// What the row shows for the cvar's current value.
std::string DeckValueText(const DeckRow &row, const Cvar &cv);
/// Slider position, 0 to 1.
double DeckFraction(const DeckRow &row, const Cvar &cv);
/// The value one step along (dir +1 / -1): toggles flip, choices wrap, sliders clamp.
std::string DeckNextValue(const DeckRow &row, const Cvar &cv, int dir);

} // namespace tartarus
