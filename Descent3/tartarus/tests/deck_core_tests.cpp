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

#include <gtest/gtest.h>

#include "../deck_core.h"

using namespace tartarus;

namespace {

const DeckRow *FindRow(const std::vector<DeckPage> &pages, const std::string &cvar) {
  for (const DeckPage &p : pages)
    for (const DeckRow &r : p.rows)
      if (r.cvar == cvar)
        return &r;
  return nullptr;
}

std::vector<DeckPageDef> Defs() {
  return {{"GRAPHICS",
           {{"r_fov", "Field of view", 5},
            {"r_vsync", "Vertical sync"},
            {"r_msaa", "Anti-aliasing", 0, {{"OFF", "0"}, {"4X", "4"}}, true},
            {"r_missing", "Not registered"}},
           {"r_"}},
          {"TARTARUS", {}, {"ui_"}, {{"Done", DeckAction::Done}}}};
}

} // namespace

class DeckFixture : public ::testing::Test {
protected:
  void SetUp() override {
    reg.RegisterFloat("r_fov", 72, 60, 120, "Field of view", CVAR_NONE);
    reg.RegisterBool("r_vsync", true, "Sync");
    reg.RegisterInt("r_msaa", 0, 0, 16, "AA", CVAR_ARCHIVE);
    reg.RegisterInt("r_newthing", 5, 0, 10, "A new saved setting", CVAR_ARCHIVE);
    reg.RegisterInt("r_unsaved", 5, 0, 10, "Not saved", CVAR_NONE);
    reg.RegisterInt("r_cheaty", 5, 0, 10, "Cheat", CVAR_ARCHIVE | CVAR_CHEAT);
    reg.RegisterBool("ui_flag", false, "Flag", CVAR_ARCHIVE);
  }
  CvarRegistry reg;
};

TEST_F(DeckFixture, CuratedRowsKeepOrderAndSkipMissing) {
  const auto pages = BuildDeckPages(reg, Defs());
  ASSERT_EQ(pages.size(), 2u);
  ASSERT_GE(pages[0].rows.size(), 3u);
  EXPECT_EQ(pages[0].rows[0].label, "Field of view");
  EXPECT_EQ(pages[0].rows[1].kind, DeckKind::Toggle);
  EXPECT_EQ(pages[0].rows[2].kind, DeckKind::Choice);
  EXPECT_TRUE(pages[0].rows[2].needs_restart);
  EXPECT_EQ(FindRow(pages, "r_missing"), nullptr);
}

TEST_F(DeckFixture, NewSavedSettingsAppearAutomatically) {
  const auto pages = BuildDeckPages(reg, Defs());
  const DeckRow *row = FindRow(pages, "r_newthing");
  ASSERT_NE(row, nullptr);
  EXPECT_EQ(row->kind, DeckKind::Slider);
  EXPECT_EQ(row->help, "A new saved setting");
  EXPECT_EQ(FindRow(pages, "r_unsaved"), nullptr);
  EXPECT_EQ(FindRow(pages, "r_cheaty"), nullptr); // cheats never show
  EXPECT_NE(FindRow(pages, "ui_flag"), nullptr);
}

TEST_F(DeckFixture, ActionsComeLast) {
  const auto pages = BuildDeckPages(reg, Defs());
  EXPECT_EQ(pages[1].rows.back().action, DeckAction::Done);
  EXPECT_EQ(pages[1].rows.back().kind, DeckKind::Action);
}

TEST_F(DeckFixture, SliderStepsSnapAndClamp) {
  const auto pages = BuildDeckPages(reg, Defs());
  const DeckRow *fov = FindRow(pages, "r_fov");
  const Cvar *cv = reg.Find("r_fov");
  EXPECT_EQ(DeckNextValue(*fov, *cv, 1), "75.00"); // 72 snaps to the grid of 5 from 60
  reg.Set("r_fov", "120", CvarSource::Code);
  EXPECT_EQ(DeckNextValue(*fov, *cv, 1), "120.00");
  EXPECT_DOUBLE_EQ(DeckFraction(*fov, *cv), 1.0);
  reg.Set("r_fov", "60", CvarSource::Code);
  EXPECT_EQ(DeckNextValue(*fov, *cv, -1), "60.00");
  EXPECT_DOUBLE_EQ(DeckFraction(*fov, *cv), 0.0);
}

TEST_F(DeckFixture, ChoicesWrapAndNameTheNearestValue) {
  const auto pages = BuildDeckPages(reg, Defs());
  const DeckRow *aa = FindRow(pages, "r_msaa");
  const Cvar *cv = reg.Find("r_msaa");
  EXPECT_EQ(DeckValueText(*aa, *cv), "OFF");
  EXPECT_EQ(DeckNextValue(*aa, *cv, 1), "4");
  EXPECT_EQ(DeckNextValue(*aa, *cv, -1), "4");
  reg.Set("r_msaa", "3", CvarSource::Code); // set from the console, not a listed choice
  EXPECT_EQ(DeckValueText(*aa, *cv), "4X");
}

TEST_F(DeckFixture, TogglesFlip) {
  const auto pages = BuildDeckPages(reg, Defs());
  const DeckRow *v = FindRow(pages, "r_vsync");
  const Cvar *cv = reg.Find("r_vsync");
  EXPECT_EQ(DeckValueText(*v, *cv), "ON");
  EXPECT_EQ(DeckNextValue(*v, *cv, 1), "0");
}
