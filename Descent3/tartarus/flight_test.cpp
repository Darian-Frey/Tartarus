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

#include "flight_test.h"

#include <algorithm>
#include <cstdio>
#include <string>
#include <vector>

#include "classic.h"
#include "console.h"
#include "controls.h"
#include "game.h"
#include "log.h"
#include "object.h"
#include "player.h"
#include "vecmat.h"

namespace tartarus {

namespace {

// Short: thrust, a turn, a pitch, then coasting to 3.5 s.
const std::vector<FlightPhase> kShort = {
    {0.0f, 1.0f, 1.0f, 0.0f, 0.0f}, // full thrust
    {1.0f, 1.5f, 1.0f, 1.0f, 0.0f}, // thrust and turn
    {1.5f, 2.0f, 0.0f, 0.0f, 0.5f}, // pitch, no thrust
};

// Long (the Phase 5 acceptance run): 60 s of gentle circling with the nose
// bobbing up and down every 4 s, slow enough to stay clear of the walls.
std::vector<FlightPhase> LongPhases() {
  std::vector<FlightPhase> phases;
  for (int i = 0; i < 15; ++i) {
    const float t = 4.0f * static_cast<float>(i);
    phases.push_back({t, t + 4.0f, 0.35f, 0.25f, (i % 2 == 0) ? 0.12f : -0.12f});
  }
  return phases;
}

std::vector<FlightPhase> g_phases = kShort;
float kEnd = 3.5f; // stop the input and report here (coasting after the last phase)

bool g_running = false;
bool g_waiting = false; // armed; starts on the next controls read
float g_start = 0.0f;
int g_frames = 0;
float g_min_dt = 1.0f, g_max_dt = 0.0f;

void Report(float elapsed) {
  const object &o = *Player_object;
  // Back to exactly kEnd along the current velocity, so runs compare at the same game time.
  const float over = elapsed - kEnd;
  const vector pos = o.pos - o.mtype.phys_info.velocity * over;
  angvec a;
  vm_ExtractAnglesFromMatrix(&a, const_cast<matrix *>(&o.orient));
  auto deg = [](angle v) { return static_cast<double>(v) * 360.0 / 65536.0; };
  const float speed = vm_GetMagnitude(const_cast<vector *>(&o.mtype.phys_info.velocity));
  char buf[256];
  std::snprintf(buf, sizeof(buf),
                "room %d  %d frames, %.0f fps (frame %.1f-%.1f ms)  pos %.2f %.2f %.2f  speed %.2f  heading %.2f pitch %.2f bank %.2f",
                o.roomnum, g_frames, g_frames / std::max(0.001f, elapsed), g_min_dt * 1000.0f, g_max_dt * 1000.0f, pos.x(),
                pos.y(), pos.z(), speed, deg(a.h()), deg(a.p()), deg(a.b()));
  LOG_INFO << "[tartarus] flighttest: " << buf;
  Console().Print(std::string("flighttest: ") + buf);
}

} // namespace

float PhaseShare(float t, float dt, float start, float end) {
  if (!(dt > 0.0f))
    return t >= start && t < end ? 1.0f : 0.0f;
  const float lo = std::max(t, start), hi = std::min(t + dt, end);
  return std::clamp((hi - lo) / dt, 0.0f, 1.0f);
}

void FlightTest_Init() {
  Console().AddCommand({"flighttest", "flighttest [long]",
                        "Fly a fixed manoeuvre timed by game time and log where it ends; long: the 60 s acceptance run (F-061)",
                        CMD_NONE, [](const CommandArgs &a, ConsoleCore &c) {
                          if (IsClassic() || (Game_mode & GM_MULTI) || Player_object == nullptr) {
                            c.Print("flighttest: needs a single-player level in Tartarus mode");
                            return;
                          }
                          const bool long_run = a.size() > 1 && a[1] == "long";
                          g_phases = long_run ? LongPhases() : kShort;
                          kEnd = long_run ? 60.0f : 3.5f;
                          g_waiting = true;
                          g_running = false;
                          c.Print("flighttest: close the console; it starts on the next frame");
                        }});
}

void FlightTest_Apply(game_controls *controls) {
  if ((!g_running && !g_waiting) || IsClassic() || Player_object == nullptr)
    return;
  if (g_waiting) {
    g_waiting = false;
    g_running = true;
    g_start = Gametime;
    g_frames = 0;
    g_min_dt = 1.0f;
    g_max_dt = 0.0f;
  }
  const float t = Gametime - g_start;
  if (t >= kEnd) {
    g_running = false;
    Report(t);
    return;
  }
  ++g_frames;
  g_min_dt = std::min(g_min_dt, Frametime);
  g_max_dt = std::max(g_max_dt, Frametime);
  float forward = 0.0f, heading = 0.0f, pitch = 0.0f;
  for (const FlightPhase &p : g_phases) {
    const float share = PhaseShare(t, Frametime, p.start, p.end);
    forward += p.forward * share;
    heading += p.heading * share;
    pitch += p.pitch * share;
  }
  controls->forward_thrust = forward;
  controls->heading_thrust = heading;
  controls->pitch_thrust = pitch;
  controls->bank_thrust = 0.0f;
  controls->sideways_thrust = 0.0f;
  controls->vertical_thrust = 0.0f;
  controls->afterburn_thrust = 0.0f;
}

} // namespace tartarus
