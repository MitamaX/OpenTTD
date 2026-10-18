#!/usr/bin/env python3
import argparse
import fcntl
import json
import math
import os
import re
import shutil
import signal
import subprocess
import sys
import time
from pathlib import Path

VERIFY = Path(__file__).resolve().parent
REPO = VERIFY.parent
RUN = VERIFY / "run"
SHARED_RUN_DIRECTORIES = ("baseset", "game", "mini_art")
CRASH_LOG_PATTERN = "crash*.json.log"
EXE_NAME = "openttd"

DEFAULT_WAIT_FRAMES = 60
BASE_TIMEOUT_SEC = 240
TIMEOUT_SEC_PER_SHOT = 3
TIMEOUT_SEC_PER_FRAME = 1.5
DEFAULT_SPEED = 1
DEFAULT_STEP = 16
DEFAULT_SLOT_COUNT = 4
DEFAULT_STALL_SEC = 300
SHADER_FAILURE_MARK = "shader does not compile"
POLL_SEC = 0.5
KILL_GRACE_SEC = 5
LOG_TAIL_LINES = 25
FALLBACK_RESOLUTION = (1600, 900)
XVFB_DEPTH = 24
XVFB_START_TIMEOUT_SEC = 10


def parse_args():
    parser = argparse.ArgumentParser(description="Capture mini UI shots on a virtual X display.")
    parser.add_argument("-Label", "--label", default="sample")
    parser.add_argument("-ShotList", "--shot-list", default="")
    parser.add_argument("-Save", "--save", default="mini3d_pv.sav")
    parser.add_argument("-Config", "--config", default="openttd.cfg")
    parser.add_argument("-Seed", "--seed", type=int, default=-1)
    parser.add_argument("-Speed", "--speed", type=int, default=DEFAULT_SPEED, help="negative leaves the scene clock free")
    parser.add_argument("-Step", "--step", type=int, default=DEFAULT_STEP, help="negative leaves the scene clock free")
    parser.add_argument("-TimeoutSec", "--timeout-sec", type=int)
    parser.add_argument("-StallSec", "--stall-sec", type=int, default=DEFAULT_STALL_SEC)
    parser.add_argument("-DebugLevels", "--debug-levels", default="3")
    parser.add_argument("-Build", "--build", default="", help="build folder or openttd executable")
    parser.add_argument("-Hud", "--hud", action="store_true")
    parser.add_argument("-Slots", "--slots", type=int, default=int(os.environ.get("OTTD_MINI_SLOTS", DEFAULT_SLOT_COUNT)))
    parser.add_argument("-RenderThreads", "--render-threads", type=int, help="llvmpipe threads, LP_NUM_THREADS")
    return parser.parse_args()


def resolve_exe(build):
    path = Path(build).resolve() if build else REPO / "build"
    exe = path / EXE_NAME if path.is_dir() else path
    if not os.access(exe, os.X_OK):
        sys.exit(f"no {EXE_NAME} executable at {exe}")
    return exe


def measure_shot_list(path):
    wait, pngs, frames = DEFAULT_WAIT_FRAMES, 0, 0
    for line in path.read_text().splitlines():
        fields = line.split()
        if not fields or fields[0].startswith("#"):
            continue
        if fields[0] == "wait":
            if len(fields) > 1:
                wait = max(1, int(fields[1]))
        elif fields[0] == "glide":
            frames += max(1, int(fields[1]))
        else:
            pngs += 1
            frames += wait
    return pngs, frames


def scaled_timeout_sec(pngs, frames):
    return math.ceil(BASE_TIMEOUT_SEC + pngs * TIMEOUT_SEC_PER_SHOT + frames * TIMEOUT_SEC_PER_FRAME)


def config_resolution(config):
    match = re.search(r"^resolution\s*=\s*(\d+)\s*,\s*(\d+)", config.read_text(), re.MULTILINE) if config.exists() else None
    return (int(match[1]), int(match[2])) if match else FALLBACK_RESOLUTION


def lock_slot(slot_count):
    announced = False
    while True:
        for index in range(slot_count):
            handle = open(RUN / f"slot{index}.lock", "w")
            try:
                fcntl.flock(handle, fcntl.LOCK_EX | fcntl.LOCK_NB)
                return index, handle
            except BlockingIOError:
                handle.close()
        if not announced:
            print(f"waiting for a free game slot ({slot_count} busy)", flush=True)
            announced = True
        time.sleep(POLL_SEC)


def initialize_slot(index):
    slot = RUN / f"slot{index}"
    slot.mkdir(exist_ok=True)
    for name in SHARED_RUN_DIRECTORIES:
        link, target = slot / name, RUN / name
        if target.exists() and not link.exists():
            link.symlink_to(target, target_is_directory=True)
    for config in RUN.glob("*.cfg"):
        shutil.copy2(config, slot)
    return slot


def crash_logs(slot):
    return {path.name for path in slot.glob(CRASH_LOG_PATTERN)}


def crash_reason(slot, known):
    fresh = sorted((p for p in slot.glob(CRASH_LOG_PATTERN) if p.name not in known), key=lambda p: p.stat().st_mtime)
    if not fresh:
        return None
    try:
        return json.loads(fresh[-1].read_text())["crash"]["reason"]
    except (ValueError, KeyError, OSError):
        return f"unreadable {fresh[-1].name}"


def start_xvfb(resolution):
    read_end, write_end = os.pipe()
    width, height = resolution
    xvfb = subprocess.Popen(
        ["Xvfb", "-displayfd", str(write_end), "-screen", "0", f"{width}x{height}x{XVFB_DEPTH}", "-nolisten", "tcp", "-noreset"],
        pass_fds=(write_end,), stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    os.close(write_end)
    deadline = time.monotonic() + XVFB_START_TIMEOUT_SEC
    number = b""
    with os.fdopen(read_end, "rb") as pipe:
        while not number.endswith(b"\n") and time.monotonic() < deadline:
            chunk = pipe.read(1)
            if not chunk:
                break
            number += chunk
    if not number.strip():
        xvfb.kill()
        sys.exit("Xvfb did not report a display")
    return xvfb, f":{number.decode().strip()}"


def game_environment(args, out, display):
    env = dict(os.environ)
    for name in ("OTTD_MINI_CAPTURE_CLEAN", "OTTD_MINI_CAPTURE_SPEED", "OTTD_MINI_CAPTURE_STEP"):
        env.pop(name, None)
    env.update(DISPLAY=display, SDL_AUDIODRIVER="dummy", SDL_VIDEODRIVER="x11",
               LIBGL_ALWAYS_SOFTWARE="1", GALLIUM_DRIVER="llvmpipe", OTTD_MINI_CAPTURE=str(out / "shots.txt"))
    if not args.hud:
        env["OTTD_MINI_CAPTURE_CLEAN"] = "1"
    if args.speed >= 0:
        env["OTTD_MINI_CAPTURE_SPEED"] = str(args.speed)
    if args.step >= 0:
        env["OTTD_MINI_CAPTURE_STEP"] = str(args.step)
    if args.render_threads:
        env["LP_NUM_THREADS"] = str(args.render_threads)
    return env


def png_count(out):
    return sum(1 for _ in out.glob("*.png"))


def tail(path):
    lines = path.read_text(errors="replace").splitlines() if path.exists() else []
    return "\n".join(lines[-LOG_TAIL_LINES:])


class Game:
    def __init__(self, command, env, log):
        self.log_handle = open(log, "wb")
        self.process = subprocess.Popen(command, cwd=RUN, env=env, stdout=subprocess.DEVNULL, stderr=self.log_handle, start_new_session=True)
        self.status = None
        self.usage = None

    def poll(self):
        if self.status is None:
            pid, status, usage = os.wait4(self.process.pid, os.WNOHANG)
            if pid:
                self.status, self.usage = status, usage
        return self.status is not None

    def stop(self):
        if self.status is None:
            os.killpg(self.process.pid, signal.SIGKILL)
            _, self.status, self.usage = os.wait4(self.process.pid, 0)
        self.log_handle.close()

    def exit_description(self):
        if os.WIFSIGNALED(self.status):
            return f"signal {signal.Signals(os.WTERMSIG(self.status)).name}"
        return f"exit {os.WEXITSTATUS(self.status)}"

    def succeeded(self):
        return os.WIFEXITED(self.status) and os.WEXITSTATUS(self.status) == 0


def watch(game, out, expected, timeout_sec, stall_sec, slot, known_crashes):
    started = last_progress = time.monotonic()
    last_count = 0
    while not game.poll():
        time.sleep(POLL_SEC)
        now = time.monotonic()
        count = png_count(out)
        if count != last_count:
            last_count, last_progress = count, now
        if crash_reason(slot, known_crashes):
            return "crash log written"
        if now - started > timeout_sec:
            return f"timeout {timeout_sec}s with {count} of {expected} shots"
        if expected > 0 and now - last_progress > stall_sec:
            return f"stalled {stall_sec}s with {count} of {expected} shots"
    return None


def main():
    args = parse_args()
    exe = resolve_exe(args.build)
    generating = args.seed >= 0
    shot_list = Path(args.shot_list) if args.shot_list else VERIFY / ("shots-idle.txt" if generating else "shots-main.txt")
    out = VERIFY / "shots" / args.label
    if out.exists():
        shutil.rmtree(out)
    out.mkdir(parents=True)
    shutil.copyfile(shot_list, out / "shots.txt")
    log = out / "game.log"

    pngs, frames = measure_shot_list(shot_list)
    expected = 0 if generating else pngs
    timeout_sec = args.timeout_sec or (BASE_TIMEOUT_SEC if generating else scaled_timeout_sec(pngs, frames))
    world = ["-G", str(args.seed), "-g"] if generating else ["-g", f"save/{args.save}"]

    index, lock = lock_slot(args.slots)
    xvfb = game = None
    try:
        slot = initialize_slot(index)
        known_crashes = crash_logs(slot)
        xvfb, display = start_xvfb(config_resolution(slot / args.config))
        command = [str(exe), "-c", str(slot / args.config), "-s", "null", "-m", "null", "-v", "sdl-opengl", "-d", args.debug_levels, *world]
        started = time.monotonic()
        game = Game(command, game_environment(args, out, display), log)
        failure = watch(game, out, expected, timeout_sec, args.stall_sec, slot, known_crashes)
        game.stop()
        seconds = time.monotonic() - started
        if failure is None and not game.succeeded():
            failure = game.exit_description()
        if failure is None:
            failure = next((line for line in log.read_text(errors="replace").splitlines() if SHADER_FAILURE_MARK in line), None)
        reason = crash_reason(slot, known_crashes)
        if reason:
            failure = "; ".join(part for part in (failure, f"crash: {reason}") if part)
    finally:
        if game is not None and game.status is None:
            game.stop()
        if xvfb is not None:
            xvfb.terminate()
            try:
                xvfb.wait(KILL_GRACE_SEC)
            except subprocess.TimeoutExpired:
                xvfb.kill()
                xvfb.wait()
        lock.close()

    count = png_count(out)
    cpu_seconds = game.usage.ru_utime + game.usage.ru_stime
    print(f"slot={index} display={display} {game.exit_description()} seconds={seconds:.0f} shots={count}/{expected} "
          f"cpu={cpu_seconds:.0f}s cores={cpu_seconds / max(seconds, 1e-9):.2f} maxrss={game.usage.ru_maxrss // 1024}MB")
    if failure:
        print(f"FAILED: {failure}")
        print(tail(log))
        return 1
    if count != expected:
        print(tail(log))
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
