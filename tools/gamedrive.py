# Tartarus, a fork of Descent 3 -- GPL-3.0-or-later, see LICENSE.
"""Scripted test runs: launch the game, find its window, send XTest input ONLY
while the game window has focus, take window shots, stop the game by PID.
Needs python-xlib and ImageMagick's `import`. Use through tools/run_probe.sh.

A probe is a Python file:
    from gamedrive import run_probe
    def body(g):
        g.console("where"); g.shot("view.png")
    run_probe(body)
"""
import os
import signal
import subprocess
import sys
import time

from Xlib import X, XK, display
from Xlib.ext import xtest


class FocusLost(Exception):
    pass


class Game:
    NAMES = {" ": ("space", False), "_": ("minus", True), ".": ("period", False), ";": ("semicolon", False),
             "-": ("minus", False)}

    def __init__(self, out, binary, args):
        self.out = out
        self.log = open(os.path.join(out, "drive.log"), "w")
        self.d = display.Display()
        self.root = self.d.screen().root
        self.proc = subprocess.Popen([binary, *args], cwd=out, stdout=open(os.path.join(out, "stdout.txt"), "w"),
                                     stderr=subprocess.STDOUT)
        self.win = None

    def say(self, *a):
        print(*a, file=self.log, flush=True)

    def find_window(self, timeout=60):
        def walk(w):
            try:
                name = w.get_wm_name()
                if name and "Descent 3" in str(name):
                    return w
                for c in w.query_tree().children:
                    r = walk(c)
                    if r:
                        return r
            except Exception:
                return None
            return None
        for _ in range(timeout):
            time.sleep(1)
            self.win = walk(self.root)
            if self.win:
                return True
        return False

    def focused(self):
        f = self.d.get_input_focus().focus
        if isinstance(f, int):
            return False

        def contains(win):
            return win.id == f.id or any(contains(c) for c in win.query_tree().children)
        try:
            return contains(self.win)
        except Exception:
            return False

    def _check(self, what):
        if not self.focused():
            raise FocusLost(what)

    def key(self, name, shift=False):
        self._check(name)
        kc = self.d.keysym_to_keycode(XK.string_to_keysym(name))
        sk = self.d.keysym_to_keycode(XK.string_to_keysym("Shift_L"))
        if shift:
            xtest.fake_input(self.d, X.KeyPress, sk)
        xtest.fake_input(self.d, X.KeyPress, kc)
        self.d.sync()
        time.sleep(0.04)
        xtest.fake_input(self.d, X.KeyRelease, kc)
        if shift:
            xtest.fake_input(self.d, X.KeyRelease, sk)
        self.d.sync()
        time.sleep(0.06)

    def type(self, text):
        for ch in text:
            name, shift = self.NAMES.get(ch, (ch, False))
            self.key(name, shift)

    def console(self, *lines):
        self.key("grave")
        time.sleep(0.5)
        for line in lines:
            self.type(line)
            self.key("Return")
            time.sleep(0.2)
        self.key("grave")
        time.sleep(0.6)

    def motion(self, n, dx, dy=0, gap=0.005):
        for _ in range(n):
            self._check("motion")
            xtest.fake_input(self.d, X.MotionNotify, True, x=dx, y=dy)
            self.d.sync()
            time.sleep(gap)

    def click(self, button):
        self._check("click")
        xtest.fake_input(self.d, X.ButtonPress, button)
        self.d.sync()
        time.sleep(0.12)
        xtest.fake_input(self.d, X.ButtonRelease, button)
        self.d.sync()

    def shot(self, name):
        subprocess.run(["import", "-window", hex(self.win.id), os.path.join(self.out, name)], check=False)
        self.say("shot", name)

    def close(self):
        self.proc.send_signal(signal.SIGTERM)
        try:
            self.proc.wait(5)
        except subprocess.TimeoutExpired:
            self.proc.kill()
        self.say("exit", self.proc.returncode)


def run_probe(body):
    """argv: <outdir> <binary> [args...]; env LEVEL_WAIT = seconds before body(game)."""
    out, binary, *args = sys.argv[1:]
    g = Game(out, binary, args)
    try:
        if not g.find_window():
            g.say("no window")
            sys.exit(1)
        time.sleep(int(os.environ.get("LEVEL_WAIT", "12")))
        body(g)
        g.say("done")
    except FocusLost as e:
        g.say("STOPPED: game window lost focus before", e)
    finally:
        g.close()
