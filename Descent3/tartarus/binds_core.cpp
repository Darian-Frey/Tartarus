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

// Console key bindings, pure part. See binds_core.h.

#include "binds_core.h"

#include <cctype>

#include "cmdparse.h"

namespace tartarus {

namespace {

std::string Lower(std::string_view s) {
  std::string out(s);
  for (char &c : out)
    c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  return out;
}

} // namespace

BindTable::BindTable(std::vector<KeyName> names) : names_(std::move(names)) {}

std::optional<int> BindTable::ParseKey(std::string_view text) const {
  std::string rest = Lower(text);
  int mods = 0;
  for (;;) {
    const size_t plus = rest.find('+');
    if (plus == std::string::npos || plus == 0 || plus + 1 >= rest.size())
      break; // no modifier left (a lone "+" is not a key we name)
    const std::string mod = rest.substr(0, plus);
    if (mod == "shift")
      mods |= kBindShift;
    else if (mod == "ctrl" || mod == "control")
      mods |= kBindCtrl;
    else if (mod == "alt")
      mods |= kBindAlt;
    else
      return std::nullopt;
    rest = rest.substr(plus + 1);
  }
  for (const KeyName &k : names_) {
    if (k.name == rest)
      return k.code | mods;
  }
  return std::nullopt;
}

std::string BindTable::KeyToString(int key) const {
  std::string out;
  if (key & kBindCtrl)
    out += "ctrl+";
  if (key & kBindAlt)
    out += "alt+";
  if (key & kBindShift)
    out += "shift+";
  const int code = key & kBindCodeMask;
  for (const KeyName &k : names_) {
    if (k.code == code)
      return out + k.name;
  }
  return out + "key" + std::to_string(code);
}

void BindTable::Bind(int key, std::string line) { binds_[key] = std::move(line); }

bool BindTable::Unbind(int key) { return binds_.erase(key) != 0; }

const std::string *BindTable::Find(int key) const {
  auto it = binds_.find(key);
  return it == binds_.end() ? nullptr : &it->second;
}

std::string BindTable::Serialize() const {
  std::string out = "// Tartarus key bindings (console 'bind'). Written by the game.\n";
  for (const auto &[key, line] : binds_)
    out += "bind " + KeyToString(key) + " " + QuoteArg(line) + "\n";
  return out;
}

} // namespace tartarus
