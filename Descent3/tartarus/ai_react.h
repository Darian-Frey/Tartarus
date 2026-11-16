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

// Robot reactions to Tartarus tools (F-070). All through stock AI machinery,
// nothing in the robot structs changes (AV-003):
// - Stunned: the stock AIF_DISABLED switch (what scripts use to power robots
//   down) for a while; the robot stops thinking and firing and drifts. Timers
//   live here and go beside saves, so a robot saved mid-stun wakes up again.
//   Robots a script has already switched off are left alone.
// - Decloak noise: when the cloak drops, the stock hearing system is told the
//   player made a noise, so robots in earshot become aware and come to look.
// - Gravity escape is in gravwell.cpp (robots thrust against the pull).
// Nothing in Classic Mode or multiplayer.

#pragma once

#include <filesystem>

struct object;

namespace tartarus {

/// Registers the ai_ cvars and the stun cheat command. Called from Tartarus_Init().
void AiReact_Init();

/// Stuns a robot for seconds (a longer stun extends, a shorter one does not
/// shorten). False if it cannot be stunned: not an AI robot, the Guide-Bot,
/// switched off by a script, or Classic Mode / multiplayer.
bool AiReact_Stun(object *robot, float seconds);

/// True while a robot is stunned by Tartarus.
bool AiReact_IsStunned(const object *robot);

/// Per frame, from Frame(): wakes robots whose stun is over, sparks the rest.
void AiReact_Frame();

/// The cloak has dropped (cloak.cpp): robots in earshot hear it.
void AiReact_Decloaked();

/// A new level: no stuns carried over.
void AiReact_Reset();

/// Beside a save: appended to the .tstate file the gravity missile writes.
void AiReact_Save(const std::filesystem::path &save_path);
void AiReact_Load(const std::filesystem::path &save_path);

} // namespace tartarus
