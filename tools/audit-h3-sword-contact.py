#!/usr/bin/env python3
"""Audit official H3EK sword exports offline; never opens or changes MCC.

Inputs are tool.exe export-tag-to-xml exports and export-render-model-mesh.
No generated geometry is installed. X-file joint indices refer to the matching
render-model node list; model-space points still need the correct runtime bind
transform before being usable as contact geometry.
"""
import argparse
from collections import Counter
import hashlib
import json
import math
from pathlib import Path
import struct
import xml.etree.ElementTree as ET

from directx_x import extract_meshes, read_objects


def xml(path):
    # Official exporter emits this literal, invalid XML attribute for pageable
    # resources. Repair only that sentinel, not arbitrary malformed content.
    text = path.read_text(encoding="utf-8", errors="replace")
    return ET.fromstring(text.replace('value="<unavailable>"',
                                     'value="&lt;unavailable&gt;"'))


def value(element, name):
    field = element.find(f"field[@name='{name}']")
    if field is None:
        raise ValueError(f"missing {name}")
    return field.attrib["value"]


def bounds(points):
    if not points or not all(math.isfinite(v) for p in points for v in p):
        raise ValueError("empty or non-finite geometry")
    return {"min": [min(p[a] for p in points) for a in range(3)],
            "max": [max(p[a] for p in points) for a in range(3)]}


def sword_bind_audit(render, vertices, joints):
    """Validate this sword's translation-only rest pose, not arbitrary skeletons.

    H3EK explicitly warns that inverse scale is displayed last instead of
    first. Preserve the serialized float order, then decode scale/basis/position.
    See HALO3-SWORD-CONTACT-EVIDENCE.md for the native consumer proof.
    """
    result, rest_positions = [], {}
    for node in render.findall("block[@name='nodes']/element"):
        index = int(node.attrib["index"])
        parent = int(value(node, "parent node").split(",")[-1])
        rotation = [float(x) for x in value(node, "default rotation").split(",")]
        translation = [float(x) for x in value(node, "default translation").split(",")]
        if (len(rotation) != 4 or len(translation) != 3
                or not all(math.isfinite(x) for x in rotation + translation)
                or any(abs(x) > 1e-6 for x in rotation[:3])
                or abs(abs(rotation[3]) - 1) > 1e-6):
            raise ValueError("bind audit requires the sword's identity rest rotations")
        if parent != -1 and parent not in rest_positions:
            raise ValueError("invalid or unordered sword parent node")
        origin = rest_positions.get(parent, [0, 0, 0])
        rest = [origin[k] + translation[k] for k in range(3)]
        rest_positions[index] = rest
        serialized = [float(x) for name in (
            "inverse forward", "inverse left", "inverse up", "inverse position",
            "inverse scale") for x in value(node, name).split(",")]
        if len(serialized) != 13 or not all(math.isfinite(x) for x in serialized):
            raise ValueError("invalid inverse matrix serialization")
        scale, basis, position = serialized[0], serialized[1:10], serialized[10:13]
        expected = [1, 0, 0, 0, 1, 0, 0, 0, 1]
        if abs(scale - 1) > 1e-6 or any(abs(a-b) > 1e-6 for a, b in zip(basis, expected)):
            raise ValueError("unexpected sword inverse scale or basis")
        points = [p for p, joint in zip(vertices, joints) if joint == index]
        local = [[p[k] + position[k] for k in range(3)] for p in points]
        error = max(abs(q[k] + rest[k] - p[k])
                    for p, q in zip(points, local) for k in range(3))
        if error > 1e-6:
            raise ValueError("authored rest pose does not cancel the inverse bind")
        result.append({"index": index, "parent": parent,
                       "inverse_scale": scale, "inverse_basis": basis,
                       "inverse_position": position, "rest_position": rest,
                       "node_local_bounds": bounds(local),
                       "rest_roundtrip_max_error": error,
                       "checked_vertices": len(points)})
    return result


def influences(mesh_object, vertex_count):
    decls = [o for o in mesh_object.walk() if o.type_name == "DeclData"]
    if len(decls) != 1:
        raise ValueError("expected one vertex declaration")
    records = decls[0].records
    if len(records) != 1 or records[0].kind != "INTEGER_LIST":
        raise ValueError("unsupported declaration encoding")
    words = records[0].value
    count = words[0]
    if count < 1 or count > 16 or len(words) < 2 + count * 4:
        raise ValueError("invalid declaration size")
    # D3D9 DeclData elements: type, method, usage, usage index. Only accept
    # the concrete float1/2/3/4 and UBYTE4 formats present in this export.
    widths = {0: 1, 1: 2, 2: 3, 3: 4, 5: 1}
    fields = {}
    stride = 0
    for n in range(count):
        typ, method, usage, usage_index = words[1+n*4:5+n*4]
        if typ not in widths or method != 0 or (usage, usage_index) in fields:
            raise ValueError("unsupported or duplicate declaration element")
        fields[usage, usage_index] = (typ, stride)
        stride += widths[typ]
    size = words[1 + count * 4]
    data = words[2 + count * 4:]
    if size != len(data) or size != vertex_count * stride:
        raise ValueError("vertex data size mismatch")
    index_type, index_offset = fields[2, 0]
    weight_type, weight_offset = fields[1, 0]
    if index_type != 5 or weight_type != 3:
        raise ValueError("expected UBYTE4 indices and FLOAT4 weights")
    result = []
    for n in range(vertex_count):
        row = data[n*stride:(n+1)*stride]
        indices = struct.pack("<I", row[index_offset])
        weights = struct.unpack("<4f", struct.pack(
            "<4I", *row[weight_offset:weight_offset+4]))
        if (not all(math.isfinite(w) and 0 <= w <= 1 for w in weights)
                or abs(sum(weights) - 1) > 1e-5):
            raise ValueError("invalid blend weights")
        active = tuple(indices[k] for k, w in enumerate(weights) if w > 1e-6)
        if len(active) != 1 or active[0] == 255:
            raise ValueError("audit requires a single rigid influence per vertex")
        result.append(active[0])
    return result


def variant_selection_audit(model, render):
    regions = {}
    for region in render.findall("block[@name='regions']/element"):
        regions[value(region, "name")] = {
            value(p, "name"): {"mesh_index": int(value(p, "mesh index")),
                               "mesh_count": int(value(p, "mesh count"))}
            for p in region.findall("block[@name='permutations']/element")}
    variants = []
    for variant in model.findall("block[@name='variants']/element"):
        selections = []
        for region in variant.findall("block[@name='regions']/element"):
            name = value(region, "region name")
            for p in region.findall("block[@name='permutations']/element"):
                requested = value(p, "permutation name")
                probability = float(value(p, "probability"))
                if not math.isfinite(probability) or probability < 0:
                    raise ValueError("invalid variant probability")
                selections.append({"region": name, "requested_permutation": requested,
                                   "probability": probability,
                                   "matching_render_permutation": regions.get(name, {}).get(requested)})
        variants.append({"name": value(variant, "name"), "selections": selections})
    return {"render_regions": regions, "variants": variants,
            "scope": "name correspondence only; missing names do not prove hidden geometry"}


def audit(directory, collision_path):
    paths = [directory / "energy_blade.model.xml",
             directory / "fp_energy_blade.render_model.xml",
             directory / "fp_energy_blade.mesh.x", collision_path]
    model, render, collision = xml(paths[0]), xml(paths[1]), xml(paths[3])
    roots = read_objects(paths[2])
    meshes = extract_meshes(roots)
    if len(roots) != 1 or roots[0].type_name != "Mesh" or len(meshes) != 1:
        raise ValueError("expected one exported mesh")
    mesh = meshes[0]
    joints = influences(roots[0], len(mesh.vertices))
    nodes = {int(n.attrib["index"]): value(n, "name")
             for n in render.findall("block[@name='nodes']/element")}
    if any(j not in nodes for j in joints):
        raise ValueError("export references a node absent from its render model")
    face_nodes = []
    for face in mesh.faces:
        if len(face) != 3 or any(i < 0 or i >= len(joints) for i in face):
            raise ValueError("invalid triangle")
        unique = {joints[i] for i in face}
        if len(unique) != 1:
            raise ValueError("triangle spans independently animated nodes")
        face_nodes.append(next(iter(unique)))
    counts = Counter(face_nodes)
    return {
        "schema": 3,
        "scope": "official H3EK exports only; not retail runtime coverage",
        "inputs": [{"path": str(p.resolve()), "sha256": hashlib.sha256(
            p.read_bytes()).hexdigest()} for p in paths],
        "model_variants": [value(e, "name") for e in
                           model.findall("block[@name='variants']/element")],
        "collision_regions": [value(e, "name") for e in
                              collision.findall("block[@name='regions']/element")],
        "render_regions": [value(e, "name") for e in
                           render.findall("block[@name='regions']/element")],
        "export_vertices": len(mesh.vertices), "export_triangles": len(mesh.faces),
        "bind_audit": sword_bind_audit(render, mesh.vertices, joints),
        "variant_selection_audit": variant_selection_audit(model, render),
        "nodes": [{"index": index, "name": name,
                   "vertices": joints.count(index), "triangles": counts[index],
                   "export_model_space_bounds": bounds([
                       p for i, p in enumerate(mesh.vertices) if joints[i] == index])}
                  for index, name in nodes.items()],
        "runtime_requirements": [
            "Verify retail render tag identity and node mapping against these exports",
            "Apply the current animated node transform with verified inverse bind",
            "Honor visible region/permutation and blade-off state",
            "Retain the gap between blades rather than one enclosing convex hull",
            "Measure contact/render cost before enabling additional geometry"],
    }


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("directory", type=Path)
    parser.add_argument("collision_xml", type=Path)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--blade-fixture", type=Path,
                        help="write authored blade triangles for the offline native benchmark")
    args = parser.parse_args()
    report = audit(args.directory, args.collision_xml)
    args.output.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    if args.blade_fixture:
        roots = read_objects(args.directory / "fp_energy_blade.mesh.x")
        mesh = extract_meshes(roots)[0]
        joints = influences(roots[0], len(mesh.vertices))
        blade_nodes = [n["index"] for n in report["nodes"] if n["name"] == "blades"]
        if len(blade_nodes) != 1:
            raise ValueError("expected one explicitly named blades node")
        triangles = [face for face in mesh.faces
                     if all(joints[i] == blade_nodes[0] for i in face)]
        args.blade_fixture.write_text(str(len(triangles)) + "\n" + "\n".join(
            " ".join(format(v, ".9g") for i in face for v in mesh.vertices[i])
            for face in triangles) + "\n", encoding="ascii")
    print(json.dumps({k: v for k, v in report.items() if k != "inputs"}, indent=2))
