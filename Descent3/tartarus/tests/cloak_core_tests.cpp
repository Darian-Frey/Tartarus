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

#include <gtest/gtest.h>

#include "../cloak_core.h"

using tartarus::CloakEnd;
using tartarus::CloakMachine;
using tartarus::CloakSettings;
using tartarus::CloakState;

namespace {
const CloakSettings kSpec; // SPEC.md §6 defaults
}

TEST(Cloak, ActivationChargesTheCostAndStarts) {
  CloakMachine m;
  float energy = 100.0f;
  ASSERT_TRUE(m.TryActivate(&energy, kSpec));
  EXPECT_EQ(m.State(), CloakState::Active);
  EXPECT_FLOAT_EQ(energy, 70.0f);
  EXPECT_FLOAT_EQ(m.Left(), 8.0f);
  EXPECT_FLOAT_EQ(m.Fraction(), 1.0f);
}

TEST(Cloak, RefusedBelowTheEnergyToStartOrWhenNotReady) {
  EXPECT_FLOAT_EQ(tartarus::CloakEnergyToStart(kSpec), 58.0f); // 30 cost + 20 floor + 8 for a second
  CloakMachine m;
  float energy = 57.9f;
  EXPECT_FALSE(m.TryActivate(&energy, kSpec));
  EXPECT_FLOAT_EQ(energy, 57.9f);
  EXPECT_EQ(m.State(), CloakState::Ready);
  energy = 100.0f;
  ASSERT_TRUE(m.TryActivate(&energy, kSpec));
  EXPECT_FALSE(m.TryActivate(&energy, kSpec)); // already active: no second charge
  EXPECT_FLOAT_EQ(energy, 70.0f);
}

TEST(Cloak, MinEnergyStillCountsWhenHigher) {
  CloakSettings s;
  s.min_energy = 90.0f;
  CloakMachine m;
  float energy = 80.0f;
  EXPECT_FALSE(m.TryActivate(&energy, s));
}

TEST(Cloak, DrainsWhileActive) {
  CloakMachine m;
  float energy = 100.0f;
  m.TryActivate(&energy, kSpec);
  EXPECT_EQ(m.Tick(1.0f, &energy, kSpec), CloakEnd::None);
  EXPECT_FLOAT_EQ(energy, 62.0f);
  EXPECT_FLOAT_EQ(m.Left(), 7.0f);
}

TEST(Cloak, TimesOutAtMaxTimeIntoTheShortCooldown) {
  CloakMachine m;
  float energy = 200.0f;
  m.TryActivate(&energy, kSpec);
  CloakEnd end = CloakEnd::None;
  for (int i = 0; i < 81 && end == CloakEnd::None; ++i)
    end = m.Tick(0.1f, &energy, kSpec);
  EXPECT_EQ(end, CloakEnd::Voluntary);
  EXPECT_EQ(m.State(), CloakState::Cooldown);
  EXPECT_FLOAT_EQ(m.Left(), 3.0f);
}

TEST(Cloak, ReachingTheFloorEndsItWithoutGoingUnder) {
  CloakMachine m;
  float energy = 58.0f; // 28 after the cost: 1 s of drain to the floor of 20
  m.TryActivate(&energy, kSpec);
  EXPECT_EQ(m.Tick(0.5f, &energy, kSpec), CloakEnd::None);
  EXPECT_FLOAT_EQ(energy, 24.0f);
  EXPECT_EQ(m.Tick(1.0f, &energy, kSpec), CloakEnd::Voluntary);
  EXPECT_FLOAT_EQ(energy, 20.0f);
  EXPECT_EQ(m.State(), CloakState::Cooldown);
}

TEST(Cloak, AlreadyUnderTheFloorItIsNotDrainedFurther) {
  CloakMachine m;
  float energy = 100.0f;
  m.TryActivate(&energy, kSpec);
  energy = 15.0f; // weapons spent it while cloaked
  EXPECT_EQ(m.Tick(0.1f, &energy, kSpec), CloakEnd::Voluntary);
  EXPECT_FLOAT_EQ(energy, 15.0f);
}

TEST(Cloak, ForcedDropUsesTheLongCooldown) {
  CloakMachine m;
  float energy = 100.0f;
  m.TryActivate(&energy, kSpec);
  EXPECT_EQ(m.Drop(true, kSpec), CloakEnd::Forced);
  EXPECT_EQ(m.State(), CloakState::Cooldown);
  EXPECT_FLOAT_EQ(m.Left(), 6.0f);
  EXPECT_EQ(m.Drop(true, kSpec), CloakEnd::None); // not active any more
}

TEST(Cloak, VoluntaryDropUsesTheShortCooldown) {
  CloakMachine m;
  float energy = 100.0f;
  m.TryActivate(&energy, kSpec);
  EXPECT_EQ(m.Drop(false, kSpec), CloakEnd::Voluntary);
  EXPECT_FLOAT_EQ(m.Left(), 3.0f);
}

TEST(Cloak, CooldownCountsDownToReadyAndBlocksActivation) {
  CloakMachine m;
  float energy = 100.0f;
  m.TryActivate(&energy, kSpec);
  m.Drop(false, kSpec);
  EXPECT_FALSE(m.TryActivate(&energy, kSpec));
  m.Tick(2.9f, &energy, kSpec);
  EXPECT_EQ(m.State(), CloakState::Cooldown);
  EXPECT_FLOAT_EQ(energy, 70.0f); // no drain while cooling down
  m.Tick(0.2f, &energy, kSpec);
  EXPECT_EQ(m.State(), CloakState::Ready);
  EXPECT_TRUE(m.TryActivate(&energy, kSpec));
}

TEST(Cloak, ResetGoesStraightToReady) {
  CloakMachine m;
  float energy = 100.0f;
  m.TryActivate(&energy, kSpec);
  m.Reset();
  EXPECT_EQ(m.State(), CloakState::Ready);
  EXPECT_FLOAT_EQ(m.Fraction(), 0.0f);
}

TEST(Cloak, NoTimeNoChange) {
  CloakMachine m;
  float energy = 100.0f;
  m.TryActivate(&energy, kSpec);
  EXPECT_EQ(m.Tick(0.0f, &energy, kSpec), CloakEnd::None);
  EXPECT_FLOAT_EQ(energy, 70.0f);
}

TEST(Cloak, ZeroCooldownGoesStraightBackToReady) {
  CloakSettings s;
  s.cooldown = 0.0f;
  CloakMachine m;
  float energy = 100.0f;
  m.TryActivate(&energy, s);
  m.Drop(false, s);
  EXPECT_EQ(m.State(), CloakState::Ready);
}
