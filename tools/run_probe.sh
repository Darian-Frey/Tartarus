#!/bin/sh
# Tartarus: run a scripted test in a silent, throwaway copy of the player's
# profile. The real profile is only read. Usage:
#   tools/run_probe.sh <work-dir> <probe.py> [extra game args...]
# Extra tartarus.cfg lines come from $CFG (newline-separated).
set -e
root=$(cd "$(dirname "$0")/.." && pwd)
work=$1; probe=$(realpath "$2"); shift 2
src="${XDG_DATA_HOME:-$HOME/.local/share}/Outrage Entertainment/Descent 3"
rm -rf "$work"; mkdir -p "$work/xdg/Outrage Entertainment"
work=$(realpath "$work")
cp -r "$src" "$work/xdg/Outrage Entertainment/"
p="$work/xdg/Outrage Entertainment/Descent 3"
rm -rf "$p/savegame"/*
sed -i 's/"SND_mastervol"="[0-9.]*"/"SND_mastervol"="0.000000"/; s/"MUS_mastervol"="[0-9.]*"/"MUS_mastervol"="0.000000"/;
        s/"RS_fullscreen"=dword:[0-9A-Fa-f]*/"RS_fullscreen"=dword:0/; s/"RS_width"=dword:[0-9A-Fa-f]*/"RS_width"=dword:500/;
        s/"RS_height"=dword:[0-9A-Fa-f]*/"RS_height"=dword:2D0/' "$p/.Descent3Registry"
printf 'set classic 0\n' > "$p/tartarus.cfg"
[ -n "$CFG" ] && printf '%s\n' "$CFG" >> "$p/tartarus.cfg"
cd "$work"
PYTHONPATH="$root/tools" XDG_DATA_HOME="$work/xdg" timeout 200 python3 "$probe" "$work" \
  "$root/builds/linux/build/Release/Descent3" -windowed -logfile -nosound -nomusic -pilot bob \
  -mission d3 -loadlevel 1 -skipbriefing -skipcinematics "$@" >/dev/null 2>&1 || true
tail -1 "$work/drive.log"
