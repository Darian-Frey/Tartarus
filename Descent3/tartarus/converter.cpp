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

#include "converter.h"

#include <optional>

#include "binds.h"
#include "classic.h"
#include "converter_core.h"
#include "cvar.h"
#include "ddio.h"
#include "game.h"
#include "hlsoundlib.h"
#include "hud.h"
#include "mouse_flight.h"
#include "object.h"
#include "player.h"
#include "sounds.h"

namespace tartarus {

namespace {

Cvar *conv_rate = nullptr;
Cvar *conv_ratio = nullptr;
Cvar *conv_floor = nullptr;
Cvar *conv_to_shields_key = nullptr;
Cvar *conv_to_energy_key = nullptr;

constexpr float kSoundDelay = 0.25f; // as the energy centre (FUELCEN_SOUND_DELAY)

bool g_active = false;
float g_sound_wait = 0.0f;
int g_held = 0;           // 1 to shields, -1 to energy, 0 neither
bool g_told_stop = false; // the "can't go on" message, once per press

bool KeyDown(const Cvar *name) {
  const std::optional<int> key = Binds_ParseKey(name->GetString());
  return key.has_value() && (KEY_STATE(*key & 0xFF) != 0);
}

} // namespace

void Converter_Init() {
  const uint32_t flags = CVAR_ARCHIVE | CVAR_CLASSIC_LOCKED;
  CvarRegistry &cv = Cvars();
  conv_rate = cv.RegisterFloat("conv_rate", 25.0f, 1.0f, 200.0f, "Converter: units a second taken from the source", flags);
  conv_ratio = cv.RegisterFloat("conv_ratio", 0.5f, 0.05f, 1.0f, "Converter: units gained for each unit taken", flags);
  conv_floor = cv.RegisterFloat("conv_floor", 20.0f, 0.0f, 100.0f, "Converter: never takes the source below this", flags);
  conv_to_shields_key = cv.RegisterString("conv_to_shields_key", "b", "Converter: energy into shields while held", flags);
  conv_to_energy_key = cv.RegisterString("conv_to_energy_key", "n", "Converter: shields into energy while held", flags);
}

void Converter_Frame() {
  g_active = false;
  if (IsClassic() || (Game_mode & GM_MULTI) || !Flight_ContextActive() || Player_object == nullptr ||
      Player_object->type != OBJ_PLAYER) {
    g_held = 0;
    return;
  }
  const int held = KeyDown(conv_to_shields_key) ? 1 : KeyDown(conv_to_energy_key) ? -1 : 0;
  if (held != g_held) {
    g_held = held;
    g_told_stop = false;
    g_sound_wait = 0.0f;
  }
  if (held == 0)
    return;

  float &energy = Players[Player_num].energy;
  float &shields = Player_object->shields;
  const bool to_shields = held > 0;
  const float source = to_shields ? energy : shields;
  const float destination = to_shields ? shields : energy;
  const ConvertResult r = ConvertStep(source, destination, to_shields ? MAX_SHIELDS : MAX_ENERGY, conv_rate->GetFloat(),
                                      conv_ratio->GetFloat(), conv_floor->GetFloat(), Frametime);
  if (r.source == source) {
    if (!g_told_stop) {
      g_told_stop = true;
      if (destination >= (to_shields ? MAX_SHIELDS : MAX_ENERGY))
        AddHUDMessage(to_shields ? "Converter: shields full" : "Converter: energy full");
      else
        AddHUDMessage(to_shields ? "Converter: energy too low" : "Converter: shields too low");
    }
    return;
  }
  (to_shields ? energy : shields) = r.source;
  (to_shields ? shields : energy) = r.destination;
  g_active = true;
  g_sound_wait -= Frametime;
  if (g_sound_wait <= 0.0f) {
    g_sound_wait = kSoundDelay;
    Sound_system.Play3dSound(SOUND_REFUELING, SND_PRIORITY_HIGH, Player_object);
  }
}

bool Converter_Active() { return g_active; }

} // namespace tartarus
