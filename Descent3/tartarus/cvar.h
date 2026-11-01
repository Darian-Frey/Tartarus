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

// Cvar system (F-011, SPEC.md §2). Pure standard C++ so it can be unit-tested;
// engine glue (where tartarus.cfg lives, Debug-build cheat policy, the classic
// flag) is injected through CvarPolicy.
//
// Typical use from a module:
//
//   static tartarus::Cvar *cloak_cost =
//       tartarus::Cvars().RegisterFloat("cloak_cost", 30, 0, 200, "Energy charged on activation",
//                                       tartarus::CVAR_ARCHIVE | tartarus::CVAR_CLASSIC_LOCKED);
//   ...
//   energy -= cloak_cost->GetFloat();
//
// Cvar pointers stay valid for the life of the registry.

#pragma once

#include <cstdint>
#include <filesystem>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace tartarus {

enum CvarFlags : uint32_t {
  CVAR_NONE = 0,
  CVAR_ARCHIVE = 1u << 0,        // persisted to tartarus.cfg
  CVAR_CHEAT = 1u << 1,          // changing it needs the matching cheat unlock, or a Debug build
  CVAR_CLASSIC_LOCKED = 1u << 2, // reads as its default while Classic Mode is on (AV-001)
  CVAR_LATCH = 1u << 3,          // a new value takes effect at the next ApplyLatched() (level load)
  CVAR_READONLY = 1u << 4,       // cannot be changed from the console or config
};

enum class CvarType { Bool, Int, Float, String };

enum class CvarSetResult {
  Ok,
  Clamped,       // numeric value was outside [min, max] and has been clamped
  Latched,       // accepted; takes effect at the next ApplyLatched()
  Unknown,       // no such cvar
  Invalid,       // value does not parse for this type
  ReadOnly,      // CVAR_READONLY
  CheatLocked,   // CVAR_CHEAT and the policy refused
  ClassicLocked, // CVAR_CLASSIC_LOCKED while Classic Mode is on
};

/// True when a set result means the value was stored.
inline bool CvarSetAccepted(CvarSetResult r) {
  return r == CvarSetResult::Ok || r == CvarSetResult::Clamped || r == CvarSetResult::Latched;
}
const char *CvarSetResultText(CvarSetResult r);

/// Who is changing a cvar. Code may change anything; the user (console,
/// config file) is subject to READONLY / CHEAT / CLASSIC_LOCKED.
enum class CvarSource { Code, User, Config };

class Cvar {
public:
  const std::string &Name() const { return name_; }
  const std::string &Description() const { return description_; }
  CvarType Type() const { return type_; }
  uint32_t Flags() const { return flags_; }
  bool HasFlag(CvarFlags f) const { return (flags_ & f) != 0; }

  // Effective value: the default while Classic Mode locks this cvar.
  bool GetBool() const;
  int GetInt() const;
  float GetFloat() const;
  const std::string &GetString() const;

  const std::string &DefaultString() const { return default_; }
  const std::string &StoredString() const { return value_; } // ignores the classic lock
  std::optional<std::string> LatchedString() const { return latched_; }
  bool IsDefault() const { return value_ == default_; }
  bool IsEffectivelyLocked() const;

  double Min() const { return min_; }
  double Max() const { return max_; }

  /// Called after the stored value changes (not when a latched value is queued).
  void AddChangeCallback(std::function<void(const Cvar &)> fn) { callbacks_.push_back(std::move(fn)); }

  /// "name = value (default d) — description", plus lock/latch notes.
  std::string Describe() const;

private:
  friend class CvarRegistry;
  Cvar() = default;

  std::string name_;
  std::string description_;
  CvarType type_ = CvarType::String;
  uint32_t flags_ = 0;
  std::string default_;
  std::string value_;
  std::optional<std::string> latched_;
  double min_ = 0, max_ = 0;
  double num_ = 0, def_num_ = 0; // parsed numeric value / default (Bool, Int, Float)
  bool has_range_ = false;
  std::vector<std::function<void(const Cvar &)>> callbacks_;
  const class CvarRegistry *owner_ = nullptr;
};

/// Hooks the registry needs from the engine. All optional.
struct CvarPolicy {
  std::function<bool()> is_classic;                       // Classic Mode on?
  std::function<bool(const Cvar &)> cheat_allowed;        // may the user change this CHEAT cvar?
  std::function<void(const std::string &)> log;           // diagnostics (parse errors etc.)
  std::filesystem::path config_path;                      // tartarus.cfg; empty = no persistence
  bool autosave = true;                                   // save after every accepted user change to an ARCHIVE cvar
};

class CvarRegistry {
public:
  CvarRegistry() = default;
  CvarRegistry(const CvarRegistry &) = delete;
  CvarRegistry &operator=(const CvarRegistry &) = delete;

  void SetPolicy(CvarPolicy policy) { policy_ = std::move(policy); }
  const CvarPolicy &Policy() const { return policy_; }

  // Registration. Registering an existing name returns the existing cvar
  // (types must match; a mismatch is logged and returns nullptr). If the config
  // file already supplied a value for this name, it is applied now.
  Cvar *RegisterBool(std::string_view name, bool def, std::string_view desc, uint32_t flags = CVAR_NONE);
  Cvar *RegisterInt(std::string_view name, int def, int min, int max, std::string_view desc, uint32_t flags = CVAR_NONE);
  Cvar *RegisterFloat(std::string_view name, float def, float min, float max, std::string_view desc,
                      uint32_t flags = CVAR_NONE);
  Cvar *RegisterString(std::string_view name, std::string_view def, std::string_view desc, uint32_t flags = CVAR_NONE);

  Cvar *Find(std::string_view name) const;
  /// All cvars whose name starts with `prefix`, sorted by name.
  std::vector<Cvar *> List(std::string_view prefix = {}) const;

  CvarSetResult Set(std::string_view name, std::string_view value, CvarSource source = CvarSource::User);
  CvarSetResult Reset(std::string_view name, CvarSource source = CvarSource::User); // back to default

  /// Applies queued LATCH values (call at level load).
  void ApplyLatched();

  /// Reads `set <name> <value>` lines. Values for not-yet-registered names are
  /// remembered and applied when they register (and preserved on save).
  /// Returns false only if the file exists but cannot be read.
  bool LoadConfig();
  bool LoadConfigFrom(const std::filesystem::path &path);
  /// Writes every ARCHIVE cvar that differs from its default, plus remembered
  /// unknown entries, atomically (temp file + rename).
  bool SaveConfig() const;
  bool SaveConfigTo(const std::filesystem::path &path) const;

  bool IsClassic() const { return policy_.is_classic && policy_.is_classic(); }

private:
  Cvar *Register(std::string_view name, CvarType type, std::string def, double min, double max, bool has_range,
                 std::string_view desc, uint32_t flags);
  CvarSetResult Store(Cvar &cv, std::string value, CvarSource source);
  std::optional<std::string> Normalise(const Cvar &cv, std::string_view in, bool *clamped) const;
  void Log(const std::string &msg) const;

  CvarPolicy policy_;
  std::map<std::string, std::unique_ptr<Cvar>, std::less<>> cvars_;
  std::map<std::string, std::string, std::less<>> pending_; // config values for unregistered names
};

/// The game's registry.
CvarRegistry &Cvars();

} // namespace tartarus
