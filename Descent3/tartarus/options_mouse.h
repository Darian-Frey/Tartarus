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

// Options menu (F2) "Mouse flight" tab (F-067, first page of F-069). Follows
// the pattern of the upstream sheets in config.cpp: setup() builds the page,
// finish() writes the values back, here into the m_* cvars (saved to
// tartarus.cfg).

#pragma once

#include <cstdint>

class newuiMenu;
class newuiSheet;

namespace tartarus {

class MouseOptionsSheet {
public:
  void Setup(newuiMenu *menu);
  void Finish();

private:
  newuiSheet *sheet_ = nullptr;
  bool classic_ = false;
  int *mode_ = nullptr;
  int16_t *sensitivity_ = nullptr;
  bool *invert_y_ = nullptr;
  bool *indicator_ = nullptr;
  bool *roll_x_ = nullptr;
};

} // namespace tartarus
