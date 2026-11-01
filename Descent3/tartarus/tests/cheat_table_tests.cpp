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

#include <set>
#include <string>

#include <gtest/gtest.h>

#include "../cheat_table.h"

using namespace tartarus;

TEST(CheatTable, MatchesSpecRows) {
  const auto table = CheatTable();
  ASSERT_EQ(table.size(), kCheatCount);
  std::set<std::string> seen;
  for (size_t i = 0; i < table.size(); ++i) {
    EXPECT_EQ(static_cast<size_t>(table[i].id), i);
    const std::string pw = table[i].password;
    EXPECT_FALSE(pw.empty());
    for (char c : pw)
      EXPECT_TRUE(c >= 'A' && c <= 'Z') << pw;
    EXPECT_TRUE(seen.insert(pw).second) << "duplicate password " << pw;
  }
}

// AV-004: every unlock path marks the player as a cheater, except the relock row.
TEST(CheatTable, EveryRowButCatharsisMarksCheater) {
  for (const CheatInfo &row : CheatTable())
    EXPECT_EQ(row.marks_cheater, row.id != CheatId::Catharsis) << row.password;
}

TEST(CheatTable, PasswordsAreCaseInsensitiveLettersOnly) {
  ASSERT_NE(FindCheatByPassword("aegis"), nullptr);
  EXPECT_EQ(FindCheatByPassword("AeGiS")->id, CheatId::Aegis);
  EXPECT_EQ(FindCheatByPassword("  ambrosia \n")->id, CheatId::Ambrosia);
  EXPECT_EQ(FindCheatByPassword("aegis1"), nullptr);
  EXPECT_EQ(FindCheatByPassword("ae gis"), nullptr);
  EXPECT_EQ(FindCheatByPassword("aegi"), nullptr);
  EXPECT_EQ(FindCheatByPassword(""), nullptr);
  EXPECT_EQ(FindCheatByPassword("   "), nullptr);
}

TEST(CheatState, UnlockOnceThenAlready) {
  CheatState s;
  const CheatInfo *row = nullptr;
  EXPECT_EQ(s.Unlock("hydra", &row), CheatState::UnlockResult::Unlocked);
  ASSERT_NE(row, nullptr);
  EXPECT_EQ(row->id, CheatId::Hydra);
  EXPECT_TRUE(s.IsUnlocked(CheatId::Hydra));
  EXPECT_FALSE(s.IsUnlocked(CheatId::Aegis));
  EXPECT_EQ(s.Unlock("HYDRA"), CheatState::UnlockResult::AlreadyUnlocked);
  EXPECT_EQ(s.Unlock("nope", &row), CheatState::UnlockResult::Unknown);
  EXPECT_EQ(row, nullptr);
}

TEST(CheatState, TogglesNeedUnlockAndAToggleRow) {
  CheatState s;
  EXPECT_FALSE(s.SetActive(CheatId::Aegis, true)); // locked
  s.Unlock("aegis");
  EXPECT_TRUE(s.SetActive(CheatId::Aegis, true));
  EXPECT_TRUE(s.IsActive(CheatId::Aegis));
  s.Unlock("cornucopia");
  EXPECT_FALSE(s.SetActive(CheatId::Cornucopia, true)); // button
  s.Unlock("wraith");
  EXPECT_FALSE(s.SetActive(CheatId::Wraith, true)); // not implemented yet
  EXPECT_TRUE(s.SetActive(CheatId::Aegis, false));
  EXPECT_FALSE(s.AnyActive());
}

TEST(CheatState, GodModeHoldsEverything) {
  CheatState s;
  s.Unlock("ambrosia");
  s.SetActive(CheatId::Ambrosia, true);
  const CheatHolds h = s.Holds();
  EXPECT_TRUE(h.shields && h.energy && h.primary_ammo && h.secondary_ammo && h.afterburner && h.invulnerable);
  EXPECT_FALSE(h.cloak);
  EXPECT_FALSE(h.stats);
}

TEST(CheatState, SingleHolds) {
  CheatState s;
  for (const char *pw : {"aegis", "zephyr"})
    s.Unlock(pw);
  s.SetActive(CheatId::Aegis, true);
  s.SetActive(CheatId::Zephyr, true);
  const CheatHolds h = s.Holds();
  EXPECT_TRUE(h.shields);
  EXPECT_TRUE(h.afterburner);
  EXPECT_FALSE(h.energy || h.invulnerable || h.primary_ammo || h.secondary_ammo);
}

TEST(CheatState, TimeScaleClampsAndCountsAsActive) {
  CheatState s;
  EXPECT_FALSE(s.AnyActive());
  s.SetTimeScale(0.5f);
  EXPECT_FLOAT_EQ(s.TimeScale(), 0.5f);
  EXPECT_TRUE(s.AnyActive());
  s.SetTimeScale(100.0f);
  EXPECT_FLOAT_EQ(s.TimeScale(), kTimescaleMax);
  s.SetTimeScale(0.0f);
  EXPECT_FLOAT_EQ(s.TimeScale(), kTimescaleMin);
  s.DeactivateAll();
  EXPECT_FLOAT_EQ(s.TimeScale(), 1.0f);
}

TEST(CheatState, RelockClearsEverything) {
  CheatState s;
  s.Unlock("aegis");
  s.SetActive(CheatId::Aegis, true);
  s.SetTimeScale(0.3f);
  s.RelockAll();
  EXPECT_FALSE(s.IsUnlocked(CheatId::Aegis));
  EXPECT_FALSE(s.AnyActive());
}

TEST(CheatState, DeactivateKeepsUnlocks) {
  CheatState s;
  s.Unlock("aegis");
  s.SetActive(CheatId::Aegis, true);
  s.DeactivateAll();
  EXPECT_TRUE(s.IsUnlocked(CheatId::Aegis));
  EXPECT_FALSE(s.IsActive(CheatId::Aegis));
}

TEST(CheatState, NewPilotRelocks) {
  CheatState s;
  EXPECT_FALSE(s.SetOwner("bob"));
  s.Unlock("aegis");
  EXPECT_FALSE(s.SetOwner("bob"));
  EXPECT_TRUE(s.IsUnlocked(CheatId::Aegis));
  EXPECT_TRUE(s.SetOwner("alice"));
  EXPECT_FALSE(s.IsUnlocked(CheatId::Aegis));
}

TEST(CheatState, DebugBuildUnlocksAllButKeepsTogglesOff) {
  CheatState s;
  s.SetAllUnlocked(true);
  EXPECT_TRUE(s.IsUnlocked(CheatId::Thanatos));
  EXPECT_FALSE(s.AnyActive());
  EXPECT_TRUE(s.SetActive(CheatId::Shade, true));
}
