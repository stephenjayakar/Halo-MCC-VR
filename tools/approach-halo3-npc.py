"""Approach one live Halo 3 NPC through addressed keyboard messages.

Diagnostic only: read-only object snapshots and ordinary movement input, no
process-memory writes. Requires an explicitly observed object table address.
Learns the current W/D movement directions instead of assuming camera yaw.
"""
import argparse
import json
import math
from pathlib import Path
import subprocess
import sys
import time

ROOT = Path(__file__).resolve().parent

def snapshot(table):
    result = subprocess.run([sys.executable, str(ROOT / "probe-h3-objects.py"), "--table", table],
                            capture_output=True, text=True, check=True)
    return json.loads(result.stdout)

def move(key, milliseconds):
    subprocess.run(["pwsh", "-NoProfile", "-File", str(ROOT / "send-mcc-keys.ps1"),
                    "-Keys", key, "-Background", "-HoldMilliseconds", str(milliseconds)], check=True)

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--table", required=True)
    parser.add_argument("--target", help="Explicit live handle; otherwise nearest living NPC")
    parser.add_argument("--steps", type=int, default=16)
    args = parser.parse_args()
    if not 1 <= args.steps <= 24:
        raise ValueError("steps must be 1..24")
    initial = snapshot(args.table)
    pid = initial["pid"]
    players = initial["local_player_units"]
    if len(players) != 1:
        raise ValueError("Ambiguous local player")
    player_handle = players[0]
    def objects(state):
        if state["pid"] != pid or state["local_player_units"] != players:
            raise ValueError("Game or player changed")
        return {obj["handle"]: obj for obj in state["objects"]}
    bank = objects(initial)
    player = bank[player_handle]
    def distance(a, b):
        return math.dist(a["position"], b["position"])
    eligible = [o for o in bank.values() if o["handle"] != player_handle and
                o["health"] > 0 and not o["damage_dead"] and o.get("node_byte_size", 0) > 0 and
                distance(o, player) < 30]
    target = args.target.lower() if args.target else min(eligible, key=lambda o: distance(o, player))["handle"]
    if target not in {o["handle"] for o in eligible}:
        raise ValueError("Target is not a nearby living NPC")
    print(json.dumps({"pid": pid, "target": target, "distance_world_units": distance(bank[target], player)}), flush=True)
    basis = []
    for key in ("W", "D"):
        before = objects(snapshot(args.table))[player_handle]["position"]
        move(key, 450)
        after = objects(snapshot(args.table))[player_handle]["position"]
        vector = [(after[i] - before[i]) / 0.45 for i in (0, 1)]
        if math.hypot(*vector) < 0.25:
            raise ValueError("Movement calibration blocked; inspect the scene")
        basis.append(vector)
    forward, right = basis
    determinant = forward[0] * right[1] - right[0] * forward[1]
    if abs(determinant) < 0.1:
        raise ValueError("Movement axes did not resolve")
    stalled = 0
    for step in range(args.steps):
        bank = objects(snapshot(args.table))
        if target not in bank or bank[target]["health"] <= 0:
            raise ValueError("Target disappeared or died")
        player, npc = bank[player_handle], bank[target]
        remaining = distance(player, npc)
        print(json.dumps({"step": step, "target": target, "distance_world_units": remaining}), flush=True)
        if remaining <= 1.0:
            return
        delta = [npc["position"][i] - player["position"][i] for i in (0, 1)]
        f = (delta[0] * right[1] - right[0] * delta[1]) / determinant
        r = (forward[0] * delta[1] - delta[0] * forward[1]) / determinant
        duration, key = (abs(f), "W" if f > 0 else "S") if abs(f) >= abs(r) else (abs(r), "D" if r > 0 else "A")
        move(key, int(max(100, min(1500, duration * 750))))
        after = objects(snapshot(args.table))[player_handle]
        stalled = stalled + 1 if distance(after, player) < 0.05 else 0
        if stalled >= 2:
            raise ValueError("Path blocked; inspect the scene")
    raise ValueError("Approach step limit reached")

if __name__ == "__main__":
    main()
