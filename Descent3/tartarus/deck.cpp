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

// Options deck. See deck.h.

#include "deck.h"

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

#include "classic.h"
#include "console.h"
#include "controls.h"
#include "ctlconfig.h"
#include "cvar.h"
#include "d3music.h"
#include "ddio.h"
#include "deck_core.h"
#include "descent.h"
#include "display.h"
#include "game.h"
#include "gamefont.h"
#include "gamesequence.h"
#include "graphics.h"
#include "pilot.h"
#include "version.h"
#include "grdefs.h"
#include "grtext.h"
#include "hlsoundlib.h"
#include "init.h"
#include "mouse_compat.h"
#include "newui.h"
#include "mmItem.h"
#include "multi.h"
#include "renderer.h"
#include "small_views.h"
#include "uisys.h"

namespace tartarus {

namespace {

struct Theme {
  ddgr_color crt, ph, dim, faint;
};
struct Casing {
  ddgr_color base, hi, lo, ink, key, key_lo;
};
const Theme kThemes[] = {
    {GR_RGB(12, 10, 6), GR_RGB(255, 176, 0), GR_RGB(122, 84, 0), GR_RGB(46, 32, 3)},    // amber
    {GR_RGB(4, 11, 6), GR_RGB(70, 255, 122), GR_RGB(31, 122, 59), GR_RGB(11, 42, 21)},  // green
    {GR_RGB(6, 10, 16), GR_RGB(169, 228, 255), GR_RGB(74, 125, 150), GR_RGB(20, 40, 58)}, // ice
    {GR_RGB(16, 17, 20), GR_RGB(242, 242, 238), GR_RGB(154, 154, 148), GR_RGB(42, 43, 46)}, // plain
};
const Casing kCasings[] = {
    {GR_RGB(59, 62, 67), GR_RGB(85, 89, 95), GR_RGB(37, 39, 42), GR_RGB(217, 214, 204), GR_RGB(74, 78, 84),
     GR_RGB(44, 46, 50)}, // gunmetal
    {GR_RGB(201, 193, 173), GR_RGB(226, 219, 201), GR_RGB(143, 136, 117), GR_RGB(43, 42, 38), GR_RGB(216, 209, 191),
     GR_RGB(156, 149, 131)}, // putty
};
const ddgr_color kLed = GR_RGB(255, 120, 30);
const ddgr_color kStripe = GR_RGB(200, 84, 29);

struct Rect {
  int x1 = 0, y1 = 0, x2 = 0, y2 = 0;
  bool Has(int x, int y) const { return x >= x1 && x < x2 && y >= y1 && y < y2; }
};

bool g_open = false;
bool g_want_controls = false; // key bindings asked for in a level
bool g_modal = false;      // running from the main menu, in Deck_RunFromMenu()
bool g_want_stock = false; // the Stock options entry was chosen there
std::vector<DeckPage> g_pages;
int g_page = 0;
std::vector<int> g_sel, g_scroll;
bool g_restart_needed = false;
int g_mouse_x = 0, g_mouse_y = 0;
std::vector<Rect> g_key_rects;
std::vector<std::pair<Rect, int>> g_row_rects;

Cvar *ui_scanlines = nullptr;
Cvar *ui_phosphor = nullptr;
Cvar *ui_casing = nullptr;
Cvar *s_sfx_volume = nullptr;
Cvar *s_music_volume = nullptr;
bool g_syncing = false;

float g_s = 1.0f; // layout scale
int g_ox = 0, g_oy = 0;
int X(int v) { return g_ox + static_cast<int>(v * g_s); }
int Y(int v) { return g_oy + static_cast<int>(v * g_s); }

void Fill(ddgr_color c, int x1, int y1, int x2, int y2) { rend_FillRect(c, X(x1), Y(y1), X(x2), Y(y2)); }

ddgr_color Mix(ddgr_color a, ddgr_color b, float t) {
  auto m = [t](int x, int y) { return static_cast<int>(x + (y - x) * t); };
  return GR_RGB(m(GR_COLOR_RED(a), GR_COLOR_RED(b)), m(GR_COLOR_GREEN(a), GR_COLOR_GREEN(b)),
                m(GR_COLOR_BLUE(a), GR_COLOR_BLUE(b)));
}

// Scanlines are drawn under the text, so it stays crisp (rend_DrawLine has no alpha): every third
// row of an area becomes `line`.
void Scanlines(ddgr_color line, int x1, int y1, int x2, int y2) {
  rend_SetFlatColor(line);
  for (int y = Y(y1); y < Y(y2); y += 3)
    rend_DrawLine(X(x1), y, X(x2), y);
}

float ScanStrength() { return ui_phosphor->GetInt() == 3 ? 0.0f : ui_scanlines->GetInt() / 60.0f; }

// Text is buffered by grtext, so each call flushes: rectangles drawn later stay behind nothing.
// `size` is the text height in layout units; the large font scaled down stays sharp.
float FontScale(float size) { return size * g_s / static_cast<float>(grfont_GetHeight(BIG_BRIEFING_FONT)); }

void Text(int x, int y, ddgr_color c, float size, const std::string &s) {
  grtext_SetFontScale(FontScale(size));
  grtext_SetColor(c);
  grtext_Puts(X(x), Y(y), s.c_str());
  grtext_Flush();
}

int TextWidth(float size, const std::string &s) {
  grtext_SetFontScale(FontScale(size));
  return static_cast<int>(grtext_GetTextLineWidth(s.c_str()) / g_s);
}

void SyncMirrors() {
  g_syncing = true;
  Cvars().Set("s_sfx_volume", std::to_string(Sound_system.GetMasterVolume()), CvarSource::Code);
  Cvars().Set("s_music_volume", std::to_string(D3MusicGetVolume()), CvarSource::Code);
  g_syncing = false;
  SmallViews_SyncMirrorCvar();
}

const DeckRow *CurrentRow() {
  const DeckPage &p = g_pages[static_cast<size_t>(g_page)];
  if (p.rows.empty())
    return nullptr;
  return &p.rows[static_cast<size_t>(g_sel[static_cast<size_t>(g_page)])];
}

void Open() {
  g_pages = BuildDeckPages(Cvars(), DefaultDeckPages());
  Display_SyncModeCvars();
  DeckSetChoices(g_pages, "r_resolution", Display_ModeChoices());
  g_sel.assign(g_pages.size(), 0);
  g_scroll.assign(g_pages.size(), 0);
  g_page = 0;
  g_restart_needed = false;
  SyncMirrors();
  ddio_MouseBtnDownCount(0); // drop a click left over from flying
  g_open = true;
  if (!g_modal)
    SuspendControls();
}

void RunAction(DeckAction a) {
  if (a == DeckAction::Done) {
    Deck_Close();
  } else if (a == DeckAction::KeyBindings) {
    if (g_modal) {
      CtlConfig(CTLCONFIG_KEYBOARD);
    } else {
      // A whole screen cannot run inside the key handler: go through the
      // game's menu loop, as Stock options does.
      g_want_controls = true;
      Deck_Close();
      Game_interface_mode = GAME_OPTIONS_INTERFACE;
    }
  } else if (a == DeckAction::StockOptions) {
    g_want_stock = g_modal;
    Deck_Close();
    if (!g_modal)
      Game_interface_mode = GAME_OPTIONS_INTERFACE;
  }
}

void Change(const DeckRow &row, int dir) {
  if (row.kind == DeckKind::Action) {
    RunAction(row.action);
    return;
  }
  const Cvar *cv = Cvars().Find(row.cvar);
  if (cv == nullptr)
    return;
  Cvars().Set(row.cvar, DeckNextValue(row, *cv, dir), CvarSource::User);
  if (row.needs_restart)
    g_restart_needed = true;
  if (IsClassic())
    Deck_Close(); // Classic Mode was switched on: the stock menu takes over
}

void MoveSel(int d) {
  const auto &rows = g_pages[static_cast<size_t>(g_page)].rows;
  if (rows.empty())
    return;
  int &s = g_sel[static_cast<size_t>(g_page)];
  const int n = static_cast<int>(rows.size());
  s = (s + d + n) % n;
}

// ---- Drawing ----------------------------------------------------------------

constexpr int kW = 1120, kH = 640;       // casing
constexpr int kCx = 24, kCy = 60;        // CRT top-left inside the casing
constexpr int kCw = kW - 48, kCh = 480;  // CRT size
constexpr int kSplit = 680;              // left pane width
constexpr int kRowH = 34;

void DrawCasing(const Casing &c, const Theme &t) {
  Fill(c.lo, 0, 0, kW, kH);
  Fill(c.hi, 0, 0, kW, 3);
  Fill(c.base, 3, 3, kW - 3, kH - 4);
  Text(24, 14, c.ink, 32.0f, "TARTARUS");
  const int after = 24 + TextWidth(32.0f, "TARTARUS") + 18;
  const std::string model = "PTMC SYSTEM DECK / MODEL T-3";
  const int mw = TextWidth(17.0f, model);
  for (int x = after; x + 10 < kW - mw - 40; x += 14)
    Fill(kStripe, x, 22, x + 10, 34);
  Text(kW - mw - 24, 20, c.ink, 17.0f, model);
  Fill(GR_RGB(0, 0, 0), kCx - 8, kCy - 8, kCx + kCw + 8, kCy + kCh + 8);
  Fill(t.crt, kCx, kCy, kCx + kCw, kCy + kCh);
  if (ScanStrength() > 0.0f)
    Scanlines(Mix(t.crt, t.dim, 0.35f * ScanStrength()), kCx, kCy, kCx + kCw, kCy + kCh);
}

void DrawKeys(const Casing &c) {
  g_key_rects.clear();
  const int n = static_cast<int>(g_pages.size());
  const int gap = 8, w = (kW - 48 - gap * (n - 1)) / n, y = kCy + kCh + 18, h = 56;
  for (int i = 0; i < n; ++i) {
    const int x = 24 + i * (w + gap);
    const bool on = i == g_page;
    const int dy = on ? 4 : 0;
    Fill(c.lo, x, y + 4, x + w, y + h + 4);
    Fill(c.key_lo, x, y + dy, x + w, y + h - 4 + dy);
    Fill(c.key, x + 2, y + 2 + dy, x + w - 2, y + h - 10 + dy);
    if (on)
      Fill(kLed, x + w / 2 - 16, y + 7 + dy, x + w / 2 + 16, y + 12 + dy); // the page LED
    const std::string &name = g_pages[static_cast<size_t>(i)].name;
    Text(x + (w - TextWidth(17.0f, name)) / 2, y + 20 + dy, c.ink, 17.0f, name);
    g_key_rects.push_back({X(x), Y(y), X(x + w), Y(y + h)});
  }
}

void DrawBar(int x, int y, double frac, ddgr_color on, ddgr_color off) {
  const int n = 12, filled = static_cast<int>(std::lround(frac * n));
  for (int i = 0; i < n; ++i)
    Fill(i < filled ? on : off, x + i * 10, y, x + i * 10 + 7, y + 16);
}

void DrawRows(const Theme &t) {
  g_row_rects.clear();
  const DeckPage &p = g_pages[static_cast<size_t>(g_page)];
  const int px = kCx + 18, py = kCy + 14;
  Text(px, py, t.ph, 24.0f, p.name);
  const std::string count = "PAGE " + std::to_string(g_page + 1) + "/" + std::to_string(g_pages.size());
  Text(kCx + kSplit - 18 - TextWidth(24.0f, count), py, t.ph, 24.0f, count);
  Fill(t.dim, px, py + 30, kCx + kSplit - 18, py + 32);

  const int top = py + 42, visible = (kCh - 42 - 14 - 44) / kRowH;
  int &sel = g_sel[static_cast<size_t>(g_page)], &scroll = g_scroll[static_cast<size_t>(g_page)];
  if (sel < scroll)
    scroll = sel;
  if (sel >= scroll + visible)
    scroll = sel - visible + 1;

  for (int i = scroll; i < static_cast<int>(p.rows.size()) && i < scroll + visible; ++i) {
    const DeckRow &row = p.rows[static_cast<size_t>(i)];
    const int y = top + (i - scroll) * kRowH;
    const bool on = i == sel;
    const ddgr_color fg = on ? t.crt : t.ph;
    if (on) {
      Fill(t.ph, px - 6, y - 3, kCx + kSplit - 12, y + kRowH - 5);
      if (ScanStrength() > 0.0f)
        Scanlines(Mix(t.ph, t.crt, 0.3f * ScanStrength()), px - 6, y - 3, kCx + kSplit - 12, y + kRowH - 5);
    }
    Text(px, y, fg, 23.0f, row.label);
    g_row_rects.push_back({{X(px - 6), Y(y - 3), X(kCx + kSplit - 12), Y(y + kRowH - 5)}, i});
    const Cvar *cv = row.cvar.empty() ? nullptr : Cvars().Find(row.cvar);
    if (cv == nullptr)
      continue;
    const std::string v = DeckValueText(row, *cv);
    const int right = kCx + kSplit - 24;
    if (row.kind == DeckKind::Slider) {
      const int vw = TextWidth(23.0f, v);
      DrawBar(right - vw - 134, y + 3, DeckFraction(row, *cv), fg, on ? t.dim : t.faint);
      Text(right - vw, y, fg, 23.0f, v);
    } else {
      const std::string shown = row.kind == DeckKind::Toggle ? "[" + v + "]" : "< " + v + " >";
      Text(right - TextWidth(23.0f, shown), y, fg, 23.0f, shown);
    }
  }
  Text(px, kCy + kCh - 40, t.dim, 15.0f, "UP/DOWN SELECT  LEFT/RIGHT CHANGE  TAB PAGE  ESC DONE");
}

void DrawReadout(const Theme &t) {
  const int x = kCx + kSplit + 22, w = kCw - kSplit - 44;
  Fill(t.dim, kCx + kSplit, kCy + 14, kCx + kSplit + 2, kCy + kCh - 14);
  Text(x, kCy + 16, t.dim, 19.0f, "FRAME LIMIT");
  const Cvar *fps = Cvars().Find("r_maxfps");
  const int cap = fps != nullptr ? fps->GetInt() : 60;
  char buf[8];
  std::snprintf(buf, sizeof(buf), "%03d", cap > 999 ? 999 : cap);
  Text(x, kCy + 40, t.ph, 66.0f, cap == 0 ? "MAX" : buf);
  const int bars = std::min(16, cap == 0 ? 16 : cap * 16 / 240);
  for (int i = 0; i < 16; ++i)
    Fill(i < bars ? t.ph : t.faint, x + i * 16, kCy + 118, x + i * 16 + 11, kCy + 134);
  const DeckRow *row = CurrentRow();
  if (row == nullptr)
    return;
  int y = kCy + 156;
  if (!row->cvar.empty()) {
    Text(x, y, t.dim, 17.0f, "CONSOLE: " + row->cvar);
    y += 28;
  }
  // Word-wrap the setting's own description.
  std::string line, word;
  const std::string text = row->kind == DeckKind::Action
                               ? (row->action == DeckAction::StockOptions  ? "Opens the stock Options menu."
                                  : row->action == DeckAction::KeyBindings ? "Keyboard, joystick and mouse bindings, and weapon order."
                                                                           : "Close this screen.")
                               : row->help + (row->needs_restart ? " Applies when this screen closes." : "");
  for (size_t i = 0; i <= text.size(); ++i) {
    if (i == text.size() || text[i] == ' ') {
      const std::string tryline = line.empty() ? word : line + " " + word;
      if (TextWidth(19.0f, tryline) > w) {
        Text(x, y, t.ph, 19.0f, line);
        y += 25;
        line = word;
      } else {
        line = tryline;
      }
      word.clear();
    } else {
      word.push_back(text[i]);
    }
  }
  if (!line.empty())
    Text(x, y, t.ph, 19.0f, line);
}

void DrawPointer(const Theme &t) {
  rend_SetFlatColor(t.ph);
  const int x = g_mouse_x, y = g_mouse_y, s = std::max(8, static_cast<int>(14 * g_s));
  rend_DrawLine(x, y, x, y + s);
  rend_DrawLine(x, y, x + s * 3 / 4, y + s * 3 / 4);
  rend_DrawLine(x, y + s, x + s * 3 / 4, y + s * 3 / 4);
}

// Native-resolution menu frame for as long as a deck screen runs (D-016).
struct NativeFrame {
  bool was = Display_NativeMenuActive();
  NativeFrame() { Display_NativeMenu(true); }
  ~NativeFrame() { Display_NativeMenu(was); }
};

void MainBackground() {
  MenuScene();
  tMusicSeqInfo music_info{};
  Sound_system.BeginSoundFrame(false);
  music_info.frametime = UIFrameTime;
  D3MusicDoFrame(&music_info);
  Sound_system.EndSoundFrame();
}

// Pointer position in frame pixels, whatever the shape of the mouse space.
void ReadPointer() {
  int mx = 0, my = 0, dx = 0, dy = 0, left = 0, top = 0, right = 1, bottom = 1;
  ddio_MouseGetState(&mx, &my, &dx, &dy);
  ddio_MouseGetLimits(&left, &top, &right, &bottom);
  g_mouse_x = static_cast<int>(static_cast<int64_t>(mx) * Max_window_w / std::max(right, 1));
  g_mouse_y = static_cast<int>(static_cast<int64_t>(my) * Max_window_h / std::max(bottom, 1));
}

void HandleMouse() {
  ReadPointer();
  if (ddio_MouseBtnDownCount(0) <= 0)
    return;
  for (size_t i = 0; i < g_key_rects.size(); ++i) {
    if (g_key_rects[i].Has(g_mouse_x, g_mouse_y)) {
      g_page = static_cast<int>(i);
      return;
    }
  }
  for (const auto &[rect, index] : g_row_rects) {
    if (!rect.Has(g_mouse_x, g_mouse_y))
      continue;
    int &sel = g_sel[static_cast<size_t>(g_page)];
    if (sel != index) {
      sel = index;
      return;
    }
    // Second click on the selected row: left half steps down, right half steps up.
    Change(g_pages[static_cast<size_t>(g_page)].rows[static_cast<size_t>(index)],
           g_mouse_x < (rect.x1 + rect.x2) / 2 ? -1 : 1);
    return;
  }
}

} // namespace

void Deck_Init() {
  CvarRegistry &cv = Cvars();
  ui_scanlines = cv.RegisterInt("ui_scanlines", 28, 0, 60, "Scanline strength on the options screen", CVAR_ARCHIVE);
  ui_phosphor = cv.RegisterInt("ui_phosphor", 0, 0, 3, "Options screen colour (3 = plain, easiest to read)", CVAR_ARCHIVE);
  ui_casing = cv.RegisterInt("ui_casing", 0, 0, 1, "Options screen casing", CVAR_ARCHIVE);
  // Mirrors of the stock volumes, which the stock settings save.
  s_sfx_volume = cv.RegisterFloat("s_sfx_volume", 1.0f, 0.0f, 1.0f, "Sound effects volume");
  s_music_volume = cv.RegisterFloat("s_music_volume", 1.0f, 0.0f, 1.0f, "Music volume");
  s_sfx_volume->AddChangeCallback([](const Cvar &c) {
    if (!g_syncing)
      Sound_system.SetMasterVolume(c.GetFloat());
  });
  s_music_volume->AddChangeCallback([](const Cvar &c) {
    if (!g_syncing)
      D3MusicSetVolume(c.GetFloat());
  });
}

bool Deck_IsOpen() { return g_open; }

void Deck_Close() {
  if (!g_open)
    return;
  g_open = false;
  if (!g_modal && !Console_IsOpen())
    ResumeControls();
  if (g_restart_needed) {
    g_restart_needed = false;
    Graphics_RequestRestart();
  }
}

bool Deck_RunPendingControls() {
  if (!g_want_controls)
    return false;
  g_want_controls = false;
  CtlConfig(CTLCONFIG_KEYBOARD);
  return true;
}

bool Deck_RunFromMenu() {
  if (IsClassic())
    return false;
  NativeFrame frame;
  g_modal = true;
  g_want_stock = false;
  Open();
  ui_HideCursor();
  while (g_open) {
    Descent->defer();
    MainBackground();
    Deck_Render();
    rend_Flip();
    int key;
    while (g_open && (key = ddio_KeyInKey()) != 0)
      Deck_HandleKey(key);
  }
  ui_ShowCursor();
  ui_Flush();
  Display_ApplyPending(); // resolution or fullscreen chosen here
  g_modal = false;
  SaveGameSettings(); // stock-owned settings changed here (FOV, VSync, brightness, volumes)
  return !g_want_stock;
}

// ---- List screens -----------------------------------------------------------

namespace {

void BeginLayout() {
  g_s = std::min(Max_window_w / 1160.0f, Max_window_h / 680.0f);
  g_ox = (Max_window_w - static_cast<int>(kW * g_s)) / 2;
  g_oy = (Max_window_h - static_cast<int>(kH * g_s)) / 2;
  StartFrame(0, 0, Max_window_w, Max_window_h, false);
  grtext_SetFont(BIG_BRIEFING_FONT);
  grtext_SetFlags(0);
  grtext_SetAlpha(255);
}

void EndLayout() {
  grtext_SetFontScale(1.0f);
  EndFrame();
}

void WrapText(int x, int y, int w, float size, ddgr_color c, const std::string &text) {
  std::string line, word;
  for (size_t i = 0; i <= text.size(); ++i) {
    if (i == text.size() || text[i] == ' ') {
      const std::string tryline = line.empty() ? word : line + " " + word;
      if (TextWidth(size, tryline) > w) {
        Text(x, y, c, size, line);
        y += static_cast<int>(size * 1.3f);
        line = word;
      } else {
        line = tryline;
      }
      word.clear();
    } else {
      word.push_back(text[i]);
    }
  }
  if (!line.empty())
    Text(x, y, c, size, line);
}

void DrawList(const DeckListSpec &spec, int sel, int *scroll, std::vector<std::pair<Rect, int>> *rects) {
  const Theme &t = kThemes[std::clamp(ui_phosphor->GetInt(), 0, 3)];
  const Casing &c = kCasings[std::clamp(ui_casing->GetInt(), 0, 1)];
  BeginLayout();
  DrawCasing(c, t);
  const int px = kCx + 18, py = kCy + 14;
  Text(px, py, t.ph, 24.0f, spec.title);
  Fill(t.dim, px, py + 30, kCx + kSplit - 18, py + 32);

  const int row_h = spec.compact ? 22 : kRowH;
  const float size = spec.compact ? 17.0f : 23.0f;
  const int top = py + 42, visible = (kCh - 42 - 14 - 44) / row_h;
  const int n = static_cast<int>(spec.items.size());
  if (sel < *scroll)
    *scroll = sel;
  if (sel >= *scroll + visible)
    *scroll = sel - visible + 1;
  rects->clear();
  for (int i = *scroll; i < n && i < *scroll + visible; ++i) {
    const DeckListItem &it = spec.items[static_cast<size_t>(i)];
    const int y = top + (i - *scroll) * row_h;
    const bool on = i == sel;
    if (on) {
      Fill(t.ph, px - 6, y - 3, kCx + kSplit - 12, y + row_h - 5);
      if (ScanStrength() > 0.0f)
        Scanlines(Mix(t.ph, t.crt, 0.3f * ScanStrength()), px - 6, y - 3, kCx + kSplit - 12, y + row_h - 5);
    }
    const ddgr_color fg = on ? t.crt : (it.enabled ? t.ph : t.dim);
    Text(px, y, fg, size, it.label);
    if (!it.value.empty())
      Text(kCx + kSplit - 24 - TextWidth(size, it.value), y, fg, size, it.value);
    rects->push_back({{X(px - 6), Y(y - 3), X(kCx + kSplit - 12), Y(y + row_h - 5)}, i});
  }
  if (n > visible)
    Text(kCx + kSplit - 24 - TextWidth(15.0f, "MORE"), kCy + kCh - 40, t.dim, 15.0f,
         *scroll + visible < n ? "MORE" : "");
  Text(px, kCy + kCh - 40, t.dim, 15.0f,
       !spec.footer.empty() ? spec.footer.c_str()
       : spec.read_only     ? "UP/DOWN SCROLL  ESC BACK"
       : spec.allow_cancel  ? "UP/DOWN SELECT  ENTER CHOOSE  ESC BACK"
                            : "UP/DOWN SELECT  ENTER CHOOSE");

  const int x = kCx + kSplit + 22, w = kCw - kSplit - 44;
  Fill(t.dim, kCx + kSplit, kCy + 14, kCx + kSplit + 2, kCy + kCh - 14);
  int y = kCy + 16;
  for (const std::string &line : spec.info) {
    Text(x, y, t.ph, 19.0f, line);
    y += 26;
  }
  y += 10;
  if (spec.preview != nullptr && n > 0) {
    const int h = w * 3 / 4;
    spec.preview(sel, X(x), Y(y), X(x + w), Y(y + h));
    y += h + 12;
  }
  if (n > 0 && !spec.items[static_cast<size_t>(sel)].help.empty())
    WrapText(x, y, w, 19.0f, t.ph, spec.items[static_cast<size_t>(sel)].help);
  // The bottom strip carries the build, where the page keys sit on the options deck.
  Text(24, kCy + kCh + 26, c.ink, 15.0f, BuildString());
  Text(24, kCy + kCh + 48, c.ink, 15.0f, UpstreamString());
  DrawPointer(t);
  EndLayout();
}

} // namespace

namespace {

void DrawBackground(const DeckListSpec &spec) {
  if (spec.background != nullptr) {
    spec.background();
  } else if (GetUICallback() == MenuScene) {
    MainBackground(); // not the stock frame: it would draw the stock menu items
  } else {
    // In a level the menu frame has no art, and some callbacks draw nothing.
    if (GetScreenMode() == SM_MENU) {
      StartFrame(0, 0, Max_window_w, Max_window_h, false);
      rend_FillRect(GR_BLACK, 0, 0, Max_window_w, Max_window_h);
      EndFrame();
    }
    DoUIFrameWithoutInput();
  }
}

} // namespace

int Deck_RunList(const DeckListSpec &spec) {
  const int n = static_cast<int>(spec.items.size());
  int sel = std::clamp(spec.selected, 0, std::max(0, n - 1)), scroll = 0, result = -2;
  std::vector<std::pair<Rect, int>> rects;
  NativeFrame frame;
  ReadPointer();
  int last_x = g_mouse_x, last_y = g_mouse_y; // a resting pointer must not move the selection
  ddio_MouseBtnDownCount(0);
  ui_HideCursor();
  ddio_KeyFlush();
  while (result == -2) {
    Descent->defer();
    DrawBackground(spec);
    ReadPointer();
    DrawList(spec, sel, &scroll, &rects);
    rend_Flip();

    // Moving the pointer over a row selects it; a click chooses it.
    const bool moved = g_mouse_x != last_x || g_mouse_y != last_y;
    const bool clicked = ddio_MouseBtnDownCount(0) > 0;
    last_x = g_mouse_x;
    last_y = g_mouse_y;
    for (const auto &[rect, index] : rects) {
      if (rect.Has(g_mouse_x, g_mouse_y) && spec.items[static_cast<size_t>(index)].enabled) {
        if (moved)
          sel = index;
        if (clicked)
          result = index;
      }
    }
    if (Multi_bail_ui_menu)
      result = -1;
    int key;
    while (result == -2 && (key = ddio_KeyInKey()) != 0) {
      const int code = key & 0xFF;
      if (spec.close_key != 0 && key == spec.close_key) {
        result = -1;
      } else if ((code == KEY_UP || code == KEY_DOWN) && n > 0) {
        // Headings and empty slots are stepped over.
        const int step = code == KEY_UP ? n - 1 : 1;
        int next = (sel + step) % n;
        while (next != sel && !spec.items[static_cast<size_t>(next)].enabled)
          next = (next + step) % n;
        sel = next;
      } else if ((code == KEY_ENTER || code == KEY_PADENTER || code == KEY_SPACEBAR) && n > 0) {
        if (spec.items[static_cast<size_t>(sel)].enabled)
          result = sel;
      } else if (code == KEY_ESC) {
        if (spec.allow_cancel)
          result = -1;
      } else {
        for (int i = 0; i < n; ++i) {
          const DeckListItem &it = spec.items[static_cast<size_t>(i)];
          if (it.hotkey != 0 && it.hotkey == key && it.enabled)
            result = i;
        }
      }
    }
  }
  ui_ShowCursor();
  ui_Flush();
  return result;
}

bool Deck_Confirm(const std::string &title, const std::string &text) {
  DeckListSpec spec;
  spec.title = title;
  spec.selected = 1;
  spec.items = {{"Yes", "", text, KEY_Y}, {"No", "", text, KEY_N}};
  return Deck_RunList(spec) == 0;
}

void Deck_Message(const std::string &title, const std::string &text) {
  DeckListSpec spec;
  spec.title = title;
  spec.items = {{"OK", "", text}};
  Deck_RunList(spec);
}

void Deck_RunPoll(const DeckListSpec &spec, bool (*poll)(int key, void *data), void *data) {
  int scroll = 0;
  std::vector<std::pair<Rect, int>> rects;
  NativeFrame frame;
  ui_HideCursor();
  ddio_KeyFlush();
  bool done = false;
  while (!done) {
    Descent->defer();
    DrawBackground(spec);
    ReadPointer();
    DrawList(spec, 0, &scroll, &rects);
    rend_Flip();
    done = poll(ddio_KeyInKey(), data) || Multi_bail_ui_menu;
  }
  ui_ShowCursor();
  ui_Flush();
}

void Deck_RunView(std::vector<std::string> (*lines)(), const std::string &footer,
                  bool (*frame)(int left, int top, int right, int bottom), void (*key)(int key)) {
  NativeFrame native;
  ui_HideCursor();
  ddio_KeyFlush();
  bool done = false;
  while (!done) {
    Descent->defer();
    const Theme &t = kThemes[std::clamp(ui_phosphor->GetInt(), 0, 3)];
    const Casing &c = kCasings[std::clamp(ui_casing->GetInt(), 0, 1)];
    StartFrame(0, 0, Max_window_w, Max_window_h, false);
    rend_FillRect(GR_BLACK, 0, 0, Max_window_w, Max_window_h);
    EndFrame();
    BeginLayout();
    DrawCasing(c, t);
    Fill(GR_BLACK, kCx, kCy, kCx + kCw, kCy + kCh); // also clears the depth for the picture
    Text(24, kCy + kCh + 26, c.ink, 15.0f, footer);
    EndLayout();
    done = frame(X(kCx), Y(kCy), X(kCx + kCw), Y(kCy + kCh));
    BeginLayout();
    int y = kCy + 10;
    for (const std::string &line : lines()) {
      Text(kCx + 14, y, t.ph, y == kCy + 10 ? 22.0f : 17.0f, line);
      y += 26;
    }
    EndLayout();
    rend_Flip();
    int pressed;
    while ((pressed = ddio_KeyInKey()) != 0)
      key(pressed);
    if (Multi_bail_ui_menu)
      done = true;
  }
  ui_ShowCursor();
  ui_Flush();
}

bool Deck_Ready() { return ui_phosphor != nullptr && ui_casing != nullptr; }

bool Deck_RunText(const std::string &title, const std::string &prompt, std::string *text, size_t max_len,
                  bool allow_cancel) {
  std::string line = *text;
  DeckListSpec spec;
  spec.title = title;
  spec.allow_cancel = allow_cancel;
  spec.footer = allow_cancel ? "ENTER ACCEPT  ESC CANCEL" : "ENTER ACCEPT";
  spec.items.resize(1);
  spec.items[0].help = prompt;
  int scroll = 0, result = -1;
  std::vector<std::pair<Rect, int>> rects;
  NativeFrame frame;
  ui_HideCursor();
  ddio_KeyFlush();
  while (result < 0) {
    Descent->defer();
    DrawBackground(spec);
    ReadPointer();
    spec.items[0].label = line + "_";
    DrawList(spec, 0, &scroll, &rects);
    rend_Flip();
    int key;
    while (result < 0 && (key = ddio_KeyInKey()) != 0) {
      const int code = key & 0xFF;
      if (code == KEY_ENTER || code == KEY_PADENTER) {
        result = 1;
      } else if (code == KEY_ESC) {
        if (allow_cancel)
          result = 0;
      } else if (code == KEY_BACKSP) {
        if (!line.empty())
          line.pop_back();
      } else {
        const int ascii = ddio_KeyToAscii(key);
        if (ascii >= 32 && ascii < 127 && line.size() < max_len)
          line.push_back(static_cast<char>(ascii));
      }
    }
  }
  ui_ShowCursor();
  ui_Flush();
  if (result == 1)
    *text = line;
  return result == 1;
}

void Deck_DrawChunks(const int *bm_array, int chunks_w, int chunks_h, int src_w, int src_h, float x, float y, float scale) {
  if (bm_array == nullptr)
    return;
  // Chunks are clamped and edged in floats, so no seams at any scale. The last
  // row and column may be part-filled.
  const int piece = bm_w(bm_array[0], 0);
  rend_SetOverlayType(OT_NONE);
  rend_SetLighting(LS_NONE);
  rend_SetColorModel(CM_MONO);
  rend_SetZBufferState(0);
  rend_SetAlphaType(AT_CONSTANT_TEXTURE);
  rend_SetAlphaValue(255);
  rend_SetWrapType(WT_CLAMP);
  for (int row = 0; row < chunks_h; ++row) {
    const int ph = std::min(piece, src_h - row * piece);
    for (int col = 0; col < chunks_w; ++col) {
      const int pw = std::min(piece, src_w - col * piece);
      if (pw <= 0 || ph <= 0)
        continue;
      const int x1 = static_cast<int>(std::lround(x + col * piece * scale));
      const int y1 = static_cast<int>(std::lround(y + row * piece * scale));
      const int x2 = static_cast<int>(std::lround(x + (col * piece + pw) * scale));
      const int y2 = static_cast<int>(std::lround(y + (row * piece + ph) * scale));
      rend_DrawScaledBitmap(x1, y1, x2, y2, bm_array[row * chunks_w + col], 0, 0, pw / static_cast<float>(piece),
                            ph / static_cast<float>(piece));
    }
  }
  rend_SetWrapType(WT_WRAP);
  rend_SetZBufferState(1);
}

bool Deck_DrawMenuArt(int *bm_array, int chunks_w, int chunks_h) {
  if (IsClassic())
    return false;
  // Scale the 640x480 art to cover the frame, cropping rather than distorting.
  constexpr int kArtW = 640, kArtH = 480;
  const float s = std::max(Max_window_w / static_cast<float>(kArtW), Max_window_h / static_cast<float>(kArtH));
  Deck_DrawChunks(bm_array, chunks_w, chunks_h, kArtW, kArtH, (Max_window_w - kArtW * s) / 2,
                  (Max_window_h - kArtH * s) / 2, s);
  return true;
}

int MainDeck_Run() {
  if (IsClassic())
    return -1;
  SetUICallback(MenuScene); // as mmInterface::DoUI(): stock screens opened from here keep the art behind them
  static int last = 0;
  char pilot[64] = "";
  Current_pilot.get_name(pilot);
  DeckListSpec spec;
  spec.title = "MAIN MENU";
  spec.allow_cancel = false;
  spec.selected = last;
  spec.background = MainBackground;
  spec.info = {"PILOT: " + std::string(pilot)};
  spec.items = {
      {"New game", "", "Start a mission.", KEY_N},
      {"Load game", "", "Continue from a saved game.", KEY_L},
      {"View demo", "", "Play back a recorded demo.", KEY_D},
      {"Options", "", "Graphics, controls, audio and Tartarus settings.", KEY_O},
      {"Pilots", "", "Choose, add or change a pilot.", KEY_P},
      {"Multiplayer", "", "Network games.", KEY_M},
      {"Credits", "", "Who made the game.", KEY_C},
      {"Quit", "", "Leave Tartarus.", KEY_Q},
  };
  last = Deck_RunList(spec);
  return last;
}

bool Deck_HandleKey(int key) {
  const int code = key & 0xFF;
  const bool mods = (key & (KEY_SHIFTED | KEY_CTRLED | KEY_ALTED)) != 0;
  if (!g_open) {
    if (key != KEY_F2 || IsClassic() || (Game_mode & GM_MULTI) || GetFunctionMode() != GAME_MODE ||
        Game_interface_mode != GAME_INTERFACE)
      return false; // stock Options menu
    Open();
    return true;
  }
  const DeckRow *row = CurrentRow();
  const int pages = static_cast<int>(g_pages.size());
  switch (code) {
  case KEY_ESC:
  case KEY_F2:
    Deck_Close();
    break;
  case KEY_UP:
    MoveSel(-1);
    break;
  case KEY_DOWN:
    MoveSel(1);
    break;
  case KEY_LEFT:
    if (row != nullptr && row->kind != DeckKind::Action)
      Change(*row, -1);
    break;
  case KEY_RIGHT:
    if (row != nullptr && row->kind != DeckKind::Action)
      Change(*row, 1);
    break;
  case KEY_ENTER:
  case KEY_PADENTER:
  case KEY_SPACEBAR:
    if (row != nullptr)
      Change(*row, 1);
    break;
  case KEY_TAB:
    g_page = (g_page + ((key & KEY_SHIFTED) ? pages - 1 : 1)) % pages;
    break;
  case KEY_PAGEUP:
    g_page = (g_page + pages - 1) % pages;
    break;
  case KEY_PAGEDOWN:
    g_page = (g_page + 1) % pages;
    break;
  default:
    (void)mods;
    break;
  }
  return true; // the open deck swallows every key
}

void Deck_Render() {
  if (!g_open)
    return;
  if (!g_modal && Control_poll_flag)
    SuspendControls(); // some game events resume controls behind our back
  g_s = std::min(Max_window_w / 1160.0f, Max_window_h / 680.0f);
  g_ox = (Max_window_w - static_cast<int>(kW * g_s)) / 2;
  g_oy = (Max_window_h - static_cast<int>(kH * g_s)) / 2;
  const Theme &t = kThemes[std::clamp(ui_phosphor->GetInt(), 0, 3)];
  const Casing &c = kCasings[std::clamp(ui_casing->GetInt(), 0, 1)];

  StartFrame(0, 0, Max_window_w, Max_window_h, false);
  grtext_SetFont(BIG_BRIEFING_FONT);
  grtext_SetFlags(0);
  grtext_SetAlpha(255);
  DrawCasing(c, t);
  DrawKeys(c);
  DrawRows(t);
  DrawReadout(t);
  DrawPointer(t);
  grtext_SetFontScale(1.0f);
  EndFrame();
  HandleMouse();
}

} // namespace tartarus
