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

// Tests for F-003 Steam install discovery (Descent3/tartarus/datadir_steam.cpp).

#include <gtest/gtest.h>

#include <algorithm>
#include <fstream>
#include <random>
#include <string>

#include "../datadir_steam.h"

namespace fs = std::filesystem;
using namespace tartarus::datadir;

namespace {

// A throwaway directory tree, removed when the fixture ends.
class SteamTree : public ::testing::Test {
protected:
  void SetUp() override {
    std::random_device rd;
    root_ = fs::temp_directory_path() / ("tartarus_datadir_test_" + std::to_string(rd()));
    fs::create_directories(root_);
  }
  void TearDown() override {
    std::error_code ec;
    fs::remove_all(root_, ec);
  }

  static void Touch(const fs::path &file, const std::string &content = "") {
    fs::create_directories(file.parent_path());
    std::ofstream(file, std::ios::binary) << content;
  }
  static fs::path Install(const fs::path &library) { return library / "steamapps" / "common" / "Descent 3"; }

  fs::path root_;
};

} // namespace

TEST(ParseLibraryFoldersVdf, ReadsEveryLibraryPath) {
  const char *vdf = R"("libraryfolders"
{
	"0"
	{
		"path"		"/home/pilot/.local/share/Steam"
		"label"		""
		"contentid"		"123"
		"apps"
		{
			"273590"		"978000000"
		}
	}
	"1"
	{
		"path"		"/mnt/games/SteamLibrary"
	}
})";
  const auto paths = ParseLibraryFoldersVdf(vdf);
  ASSERT_EQ(paths.size(), 2u);
  EXPECT_EQ(paths[0], fs::path("/home/pilot/.local/share/Steam"));
  EXPECT_EQ(paths[1], fs::path("/mnt/games/SteamLibrary"));
}

TEST(ParseLibraryFoldersVdf, DecodesEscapesAndIgnoresPathAsValue) {
  const char *vdf = R"("libraryfolders"
{
	// a comment with "path" "/not/this"
	"0"
	{
		"label"		"path"
		"PATH"		"/media/with \"quotes\" and \\ backslash"
	}
})";
  const auto paths = ParseLibraryFoldersVdf(vdf);
  ASSERT_EQ(paths.size(), 1u);
  EXPECT_EQ(paths[0], fs::path(R"(/media/with "quotes" and \ backslash)"));
}

TEST(ParseLibraryFoldersVdf, ToleratesEmptyAndTruncatedInput) {
  EXPECT_TRUE(ParseLibraryFoldersVdf("").empty());
  EXPECT_TRUE(ParseLibraryFoldersVdf(R"("libraryfolders" { "0" { "path" "/unterminated)").size() <= 1);
}

TEST(SteamRoots, HonoursXdgDataHome) {
  const auto with_xdg = SteamRoots("/home/p", "/data");
  const auto without = SteamRoots("/home/p", "");
  EXPECT_NE(std::find(with_xdg.begin(), with_xdg.end(), fs::path("/data/Steam")), with_xdg.end());
  EXPECT_NE(std::find(without.begin(), without.end(), fs::path("/home/p/.local/share/Steam")), without.end());
  EXPECT_EQ(without.front(), fs::path("/home/p/.steam/steam"));
}

TEST_F(SteamTree, IsD3InstallMatchesHogNameCaseInsensitively) {
  EXPECT_FALSE(IsD3Install(root_ / "missing"));
  EXPECT_FALSE(IsD3Install(root_));
  Touch(root_ / "D3.HOG");
  EXPECT_TRUE(IsD3Install(root_));
}

TEST_F(SteamTree, FindsInstallsInRootAndListedLibrariesWithoutDuplicates) {
  const fs::path steam = root_ / "Steam";
  const fs::path library = root_ / "SteamLibrary";
  Touch(Install(steam) / "d3.hog");
  Touch(Install(library) / "d3.hog");
  Touch(steam / "steamapps" / "libraryfolders.vdf",
        "\"libraryfolders\" { \"0\" { \"path\" \"" + steam.string() + "\" } \"1\" { \"path\" \"" + library.string() +
            "\" } }");
  // ~/.steam/steam is normally a symlink to the real root.
  fs::create_symlink(steam, root_ / "steam-link");

  const auto installs = FindSteamInstalls({root_ / "steam-link", steam, root_ / "does-not-exist"});
  ASSERT_EQ(installs.size(), 2u);
  EXPECT_EQ(installs[0], fs::canonical(Install(steam)));
  EXPECT_EQ(installs[1], fs::canonical(Install(library)));
}

TEST_F(SteamTree, SkipsLibrariesWithoutTheGame) {
  const fs::path steam = root_ / "Steam";
  fs::create_directories(Install(steam)); // folder exists, but no d3.hog
  EXPECT_TRUE(FindSteamInstalls({steam}).empty());
}
