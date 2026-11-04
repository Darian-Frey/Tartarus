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

// Save and load screens in the deck style. See deck_save.h.

#include "deck_save.h"

#include <cstdio>
#include <cstring>
#include <string>
#include <system_error>
#include <vector>

#include "bitmap.h"
#include "cfile.h"
#include "classic.h"
#include "deck.h"
#include "gamesave.h"
#include "hud.h"
#include "stringtable.h"

namespace tartarus {

namespace {

constexpr int kSlots = 8; // must equal N_SAVE_SLOTS in gamesave.cpp

struct Slot {
  bool used = false;  // holds a readable save
  bool taken = false; // a file exists, readable or not
  std::string desc;
};

chunked_bitmap g_thumb{};
int g_thumb_slot = -1;

void FreeThumb() {
  if (g_thumb.bm_array != nullptr)
    bm_DestroyChunkedBitmap(&g_thumb);
  g_thumb = chunked_bitmap{};
  g_thumb_slot = -1;
}

// The screenshot stored in the save, loaded when the selection changes.
void Preview(int row, int x1, int y1, int x2, int /*y2*/) {
  if (row != g_thumb_slot) {
    FreeThumb();
    g_thumb_slot = row;
    char desc[GAMESAVE_DESCLEN + 1];
    int bm_handle = -1;
    std::error_code ec;
    const std::filesystem::path path = SaveDeck_SlotPath(row);
    if (std::filesystem::exists(path, ec) && GetGameStateInfo(path, desc, &bm_handle) && bm_handle > 0) {
      // rend_Screenshot() leaves the opaque bit clear, and the renderer draws
      // such pixels as fully transparent (BUG-013).
      uint16_t *pixels = bm_data(bm_handle, 0);
      for (int i = 0, n = bm_w(bm_handle, 0) * bm_h(bm_handle, 0); i < n; ++i)
        pixels[i] |= OPAQUE_FLAG;
      bm_CreateChunkedBitmap(bm_handle, &g_thumb);
      bm_FreeBitmap(bm_handle);
    }
  }
  if (g_thumb.bm_array != nullptr)
    Deck_DrawChunks(g_thumb.bm_array, g_thumb.w, g_thumb.h, g_thumb.pw, g_thumb.ph, static_cast<float>(x1),
                    static_cast<float>(y1), (x2 - x1) / static_cast<float>(g_thumb.pw));
}

std::vector<Slot> ReadSlots() {
  std::vector<Slot> slots(kSlots);
  for (int i = 0; i < kSlots; ++i) {
    std::error_code ec;
    const std::filesystem::path path = SaveDeck_SlotPath(i);
    if (!std::filesystem::exists(path, ec))
      continue;
    slots[i].taken = true;
    char desc[GAMESAVE_DESCLEN + 1];
    if (GetGameStateInfo(path, desc)) {
      slots[i].used = true;
      slots[i].desc = desc;
    }
  }
  return slots;
}

DeckListSpec SlotList(const char *title, const std::vector<Slot> &slots, bool loading) {
  DeckListSpec spec;
  spec.title = title;
  spec.preview = Preview;
  for (int i = 0; i < kSlots; ++i) {
    const Slot &s = slots[static_cast<size_t>(i)];
    DeckListItem item;
    item.label = s.used ? s.desc : (s.taken ? TXT_ILLEGALSAVEGAME : TXT_EMPTY);
    item.value = std::to_string(i + 1);
    item.enabled = !loading || s.used;
    spec.items.push_back(item);
  }
  return spec;
}

} // namespace

std::filesystem::path SaveDeck_SlotPath(int slot) {
  char name[16];
  std::snprintf(name, sizeof(name), "saveg%03d", slot);
  return cf_GetWritableBaseDirectory() / "savegame" / name;
}

bool SaveDeck_Run() {
  if (IsClassic())
    return false;
  std::vector<Slot> slots = ReadSlots();
  int selected = Quicksave_game_slot >= 0 ? Quicksave_game_slot : 0;
  for (;;) {
    DeckListSpec spec = SlotList(TXT_SAVEGAME, slots, false);
    spec.selected = selected;
    const int slot = Deck_RunList(spec);
    FreeThumb();
    if (slot < 0)
      return true;
    selected = slot;

    std::string desc = slots[static_cast<size_t>(slot)].desc;
    bool named = false;
    while (!named) {
      if (!Deck_RunText(TXT_SAVEGAME, TXT_DESCRIPTION, &desc, GAMESAVE_DESCLEN))
        break;
      named = true;
      if (desc.find_first_not_of(' ') == std::string::npos) {
        Deck_Message(TXT_SAVEGAME, TXT_SAVEGAMENAME);
        named = false;
        continue;
      }
      for (int i = 0; i < kSlots; ++i) {
        const Slot &s = slots[static_cast<size_t>(i)];
        if (i != slot && s.used && stricmp(s.desc.c_str(), desc.c_str()) == 0) {
          Deck_Message(TXT_SAVEGAME, TXT_SAVEGAMEDUP);
          named = false;
          break;
        }
      }
    }
    if (!named)
      continue;
    if (slots[static_cast<size_t>(slot)].taken && !Deck_Confirm(TXT_WARNING, TXT_OVERWRITESAVE))
      continue;
    if (!SaveGameState(SaveDeck_SlotPath(slot), desc.c_str())) {
      Deck_Message(TXT_ERROR, TXT_SAVEGAMEFAILED);
      continue;
    }
    AddHUDMessage(TXT_QUICKSAVE);
    Quicksave_game_slot = slot;
    return true;
  }
}

int SaveDeck_PickLoad() {
  if (IsClassic())
    return kSaveDeckStock;
  const std::vector<Slot> slots = ReadSlots();
  int first = -1;
  for (int i = kSlots - 1; i >= 0; --i) {
    if (slots[static_cast<size_t>(i)].used)
      first = i;
  }
  if (first < 0) {
    Deck_Message(TXT_LOADGAME, TXT_ERRNOSAVEGAMES);
    return -1;
  }
  DeckListSpec spec = SlotList(TXT_LOADGAME, slots, true);
  spec.selected = first;
  const int slot = Deck_RunList(spec);
  FreeThumb();
  return slot;
}

} // namespace tartarus
