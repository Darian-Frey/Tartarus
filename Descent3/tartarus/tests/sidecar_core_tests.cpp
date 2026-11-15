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

#include <fstream>
#include <sstream>

#include <gtest/gtest.h>

#include "../sidecar_core.h"

using namespace tartarus;

namespace {

// The stock weapon names (Static_weapon_names in weapon.cpp) plus a few others
// the table has: enough to check collisions and bases.
const std::vector<std::string> kStock = {"Laser",      "Vauss",         "Microwave",   "Plasma",  "Fusion",
                                         "Super Laser", "Mass Driver",  "Napalm",      "EMD Gun", "Omega",
                                         "Concussion", "Homing",        "Impact Mortar", "Smart", "Mega",
                                         "Frag",       "Guided",        "Napalm Rocket", "Cyclone", "Black Shark",
                                         "Yellow Flare"};

} // namespace

TEST(Sidecar, ParsesBlocksKeysAndQuotedValues) {
  const SidecarParse p = ParseSidecar("// a comment\n"
                                      "weapon \"Test One\" {\n"
                                      "  base \"Concussion\"   # trailing comment\n"
                                      "  tint 0.5 0.25 1\n"
                                      "}\n");
  ASSERT_TRUE(p.errors.empty()) << p.errors[0];
  ASSERT_EQ(p.weapons.size(), 1u);
  EXPECT_EQ(p.weapons[0].name, "Test One");
  EXPECT_EQ(p.weapons[0].line, 2);
  ASSERT_NE(p.weapons[0].Find("base"), nullptr);
  EXPECT_EQ(p.weapons[0].Find("base")->values[0], "Concussion");
  ASSERT_EQ(p.weapons[0].Find("tint")->values.size(), 3u);
  EXPECT_EQ(p.weapons[0].Find("tint")->values[2], "1");
  EXPECT_EQ(p.weapons[0].Find("missing"), nullptr);
}

TEST(Sidecar, BadBlockIsLeftOutAndTheRestStillLoad) {
  const SidecarParse p = ParseSidecar("weapon \"Bad\" {\n"
                                      "  base \"Frag\n" // quote not closed
                                      "}\n"
                                      "weapon \"Good\" {\n"
                                      "  base Frag\n"
                                      "}\n");
  ASSERT_EQ(p.weapons.size(), 1u);
  EXPECT_EQ(p.weapons[0].name, "Good");
  ASSERT_GE(p.errors.size(), 2u);
  EXPECT_NE(p.errors[0].find("line 2"), std::string::npos);
}

TEST(Sidecar, ReportsKeysWithoutValuesAndUnclosedBlocks) {
  SidecarParse p = ParseSidecar("weapon \"A\" {\n  speed\n}\n");
  EXPECT_TRUE(p.weapons.empty());
  EXPECT_FALSE(p.errors.empty());
  p = ParseSidecar("weapon \"B\" {\n  base Frag\n");
  EXPECT_TRUE(p.weapons.empty());
  ASSERT_EQ(p.errors.size(), 1u);
  EXPECT_NE(p.errors[0].find("not closed"), std::string::npos);
  p = ParseSidecar("speed 70\n");
  EXPECT_FALSE(p.errors.empty());
}

TEST(Sidecar, ChecksNamesBasesClassesAndFlags) {
  const SidecarParse p = ParseSidecar("weapon \"Frag\" {\n base Concussion\n}\n"          // stock name
                                      "weapon \"NoBase\" {\n class tactical\n}\n"          // no base
                                      "weapon \"Ghost\" {\n base Nothing\n}\n"             // base missing
                                      "weapon \"Slot\" {\n base Frag\n class secondary\n}\n" // no slot free
                                      "weapon \"Flag\" {\n base Frag\n flags TWF_NOPE\n}\n"
                                      "weapon \"Twice\" {\n base Frag\n}\n"
                                      "weapon \"twice\" {\n base Frag\n}\n" // same name, other case
                                      "weapon \"Fine\" {\n base frag\n class tactical\n flags TWF_GRAVWELL\n}\n");
  ASSERT_TRUE(p.errors.empty());
  const std::vector<std::string> problems = CheckSidecar(p, kStock);
  auto mentions = [&](const std::string &name) {
    for (const std::string &s : problems)
      if (s.find("\"" + name + "\"") != std::string::npos)
        return true;
    return false;
  };
  EXPECT_TRUE(mentions("Frag"));
  EXPECT_TRUE(mentions("NoBase"));
  EXPECT_TRUE(mentions("Ghost"));
  EXPECT_TRUE(mentions("Slot"));
  EXPECT_TRUE(mentions("Flag"));
  EXPECT_FALSE(mentions("Twice"));
  EXPECT_TRUE(mentions("twice"));
  EXPECT_FALSE(mentions("Fine"));
}

TEST(Sidecar, FlagsAndNumbers) {
  EXPECT_EQ(TartarusFlagBit("TWF_GRAVWELL"), TWF_GRAVWELL);
  EXPECT_EQ(TartarusFlagBit("twf_gravwell"), 0u);
  float v = 0;
  EXPECT_TRUE(SidecarNumber("1.25", &v));
  EXPECT_FLOAT_EQ(v, 1.25f);
  EXPECT_FALSE(SidecarNumber("12abc", &v));
  EXPECT_FALSE(SidecarNumber("", &v));
}

// AV-010: the file the game ships loads cleanly against the stock names.
TEST(Sidecar, ShippedFileLoadsCleanly) {
  std::ifstream in(TARTARUS_SOURCE_DIR "/data/tartarus/weapons.txt");
  ASSERT_TRUE(in.good());
  std::stringstream text;
  text << in.rdbuf();
  const SidecarParse p = ParseSidecar(text.str());
  EXPECT_TRUE(p.errors.empty()) << p.errors[0];
  EXPECT_TRUE(CheckSidecar(p, kStock).empty()) << CheckSidecar(p, kStock)[0];
  ASSERT_FALSE(p.weapons.empty());
  EXPECT_EQ(p.weapons[0].name, "GravMissile");
}
