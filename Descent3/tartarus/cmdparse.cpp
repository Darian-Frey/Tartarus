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

// Console / config line tokenizer. See cmdparse.h.

#include "cmdparse.h"

#include <cctype>

namespace tartarus {

namespace {
bool IsSpace(char c) { return std::isspace(static_cast<unsigned char>(c)) != 0; }
} // namespace

std::vector<CommandArgs> ParseCommandLine(std::string_view line) {
  std::vector<CommandArgs> commands;
  CommandArgs current;

  auto end_command = [&] {
    if (!current.empty()) {
      commands.push_back(std::move(current));
    }
    current.clear();
  };

  size_t i = 0;
  while (i < line.size()) {
    const char c = line[i];
    if (IsSpace(c)) {
      ++i;
    } else if (c == ';') {
      end_command();
      ++i;
    } else if (c == '/' && i + 1 < line.size() && line[i + 1] == '/') {
      break; // comment to end of line
    } else if (c == '"') {
      std::string word;
      ++i;
      while (i < line.size() && line[i] != '"') {
        if (line[i] == '\\' && i + 1 < line.size() && (line[i + 1] == '"' || line[i + 1] == '\\')) {
          ++i;
        }
        word += line[i++];
      }
      ++i; // closing quote, if any
      current.push_back(std::move(word));
    } else {
      std::string word;
      while (i < line.size() && !IsSpace(line[i]) && line[i] != ';' && line[i] != '"' &&
             !(line[i] == '/' && i + 1 < line.size() && line[i + 1] == '/')) {
        word += line[i++];
      }
      current.push_back(std::move(word));
    }
  }
  end_command();
  return commands;
}

std::string QuoteArg(std::string_view s) {
  bool needs = s.empty() || s.find("//") != std::string_view::npos;
  for (char c : s) {
    if (IsSpace(c) || c == ';' || c == '"' || c == '\\') {
      needs = true;
    }
  }
  if (!needs) {
    return std::string(s);
  }
  std::string out = "\"";
  for (char c : s) {
    if (c == '"' || c == '\\') {
      out += '\\';
    }
    out += c;
  }
  out += '"';
  return out;
}

} // namespace tartarus
