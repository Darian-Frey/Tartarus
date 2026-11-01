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

// Tests for the cvar system (F-011, Descent3/tartarus/cvar.cpp).

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <random>
#include <sstream>

#include "../cvar.h"

namespace fs = std::filesystem;
using namespace tartarus;

namespace {

class CvarTest : public ::testing::Test {
protected:
  void SetUp() override {
    std::random_device rd;
    dir_ = fs::temp_directory_path() / ("tartarus_cvar_test_" + std::to_string(rd()));
    fs::create_directories(dir_);
    cfg_ = dir_ / "tartarus.cfg";
    reg_ = std::make_unique<CvarRegistry>();
    reg_->SetPolicy(MakePolicy());
  }
  void TearDown() override {
    std::error_code ec;
    fs::remove_all(dir_, ec);
  }

  CvarPolicy MakePolicy() {
    CvarPolicy p;
    p.is_classic = [this] { return classic_; };
    p.cheat_allowed = [this](const Cvar &) { return cheats_; };
    p.log = [this](const std::string &m) { log_.push_back(m); };
    p.config_path = cfg_;
    return p;
  }

  std::string ReadCfg() const {
    std::ifstream in(cfg_);
    std::stringstream ss;
    ss << in.rdbuf();
    return ss.str();
  }

  fs::path dir_, cfg_;
  std::unique_ptr<CvarRegistry> reg_;
  bool classic_ = false;
  bool cheats_ = false;
  std::vector<std::string> log_;
};

} // namespace

TEST_F(CvarTest, RegistersTypedValuesWithDefaults) {
  Cvar *b = reg_->RegisterBool("hud_uptick", true, "Up-tick");
  Cvar *i = reg_->RegisterInt("con_lines", 2000, 100, 10000, "Scrollback");
  Cvar *f = reg_->RegisterFloat("cloak_cost", 30, 0, 200, "Energy");
  Cvar *s = reg_->RegisterString("map_filter", "all", "Filter");
  EXPECT_TRUE(b->GetBool());
  EXPECT_EQ(i->GetInt(), 2000);
  EXPECT_FLOAT_EQ(f->GetFloat(), 30.0f);
  EXPECT_EQ(s->GetString(), "all");
  EXPECT_EQ(reg_->Find("cloak_cost"), f);
  EXPECT_EQ(reg_->Find("nope"), nullptr);
  EXPECT_EQ(reg_->RegisterFloat("cloak_cost", 99, 0, 1, "again"), f) << "re-registering returns the original";
  EXPECT_FLOAT_EQ(f->GetFloat(), 30.0f);
  EXPECT_EQ(reg_->RegisterInt("cloak_cost", 1, 0, 2, "wrong type"), nullptr);
}

TEST_F(CvarTest, ParsesAndValidatesPerType) {
  reg_->RegisterBool("b", false, "");
  reg_->RegisterInt("i", 5, 0, 10, "");
  reg_->RegisterFloat("f", 1, 0, 2, "");
  for (const char *yes : {"1", "true", "ON", "Yes"}) {
    EXPECT_EQ(reg_->Set("b", yes), CvarSetResult::Ok) << yes;
    EXPECT_TRUE(reg_->Find("b")->GetBool());
    reg_->Set("b", "0");
  }
  EXPECT_EQ(reg_->Set("b", "maybe"), CvarSetResult::Invalid);
  EXPECT_EQ(reg_->Set("i", "7"), CvarSetResult::Ok);
  EXPECT_EQ(reg_->Set("i", "7.5"), CvarSetResult::Invalid);
  EXPECT_EQ(reg_->Set("i", "abc"), CvarSetResult::Invalid);
  EXPECT_EQ(reg_->Set("i", "99"), CvarSetResult::Clamped);
  EXPECT_EQ(reg_->Find("i")->GetInt(), 10);
  EXPECT_EQ(reg_->Set("f", "0.25"), CvarSetResult::Ok);
  EXPECT_FLOAT_EQ(reg_->Find("f")->GetFloat(), 0.25f);
  EXPECT_EQ(reg_->Set("f", "-3"), CvarSetResult::Clamped);
  EXPECT_FLOAT_EQ(reg_->Find("f")->GetFloat(), 0.0f);
  EXPECT_EQ(reg_->Set("f", "nan"), CvarSetResult::Invalid);
  EXPECT_EQ(reg_->Set("f", "inf"), CvarSetResult::Invalid);
  EXPECT_EQ(reg_->Set("f", "1x"), CvarSetResult::Invalid);
  EXPECT_EQ(reg_->Set("missing", "1"), CvarSetResult::Unknown);
}

TEST_F(CvarTest, ReadOnlyAndCheatFlagsApplyToUsersNotCode) {
  reg_->RegisterInt("version", 1, 0, 9, "", CVAR_READONLY);
  reg_->RegisterBool("god", false, "", CVAR_CHEAT);
  EXPECT_EQ(reg_->Set("version", "2"), CvarSetResult::ReadOnly);
  EXPECT_EQ(reg_->Set("version", "2", CvarSource::Code), CvarSetResult::Ok);
  EXPECT_EQ(reg_->Set("god", "1"), CvarSetResult::CheatLocked);
  EXPECT_FALSE(reg_->Find("god")->GetBool());
  cheats_ = true;
  EXPECT_EQ(reg_->Set("god", "1"), CvarSetResult::Ok);
  EXPECT_TRUE(reg_->Find("god")->GetBool());
}

TEST_F(CvarTest, ClassicLockedReadsDefaultAndRefusesUserChanges) {
  Cvar *c = reg_->RegisterFloat("cloak_cost", 30, 0, 200, "", CVAR_CLASSIC_LOCKED);
  ASSERT_EQ(reg_->Set("cloak_cost", "50"), CvarSetResult::Ok);
  classic_ = true;
  EXPECT_FLOAT_EQ(c->GetFloat(), 30.0f) << "Classic Mode reads the default (AV-001)";
  EXPECT_EQ(c->StoredString(), "50") << "the stored value survives";
  EXPECT_EQ(reg_->Set("cloak_cost", "60"), CvarSetResult::ClassicLocked);
  classic_ = false;
  EXPECT_FLOAT_EQ(c->GetFloat(), 50.0f) << "leaving Classic Mode restores the user's value";
}

TEST_F(CvarTest, LatchedValuesWaitForApplyLatched) {
  int calls = 0;
  Cvar *c = reg_->RegisterInt("r_detail", 1, 0, 3, "", CVAR_LATCH);
  c->AddChangeCallback([&](const Cvar &) { ++calls; });
  EXPECT_EQ(reg_->Set("r_detail", "3"), CvarSetResult::Latched);
  EXPECT_EQ(c->GetInt(), 1);
  EXPECT_EQ(c->LatchedString(), "3");
  EXPECT_EQ(calls, 0);
  reg_->ApplyLatched();
  EXPECT_EQ(c->GetInt(), 3);
  EXPECT_FALSE(c->LatchedString());
  EXPECT_EQ(calls, 1);
}

TEST_F(CvarTest, ChangeCallbacksFireOnlyOnRealChanges) {
  int calls = 0;
  Cvar *c = reg_->RegisterBool("x", false, "");
  c->AddChangeCallback([&](const Cvar &cv) {
    ++calls;
    EXPECT_TRUE(cv.GetBool());
  });
  reg_->Set("x", "1");
  reg_->Set("x", "on");
  EXPECT_EQ(calls, 1);
}

TEST_F(CvarTest, ListFiltersByPrefixInNameOrder) {
  reg_->RegisterInt("gw_time", 1, 0, 9, "");
  reg_->RegisterInt("gw_force", 1, 0, 9, "");
  reg_->RegisterInt("cloak_cost", 1, 0, 9, "");
  const auto gw = reg_->List("gw_");
  ASSERT_EQ(gw.size(), 2u);
  EXPECT_EQ(gw[0]->Name(), "gw_force");
  EXPECT_EQ(gw[1]->Name(), "gw_time");
  EXPECT_EQ(reg_->List().size(), 3u);
}

TEST_F(CvarTest, ConfigRoundTripStoresOnlyNonDefaultArchiveValues) {
  reg_->RegisterFloat("cloak_cost", 30, 0, 200, "", CVAR_ARCHIVE);
  reg_->RegisterString("map_filter", "all", "", CVAR_ARCHIVE);
  reg_->RegisterInt("session_only", 1, 0, 9, "");
  reg_->Set("cloak_cost", "42.5");      // autosaves
  reg_->Set("map_filter", "keys,doors"); // value with a comma survives
  reg_->Set("session_only", "5");       // not ARCHIVE: never written
  const std::string cfg = ReadCfg();
  EXPECT_NE(cfg.find("set cloak_cost 42.5"), std::string::npos) << cfg;
  EXPECT_NE(cfg.find("set map_filter keys,doors"), std::string::npos) << cfg;
  EXPECT_EQ(cfg.find("session_only"), std::string::npos) << cfg;
  EXPECT_FALSE(fs::exists(fs::path(cfg_) += ".tmp")) << "temp file is renamed away";

  CvarRegistry fresh;
  fresh.SetPolicy(MakePolicy());
  ASSERT_TRUE(fresh.LoadConfig());
  EXPECT_FLOAT_EQ(fresh.RegisterFloat("cloak_cost", 30, 0, 200, "", CVAR_ARCHIVE)->GetFloat(), 42.5f)
      << "values loaded before registration are applied at registration";
  EXPECT_EQ(fresh.RegisterString("map_filter", "all", "", CVAR_ARCHIVE)->GetString(), "keys,doors");
}

TEST_F(CvarTest, ConfigKeepsUnknownEntriesAndReportsBadLines) {
  {
    std::ofstream out(cfg_);
    out << "// comment\n"
           "set future_module_value 7\n"
           "set cloak_cost banana\n"
           "bind F6 quit\n"
           "set god 1\n";
  }
  reg_->RegisterFloat("cloak_cost", 30, 0, 200, "", CVAR_ARCHIVE);
  reg_->RegisterBool("god", false, "", CVAR_ARCHIVE | CVAR_CHEAT);
  ASSERT_TRUE(reg_->LoadConfig());
  EXPECT_FLOAT_EQ(reg_->Find("cloak_cost")->GetFloat(), 30.0f);
  EXPECT_FALSE(reg_->Find("god")->GetBool()) << "the config cannot switch on a cheat (AV-004)";
  EXPECT_GE(log_.size(), 3u);

  reg_->Set("cloak_cost", "31"); // triggers a save
  EXPECT_NE(ReadCfg().find("set future_module_value 7"), std::string::npos)
      << "settings of modules not registered this run are preserved";
}

TEST_F(CvarTest, MissingConfigIsFirstRunNotAnError) {
  EXPECT_TRUE(reg_->LoadConfig());
  EXPECT_TRUE(log_.empty());
}

TEST_F(CvarTest, DescribeShowsValueDefaultAndLocks) {
  reg_->RegisterFloat("cloak_cost", 30, 0, 200, "Energy charged on activation", CVAR_ARCHIVE | CVAR_CLASSIC_LOCKED);
  reg_->Set("cloak_cost", "45");
  classic_ = true;
  const std::string d = reg_->Find("cloak_cost")->Describe();
  EXPECT_NE(d.find("cloak_cost = 45"), std::string::npos) << d;
  EXPECT_NE(d.find("default 30"), std::string::npos) << d;
  EXPECT_NE(d.find("Energy charged on activation"), std::string::npos) << d;
  EXPECT_NE(d.find("Classic Mode"), std::string::npos) << d;
}
