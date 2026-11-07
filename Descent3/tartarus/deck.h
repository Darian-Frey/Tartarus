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

// Options deck (F-069, D-016, IMP-006): the Tartarus options screen, opened
// with F2 in a single-player level or Options on the main menu, in Tartarus
// mode. Classic Mode and multiplayer keep the stock Options menu. Drawn as an overlay from
// rectangles, lines and scaled text in a fixed layout, so it is the same
// size on screen at any resolution. Pages come from deck_core.

#pragma once

#include <string>
#include <vector>

namespace tartarus {

/// Registers ui_* and the audio mirror cvars. Called from Tartarus_Init().
void Deck_Init();

/// Offer a key after the console. F2 opens; while open the deck takes every key.
bool Deck_HandleKey(int key);

/// Draws the deck if open and handles the mouse. Called from RenderOverlays().
void Deck_Render();

/// Options from the main menu (Tartarus mode): runs the deck in its own loop
/// over the menu. Returns false if the stock Options menu should open instead
/// (Classic Mode, or the deck's Stock options entry).
bool Deck_RunFromMenu();

/// One row of a list screen.
struct DeckListItem {
  std::string label;
  std::string value;  // right-aligned text, may be empty
  std::string help;   // shown in the readout panel
  int hotkey = 0;     // engine key code that picks this row at once, or 0
  bool enabled = true;
};

struct DeckListSpec {
  std::string title;
  std::vector<DeckListItem> items;
  std::vector<std::string> info;     // lines at the top of the readout panel
  int selected = 0;
  bool allow_cancel = true;          // Escape returns -1
  bool compact = false;              // smaller rows, for long lists
  bool read_only = false;            // nothing to choose: the footer says so
  std::string footer;                // replaces the usual key hints
  int close_key = 0;                 // a second key that cancels, such as the one that opened the screen
  void (*background)() = nullptr;    // draws behind the deck; default: the menu art or the stock UI frame
  void (*preview)(int row, int x1, int y1, int x2, int y2) = nullptr; // draws a 4:3 picture in the readout panel
  int *adjust = nullptr;             // if set, Left/Right also choose a row and store -1 or 1 here (0 for Enter)
};

/// The deck's reusable list screen, in its own loop: keyboard and mouse.
/// Returns the chosen row, or -1 if cancelled.
int Deck_RunList(const DeckListSpec &spec);

/// Prompts on the deck, in the same style.
bool Deck_Confirm(const std::string &title, const std::string &text); // Yes/No, No selected
void Deck_Message(const std::string &title, const std::string &text);
/// Edits a line of text. False if cancelled; *text is only changed on Enter.
bool Deck_RunText(const std::string &title, const std::string &prompt, std::string *text, size_t max_len,
                  bool allow_cancel = true);

/// Shows spec's first row and calls poll each frame with the key pressed (0 for
/// none), until it returns true.
void Deck_RunPoll(const DeckListSpec &spec, bool (*poll)(int key, void *data), void *data);

/// A picture on the deck's screen, full width: frame draws into the rectangle,
/// in pixels, and returns true to close; key gets each key pressed; lines gives
/// the text over its top left, asked for every frame.
void Deck_RunView(std::vector<std::string> (*lines)(), const std::string &footer,
                  bool (*frame)(int left, int top, int right, int bottom), void (*key)(int key));

/// False until the deck's settings exist: early start-up errors use the stock box.
bool Deck_Ready();

/// The main menu. Returns the chosen row of kMainDeckRows, or -1 in Classic
/// Mode (the caller runs the stock menu).
inline constexpr int kMainDeckRows = 8;
int MainDeck_Run();

/// Draws a chunked bitmap of src_w x src_h at x, y.
void Deck_DrawChunks(const int *bm_array, int chunks_w, int chunks_h, int src_w, int src_h, float x, float y, float scale);

/// Draws the main menu art scaled to cover the frame. False in Classic Mode.
bool Deck_DrawMenuArt(int *bm_array, int chunks_w, int chunks_h);

/// Called from the game's menu loop before the stock Options menu: true if the
/// deck's Key bindings row asked for the controls screens and they have run.
bool Deck_RunPendingControls();

bool Deck_IsOpen();
void Deck_Close();

} // namespace tartarus
