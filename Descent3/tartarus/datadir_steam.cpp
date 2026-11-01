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

// Steam install discovery for F-003. See datadir_steam.h.

#include "datadir_steam.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <optional>
#include <sstream>
#include <string>
#include <system_error>

namespace fs = std::filesystem;

namespace tartarus::datadir {

namespace {

bool IEquals(std::string_view a, std::string_view b) {
  return a.size() == b.size() && std::equal(a.begin(), a.end(), b.begin(), [](char x, char y) {
           return std::tolower(static_cast<unsigned char>(x)) == std::tolower(static_cast<unsigned char>(y));
         });
}

std::optional<std::string> ReadFile(const fs::path &file) {
  std::ifstream in(file, std::ios::binary);
  if (!in) {
    return std::nullopt;
  }
  std::ostringstream ss;
  ss << in.rdbuf();
  return ss.str();
}

} // namespace

std::vector<fs::path> ParseLibraryFoldersVdf(std::string_view text) {
  // VDF is a tree of "key" "value" pairs and "key" { ... } sections. We only
  // need the "path" leaves, so track key/value parity and reset it at braces.
  std::vector<fs::path> paths;
  std::optional<std::string> pending_key;

  size_t i = 0;
  while (i < text.size()) {
    const char c = text[i];
    if (c == '{' || c == '}') {
      pending_key.reset(); // the pending token was a section name
      ++i;
    } else if (c == '/' && i + 1 < text.size() && text[i + 1] == '/') {
      while (i < text.size() && text[i] != '\n') {
        ++i;
      }
    } else if (c == '"') {
      std::string token;
      ++i;
      while (i < text.size() && text[i] != '"') {
        if (text[i] == '\\' && i + 1 < text.size()) {
          ++i; // keep the escaped character literally (\\ -> \, \" -> ")
        }
        token += text[i++];
      }
      ++i; // closing quote (or end of text)
      if (!pending_key) {
        pending_key = std::move(token);
      } else {
        if (IEquals(*pending_key, "path") && !token.empty()) {
          paths.emplace_back(token);
        }
        pending_key.reset();
      }
    } else {
      ++i;
    }
  }
  return paths;
}

std::vector<fs::path> SteamRoots(const fs::path &home, const fs::path &xdg_data_home) {
  const fs::path data_home = xdg_data_home.empty() ? home / ".local" / "share" : xdg_data_home;
  return {
      home / ".steam" / "steam",
      home / ".steam" / "root",
      data_home / "Steam",
      home / ".var" / "app" / "com.valvesoftware.Steam" / ".local" / "share" / "Steam", // Flatpak
      home / "snap" / "steam" / "common" / ".local" / "share" / "Steam",               // Snap
  };
}

bool IsD3Install(const fs::path &dir) {
  std::error_code ec;
  if (!fs::is_directory(dir, ec)) {
    return false;
  }
  for (fs::directory_iterator it(dir, ec), end; !ec && it != end; it.increment(ec)) {
    if (IEquals(it->path().filename().string(), "d3.hog")) {
      return it->is_regular_file(ec);
    }
  }
  return false;
}

std::vector<fs::path> FindSteamInstalls(const std::vector<fs::path> &steam_roots) {
  std::vector<fs::path> installs;

  auto consider = [&installs](const fs::path &library) {
    const fs::path candidate = library / "steamapps" / "common" / kSteamInstallDirName;
    if (!IsD3Install(candidate)) {
      return;
    }
    std::error_code ec;
    fs::path canonical = fs::canonical(candidate, ec);
    if (ec) {
      canonical = candidate.lexically_normal();
    }
    if (std::find(installs.begin(), installs.end(), canonical) == installs.end()) {
      installs.push_back(std::move(canonical));
    }
  };

  for (const fs::path &root : steam_roots) {
    std::error_code ec;
    if (!fs::is_directory(root, ec)) {
      continue;
    }
    consider(root);
    for (const char *vdf : {"steamapps/libraryfolders.vdf", "config/libraryfolders.vdf"}) {
      if (auto text = ReadFile(root / vdf)) {
        for (const fs::path &library : ParseLibraryFoldersVdf(*text)) {
          consider(library);
        }
      }
    }
  }
  return installs;
}

} // namespace tartarus::datadir
