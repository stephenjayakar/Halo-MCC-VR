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

## Native inverse-bind evidence

Read-only disassembly of official `halo3_tag_test.exe` (SHA-256
`59A78F2C96034D7CEB5D710505B2B36813AA141FC81A083E3F952973DBCE4602`)
establishes the following layout and consumer chain. RVAs below are evidence
locations, not new runtime bindings or hooks.

- The executable explicitly warns that the displayed inverse matrix fields
  are incorrect: inverse scale belongs before the matrix, although the tag
  display puts it after inverse position. Therefore the XML field labels must
  not be used individually to construct an inverse matrix.
- At `0x47588E`, the object/model/render-model chain reads object `+0x40`,
  model `+0x0C`, and the render-model nodes block `+0x30`. Node stride is
  `0x60`; its inverse matrix starts at `+0x28`. Subsequent validation names
  that exact address `&render_model_node->default_inverse_matrix`.
- The skinning builder at `0x7D2DC0` resolves a `mode` tag and, on its ordinary
  branch at `0x7D2F61`, iterates the nodes. Each iteration passes a live
  52-byte node matrix and the corresponding authored inverse at node `+0x28`
  to `0x7D26E0`, producing a 48-byte shader matrix. Input nodes advance by
  `0x34`, authored nodes by `0x60`, and output matrices by `0x30`.
- `0x7D26E0` passes those two matrices to `0x41F5E0`, then folds the resulting
  scale into the basis while packing the shader matrix. The product routine
  writes the product of the two input scales at output offset zero. This
  establishes a native inverse-bind consumer after the live palette stage;
  the remaining runtime work must preserve its transform conventions.

The pinned Steam retail module (SHA-256
`B209D8454B12DC77E54CCD2C9924EC8D44B8619D21CF98E36FFAF601E67EFB63`)
has exactly one match for the existing `kFpVisiblePaletteSig`, at `0x2C561C`.
That function maps the source nodes through the bone map and copies or composes
them with the supplied root; it does not itself apply the authored inverse.
The retail downstream skinning homolog has not yet been established here.
Nothing hooks the matrix-product routine.

Disassemblies are preserved under `out/research/20260905-sword-contact/` as
`h3ek-inverse-layout-disasm.txt`, `h3ek-inverse-consumer-disasm.txt`,
`h3ek-skinning-matrices-disasm.txt`, `h3ek-skinning-compose-disasm.txt`,
`h3ek-matrix-product-disasm.txt`, and `retail-final-palette-disasm.txt`.
The earlier `h3ek-default-inverse-xrefs.txt` zero-reference result is invalid:
its linear disassembler stopped early. Positive RIP-relative references and
assert descriptors led to the functions above; do not cite that earlier zero
as evidence of absence.

Audit schema 2 now decodes the serialized inverse floats in the verified order
and checks this sword's translation-only rest skeleton. Both inverses have unit
scale and identity basis. The blade inverse position is `(-0.117739, 0, 0)`,
canceling its authored rest position `(0.117739, 0, 0)`. All 774 vertices round
trip through inverse bind and rest pose with maximum error `1.39e-17` world
units. Removing the blade inverse translation in an in-memory negative check
correctly fails the audit. Rotated rest skeletons are explicitly rejected by
this narrow audit rather than interpreted using unverified quaternion rules.
The fixture remains in export space; this result does not change its contract
or demonstrate an animated retail collision solve.

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
