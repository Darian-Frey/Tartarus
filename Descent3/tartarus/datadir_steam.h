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

// Steam install discovery for F-003. Pure standard C++ (no engine headers) so it
// can be unit-tested in isolation; the engine glue lives in datadir.cpp.

#pragma once

#include <filesystem>
#include <string_view>
#include <vector>

namespace tartarus::datadir {

/// Steam's install folder name for Descent 3 (`steamapps/common/<this>`).
inline constexpr const char *kSteamInstallDirName = "Descent 3";

/// Extracts every library `"path"` value from the text of a Steam
/// `libraryfolders.vdf` file, in file order. Only key/value pairs whose key is
/// `path` (case-insensitive) count; `\\` and `\"` escapes are decoded.
std::vector<std::filesystem::path> ParseLibraryFoldersVdf(std::string_view text);

/// Candidate Steam root directories for a Linux user, in probe order: native
/// (`~/.steam/steam`, `~/.steam/root`, `$XDG_DATA_HOME/Steam` or
/// `~/.local/share/Steam`), Flatpak and Snap. Not filtered for existence.
std::vector<std::filesystem::path> SteamRoots(const std::filesystem::path &home,
                                              const std::filesystem::path &xdg_data_home);

/// True if `dir` is a directory containing `d3.hog` (name matched
/// case-insensitively, as the engine does).
bool IsD3Install(const std::filesystem::path &dir);

/// Every distinct Descent 3 install reachable from `steam_roots`: each root's own
/// `steamapps/common/Descent 3` plus those of all libraries listed in its
/// `libraryfolders.vdf`. Paths are canonicalised and de-duplicated (several
/// roots are usually symlinks to one another); order follows `steam_roots`.
std::vector<std::filesystem::path> FindSteamInstalls(const std::vector<std::filesystem::path> &steam_roots);

} // namespace tartarus::datadir
