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

// Cheat table and unlock state. See cheat_table.h.

#include "cheat_table.h"

#include <algorithm>

namespace tartarus {

namespace {

using K = CheatKind;
using I = CheatId;

// Order and passwords: SPEC.md §4. Plain constants by design (D-006).
constexpr std::array<CheatInfo, kCheatCount> kTable = {{
    {I::Cornucopia, "CORNUCOPIA", K::Button, "Give all weapons, ammo and keys", true, true},
    {I::Ambrosia, "AMBROSIA", K::Toggle, "God mode", true, true},
    {I::Aegis, "AEGIS", K::Toggle, "Infinite shields", true, true},
    {I::Prometheus, "PROMETHEUS", K::Toggle, "Infinite energy", true, true},
    {I::Hydra, "HYDRA", K::Toggle, "Infinite secondary ammo", true, true},
    {I::Magazine, "MAGAZINE", K::Toggle, "Infinite primary ammo", true, true},
    {I::Zephyr, "ZEPHYR", K::Toggle, "Infinite afterburner", true, true},
    {I::Titanhide, "TITANHIDE", K::Toggle, "Invulnerable", true, true},
    {I::SkeletonKey, "SKELETONKEY", K::Button, "All keys", true, true},
    {I::Thanatos, "THANATOS", K::Button, "Destroy all robots", true, true},
    {I::Shade, "SHADE", K::Toggle, "Cloak", true, true},
    {I::Wraith, "WRAITH", K::Toggle, "Noclip", false, true},
    {I::Ariadne, "ARIADNE", K::Button, "Reveal the automap", true, true},
    {I::Ferryman, "FERRYMAN", K::Button, "Level warp", true, true},
    {I::Argus, "ARGUS", K::Toggle, "Third-person camera", false, true},
    {I::Hourglass, "HOURGLASS", K::Toggle, "Frame-time and render statistics", true, true},
    {I::Torpor, "TORPOR", K::Slider, "Time scale", true, true},
    {I::Styx, "STYX", K::Button, "Self-destruct", true, true},
    {I::Catharsis, "CATHARSIS", K::Button, "Relock all cheats", true, false},
}};

static_assert([] {
  for (size_t i = 0; i < kTable.size(); ++i)
    if (static_cast<size_t>(kTable[i].id) != i)
      return false;
  return true;
}(), "kTable must be in CheatId order");

char Upper(char c) { return (c >= 'a' && c <= 'z') ? static_cast<char>(c - 'a' + 'A') : c; }
bool IsSpace(char c) { return c == ' ' || c == '\t' || c == '\r' || c == '\n'; }

} // namespace

const std::array<CheatInfo, kCheatCount> &CheatTable() { return kTable; }

const CheatInfo &GetCheatInfo(CheatId id) { return kTable[static_cast<size_t>(id)]; }

const CheatInfo *FindCheatByPassword(std::string_view text) {
  while (!text.empty() && IsSpace(text.front()))
    text.remove_prefix(1);
  while (!text.empty() && IsSpace(text.back()))
    text.remove_suffix(1);
  if (text.empty())
    return nullptr;
  std::string upper;
  upper.reserve(text.size());
  for (char c : text) {
    const char u = Upper(c);
    if (u < 'A' || u > 'Z')
      return nullptr; // letters only
    upper.push_back(u);
  }
  for (const CheatInfo &row : kTable) {
    if (upper == row.password)
      return &row;
  }
  return nullptr;
}

CheatState::UnlockResult CheatState::Unlock(std::string_view text, const CheatInfo **out) {
  const CheatInfo *row = FindCheatByPassword(text);
  if (out != nullptr)
    *out = row;
  if (row == nullptr)
    return UnlockResult::Unknown;
  bool &slot = unlocked_[Index(row->id)];
  if (slot)
    return UnlockResult::AlreadyUnlocked;
  slot = true;
  return UnlockResult::Unlocked;
}

bool CheatState::SetActive(CheatId id, bool on) {
  const CheatInfo &row = GetCheatInfo(id);
  if (row.kind != CheatKind::Toggle || !row.available || (on && !IsUnlocked(id)))
    return false;
  active_[Index(id)] = on;
  return true;
}

bool CheatState::AnyActive() const {
  return time_scale_ != 1.0f || std::any_of(active_.begin(), active_.end(), [](bool b) { return b; });
}

void CheatState::SetTimeScale(float scale) { time_scale_ = std::clamp(scale, kTimescaleMin, kTimescaleMax); }

CheatHolds CheatState::Holds() const {
  CheatHolds h;
  const bool god = IsActive(CheatId::Ambrosia);
  h.shields = god || IsActive(CheatId::Aegis);
  h.energy = god || IsActive(CheatId::Prometheus);
  h.secondary_ammo = god || IsActive(CheatId::Hydra);
  h.primary_ammo = god || IsActive(CheatId::Magazine);
  h.afterburner = god || IsActive(CheatId::Zephyr);
  h.invulnerable = god || IsActive(CheatId::Titanhide);
  h.cloak = IsActive(CheatId::Shade);
  h.stats = IsActive(CheatId::Hourglass);
  return h;
}

void CheatState::DeactivateAll() {
  active_.fill(false);
  time_scale_ = 1.0f;
}

void CheatState::RelockAll() {
  DeactivateAll();
  unlocked_.fill(false);
}

bool CheatState::SetOwner(std::string_view pilot) {
  if (owner_ && *owner_ == pilot)
    return false;
  const bool changed = owner_.has_value();
  owner_ = std::string(pilot);
  if (changed)
    RelockAll();
  return changed;
}

} // namespace tartarus
