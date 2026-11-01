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

// Tests for the console/config tokenizer (Descent3/tartarus/cmdparse.cpp).

#include <gtest/gtest.h>

#include "../cmdparse.h"

using tartarus::CommandArgs;
using tartarus::ParseCommandLine;
using tartarus::QuoteArg;

TEST(ParseCommandLine, SplitsWordsAndCommands) {
  const auto cmds = ParseCommandLine("  set cloak_cost 30 ;give all;  ;quit ");
  ASSERT_EQ(cmds.size(), 3u);
  EXPECT_EQ(cmds[0], (CommandArgs{"set", "cloak_cost", "30"}));
  EXPECT_EQ(cmds[1], (CommandArgs{"give", "all"}));
  EXPECT_EQ(cmds[2], (CommandArgs{"quit"}));
}

TEST(ParseCommandLine, QuotedArgumentsKeepSpacesSemicolonsAndComments) {
  const auto cmds = ParseCommandLine(R"(bind F6 "classic 0; say \"hi\" // not a comment" // a comment)");
  ASSERT_EQ(cmds.size(), 1u);
  EXPECT_EQ(cmds[0], (CommandArgs{"bind", "F6", R"(classic 0; say "hi" // not a comment)"}));
}

TEST(ParseCommandLine, CommentsAndBlankLinesYieldNothing) {
  EXPECT_TRUE(ParseCommandLine("").empty());
  EXPECT_TRUE(ParseCommandLine("   \t ").empty());
  EXPECT_TRUE(ParseCommandLine("// set x 1").empty());
  EXPECT_EQ(ParseCommandLine("set x 1// trailing").at(0), (CommandArgs{"set", "x", "1"}));
}

TEST(ParseCommandLine, EmptyQuotedArgumentAndUnterminatedQuote) {
  EXPECT_EQ(ParseCommandLine(R"(set name "")").at(0), (CommandArgs{"set", "name", ""}));
  EXPECT_EQ(ParseCommandLine(R"(echo "open ended)").at(0), (CommandArgs{"echo", "open ended"}));
}

TEST(QuoteArg, RoundTripsThroughTheParser) {
  for (const std::string s : {"plain", "", "two words", "semi;colon", R"(q"uote)", R"(back\slash)", "a//b", "0.2 0.8 1"}) {
    const auto cmds = ParseCommandLine("set x " + QuoteArg(s));
    ASSERT_EQ(cmds.size(), 1u) << s;
    ASSERT_EQ(cmds[0].size(), 3u) << s;
    EXPECT_EQ(cmds[0][2], s);
  }
  EXPECT_EQ(QuoteArg("plain"), "plain");
}
