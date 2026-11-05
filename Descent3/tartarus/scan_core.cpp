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

// Scanner table. See scan_core.h.

#include "scan_core.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <string>

namespace tartarus {

void ScanTable::Clear() {
  rooms_.clear();
  total_ = 0;
  generation_ = 1;
  new_generation_ = 0;
}

uint16_t ScanTable::Get(int room, int face) const {
  if (room < 0 || face < 0 || static_cast<size_t>(room) >= rooms_.size())
    return 0;
  const std::vector<uint16_t> &faces = rooms_[static_cast<size_t>(room)];
  return static_cast<size_t>(face) < faces.size() ? faces[static_cast<size_t>(face)] : 0;
}

bool ScanTable::Mark(int room, int face) {
  if (room < 0 || face < 0)
    return false;
  if (static_cast<size_t>(room) >= rooms_.size())
    rooms_.resize(static_cast<size_t>(room) + 1);
  std::vector<uint16_t> &faces = rooms_[static_cast<size_t>(room)];
  if (static_cast<size_t>(face) >= faces.size())
    faces.resize(static_cast<size_t>(face) + 1, 0);
  if (faces[static_cast<size_t>(face)] != 0)
    return false;
  faces[static_cast<size_t>(face)] = generation_;
  ++total_;
  return true;
}

bool ScanTable::IsScanned(int room, int face) const { return Get(room, face) != 0; }

size_t ScanTable::ScannedInRoom(int room) const {
  if (room < 0 || static_cast<size_t>(room) >= rooms_.size())
    return 0;
  size_t count = 0;
  for (uint16_t generation : rooms_[static_cast<size_t>(room)])
    count += generation != 0 ? 1 : 0;
  return count;
}

namespace {

constexpr uint8_t kMagic[4] = {'A', 'M', 'A', 'P'};
constexpr uint32_t kVersion = 1;
constexpr size_t kHeaderBytes = 16;
constexpr uint32_t kMostRooms = 1u << 16, kMostFaces = 1u << 20; // far past any level: guards against damage

void Put32(std::vector<uint8_t> *out, uint32_t value) {
  for (int shift = 0; shift < 32; shift += 8)
    out->push_back(static_cast<uint8_t>(value >> shift));
}

bool Get32(const std::vector<uint8_t> &bytes, size_t *at, uint32_t *value) {
  if (*at + 4 > bytes.size())
    return false;
  *value = 0;
  for (int i = 0; i < 4; ++i)
    *value |= static_cast<uint32_t>(bytes[*at + static_cast<size_t>(i)]) << (i * 8);
  *at += 4;
  return true;
}

} // namespace

std::vector<uint8_t> ScanTable::Serialize(uint32_t fingerprint) const {
  std::vector<uint8_t> out(kMagic, kMagic + 4);
  Put32(&out, kVersion);
  Put32(&out, fingerprint);
  Put32(&out, static_cast<uint32_t>(rooms_.size()));
  for (const std::vector<uint16_t> &faces : rooms_) {
    Put32(&out, static_cast<uint32_t>(faces.size()));
    uint8_t byte = 0;
    for (size_t i = 0; i < faces.size(); ++i) {
      if (faces[i] != 0)
        byte |= static_cast<uint8_t>(1u << (i % 8));
      if (i % 8 == 7 || i + 1 == faces.size()) {
        out.push_back(byte);
        byte = 0;
      }
    }
  }
  return out;
}

bool ScanTable::Deserialize(const std::vector<uint8_t> &bytes, uint32_t fingerprint) {
  Clear();
  if (bytes.size() < kHeaderBytes)
    return false;
  for (size_t i = 0; i < 4; ++i) {
    if (bytes[i] != kMagic[i])
      return false;
  }
  size_t at = 4;
  uint32_t version = 0, stamp = 0, room_count = 0;
  if (!Get32(bytes, &at, &version) || !Get32(bytes, &at, &stamp) || !Get32(bytes, &at, &room_count))
    return false;
  if (version != kVersion || stamp != fingerprint || room_count > kMostRooms)
    return false;
  for (uint32_t room = 0; room < room_count; ++room) {
    uint32_t face_count = 0;
    if (!Get32(bytes, &at, &face_count) || face_count > kMostFaces || at + (face_count + 7) / 8 > bytes.size()) {
      Clear();
      return false;
    }
    for (uint32_t face = 0; face < face_count; ++face) {
      if (bytes[at + face / 8] & (1u << (face % 8)))
        Mark(static_cast<int>(room), static_cast<int>(face));
    }
    at += (face_count + 7) / 8;
  }
  generation_ = 2; // what was loaded is generation 1: not new at the next opening
  return true;
}

void ScanTable::OpenMap() {
  new_generation_ = generation_;
  ++generation_;
}

bool ScanTable::IsNew(int room, int face) const {
  const uint16_t generation = Get(room, face);
  return generation != 0 && generation == new_generation_;
}

std::vector<SymbolLine> MapSymbolLines(int symbol) {
  std::vector<SymbolLine> lines;
  auto polygon = [&](int sides, float start, float radius) {
    for (int i = 0; i < sides; ++i) {
      const float a = start + 6.2831853f * i / sides, b = start + 6.2831853f * (i + 1) / sides;
      lines.push_back({radius * std::cos(a), radius * std::sin(a), radius * std::cos(b), radius * std::sin(b)});
    }
  };
  switch (symbol) {
  case kSymbolKey:
    polygon(4, 0.0f, 1.0f); // a diamond
    break;
  case kSymbolDoor:
  case kSymbolLockedDoor:
    polygon(4, 0.7853982f, 1.1f); // a square
    if (symbol == kSymbolLockedDoor) {
      lines.push_back({-0.78f, -0.78f, 0.78f, 0.78f});
      lines.push_back({-0.78f, 0.78f, 0.78f, -0.78f});
    }
    break;
  case kSymbolGoal:
    polygon(3, 1.5707963f, 1.2f); // two triangles make a star
    polygon(3, -1.5707963f, 1.2f);
    break;
  case kSymbolPowerup:
    lines.push_back({-0.6f, 0.0f, 0.6f, 0.0f});
    lines.push_back({0.0f, -0.6f, 0.0f, 0.6f});
    break;
  case kSymbolMarker:
    lines.push_back({-0.8f, -0.8f, 0.8f, 0.8f});
    lines.push_back({-0.8f, 0.8f, 0.8f, -0.8f});
    break;
  case kSymbolRing:
    polygon(12, 0.0f, 1.6f);
    break;
  default:
    break;
  }
  return lines;
}

namespace {

bool NameHas(const char *name, const char *word) {
  if (name == nullptr)
    return false;
  std::string lower(name);
  std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  return lower.find(word) != std::string::npos;
}

} // namespace

bool IsKeyName(const char *name) { return NameHas(name, "key"); }

bool IsHiddenName(const char *name) { return NameHas(name, "invisible"); }

bool FaceInReach(const float centre[3], const float normal[3], const float from[3], float range) {
  const float dx = centre[0] - from[0], dy = centre[1] - from[1], dz = centre[2] - from[2];
  if (dx * dx + dy * dy + dz * dz > range * range)
    return false;
  return dx * normal[0] + dy * normal[1] + dz * normal[2] < 0.0f; // its front is toward us
}

} // namespace tartarus
