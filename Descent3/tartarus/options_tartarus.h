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

// Options menu (F2) "Tartarus" tab (first page of F-069): mouse flight
// (F-067, m_* cvars) and the rear-view mirror presets for the left small view
// (F-060, hud_sv_left_* cvars). One tab because upstream's option list holds
// eight buttons. Follows the pattern of the upstream sheets in config.cpp:
// setup() builds the page, finish() writes the values back to the cvars
// (saved to tartarus.cfg).

#pragma once

#include <cstdint>

class newuiMenu;
class newuiSheet;

namespace tartarus {

class TartarusOptionsSheet {
public:
  void Setup(newuiMenu *menu);
  void Finish();

private:
  newuiSheet *sheet_ = nullptr;
  bool classic_ = false;
  int *mode_ = nullptr;
  int16_t *sensitivity_ = nullptr;
  int16_t *centre_rate_ = nullptr;
  bool *invert_y_ = nullptr;
  bool *indicator_ = nullptr;
  bool *roll_x_ = nullptr;
  int *mirror_ = nullptr;
  int mirror_initial_ = 0;
};

} // namespace tartarus
