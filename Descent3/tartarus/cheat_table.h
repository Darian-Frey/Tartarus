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

// Cheat table and unlock state (F-012, SPEC.md §4, D-006). Pure standard C++
// (no engine headers) so the unlock rules can be unit-tested (AV-004);
// cheats.cpp applies the effects to the game.

#pragma once

#include <array>
#include <cstddef>
#include <optional>
#include <string>
#include <string_view>

namespace tartarus {

enum class CheatId : int {
  Cornucopia,
  Ambrosia,
  Aegis,
  Prometheus,
  Hydra,
  Magazine,
  Zephyr,
  Titanhide,
  SkeletonKey,
  Thanatos,
  Shade,
  Wraith,
  Ariadne,
  Ferryman,
  Argus,
  Hourglass,
  Torpor,
  Styx,
  Catharsis,
  Count
};
inline constexpr size_t kCheatCount = static_cast<size_t>(CheatId::Count);

enum class CheatKind { Button, Toggle, Slider };

struct CheatInfo {
  CheatId id;
  const char *password; // upper case, letters only (SPEC.md §4)
  CheatKind kind;
  const char *label;    // shown in the cheat menu
  bool available;       // false: listed but not implemented yet
  bool marks_cheater;   // unlocking sets IsCheater (every row but CATHARSIS)
};

/// Every row of SPEC.md §4, in menu order.
const std::array<CheatInfo, kCheatCount> &CheatTable();
const CheatInfo &GetCheatInfo(CheatId id);

/// Case-insensitive; surrounding whitespace ignored; anything but letters
/// inside the word never matches.
const CheatInfo *FindCheatByPassword(std::string_view text);

/// Time-scale limits: the TORPOR slider and the `timescale` command.
inline constexpr float kTorporMin = 0.1f;
inline constexpr float kTorporMax = 1.0f;
inline constexpr float kTorporStep = 0.1f;
inline constexpr float kTimescaleMin = 0.05f;
inline constexpr float kTimescaleMax = 4.0f;

/// What the per-frame loop must hold at maximum (SPEC.md §4 "Hold-at-max").
struct CheatHolds {
  bool shields = false;
  bool energy = false;
  bool primary_ammo = false;
  bool secondary_ammo = false;
  bool afterburner = false;
  bool invulnerable = false;
  bool cloak = false;
  bool stats = false;
  bool Any() const { return shields || energy || primary_ammo || secondary_ammo || afterburner || invulnerable || cloak || stats; }
};

class CheatState {
public:
  enum class UnlockResult { Unlocked, AlreadyUnlocked, Unknown };

  /// Unlocks the row whose password is `text`; `out` receives the row on a match.
  UnlockResult Unlock(std::string_view text, const CheatInfo **out = nullptr);
  bool IsUnlocked(CheatId id) const { return unlocked_[Index(id)] || all_unlocked_; }
  /// Debug builds: every row behaves as unlocked (SPEC.md §4).
  void SetAllUnlocked(bool on) { all_unlocked_ = on; }

  /// Toggles only; false (and no change) if locked, unavailable or not a toggle.
  bool SetActive(CheatId id, bool on);
  bool IsActive(CheatId id) const { return active_[Index(id)]; }
  bool AnyActive() const;

  /// TORPOR / `timescale`. Clamped to [kTimescaleMin, kTimescaleMax].
  void SetTimeScale(float scale);
  float TimeScale() const { return time_scale_; }

  CheatHolds Holds() const;

  /// Turns every toggle off and the time scale back to 1; unlocks stay.
  void DeactivateAll();
  /// CATHARSIS: deactivate and relock everything.
  void RelockAll();

  /// Unlocks belong to the current pilot's session: a different pilot relocks.
  /// Returns true if that happened.
  bool SetOwner(std::string_view pilot);

private:
  static size_t Index(CheatId id) { return static_cast<size_t>(id); }

  std::array<bool, kCheatCount> unlocked_{};
  std::array<bool, kCheatCount> active_{};
  float time_scale_ = 1.0f;
  bool all_unlocked_ = false;
  std::optional<std::string> owner_;
};

} // namespace tartarus
