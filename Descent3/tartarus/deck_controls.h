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

// Key, controller and weapon-order settings on the deck (F-069, D-016).
// CtlConfig() asks here first. Bindings go through upstream's own functions;
// only the screens and the "press a key" loop are new. Classic Mode and
// multiplayer keep the stock screen.

#pragma once

#include <cstdint>

#include "controller.h"

struct t_cfg_element;

namespace tartarus {

/// Assigns a binding and removes its repeats: upstream's
/// ctl_cfg_set_and_verify_changes(), which is file-local.
using ControlsDeckAssign = void (*)(int16_t fnid, ct_type elem_type, uint8_t ctrl, uint8_t elem, int8_t slot);

/// Runs the screens for mode (CTLCONFIG_*). False: run stock. ui_callback keeps
/// the game running behind the screens in a level.
bool ControlsDeck_Run(int mode, void (*ui_callback)(), ControlsDeckAssign assign, const t_cfg_element *keys, int n_keys,
                      const t_cfg_element *joys, int n_joys);

} // namespace tartarus
