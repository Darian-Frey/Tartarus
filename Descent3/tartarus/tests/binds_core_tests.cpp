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

#include "../binds_core.h"
#include "../cmdparse.h"

using namespace tartarus;

namespace {
BindTable Table() { return BindTable({{"a", 0x1E}, {"f5", 0x3F}, {"kp7", 0x47}, {"space", 0x39}}); }
} // namespace

TEST(BindTable, ParsesNamesAndModifiers) {
  const BindTable t = Table();
  EXPECT_EQ(t.ParseKey("f5"), 0x3F);
  EXPECT_EQ(t.ParseKey("F5"), 0x3F);
  EXPECT_EQ(t.ParseKey("shift+f5"), 0x3F | kBindShift);
  EXPECT_EQ(t.ParseKey("Ctrl+Alt+KP7"), 0x47 | kBindCtrl | kBindAlt);
  EXPECT_EQ(t.ParseKey("control+a"), 0x1E | kBindCtrl);
  EXPECT_FALSE(t.ParseKey("hyper+a").has_value());
  EXPECT_FALSE(t.ParseKey("f13").has_value());
  EXPECT_FALSE(t.ParseKey("").has_value());
  EXPECT_FALSE(t.ParseKey("shift+").has_value());
}

TEST(BindTable, CanonicalNamesRoundTrip) {
  const BindTable t = Table();
  for (const char *s : {"a", "shift+f5", "ctrl+alt+kp7", "ctrl+alt+shift+space"}) {
    const auto key = t.ParseKey(s);
    ASSERT_TRUE(key.has_value()) << s;
    EXPECT_EQ(t.KeyToString(*key), s);
  }
  EXPECT_EQ(t.KeyToString(kBindShift | 0x3F), "shift+f5");
  EXPECT_EQ(t.KeyToString(0x99), "key153");
}

TEST(BindTable, BindFindUnbind) {
  BindTable t = Table();
  t.Bind(0x3F, "where");
  ASSERT_NE(t.Find(0x3F), nullptr);
  EXPECT_EQ(*t.Find(0x3F), "where");
  EXPECT_EQ(t.Find(0x3F | kBindShift), nullptr); // modifiers make a different key
  t.Bind(0x3F, "version");
  EXPECT_EQ(*t.Find(0x3F), "version");
  EXPECT_TRUE(t.Unbind(0x3F));
  EXPECT_FALSE(t.Unbind(0x3F));
  EXPECT_EQ(t.Find(0x3F), nullptr);
}

TEST(BindTable, SerializeParsesBack) {
  BindTable t = Table();
  t.Bind(0x3F | kBindShift, "echo \"hi there\"; where");
  t.Bind(0x1E, "toggle m_vj_indicator");
  const std::string text = t.Serialize();
  int lines = 0;
  size_t pos = 0;
  while ((pos = text.find("\nbind ", pos)) != std::string::npos) {
    const size_t end = text.find('\n', pos + 1);
    const CommandArgs args = ParseCommandLine(text.substr(pos + 1, end - pos - 1)).front();
    ASSERT_EQ(args.size(), 3u);
    const auto key = t.ParseKey(args[1]);
    ASSERT_TRUE(key.has_value());
    EXPECT_EQ(*t.Find(*key), args[2]);
    ++lines;
    pos = end;
  }
  EXPECT_EQ(lines, 2);
}
