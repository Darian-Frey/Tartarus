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

// Password-gated cheat menu (F-012). See cheats.h.
//
// The effects reuse what upstream's typed cheats do (GameCheat.cpp) so the
// results match stock behaviour; the "infinite" rows are held at maximum
// every frame instead of refilled once (SPEC.md §4).

#include "cheats.h"
#include "map_scan.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>

#include "TelComAutoMap.h"
#include "classic.h"
#include "console.h"
#include "controls.h"
#include "damage.h"
#include "ddio.h"
#include "game.h"
#include "gamefont.h"
#include "gamesequence.h"
#include "grdefs.h"
#include "grtext.h"
#include "hlsoundlib.h"
#include "hud.h"
#include "object.h"
#include "object_lighting.h"
#include "objinfo.h"
#include "player.h"
#include "renderer.h"
#include "room.h"
#include "ship.h"
#include "sounds.h"
#include "weapon.h"

extern bool Display_renderer_stats; // GameCheat.cpp

namespace tartarus {

namespace {

CheatState g_state;

// What the hold loop has applied to the game, so it can be undone exactly
// once when the hold ends (toggle off, Classic switch, multiplayer, relock).
bool g_applied_invuln = false;
bool g_applied_cloak = false;
bool g_applied_stats = false;

// Menu state.
bool g_menu_open = false;
std::string g_password;
std::string g_status;
int g_selected = 0;

constexpr int kMenuKey = KEY_SHIFTED + KEY_F12; // stock F12 drops a marker
constexpr size_t kMaxPassword = 16;

bool InSinglePlayerLevel() {
  return Player_object != nullptr && Player_num >= 0 && Players[Player_num].objnum >= 0 && !(Game_mode & GM_MULTI);
}

/// Why cheats are unavailable right now, or nullptr if they are available.
const char *RefusalReason() {
  if (IsClassic())
    return "Cheats are not available in Classic Mode";
  if (Game_mode & GM_MULTI)
    return "Cheats are not available in multiplayer";
  if (!InSinglePlayerLevel())
    return "Cheats are only available during a level";
  return nullptr;
}

void Notify(const std::string &msg) {
  g_status = msg;
  AddHUDMessage("%s", msg.c_str());
  Console().Print(msg);
}

// Upstream semantics (D-006): cheated progress and scores do not count.
void MarkCheater() {
  IsCheater = true;
  Players[Player_num].score = 0;
}

void PlayCheatSound() { Sound_system.Play2dSound(SOUND_CHEATER); }

bool IsDying() { return (Players[Player_num].flags & (PLAYER_FLAGS_DYING | PLAYER_FLAGS_DEAD)) != 0; }

// ---- Hold-at-max loop ------------------------------------------------------

void FillAmmo(bool primary, bool secondary) {
  player &p = Players[Player_num];
  const ship &s = Ships[p.ship_index];
  for (int i = 0; i < MAX_PLAYER_WEAPONS; i++) {
    const bool is_primary = i < SECONDARY_INDEX;
    if (!(is_primary ? primary : secondary))
      continue;
    if ((p.weapon_flags & HAS_FLAG(i)) && s.max_ammo[i] > 0)
      p.weapon_ammo[i] = static_cast<uint16_t>(s.max_ammo[i]);
  }
}

/// Brings the game in line with `h`, undoing whatever the loop applied
/// before and no longer holds. `touch_game` false: forget what was applied
/// without touching the (possibly new, possibly networked) player object.
void Sync(const CheatHolds &h, bool touch_game = true) {
  if (!touch_game) {
    g_applied_invuln = g_applied_cloak = false;
    if (g_applied_stats) {
      Hud_stat_mask &= ~STAT_FPS;
      Display_renderer_stats = false;
      g_applied_stats = false;
    }
    return;
  }

  player &p = Players[Player_num];
  object *obj = &Objects[p.objnum];
  const bool alive = obj->type == OBJ_PLAYER && !IsDying();

  if (alive) {
    if (h.shields)
      obj->shields = MAX_SHIELDS;
    if (h.energy)
      p.energy = MAX_ENERGY;
    if (h.primary_ammo || h.secondary_ammo)
      FillAmmo(h.primary_ammo, h.secondary_ammo);
    if (h.afterburner)
      p.afterburn_time_left = AFTERBURN_TIME;
  }

  if (h.invulnerable && alive) {
    if (!(p.flags & PLAYER_FLAGS_INVULNERABLE))
      MakePlayerInvulnerable(Player_num, 1000.0f, false);
    else if (p.invulnerable_time < 100.0f)
      p.invulnerable_time = 1000.0f;
    g_applied_invuln = true;
  } else if (!h.invulnerable && g_applied_invuln) {
    MakePlayerVulnerable(Player_num);
    g_applied_invuln = false;
  }

  if (h.cloak && alive && obj->effect_info != nullptr) {
    if (!(obj->effect_info->type_flags & EF_CLOAKED))
      MakeObjectInvisible(obj, 1000.0f, 0.5f, true); // no powerup message
    else if (obj->effect_info->cloak_time < 100.0f)
      obj->effect_info->cloak_time = 1000.0f;
    g_applied_cloak = true;
  } else if (!h.cloak && g_applied_cloak) {
    if (obj->effect_info != nullptr && (obj->effect_info->type_flags & EF_CLOAKED))
      MakeObjectVisible(obj);
    g_applied_cloak = false;
  }

  if (h.stats != g_applied_stats) {
    if (h.stats)
      Hud_stat_mask |= STAT_FPS;
    else
      Hud_stat_mask &= ~STAT_FPS;
    Display_renderer_stats = h.stats;
    g_applied_stats = h.stats;
  }
}

bool AnythingApplied() { return g_applied_invuln || g_applied_cloak || g_applied_stats; }

// ---- One-shot effects (as GameCheat.cpp) -----------------------------------

void GiveAllKeys() { Players[Player_num].keys = 0xFF; }

void GiveAllWeapons() {
  player &p = Players[Player_num];
  object *objp = &Objects[p.objnum];
  p.weapon_flags = 0xFFFFFFFF;
  FillAmmo(true, true);
  p.energy = MAX_ENERGY;
  objp->shields = MAX_SHIELDS;
  p.afterburn_time_left = AFTERBURN_TIME;
  objp->dynamic_wb[LASER_INDEX].flags |= DWBF_QUAD;
  objp->dynamic_wb[SUPER_LASER_INDEX].flags |= DWBF_QUAD;

  const int quad_id = FindObjectIDName("QuadLaser");
  if (quad_id > -1)
    p.inventory.Add(OBJ_POWERUP, quad_id, nullptr, -1, -1, INVF_MISSIONITEM | INVF_TIMEOUTONSPEW);

  struct Counter {
    const char *weapon;
    const char *powerup;
    const char *fallback;
  };
  const Counter counters[] = {{"Chaff", "chaff", "Shield"},
                              {"Betty", "Betty4pack", "Energy"},
                              {"SeekerMine", "Seeker3pack", "Shield"},
                              {"Gunboy", "gunboypowerup", "Energy"},
                              {"ProxMine", "ProxMinepowerup", "Shield"}};
  for (const Counter &c : counters) {
    const int wid = FindWeaponName(c.weapon);
    if (wid == -1)
      continue;
    int pid = FindObjectIDName(c.powerup);
    if (pid == -1)
      pid = FindObjectIDName(c.fallback);
    for (int i = 0; i < 30; i++)
      p.counter_measures.Add(OBJ_WEAPON, wid, objp, OBJ_POWERUP, pid);
  }
}

void KillAllRobots() {
  for (int i = 0; i < MAX_OBJECTS; i++) {
    if (Objects[i].type == OBJ_ROBOT || (Objects[i].type == OBJ_BUILDING && Objects[i].ai_info)) {
      if (Objects[i].id != ROBOT_GUIDEBOT && Objects[i].id != ROBOT_GUIDEBOTRED)
        SetObjectDeadFlag(&Objects[i]); // spare the guide-bots, as upstream
    }
  }
}

void RevealAutomap() {
  for (int i = 0; i < MAX_ROOMS; i++) {
    if (!(Rooms[i].flags & RF_SECRET)) {
      AutomapVisMap[i] = 1;
      MapScan_RevealRoom(i);
    }
  }
}

void SelfDestruct() {
  // Invulnerability would refuse the damage.
  g_state.SetActive(CheatId::Ambrosia, false);
  g_state.SetActive(CheatId::Titanhide, false);
  Sync(g_state.Holds());
  object *obj = &Objects[Players[Player_num].objnum];
  ApplyDamageToPlayer(obj, nullptr, PD_ENERGY_WEAPON, obj->shields + 10);
}

// ---- Using a row ------------------------------------------------------------

std::string OnOff(bool on) { return on ? "on" : "off"; }

/// Uses row `id`: press a button, flip a toggle, or (slider) set `slider_value`.
void Use(CheatId id, float slider_value = -1.0f) {
  if (const char *why = RefusalReason()) {
    Notify(why);
    return;
  }
  const CheatInfo &row = GetCheatInfo(id);
  if (!g_state.IsUnlocked(id)) {
    Notify(std::string(row.label) + ": locked. Enter its password first");
    return;
  }
  if (!row.available) {
    Notify(std::string(row.label) + ": not implemented yet");
    return;
  }

  if (row.marks_cheater)
    MarkCheater();

  switch (row.kind) {
  case CheatKind::Toggle: {
    const bool on = !g_state.IsActive(id);
    g_state.SetActive(id, on);
    Sync(g_state.Holds());
    Notify(std::string(row.label) + " " + OnOff(on));
    if (on)
      PlayCheatSound();
    return;
  }
  case CheatKind::Slider: {
    const float v = slider_value >= 0.0f ? slider_value : 1.0f;
    g_state.SetTimeScale(v);
    char buf[64];
    std::snprintf(buf, sizeof(buf), "Time scale %.2f", g_state.TimeScale());
    Notify(buf);
    return;
  }
  case CheatKind::Button:
    break;
  }

  switch (id) {
  case CheatId::Cornucopia:
    GiveAllWeapons();
    GiveAllKeys();
    Notify("All weapons, ammo and keys");
    break;
  case CheatId::SkeletonKey:
    GiveAllKeys();
    Notify("All keys");
    break;
  case CheatId::Thanatos:
    KillAllRobots();
    Notify("All robots destroyed");
    break;
  case CheatId::Ariadne:
    RevealAutomap();
    Notify("Automap revealed");
    break;
  case CheatId::Ferryman:
    Cheats_CloseMenu();
    Game_interface_mode = GAME_LEVEL_WARP; // upstream's level picker
    break;
  case CheatId::Styx:
    Cheats_CloseMenu();
    SelfDestruct();
    break;
  case CheatId::Catharsis:
    g_state.RelockAll();
    Sync(g_state.Holds());
    Notify("Cheats relocked. The cheat flag clears at the next new game");
    return; // no cheat sound
  default:
    break;
  }
  PlayCheatSound();
}

void TryPassword(const std::string &text) {
  if (const char *why = RefusalReason()) {
    Notify(why);
    return;
  }
  g_state.SetOwner(Players[Player_num].callsign);
  const CheatInfo *row = nullptr;
  switch (g_state.Unlock(text, &row)) {
  case CheatState::UnlockResult::Unknown:
    Notify("Unknown password");
    return;
  case CheatState::UnlockResult::AlreadyUnlocked:
    break;
  case CheatState::UnlockResult::Unlocked:
    if (row->marks_cheater) {
      MarkCheater();
      PlayCheatSound();
    }
    break;
  }
  // Select the row so Enter uses it next.
  g_selected = static_cast<int>(row->id);
  if (row->id == CheatId::Catharsis) {
    Use(CheatId::Catharsis); // a relock needs no second step
    return;
  }
  Notify(std::string(row->label) + " unlocked" + (row->available ? "" : " (not implemented yet)"));
}

// ---- Console commands ---------------------------------------------------------

struct CommandCheat {
  const char *command;
  CheatId cheat;
};
constexpr CommandCheat kCommandCheats[] = {
    {"give", CheatId::Cornucopia},
    {"god", CheatId::Ambrosia},
    {"timescale", CheatId::Torpor},
    {"map", CheatId::Ferryman},
};

bool ParseOnOff(const std::string &s, bool current, bool *out) {
  if (s == "1" || s == "on")
    *out = true;
  else if (s == "0" || s == "off")
    *out = false;
  else if (s == "toggle")
    *out = !current;
  else
    return false;
  return true;
}

std::string Squash(const std::string &s) {
  std::string out;
  for (char c : s) {
    if (c != ' ' && c != '_' && c != '-')
      out.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
  }
  return out;
}

void RegisterCommands(ConsoleCore &con) {
  con.AddCommand({"cheat", "cheat [password] | cheat use <password> [value]",
                  "Unlock a cheat (same passwords as the cheat menu); 'use' fires a button, flips a toggle or sets the "
                  "time scale; no argument lists them",
                  CMD_NONE, [](const CommandArgs &a, ConsoleCore &c) {
                    if (a.size() > 1 && a[1] == "use") {
                      if (a.size() < 3) {
                        c.Print("usage: cheat use <password> [value]");
                        return;
                      }
                      const CheatInfo *row = FindCheatByPassword(a[2]);
                      if (row == nullptr) {
                        c.Print("cheat use: unknown cheat '" + a[2] + "'");
                        return;
                      }
                      float value = -1.0f;
                      if (row->kind == CheatKind::Slider) {
                        value = 1.0f;
                        if (a.size() > 3) {
                          char *end = nullptr;
                          value = std::strtof(a[3].c_str(), &end);
                          if (end == a[3].c_str() || *end != '\0' || !std::isfinite(value)) {
                            c.Print("cheat use: value must be a number");
                            return;
                          }
                          value = std::clamp(value, kTorporMin, kTorporMax);
                        }
                      }
                      Use(row->id, value);
                      return;
                    }
                    if (a.size() > 1) {
                      TryPassword(a[1]);
                      return;
                    }
                    for (const CheatInfo &row : CheatTable()) {
                      std::string state = g_state.IsUnlocked(row.id) ? "unlocked" : "locked";
                      if (row.kind == CheatKind::Toggle && g_state.IsActive(row.id))
                        state = "ON";
                      if (!row.available)
                        state += ", not implemented yet";
                      c.Print("  " + std::string(row.label) + " (" + state + ")");
                    }
                  }});
  con.AddCommand({"cheatmenu", "cheatmenu", "Open the cheat menu (Shift+F12)", CMD_NONE,
                  [](const CommandArgs &, ConsoleCore &) {
                    Console_Close();
                    Cheats_OpenMenu();
                  }});
  con.AddCommand({"give", "give <all|energy|shields|ammo|keys|weapon name>", "Give items", CMD_CHEAT,
                  [](const CommandArgs &a, ConsoleCore &c) {
                    if (const char *why = RefusalReason()) {
                      c.Print(why);
                      return;
                    }
                    if (a.size() < 2) {
                      c.Print("usage: give <all|energy|shields|ammo|keys|weapon name>");
                      return;
                    }
                    std::string what;
                    for (size_t i = 1; i < a.size(); ++i)
                      what += (i > 1 ? " " : "") + a[i];
                    const std::string key = Squash(what);
                    player &p = Players[Player_num];
                    MarkCheater();
                    if (key == "all") {
                      GiveAllWeapons();
                      GiveAllKeys();
                    } else if (key == "energy") {
                      p.energy = MAX_ENERGY;
                    } else if (key == "shields") {
                      Objects[p.objnum].shields = MAX_SHIELDS;
                    } else if (key == "ammo") {
                      FillAmmo(true, true);
                    } else if (key == "keys") {
                      GiveAllKeys();
                    } else {
                      int found = -1;
                      for (int i = 0; i < MAX_PLAYER_WEAPONS; i++) {
                        if (Squash(Static_weapon_names[i]) == key)
                          found = i;
                      }
                      if (found < 0) {
                        c.Print("give: unknown item '" + what + "'");
                        return;
                      }
                      p.weapon_flags |= HAS_FLAG(found);
                      const int max = Ships[p.ship_index].max_ammo[found];
                      if (max > 0)
                        p.weapon_ammo[found] = static_cast<uint16_t>(max);
                      what = Static_weapon_names[found];
                    }
                    Notify("Given: " + what);
                  }});
  con.AddCommand({"god", "god [0|1]", "God mode (the AMBROSIA cheat)", CMD_CHEAT,
                  [](const CommandArgs &a, ConsoleCore &c) {
                    const bool current = g_state.IsActive(CheatId::Ambrosia);
                    bool want = !current;
                    if (a.size() > 1 && !ParseOnOff(a[1], current, &want)) {
                      c.Print("usage: god [0|1]");
                      return;
                    }
                    if (want != current)
                      Use(CheatId::Ambrosia);
                    else
                      c.Print(std::string("God mode is already ") + OnOff(current));
                  }});
  con.AddCommand({"timescale", "timescale [f]", "Game speed, 0.05 to 4.0 (1 = normal)", CMD_CHEAT,
                  [](const CommandArgs &a, ConsoleCore &c) {
                    if (a.size() < 2) {
                      char buf[48];
                      std::snprintf(buf, sizeof(buf), "timescale = %.2f", g_state.TimeScale());
                      c.Print(buf);
                      return;
                    }
                    char *end = nullptr;
                    const float v = std::strtof(a[1].c_str(), &end);
                    if (end == a[1].c_str() || *end != '\0' || !std::isfinite(v)) {
                      c.Print("usage: timescale <0.05 to 4.0>");
                      return;
                    }
                    Use(CheatId::Torpor, std::clamp(v, kTimescaleMin, kTimescaleMax));
                  }});
  con.AddCommand({"map", "map", "Open the level-warp picker (the FERRYMAN cheat)", CMD_CHEAT,
                  [](const CommandArgs &, ConsoleCore &) {
                    Console_Close();
                    Use(CheatId::Ferryman);
                  }});
}

// ---- Menu drawing -------------------------------------------------------------

std::string RowState(const CheatInfo &row) {
  if (!g_state.IsUnlocked(row.id))
    return "locked";
  if (!row.available)
    return "not implemented yet";
  switch (row.kind) {
  case CheatKind::Toggle:
    return g_state.IsActive(row.id) ? "ON" : "off";
  case CheatKind::Slider: {
    char buf[32];
    std::snprintf(buf, sizeof(buf), "< %.1f >", g_state.TimeScale());
    return buf;
  }
  case CheatKind::Button:
    return "Enter";
  }
  return "";
}

void RenderMenu() {
  grtext_SetFont(BRIEFING_FONT);
  grtext_SetFlags(0);
  const int lh = grfont_GetHeight(BRIEFING_FONT) + 3;
  const int rows = static_cast<int>(kCheatCount);
  const int box_w = std::min(Max_window_w - 20, std::max(420, Max_window_w / 2));
  const int box_h = lh * (rows + 4) + 28;
  const int x0 = (Max_window_w - box_w) / 2;
  const int y0 = std::max(4, (Max_window_h - box_h) / 2);
  const int pad = 12;

  StartFrame(0, 0, Max_window_w, Max_window_h, false);
  rend_FillRect(GR_RGB(8, 10, 18), x0, y0, x0 + box_w, y0 + box_h);
  rend_FillRect(GR_RGB(190, 60, 50), x0, y0, x0 + box_w, y0 + 2);
  grtext_SetAlpha(255);

  int y = y0 + 8;
  grtext_SetColor(GR_RGB(255, 120, 100));
  grtext_Printf(x0 + pad, y, "%s", "CHEATS");
  grtext_SetColor(GR_RGB(120, 140, 170));
  const char *close_hint = "Shift+F12 or Escape to close";
  grtext_Printf(x0 + box_w - pad - grtext_GetTextLineWidth(close_hint), y, "%s", close_hint);
  y += lh + 4;

  grtext_SetColor(GR_RGB(255, 225, 140));
  const std::string pw = "Password: " + g_password;
  grtext_Printf(x0 + pad, y, "%s", pw.c_str());
  if (static_cast<int>(timer_GetTime() * 2.0f) % 2 == 0)
    grtext_Printf(x0 + pad + grtext_GetTextLineWidth(pw.c_str()), y, "_");
  y += lh;
  grtext_SetColor(GR_RGB(160, 200, 160));
  grtext_Printf(x0 + pad, y, "%s", g_status.c_str());
  y += lh + 4;

  const int state_x = x0 + box_w - pad - 150;
  for (int i = 0; i < rows; ++i, y += lh) {
    const CheatInfo &row = CheatTable()[static_cast<size_t>(i)];
    const bool usable = g_state.IsUnlocked(row.id) && row.available;
    if (i == g_selected)
      rend_FillRect(GR_RGB(30, 40, 70), x0 + 4, y - 1, x0 + box_w - 4, y + lh - 2);
    const ddgr_color colour =
        !usable ? GR_RGB(90, 95, 110)
                : (row.kind == CheatKind::Toggle && g_state.IsActive(row.id)) ? GR_RGB(140, 255, 140)
                                                                             : GR_RGB(200, 215, 235);
    grtext_SetColor(colour);
    grtext_Printf(x0 + pad, y, "%s%s", i == g_selected ? "> " : "  ", row.label);
    grtext_Printf(state_x, y, "%s", RowState(row).c_str());
  }

  y += 4;
  grtext_SetColor(GR_RGB(120, 140, 170));
  grtext_Printf(x0 + pad, y, "%s", "Type a password and press Enter. Up/Down select, Enter use, Left/Right adjust.");
  grtext_Flush();
  EndFrame();
}

void RenderCheaterTag() {
  grtext_SetFont(BRIEFING_FONT);
  grtext_SetFlags(0);
  grtext_SetAlpha(128); // 50 % (SPEC.md §4)
  grtext_SetColor(GR_RGB(255, 90, 80));
  const char *tag = "CHEATS";
  StartFrame(0, 0, Max_window_w, Max_window_h, false);
  // Top-right, below the score readout.
  grtext_Printf(Max_window_w - grtext_GetTextLineWidth(tag) - 8, Max_window_h * 6 / 100, "%s", tag);
  grtext_Flush();
  EndFrame();
  grtext_SetAlpha(255);
}

} // namespace

CheatState &Cheats() { return g_state; }

void Cheats_Init() {
#if defined(_DEBUG)
  g_state.SetAllUnlocked(true); // SPEC.md §4: Debug builds treat every row as unlocked
#endif
  RegisterCommands(Console());
}

bool Cheats_AllowsCommand(const char *command) {
  // In Classic Mode and multiplayer let the command run: each one checks RefusalReason() first
  // and says why, which is clearer than "unlock it first".
  if (RefusalReason() != nullptr)
    return true;
  for (const CommandCheat &cc : kCommandCheats) {
    if (std::strcmp(cc.command, command) == 0)
      return g_state.IsUnlocked(cc.cheat);
  }
  return false;
}

bool Cheats_MenuIsOpen() { return g_menu_open; }

void Cheats_OpenMenu() {
  if (g_menu_open)
    return;
  if (const char *why = RefusalReason()) {
    Notify(why);
    return;
  }
  g_menu_open = true;
  g_password.clear();
  g_status.clear();
  SuspendControls();
}

void Cheats_CloseMenu() {
  if (!g_menu_open)
    return;
  g_menu_open = false;
  g_password.clear();
  if (!Console_IsOpen())
    ResumeControls(); // also flushes the key and mouse queues
}

bool Cheats_HandleKey(int key) {
  if (key == kMenuKey) {
    g_menu_open ? Cheats_CloseMenu() : Cheats_OpenMenu();
    return true;
  }
  if (!g_menu_open)
    return false;

  const int code = key & 0xFF;
  const bool mods = (key & (KEY_CTRLED | KEY_ALTED)) != 0;
  const int rows = static_cast<int>(kCheatCount);
  const CheatInfo &row = CheatTable()[static_cast<size_t>(g_selected)];

  switch (code) {
  case KEY_ESC:
    Cheats_CloseMenu();
    break;
  case KEY_UP:
    g_selected = (g_selected + rows - 1) % rows;
    break;
  case KEY_DOWN:
    g_selected = (g_selected + 1) % rows;
    break;
  case KEY_LEFT:
  case KEY_RIGHT:
    if (row.kind == CheatKind::Slider && g_state.IsUnlocked(row.id)) {
      const float step = code == KEY_LEFT ? -kTorporStep : kTorporStep;
      const float v = std::clamp(std::round((g_state.TimeScale() + step) * 10.0f) / 10.0f, kTorporMin, kTorporMax);
      Use(row.id, v);
    }
    break;
  case KEY_BACKSP:
    if (!g_password.empty())
      g_password.pop_back();
    break;
  case KEY_ENTER:
  case KEY_PADENTER:
    if (!g_password.empty()) {
      const std::string text = g_password;
      g_password.clear();
      TryPassword(text);
    } else {
      Use(row.id, row.kind == CheatKind::Slider ? 1.0f : -1.0f); // Enter on the slider resets it
    }
    break;
  default:
    if (!mods) {
      const int ascii = ddio_KeyToAscii(key);
      if (((ascii >= 'a' && ascii <= 'z') || (ascii >= 'A' && ascii <= 'Z')) && g_password.size() < kMaxPassword)
        g_password.push_back(static_cast<char>(std::toupper(ascii)));
    }
    break;
  }
  return true; // the open menu swallows every key
}

void Cheats_Frame() {
  if (Player_object == nullptr || Player_num < 0 || Players[Player_num].objnum < 0)
    return;

  // AV-004: nothing survives into multiplayer. Forget, don't touch: the
  // player object belongs to the netgame now.
  if (Game_mode & GM_MULTI) {
    if (g_state.AnyActive() || AnythingApplied() || g_menu_open) {
      g_state.DeactivateAll();
      Sync(g_state.Holds(), false);
      Cheats_CloseMenu();
    }
    return;
  }

  // AV-001: Classic Mode early-out, after undoing anything still applied.
  if (IsClassic()) {
    if (g_state.AnyActive() || AnythingApplied() || g_menu_open) {
      g_state.DeactivateAll();
      Sync(g_state.Holds());
      Cheats_CloseMenu();
    }
    return;
  }

  if (g_state.SetOwner(Players[Player_num].callsign))
    Sync(g_state.Holds()); // a different pilot: everything relocked

  // Upstream clears IsCheater when a new game starts (menu.cpp); toggles from
  // the previous game end there too (SPEC.md §4). Unlocks stay.
  if (!IsCheater && g_state.AnyActive())
    g_state.DeactivateAll();

  if (!g_state.AnyActive() && !AnythingApplied())
    return;
  Sync(g_state.Holds());
}

float Cheats_TimeScale() {
  if (IsClassic() || (Game_mode & GM_MULTI))
    return 1.0f;
  return g_state.TimeScale();
}

void Cheats_Render() {
  if (g_menu_open) {
    // Some game events (level start, dialogs) resume controls behind our back.
    if (Control_poll_flag)
      SuspendControls();
    RenderMenu();
  }
  if (IsCheater && !IsClassic())
    RenderCheaterTag();
}

} // namespace tartarus
