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

// Tests for the console logic (F-010, Descent3/tartarus/console_core.cpp).

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <random>
#include <thread>

#include "../console_core.h"

namespace fs = std::filesystem;
using namespace tartarus;

namespace {

class ConsoleTest : public ::testing::Test {
protected:
  void SetUp() override {
    CvarPolicy p;
    p.is_classic = [this] { return classic_; };
    p.cheat_allowed = [](const Cvar &) { return false; };
    cvars_.SetPolicy(p);
    cvars_.RegisterFloat("cloak_cost", 30, 0, 200, "Energy charged on activation", CVAR_CLASSIC_LOCKED);
    cvars_.RegisterFloat("cloak_drain", 8, 0, 100, "Energy per second");
    cvars_.RegisterBool("hud_uptick", true, "Reticle up-tick");
    cvars_.RegisterString("map_visited_colour", "0.2 0.8 1.0", "Colour");
  }

  std::string Last() const { return con_.Lines().empty() ? "" : con_.Lines().back(); }
  bool Printed(std::string_view needle) const {
    for (const auto &l : con_.Lines())
      if (l.find(needle) != std::string::npos)
        return true;
    return false;
  }
  void Type(std::string_view s) { con_.InsertText(s); }

  bool classic_ = false;
  CvarRegistry cvars_;
  ConsoleCore con_{cvars_};
};

} // namespace

TEST_F(ConsoleTest, CvarNameAloneDescribesAndNameValueSets) {
  con_.Execute("cloak_cost");
  EXPECT_NE(Last().find("cloak_cost = 30"), std::string::npos) << Last();
  con_.Execute("cloak_cost 45");
  EXPECT_FLOAT_EQ(cvars_.Find("cloak_cost")->GetFloat(), 45.0f);
  con_.Execute("map_visited_colour 1 0 0"); // multi-word values are joined
  EXPECT_EQ(cvars_.Find("map_visited_colour")->GetString(), "1 0 0");
  con_.Execute("set cloak_drain 9; reset cloak_cost; toggle hud_uptick");
  EXPECT_FLOAT_EQ(cvars_.Find("cloak_drain")->GetFloat(), 9.0f);
  EXPECT_FLOAT_EQ(cvars_.Find("cloak_cost")->GetFloat(), 30.0f);
  EXPECT_FALSE(cvars_.Find("hud_uptick")->GetBool());
}

TEST_F(ConsoleTest, ReportsErrorsReadably) {
  con_.Execute("cloak_cost banana");
  EXPECT_NE(Last().find("invalid value"), std::string::npos) << Last();
  con_.Execute("nosuchthing");
  EXPECT_NE(Last().find("Unknown command or cvar: nosuchthing"), std::string::npos) << Last();
  classic_ = true;
  con_.Execute("cloak_cost 50");
  EXPECT_NE(Last().find("Classic Mode"), std::string::npos) << Last();
}

TEST_F(ConsoleTest, CheatCommandsAreRefusedUnlessAllowed) {
  int runs = 0;
  con_.AddCommand({"god", "god <0|1>", "Invulnerable", CMD_CHEAT, [&](const CommandArgs &, ConsoleCore &) { ++runs; }});
  con_.Execute("god 1");
  EXPECT_EQ(runs, 0);
  EXPECT_NE(Last().find("cheat"), std::string::npos);
  con_.cheat_allowed = [](const ConsoleCommand &) { return true; };
  con_.Execute("GOD 1"); // commands are case-insensitive
  EXPECT_EQ(runs, 1);
}

TEST_F(ConsoleTest, HelpFindAndCvarlist) {
  con_.Execute("help");
  EXPECT_TRUE(Printed("cvarlist [prefix]"));
  con_.Execute("help cloak_drain");
  EXPECT_TRUE(Printed("Energy per second"));
  con_.Execute("find cloak");
  EXPECT_NE(Last().find("2 matches"), std::string::npos) << Last();
  con_.Clear();
  con_.Execute("cvarlist hud_");
  EXPECT_NE(Last().find("1 cvar"), std::string::npos) << Last();
}

TEST_F(ConsoleTest, LineEditing) {
  Type("cloak");
  con_.MoveCursor(-5);
  Type("x");
  EXPECT_EQ(con_.Input(), "xcloak");
  con_.DeleteForward();
  EXPECT_EQ(con_.Input(), "xloak");
  con_.Backspace();
  EXPECT_EQ(con_.Input(), "loak");
  con_.MoveCursor(-100);
  EXPECT_EQ(con_.Cursor(), 0u);
  con_.CursorEnd();
  EXPECT_EQ(con_.Cursor(), 4u);
}

TEST_F(ConsoleTest, SubmitEchoesRecordsHistoryAndBrowses) {
  Type("cloak_drain 3");
  con_.Submit();
  EXPECT_TRUE(Printed("] cloak_drain 3"));
  EXPECT_EQ(con_.Input(), "");
  Type("echo hi");
  con_.Submit();
  Type("draft");
  con_.HistoryPrev();
  EXPECT_EQ(con_.Input(), "echo hi");
  con_.HistoryPrev();
  EXPECT_EQ(con_.Input(), "cloak_drain 3");
  con_.HistoryPrev(); // at the oldest: stays
  EXPECT_EQ(con_.Input(), "cloak_drain 3");
  con_.HistoryNext();
  con_.HistoryNext();
  EXPECT_EQ(con_.Input(), "draft") << "the unfinished line comes back";
  EXPECT_EQ(con_.History().size(), 2u);
  con_.ClearInput();
  Type("echo hi");
  con_.Submit();
  EXPECT_EQ(con_.History().size(), 2u) << "immediate repeats are not stored twice";
}

TEST_F(ConsoleTest, HistoryIsCapped) {
  std::vector<std::string> h;
  for (int i = 0; i < 300; ++i)
    h.push_back("echo " + std::to_string(i));
  con_.SetHistory(h);
  EXPECT_EQ(con_.History().size(), ConsoleCore::kMaxHistory);
  EXPECT_EQ(con_.History().back(), "echo 299");
}

TEST_F(ConsoleTest, TabCompletesUniqueAndCommonPrefixes) {
  Type("hud_u");
  con_.Complete();
  EXPECT_EQ(con_.Input(), "hud_uptick ");
  con_.ClearInput();
  Type("clo");
  con_.Complete();
  EXPECT_EQ(con_.Input(), "cloak_") << "extends to the longest common prefix";
  EXPECT_TRUE(Printed("cloak_cost")) << "and lists the candidates";
  con_.ClearInput();
  Type("help cloak_d");
  con_.Complete();
  EXPECT_EQ(con_.Input(), "help cloak_drain ");
  con_.ClearInput();
  Type("echo 1; cvarl");
  con_.Complete();
  EXPECT_EQ(con_.Input(), "echo 1; cvarlist ");
  con_.ClearInput();
  Type("cloak_cost 3");
  con_.Complete(); // values are not completed
  EXPECT_EQ(con_.Input(), "cloak_cost 3");
}

TEST_F(ConsoleTest, ScrollbackIsCappedAndScrollable) {
  con_.SetMaxLines(5);
  for (int i = 0; i < 8; ++i)
    con_.Print("line " + std::to_string(i));
  con_.Pump();
  ASSERT_EQ(con_.Lines().size(), 5u);
  EXPECT_EQ(con_.Lines().front(), "line 3");
  con_.Scroll(2);
  EXPECT_EQ(con_.ScrollOffset(), 2);
  con_.Print("new");
  con_.Pump();
  EXPECT_EQ(con_.ScrollOffset(), 3) << "the view stays put while scrolled back";
  con_.Scroll(100);
  EXPECT_EQ(con_.ScrollOffset(), 4);
  con_.ScrollToBottom();
  EXPECT_EQ(con_.ScrollOffset(), 0);
}

TEST_F(ConsoleTest, PrintIsThreadSafeAndSplitsLines) {
  std::thread t([this] {
    for (int i = 0; i < 500; ++i)
      con_.Print("bg");
  });
  for (int i = 0; i < 500; ++i)
    con_.Print("fg");
  t.join();
  con_.Pump();
  EXPECT_EQ(con_.Lines().size(), 1000u);
  con_.Clear();
  con_.Print("a\nb\n");
  con_.Pump();
  EXPECT_EQ(con_.Lines().size(), 2u);
}

TEST_F(ConsoleTest, ExecRunsScriptsRelativeToTheExecDirAndLimitsNesting) {
  std::random_device rd;
  const fs::path dir = fs::temp_directory_path() / ("tartarus_con_test_" + std::to_string(rd()));
  fs::create_directories(dir);
  std::ofstream(dir / "tune.cfg") << "// tuning\ncloak_drain 12\nset cloak_cost 31 ; echo done\n";
  std::ofstream(dir / "loop.cfg") << "exec loop.cfg\n";
  con_.SetExecDirectory(dir);
  con_.Execute("exec tune.cfg");
  EXPECT_FLOAT_EQ(cvars_.Find("cloak_drain")->GetFloat(), 12.0f);
  EXPECT_FLOAT_EQ(cvars_.Find("cloak_cost")->GetFloat(), 31.0f);
  EXPECT_TRUE(Printed("done"));
  con_.Execute("exec loop.cfg");
  EXPECT_TRUE(Printed("nested too deeply"));
  con_.Execute("exec missing.cfg");
  EXPECT_TRUE(Printed("cannot open"));
  std::error_code ec;
  fs::remove_all(dir, ec);
}
