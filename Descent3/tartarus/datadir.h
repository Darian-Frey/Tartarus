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

// Data-directory handling for F-003, called from InitIOSystems() in init.cpp.

#pragma once

#include <filesystem>

namespace tartarus {

/// If no base directory added so far provides d3.hog, look for a Steam install
/// of Descent 3 and add it as a read-only base directory. Never overrides
/// -additionaldir, the configured base directory or D3_DATADIR: it only runs
/// when those did not yield the game data. Runs in Classic Mode too (it is a
/// compatibility fix, not a gameplay change).
void DataDir_Probe();

/// Call with the handle cf_OpenLibrary() returned for the core HOG. A zero
/// handle stops the game with a readable error naming the file and every
/// directory searched, instead of carrying on to a black screen.
void DataDir_RequireHog(const std::filesystem::path &hog_name, int library_handle);

} // namespace tartarus
