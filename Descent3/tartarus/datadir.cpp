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

// Data-directory handling for F-003. See datadir.h.

#include "datadir.h"

#include <cstdlib>
#include <string>

#include "cfile.h"
#include "datadir_steam.h"
#include "log.h"
#include "pserror.h"

namespace tartarus {

void DataDir_Probe() {
  if (!cf_LocatePath("d3.hog").empty()) {
    LOG_DEBUG << "[tartarus] d3.hog already reachable; Steam probe skipped";
    return;
  }

  const char *home = std::getenv("HOME");
  if (home == nullptr || *home == '\0') {
    LOG_WARNING << "[tartarus] HOME is not set; cannot probe for a Steam install";
    return;
  }
  const char *xdg = std::getenv("XDG_DATA_HOME");

  const auto installs =
      datadir::FindSteamInstalls(datadir::SteamRoots(home, xdg != nullptr ? std::filesystem::path(xdg) : std::filesystem::path()));
  if (installs.empty()) {
    LOG_INFO << "[tartarus] no Steam install of Descent 3 found";
    return;
  }

  if (installs.size() > 1) {
    LOG_WARNING << "[tartarus] found " << installs.size()
                << " Steam installs of Descent 3; using the first. Pass -additionaldir to choose another:";
    for (const auto &dir : installs) {
      LOG_WARNING << "[tartarus]   " << dir;
    }
  }
  LOG_INFO << "[tartarus] using Steam install " << installs.front();
  cf_AddBaseDirectory(installs.front());
}

void DataDir_RequireHog(const std::filesystem::path &hog_name, int library_handle) {
  if (library_handle != 0) {
    return;
  }

  std::string searched;
  for (const auto &dir : Base_directories) {
    searched += "\n  " + dir.string();
  }
  if (searched.empty()) {
    searched = "\n  (no base directories)";
  }

  Error("Required game file %s was not found.\n\nSearched:%s\n\n"
        "Point Tartarus at your Descent 3 install with -additionaldir \"<path>\" "
        "(see BUILD.md section 4).",
        hog_name.string().c_str(), searched.c_str());
}

} // namespace tartarus
