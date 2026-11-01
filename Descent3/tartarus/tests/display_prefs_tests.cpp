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

// Tests for display-setting persistence (BUG-004, BUG-005).

#include <gtest/gtest.h>

#include "../display_prefs.h"

using namespace tartarus;

TEST(ChooseResolutionIndex, FindsTheSavedSizeWhereverItIsInTheList) {
  // The owner's display list (SDL, sorted): index 7 is 512x384, not 1280x720.
  const std::vector<ScreenSize> modes = {{320, 180}, {320, 240}, {360, 202}, {400, 300}, {432, 243},
                                         {480, 270}, {512, 288}, {512, 384}, {1280, 720}, {1920, 1080}};
  EXPECT_EQ(ChooseResolutionIndex(modes, ScreenSize{1280, 720}, 9), 8);
  EXPECT_EQ(ChooseResolutionIndex(modes, ScreenSize{1920, 1080}, 0), 9);
}

TEST(ChooseResolutionIndex, FallsBackToTheDefaultWhenUnsavedOrUnavailable) {
  const std::vector<ScreenSize> modes = {{640, 480}, {1280, 720}, {1920, 1080}};
  EXPECT_EQ(ChooseResolutionIndex(modes, std::nullopt, 2), 2);
  EXPECT_EQ(ChooseResolutionIndex(modes, ScreenSize{2560, 1440}, 2), 2) << "monitor changed";
  EXPECT_EQ(ChooseResolutionIndex(modes, std::nullopt, 99), 2) << "default clamped into range";
  EXPECT_EQ(ChooseResolutionIndex({}, std::nullopt, 5), 0);
}

TEST(FullscreenToSave, CommandLineFlagsAreSessionOnly) {
  // -windowed while the saved preference is fullscreen: keep fullscreen saved.
  EXPECT_TRUE(FullscreenToSave(/*now*/ false, /*stored*/ true, /*-windowed*/ true, false));
  // -fullscreen while the saved preference is windowed: keep windowed saved.
  EXPECT_FALSE(FullscreenToSave(true, false, false, true));
}

TEST(FullscreenToSave, ChangesMadeInOptionsAreSaved) {
  EXPECT_TRUE(FullscreenToSave(true, false, true, false)) << "-windowed, then fullscreen chosen in Options";
  EXPECT_FALSE(FullscreenToSave(false, true, false, true)) << "-fullscreen, then windowed chosen in Options";
  EXPECT_TRUE(FullscreenToSave(true, false, false, false)) << "no flags: save what is current";
  EXPECT_FALSE(FullscreenToSave(false, true, false, false));
}
