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

// Controls screens on the deck. See deck_controls.h.

#include "deck_controls.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <functional>
#include <string>
#include <utility>
#include <vector>

#include "CtlCfgElem.h"
#include "D3ForceFeedback.h"
#include "controls.h"
#include "ctlconfig.h"
#include "ddio.h"
#include "deck.h"
#include "deck_screens.h"
#include "descent.h"
#include "mmItem.h"
#include "forcefeedback.h"
#include "newui.h"
#include "pilot.h"
#include "stringtable.h"
#include "weapon.h"

extern float Key_ramp_speed; // Controls.cpp

namespace tartarus {

namespace {

enum class Page { Keyboard, Joystick, Weapons, Exit };

ControlsDeckAssign g_assign = nullptr;

struct Binding {
  int16_t fn = -1;
  ct_type type[CTLBINDS_PER_FUNC];
  ct_config_data value;
  uint8_t flags[CTLBINDS_PER_FUNC];
};

struct Table {
  const t_cfg_element *elements;
  int count;
  bool keyboard;
  std::vector<Binding> saved; // for "Restore settings"
};

Binding Read(int16_t fn) {
  Binding b;
  b.fn = fn;
  Controller->get_controller_function(fn, b.type, &b.value, b.flags);
  return b;
}

std::string SlotText(const Binding &b, const tCfgDataParts &parts, int slot) {
  const uint8_t ctrl = slot == 0 ? parts.ctrl_0 : parts.ctrl_1;
  const uint8_t bind = slot == 0 ? parts.bind_0 : parts.bind_1;
  const char *text = ctrl == NULL_CONTROLLER ? nullptr : cfg_binding_text(b.type[slot], ctrl, bind);
  return text != nullptr ? text : "";
}

std::string BothSlots(int16_t fn) {
  const Binding b = Read(fn);
  tCfgDataParts parts;
  parse_config_data(&parts, b.type[0], b.type[1], b.value);
  const std::string first = SlotText(b, parts, 0), second = SlotText(b, parts, 1);
  if (first.empty() || second.empty())
    return first + second;
  return first + "  " + second;
}

// The "press something" loop, as cfg_element_ui::DoUI() in CtlCfgElem.cpp.
struct Capture {
  ct_type type;
  uint8_t ctrl;
  uint8_t element;
  bool caught = false;
  bool press = false; // a button went down: wait a frame, as stock
};

bool ValidValue(ct_config_data data) { return CONTROLLER_CTL1_INFO(CONTROLLER_INFO(data)) != (int8_t)NULL_CONTROLLER; }

void TakeValue(Capture *c, ct_type type, ct_config_data data) {
  c->type = type;
  c->element = CONTROLLER_CTL1_VALUE(CONTROLLER_VALUE(data));
  c->ctrl = CONTROLLER_CTL1_INFO(CONTROLLER_INFO(data));
}

bool PollCapture(int key, void *data) {
  auto *c = static_cast<Capture *>(data);
  if (key == KEY_ESC)
    return true;
  if (c->type == ctKey) {
    if (key > 0 && cfg_binding_text(ctKey, 0, static_cast<uint8_t>(key & 0xff)) != nullptr) {
      c->element = static_cast<uint8_t>(key & 0xff);
      if (c->ctrl == NULL_CONTROLLER)
        c->ctrl = 0; // a cleared slot
      c->caught = true;
      return true;
    }
    return false;
  }
  Controller->poll();
  if (c->type == ctAxis || c->type == ctMouseAxis) {
    for (ct_type type : {ctAxis, ctMouseAxis}) {
      const ct_config_data value = Controller->get_controller_value(type);
      if (ValidValue(value)) {
        TakeValue(c, type, value);
        c->caught = true;
        return true;
      }
    }
    return false;
  }
  if (c->press) {
    c->caught = true;
    return true;
  }
  for (ct_type type : {ctMouseButton, ctPOV, ctPOV2, ctPOV3, ctPOV4, ctButton}) {
    const ct_config_data value = Controller->get_controller_value(type);
    if (ValidValue(value)) {
      TakeValue(c, type, value);
      c->press = true;
      break;
    }
  }
  return false;
}

void CaptureBinding(const std::string &title, int16_t fn, int slot, ct_type type, uint8_t ctrl, uint8_t element) {
  Capture capture{type, ctrl, element};
  DeckListSpec spec;
  spec.title = title;
  spec.items.resize(1);
  switch (type) {
  case ctKey:
    spec.items[0].label = "Press a key";
    break;
  case ctAxis:
  case ctMouseAxis:
    spec.items[0].label = "Move an axis";
    break;
  default:
    spec.items[0].label = "Press a button";
    break;
  }
  spec.footer = "ESC CANCEL";

  Descent->delay(0.3f);
  ResumeControls();
  Control_poll_flag = false; // assignments must not reach the game frame
  Deck_RunPoll(spec, PollCapture, &capture);
  SuspendControls();
  ddio_MouseQueueFlush();
  if (capture.caught)
    g_assign(fn, capture.type, capture.ctrl, capture.element, static_cast<int8_t>(slot));
}

// One function's bindings.
void FunctionPage(const std::string &title, int16_t fn) {
  int selected = 0;
  for (;;) {
    Binding b = Read(fn);
    tCfgDataParts parts;
    parse_config_data(&parts, b.type[0], b.type[1], b.value);

    enum Action { kBind, kClear, kInvert };
    std::vector<std::pair<Action, int>> actions;
    DeckListSpec spec;
    spec.title = title;
    spec.selected = selected;
    for (int slot = 0; slot < CTLBINDS_PER_FUNC; ++slot) {
      const std::string text = SlotText(b, parts, slot);
      const std::string number = std::to_string(slot + 1);
      DeckListItem row;
      row.label = "Binding " + number;
      row.value = text.empty() ? "NONE" : text;
      row.help = "Enter, then press what you want to use.";
      row.enabled = b.type[slot] != ctNone;
      spec.items.push_back(row);
      actions.emplace_back(kBind, slot);
      if (!text.empty()) {
        spec.items.push_back({"Clear binding " + number, "", ""});
        actions.emplace_back(kClear, slot);
        if (b.type[slot] == ctAxis || b.type[slot] == ctMouseAxis) {
          spec.items.push_back({"Invert " + number, (b.flags[slot] & CTFNF_INVERT) ? "[ON]" : "[OFF]", ""});
          actions.emplace_back(kInvert, slot);
        }
      }
    }
    selected = Deck_RunList(spec);
    if (selected < 0)
      return;
    const auto [action, slot] = actions[static_cast<size_t>(selected)];
    if (action == kBind) {
      CaptureBinding(title, fn, slot, b.type[slot], slot == 0 ? parts.ctrl_0 : parts.ctrl_1,
                     slot == 0 ? parts.bind_0 : parts.bind_1);
      continue;
    }
    if (action == kClear) {
      (slot == 0 ? parts.bind_0 : parts.bind_1) = NULL_BINDING;
      (slot == 0 ? parts.ctrl_0 : parts.ctrl_1) = NULL_CONTROLLER;
      selected = 0;
    } else {
      b.flags[slot] ^= CTFNF_INVERT;
    }
    Controller->set_controller_function(fn, b.type, unify_config_data(&parts), b.flags);
  }
}

// Bindings for the keyboard or the joystick and mouse. Returns the next page.
// The Adjust settings screens, in place of the stock key_settings_dialog() and
// joystick_settings_dialog(): the same settings, changed on a copy and kept only
// on Accept, as the stock OK and Cancel.
struct Setting {
  std::string label;
  std::function<std::string()> value; // empty for a heading
  std::function<void(int)> change;    // -1 or 1 from Left/Right; 0 from Enter or a click, which wraps
  std::string help;
};

/// True if the player chose Accept.
bool RunSettings(const std::string &title, const std::vector<Setting> &rows) {
  int selected = 1; // the first setting, past any heading
  while (selected <= static_cast<int>(rows.size()) && !rows[static_cast<size_t>(selected - 1)].change)
    ++selected;
  for (;;) {
    DeckListSpec spec;
    spec.title = title;
    spec.compact = true;
    spec.selected = selected;
    spec.footer = "LEFT/RIGHT CHANGE  ESC CANCEL";
    int dir = 0;
    spec.adjust = &dir;
    spec.items.push_back({"Accept", "", "Keeps these settings. Escape leaves them as they were."});
    for (const Setting &row : rows) {
      DeckListItem item;
      item.label = row.change ? row.label : "- " + row.label + " -";
      item.value = row.value ? row.value() : "";
      item.help = row.help;
      item.enabled = static_cast<bool>(row.change);
      spec.items.push_back(item);
    }
    selected = Deck_RunList(spec);
    if (selected < 0)
      return false;
    if (selected == 0) {
      if (dir == 0)
        return true;
      continue;
    }
    rows[static_cast<size_t>(selected - 1)].change(dir);
  }
}

/// A value on a grid of step from 0 to max: Left/Right stop at the ends, Enter wraps.
float Step(float value, int dir, float max, float step) {
  const int top = static_cast<int>(std::lround(max / step));
  int n = static_cast<int>(std::lround(value / step));
  n = dir == 0 ? (n >= top ? 0 : n + 1) : std::clamp(n + dir, 0, top);
  return n * step;
}

std::string Fixed(float value, const char *unit = "") {
  char text[32];
  std::snprintf(text, sizeof(text), "%.2f%s", value, unit);
  return text;
}

Setting Toggle(const std::string &label, bool *on, const std::string &help) {
  return {label, [on] { return std::string(*on ? "ON" : "OFF"); }, [on](int) { *on = !*on; }, help};
}

Setting Heading(const std::string &label) { return {label, nullptr, nullptr, ""}; }

void KeySettingsPage() {
  float ramp = Key_ramp_speed;
  const std::vector<Setting> rows = {
      {"Key ramping time", [&ramp] { return Fixed(ramp, " s"); },
       [&ramp](int dir) { ramp = Step(ramp, dir, 1.0f, 0.05f); },
       "How long a turn key takes to reach full speed. 0 turns at full speed at once."}};
  if (RunSettings("Keyboard settings", rows))
    Key_ramp_speed = ramp;
}

void JoystickSettingsPage() {
  const char axis_name[N_JOY_AXIS] = {'X', 'Y', 'Z', 'R', 'U', 'V'};
  float joy[N_JOY_AXIS], mouse[N_MOUSE_AXIS];
  for (int i = 0; i < N_JOY_AXIS; ++i)
    joy[i] = Controller->get_axis_sensitivity(ctAxis, i + 1);
  for (int i = 0; i < N_MOUSE_AXIS; ++i)
    mouse[i] = Controller->get_axis_sensitivity(ctMouseAxis, i + 1);
  bool joy_on = (Current_pilot.read_controller & READF_JOY) != 0;
  bool mouse_on = (Current_pilot.read_controller & READF_MOUSE) != 0;
  bool mouselook = Current_pilot.mouselook_control;
  bool ff_found = false;
  ddio_ff_GetInfo(&ff_found, nullptr);
  const bool ff_centre_supported = ff_found && ddio_ffjoy_SupportAutoCenter(kJoy1);
  bool ff_on = ff_found && ForceIsEnabled();
  bool ff_centre = ff_centre_supported && ForceIsAutoCenter();
  float ff_gain = ff_found ? ForceGetGain() : 0.0f;

  const std::string mouse_note = " Only for the Classic mouse mode: the virtual joystick has its own settings on the "
                                 "options deck's Controls page.";
  auto axis = [&](float *value, char name, float max, const std::string &help) {
    char label[32];
    std::snprintf(label, sizeof(label), TXT_CONTAXIS, name);
    return Setting{label, [value] { return Fixed(*value); },
                   [value, max](int dir) { *value = Step(*value, dir, max, 0.05f); }, help};
  };
  std::vector<Setting> rows;
  rows.push_back(Heading(TXT_SENSITIVITY));
  for (int i = 0; i < N_JOY_AXIS; ++i)
    rows.push_back(axis(&joy[i], axis_name[i], JOY_AXIS_SENS_RANGE, "Joystick axis sensitivity, 0 to 4; 1 is normal."));
  rows.push_back(Heading(TXT_MSESENS));
  for (int i = 0; i < N_MOUSE_AXIS; ++i)
    rows.push_back(axis(&mouse[i], axis_name[i], MSE_AXIS_SENS_RANGE,
                        "Mouse axis sensitivity, 0 to 4; 1 is normal." + mouse_note));
  rows.push_back(Heading(TXT_CONTROL_TOGGLES));
  rows.push_back(Toggle(TXT_JOYENABLED, &joy_on, "Read the joystick."));
  rows.push_back(Toggle(TXT_CFG_MOUSEENABLED, &mouse_on, "Read the mouse."));
  rows.push_back({TXT_MOUSECONTROL,
                  [&mouselook] { return std::string(mouselook ? TXT_MOUSELOOK : TXT_MOUSEFLIGHTSIM); },
                  [&mouselook](int) { mouselook = !mouselook; },
                  "Flight sim: the mouse turns the ship like a joystick axis. Mouselook: it aims the ship directly." +
                      mouse_note});
  if (ff_found) {
    rows.push_back(Heading(TXT_TITLE_FORCEFEEDBACK));
    rows.push_back(Toggle(TXT_CFG_FFENABLED, &ff_on, "Force feedback effects on the joystick."));
    if (ff_centre_supported)
      rows.push_back(Toggle(TXT_CFG_FFAUTOCENTER, &ff_centre, "The joystick pulls itself back to centre."));
    rows.push_back({TXT_CFG_FORCEGAIN,
                    [&ff_gain] { return std::to_string(static_cast<int>(std::lround(ff_gain * 100.0f))) + "%"; },
                    [&ff_gain](int dir) { ff_gain = Step(ff_gain, dir, 1.0f, 0.05f); }, "Strength of the effects."});
  }
  if (!RunSettings(TXT_JOYMOUSESETTINGS, rows))
    return;
  for (int i = 0; i < N_JOY_AXIS; ++i)
    Controller->set_axis_sensitivity(ctAxis, i + 1, joy[i]);
  for (int i = 0; i < N_MOUSE_AXIS; ++i)
    Controller->set_axis_sensitivity(ctMouseAxis, i + 1, mouse[i]);
  Current_pilot.read_controller = (joy_on ? READF_JOY : 0) | (mouse_on ? READF_MOUSE : 0);
  Current_pilot.mouselook_control = mouselook;
  if (ff_found) {
    ff_on ? ForceEnable() : ForceDisable();
    if (ff_centre_supported)
      ff_centre ? ForceEnableAutoCenter() : ForceDisableAutoCenter();
    ForceSetGain(ff_gain); // 0 to 1; the stock slider gets this wrong (BUG-014)
  }
}

Page BindingsPage(Table *table, Page other) {
  enum Top { kReset, kAdjust, kRestore, kOther, kWeapons, kFirst };
  const char *other_name = table->keyboard ? "Joystick and mouse" : "Keyboard";
  int selected = kFirst + 1; // kFirst is a heading
  for (;;) {
    DeckListSpec spec;
    spec.title = table->keyboard ? "Keyboard" : "Joystick and mouse";
    spec.compact = true;
    spec.selected = selected;
    spec.items = {{TXT_RESETTODEFAULT, "", "Every binding on this page goes back to its default."},
                  {TXT_ADJUSTSETTINGS, "",
                   table->keyboard ? "Key ramping time."
                                   : "Axis sensitivity, which devices to read, and the mouse control style."},
                  {TXT_REVERTCONTROLS, "", "Undoes everything since you opened this screen."},
                  {other_name, "", "Go to this page."},
                  {"Weapon order", "", "Go to this page."}};
    for (int i = 0; i < table->count; ++i) {
      const t_cfg_element &e = table->elements[i];
      DeckListItem row;
      row.label = e.fn_id == -1 ? std::string("- ") + TXT(e.text) + " -" : TXT(e.text);
      row.enabled = e.fn_id != -1;
      if (row.enabled) {
        row.value = BothSlots(e.fn_id);
        row.help = "Enter to change this binding.";
      }
      spec.items.push_back(row);
    }
    selected = Deck_RunList(spec);
    switch (selected) {
    case -1:
      return Page::Exit;
    case kReset:
      if (Deck_Confirm(TXT_RESETTODEFAULT, "Reset every binding on this page?")) {
        for (int i = 0; i < NUM_CONTROLLER_FUNCTIONS; ++i) {
          if ((Controller_needs[i].ctype[0] == ctKey) == table->keyboard)
            Controller->assign_function(&Controller_needs[i]);
        }
      }
      break;
    case kAdjust:
      if (table->keyboard)
        KeySettingsPage();
      else
        JoystickSettingsPage();
      break;
    case kRestore:
      for (Binding &b : table->saved)
        Controller->set_controller_function(b.fn, b.type, b.value, b.flags);
      break;
    case kOther:
      return other;
    case kWeapons:
      return Page::Weapons;
    default: {
      const t_cfg_element &e = table->elements[selected - kFirst];
      FunctionPage(TXT(e.text), e.fn_id);
      break;
    }
    }
  }
}

uint16_t WeaponAt(bool secondary, int slot) {
  return secondary ? GetAutoSelectSecondaryWpnIdx(slot) : GetAutoSelectPrimaryWpnIdx(slot);
}

void SetWeaponAt(bool secondary, int slot, uint16_t value) {
  if (secondary)
    SetAutoSelectSecondaryWpnIdx(slot, value);
  else
    SetAutoSelectPrimaryWpnIdx(slot, value);
}

std::string WeaponName(uint16_t value) { return TXT(Static_weapon_names_msg[value & ~WPNSEL_SKIP]); }

// Follows the weapon as it moves. Higher slots have higher priority.
void WeaponEntry(bool secondary, int slot, int last_slot) {
  int selected = 0;
  for (;;) {
    const uint16_t value = WeaponAt(secondary, slot);
    DeckListSpec spec;
    spec.title = WeaponName(value);
    spec.selected = selected;
    spec.items = {{"Higher priority", "", ""},
                  {"Lower priority", "", ""},
                  {"Auto-select", (value & WPNSEL_SKIP) ? "[OFF]" : "[ON]", "Off: never switch to this weapon automatically."}};
    spec.items[0].enabled = slot < last_slot;
    spec.items[1].enabled = slot > 0;
    selected = Deck_RunList(spec);
    if (selected < 0)
      return;
    if (selected == 2) {
      SetWeaponAt(secondary, slot, value ^ WPNSEL_SKIP);
      continue;
    }
    const int next = slot + (selected == 0 ? 1 : -1);
    SetWeaponAt(secondary, slot, WeaponAt(secondary, next));
    SetWeaponAt(secondary, next, value);
    slot = next;
  }
}

Page WeaponsPage() {
  struct Row {
    bool secondary;
    int slot, last;
  };
  int selected = 4; // 3 is a heading
  for (;;) {
    DeckListSpec spec;
    spec.title = "Weapon order";
    spec.compact = true;
    spec.selected = selected;
    spec.items = {{TXT_RESETTODEFAULT, "", "Both lists go back to their defaults."},
                  {"Keyboard", "", "Go to this page."},
                  {"Joystick and mouse", "", "Go to this page."}};
    std::vector<Row> rows(3);
    for (bool secondary : {false, true}) {
      DeckListItem heading;
      heading.label = std::string("- ") + (secondary ? TXT_SECONDARY : TXT_PRIMARY) + " -";
      heading.enabled = false;
      spec.items.push_back(heading);
      rows.push_back({});
      int last = -1;
      for (int i = 0; i < MAX_PRIMARY_WEAPONS; ++i) {
        if (WeaponAt(secondary, i) != WPNSEL_INVALID)
          last = i;
      }
      for (int i = last; i >= 0; --i) { // highest priority first
        const uint16_t value = WeaponAt(secondary, i);
        if (value == WPNSEL_INVALID)
          continue;
        spec.items.push_back({WeaponName(value), (value & WPNSEL_SKIP) ? "[SKIP]" : "", "Highest priority at the top."});
        rows.push_back({secondary, i, last});
      }
    }
    selected = Deck_RunList(spec);
    if (selected == -1)
      return Page::Exit;
    if (selected == 0) {
      if (Deck_Confirm(TXT_RESETTODEFAULT, "Reset both weapon lists?")) {
        for (int i = 0; i < MAX_PRIMARY_WEAPONS; ++i)
          SetAutoSelectPrimaryWpnIdx(i, DefaultPrimarySelectList[i]);
        for (int i = 0; i < MAX_SECONDARY_WEAPONS; ++i)
          SetAutoSelectSecondaryWpnIdx(i, DefaultSecondarySelectList[i]);
      }
    } else if (selected == 1) {
      return Page::Keyboard;
    } else if (selected == 2) {
      return Page::Joystick;
    } else {
      const Row &row = rows[static_cast<size_t>(selected)];
      WeaponEntry(row.secondary, row.slot, row.last);
    }
  }
}

std::vector<Binding> Snapshot(const t_cfg_element *elements, int count) {
  std::vector<Binding> saved;
  for (int i = 0; i < count; ++i) {
    if (elements[i].fn_id != -1)
      saved.push_back(Read(elements[i].fn_id));
  }
  return saved;
}

} // namespace

bool ControlsDeck_Run(int mode, void (*ui_callback)(), ControlsDeckAssign assign, const t_cfg_element *keys, int n_keys,
                      const t_cfg_element *joys, int n_joys) {
  if (!Deck_Available())
    return false;
  g_assign = assign;
  if (GetUICallback() != MenuScene)
    SetUICallback(ui_callback);
  Table key_table{keys, n_keys, true, Snapshot(keys, n_keys)};
  Table joy_table{joys, n_joys, false, Snapshot(joys, n_joys)};
  Page page = mode == CTLCONFIG_WPNSEL ? Page::Weapons : mode == CTLCONFIG_CONTROLLER ? Page::Joystick : Page::Keyboard;
  while (page != Page::Exit) {
    if (page == Page::Keyboard)
      page = BindingsPage(&key_table, Page::Joystick);
    else if (page == Page::Joystick)
      page = BindingsPage(&joy_table, Page::Keyboard);
    else
      page = WeaponsPage();
  }
  return true;
}

} // namespace tartarus
