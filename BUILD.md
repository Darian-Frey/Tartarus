# Build

Tartarus builds exactly as upstream Descent 3 does; nothing here replaces upstream's `BUILD.md` (kept in the repo as `docs/UPSTREAM_BUILD.md` after the fork) — this is the Linux-focused path for the reference machine plus Tartarus-specific options. Re-check against upstream after each merge (AV-002).

## 1. Supported platforms

| Platform | Status | Toolchain |
|---|---|---|
| Linux x86-64, Linux Mint 22 / Ubuntu 24.04 (reference machine: ThinkPad P15 Gen 2i, NVIDIA T1200) | Primary, tested every phase | GCC 13+ or Clang 17+, CMake ≥ 3.20, Ninja |
| Linux x86-64, Debian testing / Fedora / Arch | Expected to work | as above |
| Linux ARM64 | Inherited from upstream cross-compile; untested here | `cmake/toolchains/linux-aarch64-gcc-toolchain.cmake` |
| Windows, macOS | Not broken on purpose; not tested | upstream presets `win`, `mac` |

Upstream CI (as of fork) builds on Ubuntu 26.04 with GCC 15 and CMake 3.30; older distros work but may need vcpkg for SDL3.

## 2. Dependencies

From upstream `vcpkg.json`: **SDL3 ≥ 3.2.6** (Linux features: x11, wayland, alsa), **zlib**, **glm**, **plog**, **gtest** (tests only), **cpp-httplib ≥ 0.23**, **libxcrypt ≥ 4.4.38** (Linux). Tartarus adds nothing in Phases 0–6. Phase 7 scripting (F-080) will add a Lua (or alternative) dependency — record it in `THIRD_PARTY.md` and D-003 review when it happens.

### 2a. Build tools (APT)

```sh
sudo apt update
sudo apt install -y --no-install-recommends git ninja-build cmake g++ ca-certificates curl zip unzip tar pkg-config
```

### 2b. Libraries — Option A: vcpkg (upstream's recommendation; needed where SDL3 isn't packaged)

```sh
git clone https://github.com/microsoft/vcpkg ~/vcpkg
~/vcpkg/bootstrap-vcpkg.sh
echo 'export VCPKG_ROOT="$HOME/vcpkg"' >> ~/.bashrc && source ~/.bashrc

# vcpkg builds SDL3 from source and needs the X11/Wayland/audio headers:
sudo apt install -y --no-install-recommends curl pkg-config autoconf autoconf-archive automake libtool libltdl-dev make \
  python3-jinja2 libx11-dev libxft-dev libxext-dev libwayland-dev libxkbcommon-dev libegl1-mesa-dev \
  libibus-1.0-dev libasound2-dev libpulse-dev libaudio-dev libjack-dev libsndio-dev libxcursor-dev \
  libxfixes-dev libxi-dev libxrandr-dev libxss-dev
```

### 2c. Libraries — Option B: system packages

Only if your distro ships SDL3 ≥ 3.2.6 (Mint 22 / Ubuntu 24.04 do **not** at time of writing; Debian testing, Fedora 41+ and Arch do):

```sh
sudo apt install -y libsdl3-dev zlib1g-dev libglm-dev libplog-dev libgtest-dev libcpp-httplib-dev libcrypt-dev
cmake --preset linux -DUSE_VCPKG=OFF
```

## 3. Build commands

```sh
# Tartarus fork (once it exists); the upstream engine it is forked from is
# https://github.com/DescentDevelopers/Descent3 — build steps are identical for both.
git clone --recurse-submodules https://github.com/Darian-Frey/tartarus
cd tartarus

# Release
cmake --preset linux
cmake --build --preset linux --config Release

# Debug (cheats pre-unlocked, asserts on, force clamps checked — see SPEC §4)
cmake --build --preset linux --config Debug

# Tests
cmake --preset linux -DBUILD_TESTING=ON
cmake --build --preset linux --config Debug
ctest --test-dir builds/linux/build --output-on-failure
```

Output: `builds/linux/build/{Release,Debug}/Descent3` plus `d3-linux.hog` (level scripts built by HogMaker) and `netgames/`.

### Useful CMake options (upstream)

| Option | Default | Note |
|---|---|---|
| `USE_VCPKG` | DEFAULT (on if `$VCPKG_ROOT` set) | OFF to use system libs |
| `BUILD_TESTING` | OFF | gtest |
| `ENABLE_LOGGER` | OFF | log to terminal (Tartarus console mirrors plog regardless) |
| `FATAL_GL_ERRORS` | OFF | handy when touching HUD rendering |
| `FORCE_COLORED_OUTPUT` | OFF | nicer Ninja output |
| `FORCE_PORTABLE_INSTALL` | ON | |
| `DEFAULT_ADDITIONAL_DIRS` | empty | bake in a read-only data dir (e.g. your Steam path) so no flags are needed |
| `BUILD_EDITOR` | OFF | Windows-only; leave off |

Tartarus-specific options (added in Phase 1): `TARTARUS_DEFAULT_CLASSIC` (ON) — first-run mode; `TARTARUS_DATA_DIR` — override location of `data/tartarus/`.

## 4. Running against your Descent 3 data

Tartarus never copies or modifies your game install and never ships game data. Two kinds of directory matter (upstream's base-directory model):

- **Writable base dir**: always SDL's preference path, `~/.local/share/Outrage Entertainment/Descent 3/` on Linux. Pilots, saves, the settings registry (`.Descent3Registry`) and `custom/` go here, and later `tartarus.cfg`, screenshots and `console_history.txt`. It is not configurable: upstream removed `-setdir` and `-useexedir` in October 2024 (commit `8800f1b4`), although upstream's `USAGE.md` still documents them (BUG-001). It is shared with any other open-source Descent 3 build on the machine.
- **Read-only base dirs** (`-additionaldir`, repeatable): your game install. Read-only dirs take precedence over the writable one.

```sh
# Steam installs (native, Flatpak or Snap, any library folder) are found automatically (F-003):
./builds/linux/build/Release/Descent3

# Anything else (GOG, CD, or to pick one of several Steam installs):
./builds/linux/build/Release/Descent3 -additionaldir "<path to your Descent 3 install>"
```

The Steam probe only runs if no other base directory already provides `d3.hog`, so `-additionaldir` always wins. If `d3.hog` still cannot be found, the game stops with an error listing every directory it searched. Install locations differ per machine and store; **do not commit a real path.** Developers keep theirs in the gitignored `CLAUDE.local.md` (see CLAUDE.md §Local environment).

### 4a. Required game files

The read-only base dir must contain the following (names are case-insensitive to the engine):

| Path | Required | Notes |
|---|---|---|
| `d3.hog` | **Yes** | Core data (~190 MB in the Steam build) |
| `extra.hog` | **Yes** | Steam/GOG 1.4 content; includes the Mercenary expansion data (~210 MB) |
| `extra1.hog`, `extra13.hog` | Yes | Patch content |
| `ppics.hog` | Yes | Pilot pictures |
| `missions/d3.mn3`, `missions/d3_2.mn3` | **Yes** | The campaign (two mission files) |
| `missions/d3voice1.hog`, `missions/d3voice2.hog` | **Yes** | Campaign speech |
| `missions/merc.mn3` | Yes for Mercenary | Expansion campaign |
| `missions/training.mn3` + other `*.mn3` | Optional | Training and multiplayer levels |
| `movies/*.mve` | Optional | Intro, ending and briefing cutscenes. Only `.mve` plays; the Linux Steam depot ships `.ogv`, which the open-source engine cannot decode yet — the game runs fine without them |
| `*.pld`, `*.ctl` | Optional | Stock pilot/controller profiles |

Not needed (ignored or replaced by the build): the stock `descent3`/`main.exe` binaries, `d3-linux.hog` (Tartarus builds its own), `netgames/`, `online/`, `demo/`, `savegame/`, `language/`, `.ico`/`.hlp`/`.cnt`/`.pdf` files, `dedicated.cfg`, `motd.txt`.

Quick self-check from the install dir:

```sh
for f in d3.hog extra.hog extra1.hog extra13.hog ppics.hog missions/d3.mn3 missions/d3_2.mn3 missions/d3voice1.hog missions/d3voice2.hog missions/merc.mn3; do
  [ -e "$f" ] && echo "ok   $f" || echo "MISSING $f"; done
ls movies/*.mve >/dev/null 2>&1 && echo "ok   movies (.mve)" || echo "note movies are not .mve — cutscenes will be skipped"
```

Steam users: upstream recommends the **Windows** depot for the `.mve` movies; on Linux you can get it by forcing a Proton compatibility tool on the game in Steam and verifying files, then still run the native Tartarus binary against that directory. On Wayland, `SDL_VIDEODRIVER=wayland` may be needed.

Other flags inherited from upstream: `-windowed`/`-fullscreen`, `-width`/`-height`, `-logfile` (writes `Descent3.log` to the current directory), `-loglevel <LEVEL>`. Tartarus adds `-classic` / `-tartarus` (SPEC §1), and for quicker testing `-skipbriefing` (no pre-level briefing or ship selection) and `-skipcinematics` (in-game cinematics such as level intros end as soon as a key could skip them). Upstream's `-nointro` skips the start-up movies; `-pilot <name> -mission d3 -loadlevel 1` goes straight into a level.

## 5. Cross-compilation

Unchanged from upstream: build `HogMaker` natively first, then configure with `-DCMAKE_TOOLCHAIN_FILE=… -DHogMaker_DIR=../build-native/`.

## 6. Troubleshooting

Seeded from design; append real failures as they occur (BUGS.md for code, here for build-only issues).

| Symptom | Cause / fix |
|---|---|
| CMake: "SDL3 not found" with `USE_VCPKG=OFF` | Distro SDL3 too old or absent → use vcpkg (2b) |
| §2b lists `libjack-dev`, but apt wants to remove `libjack-jackd2-dev` | `libjack-dev` is JACK1 and conflicts with the JACK2/PipeWire headers. If `libjack-jackd2-dev` is installed, skip `libjack-dev`; the headers are the same (seen on Mint 22.3, 2026-11-01) |
| vcpkg: `building libxcrypt:x64-linux failed`; `buildtrees/libxcrypt/aclocal-*-err.log` says "System package autoconf-archive is missing" | `sudo apt install autoconf-archive`. Upstream's CI installs it but its BUILD.md omits it (seen 2026-11-01) |
| vcpkg: `curl operation failed with error code 6 (Couldn't resolve host name)` | Transient DNS failure; re-run `cmake --preset linux`, which resumes where it stopped |
| vcpkg SDL3 build fails on `wayland-scanner` / `libdecor` | Missing `libwayland-dev libxkbcommon-dev libdecor-0-dev` |
| Game starts, black screen, log says missing `d3.hog` | Wrong `-additionaldir` — install dir names often contain a space, so quote the path; run the §4a self-check |
| Movies don't play | Linux Steam depot ships `.ogv`; only `.mve` plays (see §4a) — harmless otherwise |
| Intermittent 2–3 s hitches at launch on the ThinkPad under PRIME offload | NVIDIA XRandR provider probing; run with `__NV_PRIME_RENDER_OFFLOAD=1 __GLX_VENDOR_LIBRARY_NAME=nvidia` or on the iGPU; see tools notes |
| `` ` `` opens console but also types a backtick in text fields | Console key-swallow bug — AV-008; check the GameLoop intercept order |
| Everything builds, no Tartarus features visible | You're in Classic Mode (default). `classic 0` in the console or Options → Tartarus |
