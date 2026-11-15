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

#include "sidecar_core.h"

#include <cctype>
#include <cstdlib>

namespace tartarus {

namespace {

std::string At(int line) { return "line " + std::to_string(line) + ": "; }

// Splits a line into words and "quoted strings", stopping at a comment.
bool Tokens(const std::string &line, std::vector<std::string> *out, std::string *error) {
  size_t i = 0;
  while (i < line.size()) {
    const char c = line[i];
    if (std::isspace(static_cast<unsigned char>(c))) {
      ++i;
    } else if (c == '#' || (c == '/' && i + 1 < line.size() && line[i + 1] == '/')) {
      break;
    } else if (c == '"') {
      const size_t end = line.find('"', i + 1);
      if (end == std::string::npos) {
        *error = "a quote is not closed";
        return false;
      }
      out->push_back(line.substr(i + 1, end - i - 1));
      i = end + 1;
    } else if (c == '{' || c == '}') {
      out->push_back(std::string(1, c));
      ++i;
    } else {
      size_t end = i;
      while (end < line.size() && !std::isspace(static_cast<unsigned char>(line[end])) && line[end] != '{' &&
             line[end] != '}' && line[end] != '"')
        ++end;
      out->push_back(line.substr(i, end - i));
      i = end;
    }
  }
  return true;
}

} // namespace

const SidecarKey *SidecarBlock::Find(const std::string &key) const {
  for (const SidecarKey &k : keys)
    if (k.name == key)
      return &k;
  return nullptr;
}

SidecarParse ParseSidecar(const std::string &text) {
  SidecarParse result;
  SidecarBlock current;
  bool open = false, broken = false;
  int line_no = 0;
  size_t start = 0;
  while (start <= text.size()) {
    size_t end = text.find('\n', start);
    if (end == std::string::npos)
      end = text.size();
    std::string line = text.substr(start, end - start);
    if (!line.empty() && line.back() == '\r')
      line.pop_back();
    start = end + 1;
    ++line_no;

    std::vector<std::string> tok;
    std::string error;
    if (!Tokens(line, &tok, &error)) {
      result.errors.push_back(At(line_no) + error);
      broken = broken || open;
      continue;
    }
    if (tok.empty())
      continue;
    if (!open) {
      if (tok.size() == 3 && tok[0] == "weapon" && tok[2] == "{" && !tok[1].empty()) {
        current = SidecarBlock{tok[1], line_no, {}};
        open = true;
        broken = false;
      } else {
        result.errors.push_back(At(line_no) + "expected: weapon \"Name\" {");
      }
      continue;
    }
    if (tok.size() == 1 && tok[0] == "}") {
      if (broken)
        result.errors.push_back(At(current.line) + "weapon \"" + current.name + "\" left out: it has errors");
      else
        result.weapons.push_back(current);
      open = false;
      continue;
    }
    if (tok[0] == "{" || tok[0] == "}" || tok[0] == "weapon") {
      result.errors.push_back(At(line_no) + "expected a key and its values, or }");
      broken = true;
      continue;
    }
    if (tok.size() < 2) {
      result.errors.push_back(At(line_no) + "\"" + tok[0] + "\" has no value");
      broken = true;
      continue;
    }
    current.keys.push_back({tok[0], std::vector<std::string>(tok.begin() + 1, tok.end()), line_no});
  }
  if (open)
    result.errors.push_back(At(current.line) + "weapon \"" + current.name + "\" is not closed with }");
  return result;
}

uint32_t TartarusFlagBit(const std::string &name) {
  static const struct {
    const char *name;
    uint32_t bit;
  } kFlags[] = {{"TWF_GRAVWELL", TWF_GRAVWELL}, {"TWF_FLAK", TWF_FLAK},   {"TWF_RICOCHET", TWF_RICOCHET},
                {"TWF_EMP", TWF_EMP},           {"TWF_DECOY", TWF_DECOY}, {"TWF_TETHER", TWF_TETHER}};
  for (const auto &f : kFlags)
    if (name == f.name)
      return f.bit;
  return 0;
}

bool SameWeaponName(const std::string &a, const std::string &b) {
  if (a.size() != b.size())
    return false;
  for (size_t i = 0; i < a.size(); ++i)
    if (std::tolower(static_cast<unsigned char>(a[i])) != std::tolower(static_cast<unsigned char>(b[i])))
      return false;
  return true;
}

bool SidecarNumber(const std::string &value, float *out) {
  if (value.empty())
    return false;
  char *end = nullptr;
  const float v = std::strtof(value.c_str(), &end);
  if (end == nullptr || *end != '\0')
    return false;
  *out = v;
  return true;
}

std::vector<std::string> CheckSidecar(const SidecarParse &parse, const std::vector<std::string> &existing_names) {
  std::vector<std::string> problems;
  auto taken = [&](const std::string &name, size_t before) {
    for (const std::string &n : existing_names)
      if (SameWeaponName(n, name))
        return true;
    for (size_t i = 0; i < before; ++i)
      if (SameWeaponName(parse.weapons[i].name, name))
        return true;
    return false;
  };
  for (size_t i = 0; i < parse.weapons.size(); ++i) {
    const SidecarBlock &w = parse.weapons[i];
    const std::string where = At(w.line) + "weapon \"" + w.name + "\": ";
    if (w.name.size() >= 35) // PAGENAME_LEN (35) with its terminator
      problems.push_back(where + "the name is longer than 34 characters");
    if (taken(w.name, i))
      problems.push_back(where + "the name is already a weapon (AV-010)");
    const SidecarKey *base = w.Find("base");
    if (base == nullptr) {
      problems.push_back(where + "no base weapon");
    } else {
      bool found = false;
      for (const std::string &n : existing_names)
        found = found || SameWeaponName(n, base->values[0]);
      if (!found)
        problems.push_back(where + "base \"" + base->values[0] + "\" is not a weapon");
    }
    const SidecarKey *cls = w.Find("class");
    if (cls != nullptr && cls->values[0] != "tactical")
      problems.push_back(where + "class " + cls->values[0] +
                         " is not supported: every primary and secondary slot is taken; use tactical (D-019)");
    const SidecarKey *flags = w.Find("flags");
    if (flags != nullptr)
      for (const std::string &f : flags->values)
        if (TartarusFlagBit(f) == 0)
          problems.push_back(where + "unknown flag " + f);
  }
  return problems;
}

} // namespace tartarus
