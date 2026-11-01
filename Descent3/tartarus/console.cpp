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

// Drop-down developer console (F-010). See console.h.

#include "console.h"

#include <algorithm>
#include <fstream>
#include <string>

#include <plog/Appenders/IAppender.h>

#include "cfile.h"
#include "cheats.h"
#include "classic.h"
#include "controls.h"
#include "ddio.h"
#include "game.h"
#include "gamefont.h"
#include "gamesequence.h"
#include "grdefs.h"
#include "grtext.h"
#include "log.h"
#include "renderer.h"
#include "version.h"

namespace fs = std::filesystem;

namespace tartarus {

namespace {

ConsoleCore *g_console = nullptr;
bool g_open = false;
float g_frac = 0.0f;        // 0 = hidden, 1 = fully down
float g_last_time = -1.0f;  // for the slide animation

Cvar *con_height = nullptr;
Cvar *con_anim_time = nullptr;
Cvar *con_lines = nullptr;

fs::path HistoryPath() { return cf_GetWritableBaseDirectory() / "console_history.txt"; }

void LoadHistory() {
  std::ifstream in(HistoryPath());
  std::vector<std::string> history;
  std::string line;
  while (std::getline(in, line)) {
    if (!line.empty())
      history.push_back(line);
  }
  Console().SetHistory(std::move(history));
}

void SaveHistory() {
  const fs::path path = HistoryPath();
  fs::path tmp = path;
  tmp += ".tmp";
  {
    std::ofstream out(tmp, std::ios::trunc);
    for (const auto &line : Console().History())
      out << line << '\n';
    if (!out)
      return;
  }
  std::error_code ec;
  fs::rename(tmp, path, ec);
}

// Mirrors engine logging into the scrollback (F-010). Severity tags only on
// warnings and worse, to keep normal output readable.
class ConsoleLogAppender : public plog::IAppender {
public:
  void write(const plog::Record &record) override {
    std::string msg = record.getMessage();
    switch (record.getSeverity()) {
    case plog::fatal:
    case plog::error:
      msg = "ERROR: " + msg;
      break;
    case plog::warning:
      msg = "warning: " + msg;
      break;
    default:
      break;
    }
    Console().Print(msg);
  }
};
ConsoleLogAppender g_appender;

void RegisterEngineCommands(ConsoleCore &con) {
  con.AddCommand({"quit", "quit", "Leave the game (asks to confirm, as Escape does)", CMD_NONE,
                  [](const CommandArgs &, ConsoleCore &) {
                    Console_Close();
                    Game_interface_mode = GAME_EXIT_CONFIRM;
                  }});
  con.AddCommand({"classic", "classic [0|1]", "Show or switch Classic Mode (1 = stock Descent 3)", CMD_NONE,
                  [](const CommandArgs &a, ConsoleCore &c) {
                    if (a.size() > 1) {
                      const CvarSetResult r = Cvars().Set("classic", a[1], CvarSource::User);
                      if (!CvarSetAccepted(r)) {
                        c.Print(std::string("classic: ") + CvarSetResultText(r));
                        return;
                      }
                    }
                    c.Print(IsClassic() ? "Classic Mode: stock Descent 3 behaviour ('classic 0' enables Tartarus features)"
                                        : "Tartarus mode: Tartarus features enabled ('classic 1' for stock behaviour)");
                  }});
  con.AddCommand({"screenshot", "screenshot", "Save a screenshot (as the Print Screen key does)", CMD_NONE,
                  [](const CommandArgs &, ConsoleCore &c) {
                    Console_Close(); // keep the console out of the picture
                    DoScreenshot();
                    c.Print("screenshot saved");
                  }});
  con.AddCommand({"version", "version", "Show the build and upstream revision", CMD_NONE,
                  [](const CommandArgs &, ConsoleCore &c) {
                    c.Print(std::string(BuildString()) + ", " + UpstreamString());
                  }});
}

} // namespace

ConsoleCore &Console() {
  if (g_console == nullptr)
    g_console = new ConsoleCore(Cvars()); // lives for the whole run
  return *g_console;
}

void Console_Init() {
  ConsoleCore &con = Console();
  con.cheat_allowed = [](const ConsoleCommand &cmd) { return Cheats_AllowsCommand(cmd.name.c_str()); };
  con.SetExecDirectory(cf_GetWritableBaseDirectory());

  con_height = Cvars().RegisterFloat("con_height", 0.45f, 0.1f, 1.0f, "Console height as a fraction of the screen",
                                     CVAR_ARCHIVE);
  con_anim_time =
      Cvars().RegisterFloat("con_anim_time", 0.15f, 0.0f, 1.0f, "Seconds the console takes to slide", CVAR_ARCHIVE);
  con_lines = Cvars().RegisterInt("con_lines", 2000, 100, 20000, "Scrollback lines kept", CVAR_ARCHIVE);
  con.SetMaxLines(static_cast<size_t>(con_lines->GetInt()));
  con_lines->AddChangeCallback([](const Cvar &cv) { Console().SetMaxLines(static_cast<size_t>(cv.GetInt())); });

  RegisterEngineCommands(con);
  LoadHistory();

  if (plog::get() != nullptr) {
    plog::get()->addAppender(&g_appender);
  }
  con.Print(std::string(BuildString()) + ". Type 'help' for commands, ` or Escape to close.");
}

bool Console_IsOpen() { return g_open; }

void Console_Open() {
  if (g_open)
    return;
  g_open = true;
  SuspendControls(); // nothing reaches the ship while typing (AV-008)
}

void Console_Close() {
  if (!g_open)
    return;
  g_open = false;
  ResumeControls(); // also flushes the key and mouse queues: no stuck keys, no backtick leak
}

bool Console_HandleKey(int key) {
  const int code = key & 0xFF;
  const bool ctrl = (key & KEY_CTRLED) != 0;
  const bool alt = (key & KEY_ALTED) != 0;

  if (code == KEY_LAPOSTRO && !ctrl && !alt) {
    g_open ? Console_Close() : Console_Open();
    return true;
  }
  if (!g_open)
    return false;

  ConsoleCore &con = Console();
  const int page = 10;
  switch (code) {
  case KEY_ESC:
    Console_Close();
    break;
  case KEY_ENTER:
  case KEY_PADENTER:
    con.Submit();
    SaveHistory();
    break;
  case KEY_BACKSP:
    con.Backspace();
    break;
  case KEY_DELETE:
    con.DeleteForward();
    break;
  case KEY_LEFT:
    con.MoveCursor(-1);
    break;
  case KEY_RIGHT:
    con.MoveCursor(1);
    break;
  case KEY_HOME:
    con.CursorHome();
    break;
  case KEY_END:
    con.CursorEnd();
    break;
  case KEY_UP:
    con.HistoryPrev();
    break;
  case KEY_DOWN:
    con.HistoryNext();
    break;
  case KEY_PAGEUP:
    con.Scroll(page);
    break;
  case KEY_PAGEDOWN:
    con.Scroll(-page);
    break;
  case KEY_TAB:
    con.Complete();
    break;
  default:
    if (ctrl && code == KEY_U) {
      con.ClearInput();
    } else if (ctrl && code == KEY_L) {
      con.Clear();
    } else if (!ctrl && !alt) {
      const int ascii = ddio_KeyToAscii(key);
      if (ascii >= 32 && ascii < 127 && ascii != '`') {
        const char c = static_cast<char>(ascii);
        con.InsertText(std::string_view(&c, 1));
      }
    }
    break;
  }
  return true; // the open console swallows every key
}

void Console_Render() {
  ConsoleCore &con = Console();
  con.Pump();

  // Slide animation on real time, so it works while the game is paused.
  const float now = timer_GetTime();
  const float dt = (g_last_time < 0.0f) ? 0.0f : std::clamp(now - g_last_time, 0.0f, 0.1f);
  g_last_time = now;
  const float anim = con_anim_time != nullptr ? con_anim_time->GetFloat() : 0.0f;
  const float target = g_open ? 1.0f : 0.0f;
  if (anim <= 0.0f) {
    g_frac = target;
  } else {
    const float step = dt / anim;
    g_frac = (g_frac < target) ? std::min(target, g_frac + step) : std::max(target, g_frac - step);
  }
  if (g_frac <= 0.0f)
    return;

  // Some game events (level start, dialogs) resume controls behind our back.
  if (g_open && Control_poll_flag)
    SuspendControls();

  const float height_frac = con_height != nullptr ? con_height->GetFloat() : 0.45f;
  const int w = Max_window_w;
  const int h = std::max(1, static_cast<int>(Max_window_h * height_frac * g_frac));

  StartFrame(0, 0, Max_window_w, Max_window_h, false);
  rend_FillRect(GR_RGB(8, 10, 18), 0, 0, w, h);
  rend_FillRect(GR_RGB(80, 130, 190), 0, std::max(0, h - 2), w, h);

  grtext_SetFont(BRIEFING_FONT);
  grtext_SetAlpha(255);
  grtext_SetFlags(0);
  const int lh = grfont_GetHeight(BRIEFING_FONT) + 2;
  const int x = 8;
  int y = h - 6 - lh;

  // Input line with a blinking cursor.
  const std::string prompt = "] " + con.Input();
  grtext_SetColor(GR_RGB(255, 225, 140));
  grtext_Printf(x, y, "%s", prompt.c_str());
  if (static_cast<int>(now * 2.0f) % 2 == 0) {
    const std::string upto = "] " + con.Input().substr(0, con.Cursor());
    grtext_Printf(x + grtext_GetTextLineWidth(upto.c_str()), y, "_");
  }
  y -= lh;

  // Scrollback, newest at the bottom.
  const auto &lines = con.Lines();
  int idx = static_cast<int>(lines.size()) - 1 - con.ScrollOffset();
  if (con.ScrollOffset() > 0) {
    grtext_SetColor(GR_RGB(120, 160, 210));
    grtext_Printf(x, y, "%s", "^  ^  ^  (Page Down to return)");
    y -= lh;
  }
  grtext_SetColor(GR_RGB(190, 210, 230));
  for (; idx >= 0 && y > -lh; --idx, y -= lh) {
    grtext_Printf(x, y, "%s", lines[static_cast<size_t>(idx)].c_str());
  }

  grtext_Flush();
  EndFrame();
}

} // namespace tartarus
