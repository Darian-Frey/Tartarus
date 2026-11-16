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

#include "cloak.h"

#include <algorithm>
#include <cmath>
#include <optional>
#include <string>

#include "aistruct.h"
#include "binds.h"
#include "classic.h"
#include "cloak_core.h"
#include "cvar.h"
#include "ddio.h"
#include "game.h"
#include "gamefont.h"
#include "grtext.h"
#include "hlsoundlib.h"
#include "hud.h"
#include "mouse_flight.h"
#include "object.h"
#include "object_lighting.h"
#include "objinfo.h"
#include "player.h"
#include "renderer.h"
#include "soundload.h"
#include "weapon.h"

namespace tartarus {

namespace {

Cvar *cloak_cost = nullptr;
Cvar *cloak_drain = nullptr;
Cvar *cloak_min_energy = nullptr;
Cvar *cloak_floor = nullptr;
Cvar *cloak_max_time = nullptr;
Cvar *cloak_cooldown = nullptr;
Cvar *cloak_cooldown_forced = nullptr;
Cvar *cloak_break_damage = nullptr;
Cvar *cloak_break_on_flare = nullptr;
Cvar *cloak_toggle = nullptr;
Cvar *cloak_bump_alerts = nullptr;
Cvar *cloak_key = nullptr;
Cvar *hud_cloak = nullptr;
Cvar *hud_cloak_y = nullptr;

constexpr float kFade = 0.25f;      // seconds to fade in and out (SPEC §6)
constexpr float kReadyShown = 1.5f; // "ready" stays on the gauge this long after a cooldown

CloakMachine g_cloak;
int g_player_handle = -1;
float g_since = 0.0f;        // Gametime the cloak came on: a battery fired after this breaks it
float g_ready_shown = 0.0f;  // seconds the gauge still says READY
bool g_ours = false;         // the invisibility on the player is ours to take off

CloakSettings Settings() {
  CloakSettings s;
  s.cost = cloak_cost->GetFloat();
  s.drain = cloak_drain->GetFloat();
  s.min_energy = cloak_min_energy->GetFloat();
  s.max_time = cloak_max_time->GetFloat();
  s.cooldown = cloak_cooldown->GetFloat();
  s.cooldown_forced = cloak_cooldown_forced->GetFloat();
  s.floor = cloak_floor->GetFloat();
  return s;
}

bool Usable() { return !IsClassic() && !(Game_mode & GM_MULTI) && Player_object != nullptr && Player_object->type == OBJ_PLAYER; }

bool Cloaked(const object *obj) {
  return obj->effect_info != nullptr && (obj->effect_info->type_flags & (EF_CLOAKED | EF_FADING_OUT));
}

void PlaySound(const char *name) {
  const int id = FindSoundName(name);
  if (id >= 0)
    Sound_system.Play3dSound(id, SND_PRIORITY_HIGHEST, Player_object, MAX_GAME_VOLUME / 2);
}

// Takes our invisibility off; message for a forced break or running dry.
void Uncloak(const char *why) {
  if (g_ours && Player_object != nullptr && Cloaked(Player_object))
    MakeObjectVisible(Player_object);
  g_ours = false;
  PlaySound("Cloak off");
  if (why != nullptr)
    AddHUDMessage("%s", why);
}

void Break(const char *why) {
  if (g_cloak.Drop(true, Settings()) != CloakEnd::None)
    Uncloak(why);
}

bool KeyDown() {
  const std::optional<int> key = Binds_ParseKey(cloak_key->GetString());
  return key.has_value() && (KEY_STATE(*key & 0xFF) != 0);
}

void Activate() {
  float &energy = Players[Player_num].energy;
  if (g_cloak.State() == CloakState::Cooldown) {
    AddHUDMessage("Cloak recharging: %.0f s", std::ceil(g_cloak.Left()));
    return;
  }
  if (g_cloak.State() != CloakState::Ready)
    return;
  if (Cloaked(Player_object)) {
    AddHUDMessage("Already cloaked");
    return;
  }
  if (!g_cloak.TryActivate(&energy, Settings())) {
    AddHUDMessage("Cloak needs %.0f energy", CloakEnergyToStart(Settings()));
    return;
  }
  // The engine's timer is only a backstop: Tick() ends it on time. No HUD
  // message (it would read as the powerup); the sounds are played here.
  MakeObjectInvisible(Player_object, cloak_max_time->GetFloat() + 0.5f, kFade, true);
  g_ours = true;
  g_since = Gametime;
  g_ready_shown = 0.0f;
  PlaySound("Cloak on");
}

bool FiredSinceCloaking() {
  const bool flares = cloak_break_on_flare->GetBool();
  for (int i = 0; i < MAX_WBS_PER_OBJ; ++i) {
    if (i == FLARE_INDEX && !flares)
      continue;
    if (Player_object->dynamic_wb[i].last_fire_time > g_since)
      return true;
  }
  return false;
}

void ResetAll() {
  if (g_cloak.State() == CloakState::Active)
    Uncloak(nullptr);
  g_cloak.Reset();
  g_ours = false;
  g_ready_shown = 0.0f;
}

} // namespace

void Cloak_Init() {
  const uint32_t flags = CVAR_ARCHIVE | CVAR_CLASSIC_LOCKED;
  CvarRegistry &cv = Cvars();
  cloak_cost = cv.RegisterFloat("cloak_cost", 30.0f, 0.0f, 200.0f, "Cloak: energy charged on activation", flags);
  cloak_drain = cv.RegisterFloat("cloak_drain", 8.0f, 0.0f, 100.0f, "Cloak: energy a second while cloaked", flags);
  cloak_min_energy = cv.RegisterFloat("cloak_min_energy", 40.0f, 0.0f, 200.0f, "Cloak: refused below this energy", flags);
  cloak_floor = cv.RegisterFloat("cloak_floor", 20.0f, 0.0f, 100.0f,
                                 "Cloak: drops when energy falls to this, so it never drains you to 0", flags);
  cloak_max_time = cv.RegisterFloat("cloak_max_time", 8.0f, 1.0f, 60.0f, "Cloak: seconds before it drops by itself", flags);
  cloak_cooldown = cv.RegisterFloat("cloak_cooldown", 3.0f, 0.0f, 60.0f, "Cloak: seconds before reuse after a drop or timeout", flags);
  cloak_cooldown_forced = cv.RegisterFloat("cloak_cooldown_forced", 6.0f, 0.0f, 60.0f,
                                           "Cloak: seconds before reuse after firing, a bump or a hit broke it", flags);
  cloak_break_damage = cv.RegisterFloat("cloak_break_damage", 10.0f, 0.0f, 200.0f,
                                        "Cloak: a hit of this much shield damage breaks it; 0 never", flags);
  cloak_break_on_flare = cv.RegisterBool("cloak_break_on_flare", false, "Cloak: flares break it too", flags);
  cloak_toggle = cv.RegisterBool("cloak_toggle", true, "Cloak: the key toggles (off: hold to stay cloaked)", flags);
  cloak_bump_alerts = cv.RegisterBool("cloak_bump_alerts", true, "Cloak: a robot you bump into knows you are there", flags);
  cloak_key = cv.RegisterString("cloak_key", "c", "Cloak: the key that turns it on", flags);
  hud_cloak = cv.RegisterInt("hud_cloak", 1, 0, 2, "Cloak gauge: 0 off, 1 while cloaked or recharging, 2 always", flags);
  hud_cloak_y = cv.RegisterFloat("hud_cloak_y", 0.615f, 0.05f, 0.95f, "Cloak gauge: height on the view, 0 top to 1 bottom", flags);
}

bool Cloak_HandleKey(int key) {
  if (!Usable() || !Flight_ContextActive())
    return false;
  const std::optional<int> bound = Binds_ParseKey(cloak_key->GetString());
  if (!bound.has_value() || key != *bound)
    return false;
  if (g_cloak.State() == CloakState::Active) {
    if (cloak_toggle->GetBool() && g_cloak.Drop(false, Settings()) != CloakEnd::None)
      Uncloak(nullptr);
  } else {
    Activate();
  }
  return true;
}

void Cloak_Frame() {
  if (!Usable()) {
    if (g_cloak.State() != CloakState::Ready)
      ResetAll();
    return;
  }
  // A new level or a new ship (death, restore): start from Ready (AV-007).
  if (Player_object->handle != g_player_handle || (Players[Player_num].flags & (PLAYER_FLAGS_DYING | PLAYER_FLAGS_DEAD))) {
    g_player_handle = Player_object->handle;
    if (g_cloak.State() != CloakState::Ready)
      ResetAll();
    return;
  }
  if (!Flight_ContextActive())
    return; // paused, menus, console: time stands still

  if (g_cloak.State() == CloakState::Active) {
    if (FiredSinceCloaking()) {
      Break("Cloak broken: weapon fired");
      return;
    }
    if (!cloak_toggle->GetBool() && !KeyDown()) {
      if (g_cloak.Drop(false, Settings()) != CloakEnd::None)
        Uncloak(nullptr);
      return;
    }
    if (!Cloaked(Player_object)) { // something else took it off
      g_ours = false;
      g_cloak.Drop(false, Settings());
      return;
    }
  }
  const CloakState before = g_cloak.State();
  const bool timed_out = g_cloak.State() == CloakState::Active && g_cloak.Left() <= Frametime;
  const CloakEnd end = g_cloak.Tick(Frametime, &Players[Player_num].energy, Settings());
  if (end != CloakEnd::None)
    Uncloak(timed_out ? nullptr : "Cloak: energy low");
  if (before == CloakState::Cooldown && g_cloak.State() == CloakState::Ready)
    g_ready_shown = kReadyShown;
  else if (g_ready_shown > 0.0f)
    g_ready_shown -= Frametime;
}

void Cloak_WeaponFired() {
  if (Usable() && g_cloak.State() == CloakState::Active)
    Break("Cloak broken: weapon fired");
}

void Cloak_PlayerDamaged(const object *player, float damage) {
  if (player != Player_object || !Usable() || g_cloak.State() != CloakState::Active)
    return;
  const float limit = cloak_break_damage->GetFloat();
  if (limit > 0.0f && damage >= limit)
    Break("Cloak broken: hit");
}

void Cloak_Bumped(object *robot, const object *player) {
  if (player != Player_object || !Usable() || g_cloak.State() != CloakState::Active || robot == nullptr ||
      robot->type != OBJ_ROBOT || IS_GUIDEBOT(robot))
    return;
  Break("Cloak broken: contact");
  if (cloak_bump_alerts->GetBool() && robot->ai_info != nullptr)
    robot->ai_info->awareness = AWARE_FULLY;
}

void Cloak_Render() {
  if (!Usable() || hud_cloak->GetInt() == 0 || !Flight_ContextActive())
    return;
  const CloakState state = g_cloak.State();
  const bool show = hud_cloak->GetInt() == 2 || state != CloakState::Ready || g_ready_shown > 0.0f;
  if (!show)
    return;

  // A short bar under the centre of the view: blue ready, white draining
  // while cloaked, red refilling while recharging (SPEC §6).
  const float scale = Game_window_h / 720.0f;
  const int w = static_cast<int>(std::lround(110.0f * scale));
  const int h = std::max(3, static_cast<int>(std::lround(5.0f * scale)));
  const int cx = Game_window_x + Game_window_w / 2;
  const int y = Game_window_y + static_cast<int>(Game_window_h * hud_cloak_y->GetFloat());
  const int x1 = cx - w / 2;
  float fill = 1.0f;
  ddgr_color colour = GR_RGB(90, 160, 255);
  const char *label = "CLOAK READY";
  if (state == CloakState::Active) {
    fill = g_cloak.Fraction();
    colour = GR_RGB(235, 240, 255);
    label = "CLOAKED";
  } else if (state == CloakState::Cooldown) {
    fill = 1.0f - g_cloak.Fraction();
    colour = GR_RGB(255, 80, 70);
    label = "CLOAK RECHARGING";
  }

  StartFrame(0, 0, Max_window_w, Max_window_h, false);
  rend_SetAlphaType(AT_CONSTANT);
  rend_SetAlphaValue(90);
  rend_FillRect(GR_RGB(20, 24, 30), x1 - 1, y - 1, x1 + w + 1, y + h + 1);
  rend_SetAlphaValue(200);
  rend_FillRect(colour, x1, y, x1 + static_cast<int>(std::lround(w * fill)), y + h);
  rend_SetAlphaType(AT_ALWAYS);
  grtext_SetFont(SMALL_FONT);
  grtext_SetColor(colour);
  grtext_SetAlpha(220);
  grtext_Printf(cx - grtext_GetTextLineWidth(label) / 2, y + h + static_cast<int>(3 * scale), "%s", label);
  grtext_Flush();
  EndFrame();
}

} // namespace tartarus
