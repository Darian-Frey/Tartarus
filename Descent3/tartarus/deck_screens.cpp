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

// Small stock screens on the deck. See deck_screens.h.

#include "deck_screens.h"

#include <algorithm>
#include <cstring>

#include "cfile.h"
#include "classic.h"
#include "ctlconfig.h"
#include "ddio.h"
#include "deck.h"
#include "difficulty.h"
#include "game.h"
#include "gamesequence.h"
#include "levelgoal.h"
#include "object.h"
#include "player.h"
#include "Mission.h"
#include "newui.h"
#include "pilot.h"
#include "stringtable.h"

extern int Times_game_restored; // screens.cpp, with the stock review

// Not declared in pilot.h.
bool PltDelete(pilot *Pilot);
bool PltSelectShip(pilot *Pilot);
bool PltCopyKeyConfig(pilot *src, pilot *dest);
void PilotCopyDefaultControls(pilot *Pilot);

namespace tartarus {

namespace {

bool g_force_stock_pilots = false; // the "Stock pilot screen" row

std::string OneLine(std::string text) {
  std::replace(text.begin(), text.end(), '\n', ' ');
  return text;
}

std::vector<DeckListItem> ListRows(newuiListBox *list, int count) {
  std::vector<DeckListItem> rows;
  for (int i = 0; i < count; ++i) {
    char name[256] = "";
    list->GetItem(i, name, sizeof(name));
    DeckListItem row;
    row.label = name;
    rows.push_back(row);
  }
  return rows;
}

} // namespace

bool Deck_Available() { return !IsClassic() && !(Game_mode & GM_MULTI) && GetScreenMode() != SM_NULL && Deck_Ready(); }

int Deck_MessageBox(const char *title, const char *msg, int type) {
  if (!Deck_Available())
    return kDeckStock;
  const std::string heading = title != nullptr && title[0] != 0 ? title : "Message";
  const std::string text = OneLine(msg != nullptr ? msg : "");
  switch (type) {
  case MSGBOX_YESNO:
    return Deck_Confirm(heading, text) ? 1 : 0;
  case MSGBOX_OKCANCEL: {
    DeckListSpec spec;
    spec.title = heading;
    spec.items = {{TXT_OK, "", text}, {TXT_CANCEL, "", text}};
    return Deck_RunList(spec) == 0 ? 1 : 0;
  }
  default:
    Deck_Message(heading, text);
    return 1;
  }
}

int Deck_EditBox(const char *prompt, char *buffer, int buflen, bool showcancel) {
  if (!Deck_Available() || buflen < 2)
    return kDeckStock;
  std::string text = buffer;
  if (!Deck_RunText("Enter text", OneLine(prompt != nullptr ? prompt : ""), &text, static_cast<size_t>(buflen) - 1,
                    showcancel))
    return 0;
  std::strncpy(buffer, text.c_str(), static_cast<size_t>(buflen) - 1);
  buffer[buflen - 1] = 0;
  return 1;
}

bool HelpDeck_Run(const int *text_ids) {
  if (!Deck_Available())
    return false;
  DeckListSpec spec;
  spec.title = TXT_HELP;
  spec.compact = true;
  spec.read_only = true;
  spec.close_key = KEY_F1;
  for (int i = 0; text_ids[i] > 0; i += 2)
    spec.items.push_back({TXT(text_ids[i]), TXT(text_ids[i + 1]), ""});
  spec.items.push_back({"`", "Developer console", ""});
  spec.items.push_back({"SHIFT-F12", "Cheat menu", ""});
  Deck_RunList(spec);
  return true;
}

int BuddyDeck_Pick(const char *title, const std::vector<std::string> &commands) {
  if (!Deck_Available())
    return kDeckStock;
  DeckListSpec spec;
  spec.title = title;
  spec.close_key = KEY_F4;
  for (size_t i = 0; i < commands.size(); ++i) {
    const char hotkey = i < 9 ? static_cast<char>('1' + i) : static_cast<char>('a' + (i - 9));
    DeckListItem row;
    row.label = std::string(1, static_cast<char>(i < 9 ? hotkey : hotkey - 'a' + 'A')) + ". " + commands[i];
    row.hotkey = ddio_AsciiToKey(hotkey);
    spec.items.push_back(row);
  }
  ddio_MouseQueueFlush(); // clicks from gameplay must not choose a command
  return Deck_RunList(spec);
}

int MissionDeck_Pick(newuiListBox *list, const std::vector<std::filesystem::path> &files) {
  if (!Deck_Available())
    return kDeckStock;
  DeckListSpec spec;
  spec.title = TXT_MENUNEWGAME;
  spec.items = ListRows(list, static_cast<int>(files.size()));
  for (size_t i = 0; i < files.size(); ++i) {
    tMissionInfo info{};
    if (GetMissionInfo(files[i], &info) && info.name[0] != 0) {
      std::string help = info.desc;
      if (info.author[0] != 0)
        help += std::string(help.empty() ? "" : " ") + "By " + info.author + ".";
      spec.items[i].help = OneLine(help);
      if (info.n_levels > 0)
        spec.items[i].value = std::to_string(info.n_levels) + (info.n_levels == 1 ? " LEVEL" : " LEVELS");
    }
    if (files[i] == "d3.mn3")
      spec.selected = static_cast<int>(i); // open on the main campaign
  }
  return Deck_RunList(spec);
}

int LevelDeck_Pick(newuiListBox *list, int levels, const char *mission) {
  if (!Deck_Available())
    return kDeckStock;
  DeckListSpec spec;
  spec.title = mission;
  spec.items = ListRows(list, levels);
  spec.selected = levels - 1; // the furthest level reached
  const int row = Deck_RunList(spec);
  return row < 0 ? -1 : row + 1;
}

namespace {

const char *DifficultyName(int level) {
  switch (level) {
  case 0:
    return TXT_TRAINEE;
  case 1:
    return TXT_ROOKIE;
  case 2:
    return TXT_HOTSHOT;
  case 3:
    return TXT_ACE;
  default:
    return TXT_INSANE;
  }
}

void UsePilot(const std::string &filename) {
  Current_pilot.set_filename(filename);
  PltReadFile(&Current_pilot, true, true);
  if (VerifyPilotData(&Current_pilot))
    PltWriteFile(&Current_pilot); // it had to be fixed
}

enum PilotRow { kUse, kDifficulty, kProfanity, kTaunts, kShip, kKeyboard, kController, kCopy, kStock, kDelete };
enum class PilotResult { Back, Chosen, Deleted, Stock };

// Copy controls, as the stock button: another pilot's (or a preset's) bindings,
// sensitivities, key ramping, devices and mouse control style. Saved at once.
void CopyControlsInto(pilot *work) {
  PltClearList();
  const std::vector<std::string> others = PltGetPilots(work->get_filename(), 1); // pilots, then presets
  PltClearList(); // the caller lists pilots again itself
  if (others.empty()) {
    Deck_Message(TXT_PLTERROR, TXT_NOPILOTSTOCOPY);
    return;
  }
  DeckListSpec spec;
  spec.title = TXT_CPYKEYCONF;
  std::vector<std::string> names;
  for (const std::string &file : others) {
    pilot entry;
    entry.set_filename(file);
    PltReadFile(&entry);
    char name[PILOT_STRING_SIZE] = "";
    entry.get_name(name);
    names.emplace_back(name);
    const bool preset = file.size() > 4 && stricmp(file.c_str() + file.size() - 4, ".pld") == 0;
    spec.items.push_back({name, preset ? "PRESET" : "", "Copy this pilot's controls."});
  }
  const int pick = Deck_RunList(spec);
  if (pick < 0)
    return;
  char target[PILOT_STRING_SIZE] = "";
  work->get_name(target);
  const std::string question = "Replace " + std::string(target) + "'s bindings and control settings with " +
                               names[static_cast<size_t>(pick)] + "'s?";
  if (!Deck_Confirm(TXT_CPYKEYCONF, question))
    return;
  pilot source;
  source.set_filename(others[static_cast<size_t>(pick)]);
  PltReadFile(&source); // reads the controls without applying them, as the stock screen
  if (PltCopyKeyConfig(&source, work))
    PltWriteFile(work);
}

// One pilot's page. Saves every change straight away, as the stock screen does
// when the selection moves.
PilotResult PilotPage(const std::string &filename) {
  pilot work;
  work.set_filename(filename);
  PltReadFile(&work, true, true);
  int selected = kUse;
  for (;;) {
    char name[PILOT_STRING_SIZE] = "";
    uint8_t difficulty = 0;
    bool profanity = false, taunts = false;
    work.get_name(name);
    work.get_difficulty(&difficulty);
    work.get_profanity_filter(&profanity);
    work.get_audiotaunts(&taunts);

    DeckListSpec spec;
    spec.title = name;
    spec.selected = selected;
    spec.items = {
        {"Fly as this pilot", "", "Make this the current pilot."},
        {"Difficulty", DifficultyName(difficulty), "Enter steps through the levels."},
        {"Profanity filter", profanity ? "[ON]" : "[OFF]", "Filters multiplayer chat."},
        {"Audio taunts", taunts ? "[ON]" : "[OFF]", "Plays other pilots' taunts in multiplayer."},
        {"Ship and logo", "", "Opens the stock ship configuration, used in multiplayer."},
        {"Keyboard", "", "This pilot's key bindings."},
        {"Controller", "", "This pilot's joystick and mouse bindings."},
        {"Copy controls", "", "Replaces this pilot's bindings and control settings with another pilot's."},
        {"Stock pilot screen", "", "Pilot picture and the rest of the stock settings."},
        {"Delete pilot", "", "Deletes this pilot after asking."},
    };
    selected = Deck_RunList(spec);
    switch (selected) {
    case -1:
      return PilotResult::Back;
    case kUse:
      return PilotResult::Chosen;
    case kDifficulty:
      work.set_difficulty(static_cast<uint8_t>((difficulty + 1) % 5));
      PltWriteFile(&work);
      break;
    case kProfanity:
      work.set_profanity_filter(!profanity);
      PltWriteFile(&work);
      break;
    case kTaunts:
      work.set_audiotaunts(!taunts);
      PltWriteFile(&work);
      break;
    case kShip:
      PltSelectShip(&work);
      PltWriteFile(&work);
      break;
    case kKeyboard:
    case kController:
      // The stock screens configure Current_pilot.
      PltWriteFile(&work);
      Current_pilot.set_filename(filename);
      PltReadFile(&Current_pilot, true, true);
      CtlConfig(selected == kKeyboard ? CTLCONFIG_KEYBOARD : CTLCONFIG_CONTROLLER);
      PltWriteFile(&Current_pilot, false);
      PltReadFile(&work, true, true);
      break;
    case kCopy:
      CopyControlsInto(&work);
      break;
    case kStock:
      return PilotResult::Stock;
    case kDelete: {
      char text[200];
      snprintf(text, sizeof(text), TXT_PLTOKDEL, name);
      if (Deck_Confirm(TXT_PLTDELCONF, text)) {
        PltDelete(&work);
        return PilotResult::Deleted;
      }
      break;
    }
    default:
      break;
    }
  }
}

} // namespace

bool PilotDeck_Run() {
  if (g_force_stock_pilots || !Deck_Available())
    return false;

  if (cfexist(Default_pilot) != CFES_NOT_FOUND) {
    Current_pilot.set_filename(Default_pilot);
    PltReadFile(&Current_pilot);
  }
  const std::string old_file = Current_pilot.get_filename();
  std::string chosen;
  int selected = 0;
  bool done = false;

  while (!done) {
    PltClearList();
    std::vector<std::string> files = PltGetPilots();
    if (files.empty()) {
      pilot fresh;
      if (PilotCreate(&fresh, true)) {
        PilotCopyDefaultControls(&fresh);
        PltWriteFile(&fresh);
      }
      continue;
    }

    DeckListSpec spec;
    spec.title = TXT_MENUPILOTS;
    for (size_t i = 0; i < files.size(); ++i) {
      pilot entry;
      entry.set_filename(files[i]);
      PltReadFile(&entry);
      char name[PILOT_STRING_SIZE] = "";
      entry.get_name(name);
      const bool current = files[i] == old_file;
      if (current && selected == 0)
        selected = static_cast<int>(i);
      spec.items.push_back({name, current ? "CURRENT" : "", "Enter opens this pilot's page."});
    }
    spec.items.push_back({"Add pilot", "", "Create a new pilot."});
    spec.selected = selected;

    selected = Deck_RunList(spec);
    if (selected == -1) {
      // Back: keep the old pilot if there is one.
      if (!old_file.empty() && cfexist(old_file) != CFES_NOT_FOUND) {
        chosen = old_file;
        done = true;
      } else {
        Deck_Message(TXT_PLTERROR, TXT_NEEDTOCREATE);
        selected = 0;
      }
    } else if (selected == static_cast<int>(files.size())) {
      pilot fresh;
      if (PilotCreate(&fresh, false)) {
        PilotCopyDefaultControls(&fresh);
        PltWriteFile(&fresh);
      }
    } else {
      const std::string file = files[static_cast<size_t>(selected)];
      switch (PilotPage(file)) {
      case PilotResult::Chosen:
        chosen = file;
        done = true;
        break;
      case PilotResult::Deleted:
        selected = 0;
        break;
      case PilotResult::Stock:
        PltClearList();
        g_force_stock_pilots = true;
        PilotSelect();
        g_force_stock_pilots = false;
        return true; // the stock screen has chosen a pilot
      default:
        break;
      }
    }
  }

  UsePilot(chosen);
  Default_pilot = Current_pilot.get_filename();
  PltClearList();
  Current_pilot.get_difficulty(&ingame_difficulty);
  return true;
}

bool ResultsDeck_Run(bool success) {
  if (!Deck_Available())
    return false;
  const player &pl = Players[Player_num];
  const char *difficulty[] = {TXT_TRAINEE, TXT_ROOKIE, TXT_HOTSHOT, TXT_ACE, TXT_INSANE};
  uint8_t dif = 0;
  Current_pilot.get_difficulty(&dif);
  const int seconds = static_cast<int>(pl.time_level);
  char time[16];
  snprintf(time, sizeof(time), "%d:%02d", seconds / 60, seconds % 60);

  DeckListSpec spec;
  spec.title = TXT_PLRTITLE;
  spec.info = {Level_info.name, success ? TXT_PLR_MSN_SUCCESS : TXT_PLR_MSN_FAILED};
  spec.compact = true;
  spec.footer = "ENTER CONTINUE";
  // Some stock labels end in a colon and some do not: none do here.
  auto row = [&spec](std::string label, const std::string &value) {
    while (!label.empty() && (label.back() == ':' || label.back() == ' '))
      label.pop_back();
    spec.items.push_back({label, value, ""});
  };
  row(TXT_PLTDIFFICULT, difficulty[std::min<int>(dif, 4)]);
  row(TXT_SCORE, std::to_string(pl.score));
  row(TXT_PLRTIME, time);
  row(TXT_PLRENEMIESKILL, std::to_string(pl.num_kills_level));
  if (pl.friendly_kills_level)
    row(TXT_PLRFRIENDSKILL, std::to_string(pl.friendly_kills_level));
  row(TXT_PLRSHIELD, std::to_string(static_cast<int>(Objects[pl.objnum].shields + 0.5f)));
  row(TXT_PLRENERGY, std::to_string(static_cast<int>(pl.energy + 0.5f)));
  row(TXT_NUMDEATHS, std::to_string(pl.num_deaths_level));
  row(TXT_PLRSAVERESRAT, std::to_string(Times_game_restored));
  // Objectives, as the stock review lists them: the ones TelCom shows.
  bool heading = false;
  for (int j = 0; j < Level_goals.GetNumGoals(); ++j) {
    int status = 0;
    if (!Level_goals.GoalStatus(j, LO_GET_SPECIFIED, &status) || !(status & LGF_ENABLED) ||
        !(status & LGF_TELCOM_LISTS))
      continue;
    if (!heading) {
      spec.items.push_back({std::string("- ") + TXT_OBJECTIVES1 + " -", "", "", 0, false});
      heading = true;
    }
    char name[64];
    Level_goals.GoalGetName(j, name, sizeof(name));
    row(name, (status & LGF_COMPLETED) ? TXT_COMPLETED : TXT_NOTCOMPLETEDYET);
  }

  // As the stock review: the menu screen mode, and nothing held from the level counts.
  const int old_mode = GetScreenMode();
  SetScreenMode(SM_MENU);
  SetUICallback(nullptr);
  ddio_KeyFlush();
  ddio_MouseQueueFlush();
  Deck_RunList(spec);
  SetScreenMode(old_mode);
  return true;
}

} // namespace tartarus
