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

#include "guidebot.h"

#include <cmath>
#include <functional>
#include <limits>
#include <queue>
#include <string>
#include <utility>
#include <vector>

#include "AIMain.h"
#include "BOA.h"
#include "console.h"
#include "deck.h"
#include "deck_screens.h"
#include "doorway.h"
#include "game.h"
#include "gamesequence.h"
#include "hud.h"
#include "log.h"
#include "map_scan.h"
#include "object.h"
#include "objinfo.h"
#include "osiris_share.h"
#include "player.h"
#include "room.h"
#include "scan_core.h"
#include "terrain.h"

namespace tartarus {

namespace {

constexpr float kUnreached = std::numeric_limits<float>::max();

// Stock command ids from scripts/AIGame.cpp: "Return to ship" is in the menu
// only while the bot is out of the ship, which is when it can guide.
constexpr int kGbcReturnToShip = 43;
constexpr int kGbcLastGoal = 18; // GBC_FIND_ACTIVE_SEC_GOAL_7: the goal rows come first

// Route cost from the player's room to every room and terrain region, indexed
// as the BOA indexes them. The search of AIFindAltPath() in aipath.cpp without
// its random spread, and with the player as the one opening doors.
std::vector<float> RouteCosts(object *from) {
  const int count = Highest_room_index + 1 + BOA_num_terrain_regions;
  std::vector<float> cost(static_cast<size_t>(count), kUnreached);
  const int start = BOA_INDEX(from->roomnum);
  if (start < 0 || start >= count)
    return cost;

  using Entry = std::pair<float, int>;
  std::priority_queue<Entry, std::vector<Entry>, std::greater<Entry>> open;
  cost[start] = 0.0f;
  open.push({0.0f, start});
  while (!open.empty()) {
    const auto [here_cost, here] = open.top();
    open.pop();
    if (here_cost > cost[here])
      continue;
    const bool is_room = here <= Highest_room_index;
    const int region = here - Highest_room_index - 1;
    const int portals = is_room ? Rooms[here].num_portals : BOA_num_connect[region];
    for (int p = 0; p < portals; ++p) {
      if (!BOA_PassablePortal(here, p))
        continue;
      int next = is_room ? Rooms[here].portals[p].croom : BOA_connect[region][p].roomnum;
      if (next >= 0 && next <= Highest_room_index && (Rooms[next].flags & RF_EXTERNAL)) {
        const int outer = BOA_DetermineStartRoomPortal(next, nullptr, here, nullptr);
        if (outer < 0)
          continue;
        const int cell = GetTerrainCellFromPos(&Rooms[next].portals[outer].path_pnt);
        if (cell < 0)
          continue;
        next = Highest_room_index + TERRAIN_REGION(cell) + 1;
      }
      if (next < 0 || next >= count || next == BOA_NO_PATH || BOA_LockedDoor(from, next))
        continue;
      int back = 0;
      if (next != here)
        back = BOA_DetermineStartRoomPortal(next, nullptr, here, nullptr);
      if (back < 0 || BOA_cost_array[here][p] < 0.0f)
        continue;
      const float next_cost = here_cost + BOA_cost_array[here][p] + BOA_cost_array[next][back];
      if (next_cost < cost[next]) {
        cost[next] = next_cost;
        open.push({next_cost, next});
      }
    }
  }
  return cost;
}

float CostTo(const std::vector<float> &cost, int roomnum) {
  const int index = BOA_INDEX(roomnum);
  return index >= 0 && static_cast<size_t>(index) < cost.size() ? cost[index] : kUnreached;
}

// A room the scanner has not reached, with somewhere to go: not a door, not a
// building's outer shell (scanned from the mine side), and with a solid
// surface for the scanner to find, so getting there marks it explored.
bool Unexplored(int room_index) {
  const room &rp = Rooms[room_index];
  if (!rp.used || (rp.flags & (RF_DOOR | RF_EXTERNAL)) || MapScan_RoomScanned(room_index))
    return false;
  for (int i = 0; i < rp.num_faces; ++i)
    if (rp.faces[i].portal_num == -1)
      return true;
  return false;
}

struct Target {
  bool found = false;
  bool room = false; // a room number, else an object handle
  int id = -1;
  float cost = kUnreached;
  std::string what;
  bool out_of_reach = false; // one exists, but no route to it yet
};

void Consider(Target *best, bool room, int id, float cost, std::string what) {
  if (cost == kUnreached) {
    best->out_of_reach = !best->found;
    return;
  }
  if (best->found && cost >= best->cost)
    return;
  *best = {true, room, id, cost, std::move(what), false};
}

std::string RoomName(int room_index) {
  const char *name = Rooms[room_index].name;
  return name != nullptr && name[0] != '\0' ? std::string(name) : "room " + std::to_string(room_index);
}

// Keys still lying in the level: picked-up keys are deleted in single player.
Target NextKey(const std::vector<float> &cost) {
  Target best;
  for (int i = 0; i <= Highest_object_index; ++i) {
    const object &obj = Objects[i];
    if (obj.type != OBJ_POWERUP || (obj.flags & OF_DEAD))
      continue;
    const char *name = Object_info[obj.id].name;
    if (!IsKeyName(name) || IsHiddenName(name))
      continue;
    Consider(&best, false, obj.handle, CostTo(cost, obj.roomnum), "The nearest key");
  }
  return best;
}

// Doors that need a key the player now has and lead somewhere not yet scanned.
Target OpenableDoor(object *player, const std::vector<float> &cost) {
  Target best;
  for (int i = 0; i <= Highest_object_index; ++i) {
    const object &obj = Objects[i];
    if (obj.type != OBJ_DOOR || (obj.flags & OF_DEAD) || ROOMNUM_OUTSIDE(obj.roomnum))
      continue;
    const room &rp = Rooms[obj.roomnum];
    if (rp.doorway_data == nullptr || rp.doorway_data->keys_needed == 0 ||
        !DoorwayOpenable(obj.handle, player->handle))
      continue;
    bool leads_on = !MapScan_Active(); // no scan to go by: every such door
    for (int p = 0; p < rp.num_portals && !leads_on; ++p) {
      const int beyond = rp.portals[p].croom;
      leads_on = beyond >= 0 && beyond <= Highest_room_index && Unexplored(beyond);
    }
    if (leads_on)
      Consider(&best, false, obj.handle, CostTo(cost, obj.roomnum), "The nearest such door, in " + RoomName(obj.roomnum));
  }
  return best;
}

Target NearestUnexplored(const std::vector<float> &cost) {
  Target best;
  if (!MapScan_Active())
    return best;
  for (int r = 0; r <= Highest_room_index; ++r)
    if (Unexplored(r))
      Consider(&best, true, r, cost[r], "The nearest, " + RoomName(r));
  return best;
}

void Send(const Target &target) {
  object *bot = ObjGet(Buddy_handle[Player_num]);
  if (bot == nullptr)
    return;
  int id = target.id;
  gb_com command;
  command.action = COM_DO_ACTION;
  command.index = target.room ? GBC_TARTARUS_GO_TO_ROOM : GBC_TARTARUS_GO_TO_OBJ;
  command.ptr = &id;
  LOG_INFO.printf("[tartarus] Guide-Bot to %s %d (%s), route %.0f", target.room ? "room" : "object", id,
                  target.what.c_str(), target.cost);
  AINotify(bot, AIN_USER_DEFINED, &command);
}

std::string Away(float cost) {
  return "About " + std::to_string(static_cast<int>(std::lround(cost / 10.0f)) * 10) + " units by the route.";
}

bool OutOfShip(const gb_menu &menu) {
  for (int i = 0; i < menu.num_commands; ++i)
    if (menu.command_id[i] == kGbcReturnToShip)
      return true;
  return false;
}

enum Kind { kKey, kDoor, kArea, kKinds };
const char *const kLabels[kKinds] = {"Next key", "Door I can now open", "Unexplored area"};
const char *const kOutOfReach = "There is a key, but no way to it yet: try the level goals first.";
const char *const kNone[kKinds] = {"No keys left to find that the Guide-Bot can reach.",
                                   "No door that needs a key you have leads anywhere new.",
                                   "No unexplored area the Guide-Bot can reach."};

void PickAll(Target out[kKinds]) {
  const std::vector<float> cost = RouteCosts(Player_object);
  out[kKey] = NextKey(cost);
  out[kDoor] = OpenableDoor(Player_object, cost);
  out[kArea] = NearestUnexplored(cost);
  out[kDoor].out_of_reach = out[kArea].out_of_reach = false; // only a key is worth naming out of reach
}

bool CanGuide() {
  return Deck_Available() && !(Game_mode & GM_MULTI) && Player_object != nullptr &&
         ObjGet(Buddy_handle[Player_num]) != nullptr;
}

/// guide key|door|area: the menu's commands from the console, or a bound key.
void GuideCommand(const CommandArgs &a, ConsoleCore &c) {
  static const char *const kWords[kKinds] = {"key", "door", "area"};
  int kind = -1;
  for (int k = 0; k < kKinds && a.size() >= 2; ++k)
    if (a[1] == kWords[k])
      kind = k;
  if (kind < 0) {
    c.Print("usage: guide key|door|area");
    return;
  }
  if (GetFunctionMode() != GAME_MODE || !CanGuide()) {
    c.Print("guide: needs a single-player level with the Guide-Bot, outside Classic Mode");
    return;
  }
  static gb_menu menu; // large: kept off the stack
  gb_com ask;
  ask.action = COM_GET_MENU;
  ask.index = 0;
  ask.ptr = &menu;
  menu.num_commands = 0;
  AINotify(ObjGet(Buddy_handle[Player_num]), AIN_USER_DEFINED, &ask);
  if (!OutOfShip(menu)) {
    c.Print("guide: the Guide-Bot is in the ship; release it first (F4)");
    AddHUDMessage("Release the Guide-Bot first.");
    return;
  }
  Target targets[kKinds];
  PickAll(targets);
  if (!targets[kind].found) {
    const char *why = targets[kind].out_of_reach ? kOutOfReach : kNone[kind];
    c.Print(why);
    AddHUDMessage("%s", why);
    return;
  }
  c.Print(std::string(kLabels[kind]) + ": " + targets[kind].what + ". " + Away(targets[kind].cost));
  Send(targets[kind]);
}

} // namespace

void GuideBot_Init() {
  Console().AddCommand({"guide", "guide key|door|area",
                        "Send the Guide-Bot to the next key, a door you can now open, or an unexplored area",
                        CMD_NONE, [](const CommandArgs &a, ConsoleCore &c) { GuideCommand(a, c); }});
}

int GuideBot_RunMenu(const gb_menu &menu) {
  if (!Deck_Available())
    return kDeckStock;
  std::vector<std::string> rows, help;
  std::vector<int> stock_index; // per row: the stock command, or -1 - extra index
  int insert_at = 0;
  for (int i = 0; i < menu.num_commands; ++i)
    if (menu.command_id[i] <= kGbcLastGoal)
      insert_at = i + 1;

  std::vector<Target> extras;
  if (OutOfShip(menu) && CanGuide()) {
    Target targets[kKinds];
    PickAll(targets);
    for (int k = 0; k < kKinds; ++k) {
      if (!targets[k].found && !targets[k].out_of_reach)
        continue;
      extras.push_back(targets[k]);
      rows.emplace_back(kLabels[k]);
      help.push_back(targets[k].found ? targets[k].what + ". " + Away(targets[k].cost) : kOutOfReach);
      stock_index.push_back(-1 - static_cast<int>(extras.size() - 1));
    }
  }

  // The stock rows, with the Tartarus ones after the level goals.
  std::vector<std::string> all_rows, all_help;
  std::vector<int> all_index;
  for (int i = 0; i <= menu.num_commands; ++i) {
    if (i == insert_at) {
      all_rows.insert(all_rows.end(), rows.begin(), rows.end());
      all_help.insert(all_help.end(), help.begin(), help.end());
      all_index.insert(all_index.end(), stock_index.begin(), stock_index.end());
    }
    if (i < menu.num_commands) {
      all_rows.emplace_back(menu.command_text[i]);
      all_help.emplace_back();
      all_index.push_back(i);
    }
  }

  const int row = BuddyDeck_Pick(menu.title, all_rows, &all_help);
  if (row < 0 || row >= static_cast<int>(all_index.size()))
    return row; // -1 or kDeckStock
  if (all_index[row] >= 0)
    return all_index[row];
  const Target &chosen = extras[static_cast<size_t>(-1 - all_index[row])];
  if (chosen.found)
    Send(chosen);
  else
    AddHUDMessage("%s", kOutOfReach);
  return -1;
}

} // namespace tartarus
