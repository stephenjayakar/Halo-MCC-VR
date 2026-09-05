# Halo 3 sword contact investigation, 2026-09-05

The user reports uncertain sword collision in the Floodgate headset run preserved
in `docs/HALO3-FLOODGATE-20260905-FEEDBACK.md`. The installed contact reader uses
the authored collision model; the official sword collision tag has only a handle.
This document records offline geometry evidence, not a working retail feature.

## Official inputs and reproducible audit

The original H3EK `H3EK.7z` supplies
`tags/objects/weapons/melee/energy_blade/fp_energy_blade/fp_energy_blade.render_model`.
The existing official model and weapon tags both reference this first-person
render model. No third-party or console binaries supplied the geometry.

H3EK `tool.exe export-tag-to-xml` produced the model and render-model XML;
`tool.exe export-render-model-mesh` produced `fp_energy_blade.mesh.x`.
Outputs are under `out/research/20260905-sword-contact/`. The XML exporter emits
an unescaped `value="<unavailable>"` for pageable resources; the audit repairs
only this sentinel in memory. It does not rewrite source exports or infer data
for that unavailable resource.

Run `tools/audit-h3-sword-contact.py` with that directory, the collision XML at
`out/h3-contact-re/weapon-collision-xml/objects_weapons_melee_energy_blade_energy_blade.collision_model.xml`,
`--output out/research/20260905-sword-contact/coverage-audit.json`, and optionally
`--blade-fixture out/research/20260905-sword-contact/blade-triangles.txt`.
The JSON records SHA-256 identities of every input and reports:

- Model variants: `default`, `noblade`.
- Collision regions: `handle` only.
- Render regions: `blade`, `handle`.
- Render nodes: `handle` (0), `blades` (1), child of the handle.
- Whole exported mesh: 774 vertices, 764 triangles.
- Handle-node influences: 513 vertices, 520 triangles.
- Blade-node influences: 261 vertices, 244 triangles.
- Every exported vertex has one nonzero blend influence, and no triangle spans
  two independently animated nodes.

Node influence decoding comes from the X-file DeclData UBYTE4 blend indices and
FLOAT4 blend weights. The declaration constants were independently checked in
Windows SDK `10.0.22621.0/shared/d3d9types.h`: blend-weight usage 1, blend-index
usage 2, FLOAT4 type 3, UBYTE4 type 5. The audit validates declaration lengths,
vertex counts, index ranges, finite/unit-sum weights and the matching XML nodes.
It does not confuse a render region with a node or assume every blade-region
vertex is bound to the blade node.

## Offline native-kernel result

`tools/benchmark-h3-blade-contact.cpp` reads the blade fixture and uses the same
`physical_contact_logic.h` triangle/compound and swept-contact routines as the
mod. It is a separate executable, never linked into the DLL and never connected
to MCC. A Release build on this machine produced:

| Check | Result |
| --- | --- |
| Small sphere at each authored blade triangle's centroid | 244/244 contacts |
| Probe at export-space `(0.25, 0, 0)` between the blades | No contact |
| Sweep from z=-0.03 to z=+0.03 past a selected blade point | 500/500 contacts |
| Swept-query p95 after 100 warm-up iterations | 24.6 microseconds |
| Maximum in the measured 500 samples | 41.9 microseconds |

Results are saved in `native-benchmark.json` next to the fixture. The target is
one small sphere, not an NPC or a many-part prop. These timings do not predict
whole-frame cost, establish the cause of the user's lag, or prove runtime blade
coverage. The tested gap coordinate is in the exported mesh frame, not a retail
world coordinate. Geometry totals fit the existing fixed 768-triangle capacity;
that capacity alone is not permission to enable the feature.

## Required runtime work

1. Verify the equipped retail sword's exact render identity and node mapping.
2. Apply the current animated blade node with its correctly verified inverse
   bind. Export-space positions cannot be copied into the handle's frame.
3. Respect the active region/permutation, animation and blade-off state.
4. Keep the authored space between blades; a single enclosing convex hull is
   not the demonstrated geometry.
5. Measure realistic target cost and obtain Forge/Campaign headset results.

No sword runtime behavior was changed by this investigation. The hand-recovery
candidate remains the installed, unaccepted candidate while its feedback is
pending. The user's successful enemy melee and floor nudging must be preserved.
