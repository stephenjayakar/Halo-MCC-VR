"""Extract complete numerical contact captures into JSON and a standalone C++ replay.

No process access. Generated files are restricted to the repository out folder.
The replay distinguishes bare selected convexes from the triangle contact skin;
it does not claim to reconstruct unrecorded triangles or the entire engine step.
"""
import argparse
import hashlib
import json
import math
from pathlib import Path
import re


def numbers(text, count):
    values = [float(v) for v in text.split()]
    if len(values) != count or not all(math.isfinite(v) for v in values):
        raise ValueError("Invalid finite numeric capture")
    return values


def extract(text):
    records = {}
    for match in re.finditer(r"H3 contact replay id=(\d+) (.*)", text):
        ident, line = int(match[1]), match[2].strip()
        r = records.setdefault(ident, {"id": ident, "transforms": {}, "shapes": {}})
        if line.startswith("meta:"):
            r["meta"] = dict(re.findall(r"(\w+)=([^ ]+)", line))
        elif line.startswith("transform "):
            name, payload = line[10:].split(": ", 1)
            r["transforms"][name] = numbers(payload, 13)
        elif line.startswith("shape "):
            m = re.fullmatch(r"shape (weapon|target): count=(\d+) radius=(\S+)", line)
            if not m or not 0 < int(m[2]) <= 256:
                raise ValueError("Invalid shape header")
            r["shapes"][m[1]] = {"count": int(m[2]), "radius": numbers(m[3], 1)[0], "vertices": {}}
        elif line.startswith("vertex "):
            m = re.fullmatch(r"vertex (weapon|target) (\d+): (.*)", line)
            if not m:
                raise ValueError("Invalid vertex")
            r["shapes"][m[1]]["vertices"][int(m[2])] = numbers(m[3], 3)
        elif line == "complete":
            r["complete"] = True
    complete = []
    for r in records.values():
        if not r.get("complete"):
            continue
        if "meta" not in r or set(r["transforms"]) != {"previous", "intended", "target"} or set(r["shapes"]) != {"weapon", "target"}:
            raise ValueError("Incomplete record marked complete")
        for s in r["shapes"].values():
            if set(s["vertices"]) != set(range(s["count"])):
                raise ValueError("Missing or excess vertices")
            s["vertices"] = [s["vertices"][i] for i in range(s["count"])]
        complete.append(r)
    if not complete:
        raise ValueError("No complete contact captures")
    return complete


def literal(value):
    if not math.isfinite(value):
        raise ValueError("Nonfinite C++ value")
    return f"{value:.9e}f"


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("log", type=Path)
    parser.add_argument("--out", type=Path, required=True)
    args = parser.parse_args()
    repo = Path(__file__).resolve().parent.parent
    output = args.out.resolve()
    if not output.is_relative_to(repo / "out"):
        raise ValueError("Replay output must remain under repository out")
    raw = args.log.read_bytes()
    records = extract(raw.decode("utf-8-sig", errors="replace"))
    output.mkdir(parents=True, exist_ok=True)
    (output / "capture.json").write_text(json.dumps({"log_sha256": hashlib.sha256(raw).hexdigest(), "records": records}, indent=2))
    code = ['#include "physical_contact_logic.h"', '#include <cstdio>', 'int main() {']
    for r in records:
        code.append('{')
        for name, values in r["transforms"].items():
            code.append(f"PhysicalContactTransform {name};")
            for n, field in enumerate(("position", "forward", "left", "up")):
                code.append(f"{name}.{field} = {{" + ','.join(literal(v) for v in values[n*3:n*3+3]) + '};')
            code.append(f"{name}.scale = {literal(values[12])};")
        for name, shape in r["shapes"].items():
            code.extend([f"PhysicalContactConvexShape {name}Shape;", f"{name}Shape.vertexCount = {shape['count']};", f"{name}Shape.radius = {literal(shape['radius'])};"])
            for index, vertex in enumerate(shape["vertices"]):
                code.append(f"{name}Shape.vertices[{index}] = {{" + ','.join(literal(v) for v in vertex) + '};')
        code.append('for (int skin=0; skin<2; ++skin) { auto w=weaponShape;')
        world_scale = numbers(r["meta"]["worldScale"], 1)[0]
        code.append(f"if(skin && w.vertexCount==3) w.radius += {literal(.00125*world_scale)} / previous.scale;")
        code.append('auto hit=PhysicalContactSweepConvex(w,previous,intended,targetShape,target);')
        code.append(f'printf("id={r["id"]} skin=%d previousOverlap=%d intendedOverlap=%d hit=%d reliable=%d fraction=%.9g normal=(%.9g %.9g %.9g)\\n",skin,PhysicalContactConvexIntersect(w,previous,targetShape,target),PhysicalContactConvexIntersect(w,intended,targetShape,target),hit.hit,hit.normalReliable,hit.fraction,hit.normal.x,hit.normal.y,hit.normal.z);')
        code.extend(['}', '}'])
    code.append('}')
    (output / "replay.cpp").write_text('\n'.join(code))
    (output / "CMakeLists.txt").write_text('cmake_minimum_required(VERSION 3.20)\nproject(h3_contact_replay LANGUAGES CXX)\nadd_executable(replay replay.cpp)\ntarget_compile_features(replay PRIVATE cxx_std_20)\ntarget_include_directories(replay PRIVATE "' + (repo / 'src/common').as_posix() + '")\n')
    print(output)


if __name__ == "__main__":
    main()
