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

// Console key bindings (F-010, SPEC.md §3): key names, the binding table and
// its saved form. Pure standard C++ (the engine's key codes are passed in by
// binds.cpp), unit-tested.

#pragma once

#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace tartarus {

// Modifier bits, as upstream's KEY_SHIFTED / KEY_ALTED / KEY_CTRLED (binds.cpp
// static_asserts that they match).
inline constexpr int kBindShift = 0x100;
inline constexpr int kBindAlt = 0x200;
inline constexpr int kBindCtrl = 0x400;
inline constexpr int kBindCodeMask = 0xFF;

struct KeyName {
  std::string name; // lower case, e.g. "f5", "kp7", "a"
  int code;         // engine key code without modifiers
};

class BindTable {
public:
  explicit BindTable(std::vector<KeyName> names);

  /// "f5", "Shift+F5", "ctrl+alt+kp7" -> key code with modifier bits.
  std::optional<int> ParseKey(std::string_view text) const;
  /// The canonical name: modifiers in ctrl, alt, shift order, lower case.
  std::string KeyToString(int key) const;

  void Bind(int key, std::string line);
  bool Unbind(int key);
  const std::string *Find(int key) const;
  const std::map<int, std::string> &All() const { return binds_; }
  void Clear() { binds_.clear(); }

  /// One `bind <key> "<line>"` per binding, for binds.cfg.
  std::string Serialize() const;

private:
  std::vector<KeyName> names_;
  std::map<int, std::string> binds_;
};

} // namespace tartarus
