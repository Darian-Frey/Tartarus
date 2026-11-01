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

// Build identification for F-004.

#pragma once

namespace tartarus {

/// "Tartarus <version> <our revision>", e.g. "Tartarus 0.1.0-dev 29fa32a6".
const char *BuildString();

/// "based on Descent 3 Open Source <upstream hash>".
const char *UpstreamString();

/// Draws both lines in the main menu's bottom-right corner, replacing the
/// upstream version line (mmInterface::CopyrightText). Returns true when it
/// has drawn, so the caller can skip the upstream text.
bool DrawMenuBuildString();

} // namespace tartarus
