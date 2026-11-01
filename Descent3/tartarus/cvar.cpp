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

// Cvar system (F-011). See cvar.h.

#include "cvar.h"

#include <algorithm>
#include <cctype>
#include <charconv>
#include <cmath>
#include <fstream>
#include <sstream>
#include <system_error>

#include "cmdparse.h"

namespace fs = std::filesystem;

namespace tartarus {

namespace {

std::string Lower(std::string_view s) {
  std::string out(s);
  std::transform(out.begin(), out.end(), out.begin(), [](unsigned char c) { return std::tolower(c); });
  return out;
}

std::string FormatFloat(double v) {
  char buf[64];
  auto [ptr, ec] = std::to_chars(buf, buf + sizeof(buf), static_cast<float>(v));
  return ec == std::errc{} ? std::string(buf, ptr) : std::to_string(v);
}

const char *TypeName(CvarType t) {
  switch (t) {
  case CvarType::Bool:
    return "bool";
  case CvarType::Int:
    return "int";
  case CvarType::Float:
    return "float";
  case CvarType::String:
    return "string";
  }
  return "?";
}

} // namespace

const char *CvarSetResultText(CvarSetResult r) {
  switch (r) {
  case CvarSetResult::Ok:
    return "ok";
  case CvarSetResult::Clamped:
    return "value clamped to the allowed range";
  case CvarSetResult::Latched:
    return "will take effect at the next level load";
  case CvarSetResult::Unknown:
    return "unknown command or cvar";
  case CvarSetResult::Invalid:
    return "invalid value";
  case CvarSetResult::ReadOnly:
    return "read-only";
  case CvarSetResult::CheatLocked:
    return "cheat: unlock it first";
  case CvarSetResult::ClassicLocked:
    return "not available in Classic Mode (classic 0 to enable Tartarus features)";
  }
  return "?";
}

// ---------------------------------------------------------------- Cvar

bool Cvar::IsEffectivelyLocked() const {
  return HasFlag(CVAR_CLASSIC_LOCKED) && owner_ != nullptr && owner_->IsClassic();
}

bool Cvar::GetBool() const { return (IsEffectivelyLocked() ? def_num_ : num_) != 0; }
int Cvar::GetInt() const { return static_cast<int>(IsEffectivelyLocked() ? def_num_ : num_); }
float Cvar::GetFloat() const { return static_cast<float>(IsEffectivelyLocked() ? def_num_ : num_); }
const std::string &Cvar::GetString() const { return IsEffectivelyLocked() ? default_ : value_; }

std::string Cvar::Describe() const {
  std::string s = name_ + " = " + QuoteArg(value_) + " (default " + QuoteArg(default_) + ", " + TypeName(type_);
  if (has_range_) {
    s += " " + (type_ == CvarType::Float ? FormatFloat(min_) : std::to_string(static_cast<long long>(min_))) + "…" +
         (type_ == CvarType::Float ? FormatFloat(max_) : std::to_string(static_cast<long long>(max_)));
  }
  s += ")";
  if (!description_.empty()) {
    s += " — " + description_;
  }
  std::string tags;
  if (HasFlag(CVAR_ARCHIVE))
    tags += " archive";
  if (HasFlag(CVAR_CHEAT))
    tags += " cheat";
  if (HasFlag(CVAR_CLASSIC_LOCKED))
    tags += " classic-locked";
  if (HasFlag(CVAR_LATCH))
    tags += " latch";
  if (HasFlag(CVAR_READONLY))
    tags += " read-only";
  if (!tags.empty()) {
    s += " [" + tags.substr(1) + "]";
  }
  if (IsEffectivelyLocked()) {
    s += " (Classic Mode: reads as default)";
  }
  if (latched_) {
    s += " (pending: " + QuoteArg(*latched_) + ")";
  }
  return s;
}

// ---------------------------------------------------------------- Registry

void CvarRegistry::Log(const std::string &msg) const {
  if (policy_.log) {
    policy_.log(msg);
  }
}

std::optional<std::string> CvarRegistry::Normalise(const Cvar &cv, std::string_view in, bool *clamped) const {
  *clamped = false;
  switch (cv.type_) {
  case CvarType::Bool: {
    const std::string v = Lower(in);
    if (v == "1" || v == "true" || v == "on" || v == "yes")
      return "1";
    if (v == "0" || v == "false" || v == "off" || v == "no")
      return "0";
    return std::nullopt;
  }
  case CvarType::Int: {
    long long n = 0;
    const char *b = in.data(), *e = in.data() + in.size();
    if (!in.empty() && *b == '+')
      ++b;
    auto [ptr, ec] = std::from_chars(b, e, n);
    if (ec != std::errc{} || ptr != e || in.empty())
      return std::nullopt;
    if (cv.has_range_ && (n < cv.min_ || n > cv.max_)) {
      n = static_cast<long long>(std::clamp<double>(static_cast<double>(n), cv.min_, cv.max_));
      *clamped = true;
    }
    return std::to_string(n);
  }
  case CvarType::Float: {
    const std::string tmp(in);
    char *end = nullptr;
    const double d = std::strtod(tmp.c_str(), &end);
    if (tmp.empty() || end != tmp.c_str() + tmp.size() || !std::isfinite(d))
      return std::nullopt;
    double v = d;
    if (cv.has_range_ && (v < cv.min_ || v > cv.max_)) {
      v = std::clamp(v, cv.min_, cv.max_);
      *clamped = true;
    }
    return FormatFloat(v);
  }
  case CvarType::String:
    return std::string(in);
  }
  return std::nullopt;
}

Cvar *CvarRegistry::Register(std::string_view name, CvarType type, std::string def, double min, double max,
                             bool has_range, std::string_view desc, uint32_t flags) {
  if (auto it = cvars_.find(name); it != cvars_.end()) {
    if (it->second->type_ != type) {
      Log("cvar '" + std::string(name) + "' re-registered with a different type; ignored");
      return nullptr;
    }
    return it->second.get();
  }

  auto cv = std::unique_ptr<Cvar>(new Cvar());
  cv->name_ = std::string(name);
  cv->description_ = std::string(desc);
  cv->type_ = type;
  cv->flags_ = flags;
  cv->min_ = min;
  cv->max_ = max;
  cv->has_range_ = has_range;
  cv->owner_ = this;

  bool clamped = false;
  auto norm = Normalise(*cv, def, &clamped);
  cv->default_ = norm.value_or(def);
  cv->value_ = cv->default_;
  if (type != CvarType::String) {
    cv->def_num_ = cv->num_ = std::strtod(cv->default_.c_str(), nullptr);
  }

  Cvar *raw = cv.get();
  cvars_.emplace(raw->name_, std::move(cv));

  if (auto p = pending_.find(name); p != pending_.end()) {
    const std::string value = p->second;
    pending_.erase(p);
    const CvarSetResult r = Store(*raw, value, CvarSource::Config);
    if (!CvarSetAccepted(r)) {
      Log("tartarus.cfg: " + raw->name_ + " " + QuoteArg(value) + ": " + CvarSetResultText(r));
    }
  }
  return raw;
}

Cvar *CvarRegistry::RegisterBool(std::string_view name, bool def, std::string_view desc, uint32_t flags) {
  return Register(name, CvarType::Bool, def ? "1" : "0", 0, 1, false, desc, flags);
}
Cvar *CvarRegistry::RegisterInt(std::string_view name, int def, int min, int max, std::string_view desc,
                                uint32_t flags) {
  return Register(name, CvarType::Int, std::to_string(def), min, max, true, desc, flags);
}
Cvar *CvarRegistry::RegisterFloat(std::string_view name, float def, float min, float max, std::string_view desc,
                                  uint32_t flags) {
  return Register(name, CvarType::Float, FormatFloat(def), min, max, true, desc, flags);
}
Cvar *CvarRegistry::RegisterString(std::string_view name, std::string_view def, std::string_view desc,
                                   uint32_t flags) {
  return Register(name, CvarType::String, std::string(def), 0, 0, false, desc, flags);
}

Cvar *CvarRegistry::Find(std::string_view name) const {
  auto it = cvars_.find(name);
  return it == cvars_.end() ? nullptr : it->second.get();
}

std::vector<Cvar *> CvarRegistry::List(std::string_view prefix) const {
  std::vector<Cvar *> out;
  for (auto it = cvars_.lower_bound(prefix); it != cvars_.end() && it->first.compare(0, prefix.size(), prefix) == 0;
       ++it) {
    out.push_back(it->second.get());
  }
  return out;
}

CvarSetResult CvarRegistry::Store(Cvar &cv, std::string value, CvarSource source) {
  if (source != CvarSource::Code) {
    if (cv.HasFlag(CVAR_READONLY))
      return CvarSetResult::ReadOnly;
    if (cv.HasFlag(CVAR_CHEAT) && !(policy_.cheat_allowed && policy_.cheat_allowed(cv)))
      return CvarSetResult::CheatLocked;
    // The config may store a value for a classic-locked cvar (it is read as the
    // default until Classic Mode is off); the user at the console may not.
    if (source == CvarSource::User && cv.IsEffectivelyLocked())
      return CvarSetResult::ClassicLocked;
  }

  bool clamped = false;
  auto norm = Normalise(cv, value, &clamped);
  if (!norm)
    return CvarSetResult::Invalid;

  // At startup (config) a latched value applies immediately: nothing is running yet.
  if (cv.HasFlag(CVAR_LATCH) && source == CvarSource::User) {
    if (*norm == cv.value_) {
      cv.latched_.reset();
    } else {
      cv.latched_ = *norm;
    }
    return CvarSetResult::Latched;
  }

  const bool changed = (*norm != cv.value_);
  cv.value_ = std::move(*norm);
  cv.latched_.reset();
  if (cv.type_ != CvarType::String) {
    cv.num_ = std::strtod(cv.value_.c_str(), nullptr);
  }
  if (changed) {
    for (auto &fn : cv.callbacks_) {
      fn(cv);
    }
  }
  return clamped ? CvarSetResult::Clamped : CvarSetResult::Ok;
}

CvarSetResult CvarRegistry::Set(std::string_view name, std::string_view value, CvarSource source) {
  Cvar *cv = Find(name);
  if (cv == nullptr)
    return CvarSetResult::Unknown;
  const CvarSetResult r = Store(*cv, std::string(value), source);
  if (source == CvarSource::User && CvarSetAccepted(r) && cv->HasFlag(CVAR_ARCHIVE) && policy_.autosave &&
      !policy_.config_path.empty()) {
    SaveConfig();
  }
  return r;
}

CvarSetResult CvarRegistry::Reset(std::string_view name, CvarSource source) {
  Cvar *cv = Find(name);
  if (cv == nullptr)
    return CvarSetResult::Unknown;
  return Set(name, cv->default_, source);
}

void CvarRegistry::ApplyLatched() {
  for (auto &[name, cv] : cvars_) {
    if (cv->latched_) {
      std::string v = *cv->latched_;
      cv->latched_.reset();
      Store(*cv, std::move(v), CvarSource::Code);
    }
  }
}

bool CvarRegistry::LoadConfig() { return policy_.config_path.empty() || LoadConfigFrom(policy_.config_path); }

bool CvarRegistry::LoadConfigFrom(const fs::path &path) {
  std::error_code ec;
  if (!fs::exists(path, ec)) {
    return true; // first run
  }
  std::ifstream in(path);
  if (!in) {
    Log("cannot read " + path.string());
    return false;
  }
  std::string line;
  int line_no = 0;
  while (std::getline(in, line)) {
    ++line_no;
    for (const CommandArgs &cmd : ParseCommandLine(line)) {
      const std::string where = path.filename().string() + ":" + std::to_string(line_no) + ": ";
      if (Lower(cmd[0]) != "set" || cmd.size() != 3) {
        Log(where + "expected 'set <name> <value>', skipped");
        continue;
      }
      if (Cvar *cv = Find(cmd[1])) {
        const CvarSetResult r = Store(*cv, cmd[2], CvarSource::Config);
        if (!CvarSetAccepted(r)) {
          Log(where + cmd[1] + ": " + CvarSetResultText(r));
        }
      } else {
        pending_[cmd[1]] = cmd[2];
      }
    }
  }
  return true;
}

bool CvarRegistry::SaveConfig() const { return policy_.config_path.empty() || SaveConfigTo(policy_.config_path); }

bool CvarRegistry::SaveConfigTo(const fs::path &path) const {
  std::ostringstream out;
  out << "// tartarus.cfg: written by Tartarus. One 'set <name> <value>' per line; only values that\n"
         "// differ from the default are stored. Edit while the game is not running.\n";
  std::map<std::string, std::string> lines; // sorted by name
  for (const auto &[name, cv] : cvars_) {
    if (cv->HasFlag(CVAR_ARCHIVE) && !cv->IsDefault()) {
      lines[name] = cv->value_;
    }
  }
  for (const auto &[name, value] : pending_) {
    lines.emplace(name, value); // keep settings of modules not registered this run
  }
  for (const auto &[name, value] : lines) {
    out << "set " << name << " " << QuoteArg(value) << "\n";
  }

  std::error_code ec;
  if (path.has_parent_path()) {
    fs::create_directories(path.parent_path(), ec);
  }
  fs::path tmp = path;
  tmp += ".tmp";
  {
    std::ofstream f(tmp, std::ios::binary | std::ios::trunc);
    if (!f || !(f << out.str()) || !f.flush()) {
      Log("cannot write " + tmp.string());
      return false;
    }
  }
  fs::rename(tmp, path, ec);
  if (ec) {
    Log("cannot replace " + path.string() + ": " + ec.message());
    fs::remove(tmp, ec);
    return false;
  }
  return true;
}

CvarRegistry &Cvars() {
  static CvarRegistry registry;
  return registry;
}

} // namespace tartarus
