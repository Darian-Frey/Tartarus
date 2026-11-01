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

// Console / config line tokenizer (SPEC.md §3 grammar). Pure standard C++.
//
//   line    := command (';' command)*
//   command := word arg*
//   arg     := word | '"' chars '"'
//
// Whitespace separates words; ';' outside quotes separates commands; '//'
// outside quotes starts a comment that runs to the end of the line. Inside
// quotes, \" and \\ are escapes and everything else is literal.

#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace tartarus {

using CommandArgs = std::vector<std::string>; // [0] is the command or cvar name

/// Splits one line into commands. Empty commands (";;", blank lines,
/// comment-only lines) are dropped. An unterminated quote runs to the end of
/// the line.
std::vector<CommandArgs> ParseCommandLine(std::string_view line);

/// Quotes `s` if it would not survive ParseCommandLine as a single word
/// (empty, contains whitespace, ';', '"', '\\' or "//").
std::string QuoteArg(std::string_view s);

} // namespace tartarus
