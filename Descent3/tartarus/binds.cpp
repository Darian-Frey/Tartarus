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

// Console key bindings. See binds.h.

#include "binds.h"

#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "binds_core.h"
#include "cfile.h"
#include "cheats.h"
#include "console.h"
#include "ddio.h"
#include "descent.h"
#include "game.h"
#include "gamesequence.h"
#include "log.h"

static_assert(tartarus::kBindShift == KEY_SHIFTED && tartarus::kBindAlt == KEY_ALTED &&
                  tartarus::kBindCtrl == KEY_CTRLED,
              "binds_core modifier bits must match ddio");

namespace fs = std::filesystem;

namespace tartarus {

namespace {

std::vector<KeyName> EngineKeyNames() {
  std::vector<KeyName> k = {
      {"a", KEY_A}, {"b", KEY_B}, {"c", KEY_C}, {"d", KEY_D}, {"e", KEY_E}, {"f", KEY_F}, {"g", KEY_G},
      {"h", KEY_H}, {"i", KEY_I}, {"j", KEY_J}, {"k", KEY_K}, {"l", KEY_L}, {"m", KEY_M}, {"n", KEY_N},
      {"o", KEY_O}, {"p", KEY_P}, {"q", KEY_Q}, {"r", KEY_R}, {"s", KEY_S}, {"t", KEY_T}, {"u", KEY_U},
      {"v", KEY_V}, {"w", KEY_W}, {"x", KEY_X}, {"y", KEY_Y}, {"z", KEY_Z},
      {"0", KEY_0}, {"1", KEY_1}, {"2", KEY_2}, {"3", KEY_3}, {"4", KEY_4}, {"5", KEY_5}, {"6", KEY_6},
      {"7", KEY_7}, {"8", KEY_8}, {"9", KEY_9},
      {"f1", KEY_F1}, {"f2", KEY_F2}, {"f3", KEY_F3}, {"f4", KEY_F4}, {"f5", KEY_F5}, {"f6", KEY_F6},
      {"f7", KEY_F7}, {"f8", KEY_F8}, {"f9", KEY_F9}, {"f10", KEY_F10}, {"f11", KEY_F11}, {"f12", KEY_F12},
      {"kp0", KEY_PAD0}, {"kp1", KEY_PAD1}, {"kp2", KEY_PAD2}, {"kp3", KEY_PAD3}, {"kp4", KEY_PAD4},
      {"kp5", KEY_PAD5}, {"kp6", KEY_PAD6}, {"kp7", KEY_PAD7}, {"kp8", KEY_PAD8}, {"kp9", KEY_PAD9},
      {"kpminus", KEY_PADMINUS}, {"kpplus", KEY_PADPLUS}, {"kpperiod", KEY_PADPERIOD},
      {"kpdivide", KEY_PADDIVIDE}, {"kpmultiply", KEY_PADMULTIPLY}, {"kpenter", KEY_PADENTER},
      {"minus", KEY_MINUS}, {"equal", KEY_EQUAL}, {"slash", KEY_SLASH}, {"backslash", KEY_BACKSLASH},
      {"comma", KEY_COMMA}, {"period", KEY_PERIOD}, {"semicolon", KEY_SEMICOL}, {"lbracket", KEY_LBRACKET},
      {"rbracket", KEY_RBRACKET}, {"apostrophe", KEY_RAPOSTRO},
      {"enter", KEY_ENTER}, {"backspace", KEY_BACKSP}, {"tab", KEY_TAB}, {"space", KEY_SPACEBAR},
      {"insert", KEY_INSERT}, {"home", KEY_HOME}, {"pageup", KEY_PAGEUP}, {"delete", KEY_DELETE},
      {"end", KEY_END}, {"pagedown", KEY_PAGEDOWN}, {"up", KEY_UP}, {"down", KEY_DOWN}, {"left", KEY_LEFT},
      {"right", KEY_RIGHT}, {"pause", KEY_PAUSE},
  };
  return k;
}

BindTable &Table() {
  static BindTable table(EngineKeyNames());
  return table;
}

bool g_loading = false; // while running binds.cfg: don't rewrite it

fs::path BindsPath() { return cf_GetWritableBaseDirectory() / "binds.cfg"; }

void Save() {
  if (g_loading)
    return;
  const fs::path path = BindsPath();
  fs::path tmp = path;
  tmp += ".tmp";
  {
    std::ofstream out(tmp, std::ios::trunc);
    out << Table().Serialize();
    if (!out)
      return;
  }
  std::error_code ec;
  fs::rename(tmp, path, ec);
}

// Keys that must keep their meaning: the console toggle and Escape.
bool Reserved(int key) {
  const int code = key & kBindCodeMask;
  return code == KEY_LAPOSTRO || code == KEY_ESC;
}

} // namespace

void Binds_Init() {
  ConsoleCore &con = Console();
  con.AddCommand({"bind", "bind [key [\"command\"]]",
                  "Bind a key to a console command (bind shift+f5 \"where\"); no argument lists bindings", CMD_NONE,
                  [](const CommandArgs &a, ConsoleCore &c) {
                    BindTable &t = Table();
                    if (a.size() < 2) {
                      if (t.All().empty())
                        c.Print("no key bindings");
                      for (const auto &[key, line] : t.All())
                        c.Print("  " + t.KeyToString(key) + " = " + line);
                      return;
                    }
                    const auto key = t.ParseKey(a[1]);
                    if (!key) {
                      c.Print("bind: unknown key '" + a[1] + "' (e.g. f5, kp7, shift+f5, ctrl+g)");
                      return;
                    }
                    if (Reserved(*key)) {
                      c.Print("bind: " + t.KeyToString(*key) + " is reserved");
                      return;
                    }
                    if (a.size() < 3) {
                      const std::string *line = t.Find(*key);
                      c.Print(t.KeyToString(*key) + (line ? " = " + *line : " is not bound"));
                      return;
                    }
                    std::string line = a[2];
                    for (size_t i = 3; i < a.size(); ++i)
                      line += " " + a[i];
                    t.Bind(*key, line);
                    Save();
                    if (!g_loading)
                      c.Print(t.KeyToString(*key) + " = " + line);
                  }});
  con.AddCommand({"unbind", "unbind <key|all>", "Remove a key binding", CMD_NONE,
                  [](const CommandArgs &a, ConsoleCore &c) {
                    BindTable &t = Table();
                    if (a.size() < 2) {
                      c.Print("usage: unbind <key|all>");
                      return;
                    }
                    if (a[1] == "all") {
                      t.Clear();
                      Save();
                      c.Print("all key bindings removed");
                      return;
                    }
                    const auto key = t.ParseKey(a[1]);
                    if (!key || !t.Unbind(*key)) {
                      c.Print("unbind: " + a[1] + " is not bound");
                      return;
                    }
                    Save();
                    c.Print(t.KeyToString(*key) + " unbound");
                  }});

  if (fs::exists(BindsPath())) {
    g_loading = true;
    con.ExecFile(BindsPath());
    g_loading = false;
    LOG_INFO << "[tartarus] " << Table().All().size() << " key binding(s) from " << BindsPath();
  }
}

bool Binds_HandleKey(int key) {
  key &= (kBindCodeMask | kBindShift | kBindAlt | kBindCtrl);
  const std::string *line = Table().Find(key);
  if (line == nullptr || Console_IsOpen() || Cheats_MenuIsOpen())
    return false;
  if (GetFunctionMode() != GAME_MODE || Game_interface_mode != GAME_INTERFACE)
    return false;
  const std::string run = *line; // the line may rebind this key
  Console().Execute(run);
  return true;
}

} // namespace tartarus
