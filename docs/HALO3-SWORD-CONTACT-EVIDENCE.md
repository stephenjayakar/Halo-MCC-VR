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

The extended Release benchmark additionally exercises the authored mesh at
world translation `(13.25, -4.5, 2.75)`, rotations `-0.7`, `0.3`, `1.2` radians
about its Y axis, and scales `0.5`, `1.0`, `1.5`. Probe radii and sweep step
scale with the weapon so this tests transform consistency, not changing target
size. Results in `animated-kernel-benchmark.json`:

- All 2,196 transformed triangle-centroid probes contact.
- All nine transformed gap probes remain clear.
- All nine rotational sweeps contact a probe at the most distal triangle's
  midpoint pose. Neither endpoint contacts (zero of 18), proving these hits
  came from the intervening sweep rather than endpoint overlap.
- The original translation checks still pass: 244 centroid probes, clear gap,
  and 500/500 sweeps. This run measured translation sweep p95 22.1 microseconds,
  max 42.7 microseconds; it does not measure rotational sweep performance.

The rotational sweeps span 0.4 radians with fixed root translation. They test
the native contact kernel with authored geometry, not the engine's actual
animation, render selection, inverse-bind application, wall solver or melee
dispatch. No production behavior was enabled by these tests.

`tools/h3-blade-mesh-prototype.h` now supplies an offline, fixed-capacity adapter
from authored model-space triangles through inverse bind and animated node into
the weapon root frame. It rebuilds every triangle and group bound after that
conversion, preserves groups, rejects aliasing and invalid mesh/finite inputs,
and publishes no valid output counts on a failed conversion. Like the existing
inverse-transform helper, it expects orthonormal node/root bases from a verified
pose source; it is not a general affine-matrix adapter.

The Release benchmark uses a root rotation of 0.3 radians and a separately
animated blade rotation of 0.8 radians at scale 1.5, with the blade pivot placed
at the root-transformed authored rest translation. All 244 transformed blade
probes contact and the transformed gap remains clear. Results are preserved in
`node-adapter-benchmark.json`; all earlier rotation/scale/sweep checks also pass.
The adapter is included only by the offline benchmark, not by the DLL. Live
geometry identity, visibility and animation-transition policy remain required
before promoting it into production contact code.

## Read-only live observation tool

`tools/probe-h3-weapon-render.py --output <snapshot.json>` can observe a loaded
Halo 3 diagnostic session without process-memory writes, remote threads, game
hooks, game-file changes or SteamVR configuration changes. It first runs the
pinned skinning verifier. The renderer at `0x29594A`-`0x2959E8` constructs
96-byte records in array `0x91AC60`: render tag `+4`, object handle `+0x48`,
region count `+0`, region-selected mesh indices `+0x0E`, and flags `+0x58`.
The running allocation count is loaded at `0x29591E`; the tool derives its
address from that instruction. The byte at `+0x0C` is recorded separately as
the skinning count field, not confused with the region count.

The tool correlates records with the verified first-person palette by both
render tag and object handle, reads their live matrices and loaded inverse
binds, bounds every array, and compares the buffers/counts on reread. Even an
unchanged reread is not an atomic or per-eye rendering guarantee. Schema fields
state that limitation explicitly. This is a diagnostic candidate: syntax/help
checks pass, but live observation is still untested. Its first run found zero
MCC processes and exited before opening a process or creating a snapshot.

### First live probe, 2026-09-05 20:48-20:52 UTC

The existing Forge `visible-weapon-gap` null-driver harness passed on installed
source `6ea7e86` (the existing weapon fixture, not a sword). Result and preserved
log are `out/debug-openxr/20260905-204851293Z-visible-weapon-gap-result.json`
and the adjacent `.log`; log SHA-256 is
`186B2FB65FA854725E96EC50B668625DF554A240EF16CE5B0DDFFB5B6F7B1261`.
This is not headset acceptance or a meaningful sword demo.

The read-only probe returned three palette entries with 37, 5 and 51 nodes.
Repeated snapshots, including one unchanged on reread, returned **zero matching
draw records**. A diagnostic snapshot counted 123 records but found none of the
palette render tags in them. Therefore the proposed draw-record correlation is
not runtime verified: do not promote it into a sword-visibility gate. Preserve
the negative result and investigate render-pass timing/storage selection.
Snapshots are `live-render-snapshot*.json` and `live-render-draw-debug.json`
under the sword research directory. Palette/inverse reads work; this does not
prove the correct live sword identity or selection.

The menu helper initially failed under Windows PowerShell 5 converting an Int32
directly to UIntPtr. The same session navigated successfully under PowerShell 7.
`send-mcc-keys.ps1` now constructs the key parameter explicitly from UInt32;
the Enter value 13 conversion was verified under Windows PowerShell 5 without
sending additional input.

Follow-up inspection of the same native renderer establishes that its counters
are transactional: `0x2956D4` and `0x2956E8` save the earlier allocation counts,
and `0x295D98`-`0x295DA4` restore them when the submission-success flag is false.
This does not prove that rollback caused the observed absence; external reads
also lack a render-pass boundary. It rules out treating one stable external
snapshot as a definitive list of every weapon mesh rendered that frame.

The probe now accepts `--sample-seconds` from 0 through 30. It keeps one
read-only process handle, refreshes palette identities and the bounded record
array each iteration, and records distinct matching region selections with
observation counts and elapsed timestamps. Every observation remains explicitly
non-atomic. No match still cannot establish blade-off state. Syntax validation
and rejection of an out-of-range duration pass; the sampling mode has not yet
been run against a live session.

### Sampling resolves the missing-record observation

On the next controlled Forge gap session, five seconds of read-only sampling
made 323 attempts and found all three palette identities in the draw-record
array. `out/research/20260905-sword-contact/live-render-sampling.json` preserves:

| Render tag | Palette nodes | Selected region meshes | Observations |
| --- | --- | --- | --- |
| 4220721659 | 37 | 1, 2 | 3 |
| 3969977131 | 5 | 0 | 2 |
| 4234484429 | 51 | 1, 2, 65535 | 3 |

The five-node entry is the existing weapon fixture, not a verified sword.
Matching uses the full render tag and object handle. The initial one-shot read
still found no records even though its reread was unchanged. This supersedes
the concern that the array correlation itself was wrong: it has now been
observed live, but its low external sampling hit rate makes absence unreliable
and unsuitable for a production visibility gate. No atomic or per-eye guarantee
is claimed, and no sword-specific visibility behavior was tested.

The earlier gap session also logged hand-recovery `checks=0`, `resets=0`, and
`missingPose=19161`. Its gap-test pass must not be represented as testing the
30 cm recovery behavior; that null-driver fixture did not provide the required
tracked-hand reference.

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
The downstream retail consumer is now verified below. Nothing hooks the
matrix-product routine.

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

## Verified retail palette-to-skinning chain

`tools/verify-h3-sword-skinning.py` verifies the exact pinned module hash, two
unique executable-code sequences, five direct call targets, and both references
to the same first-person palette array. Its successful output is preserved as
`out/research/20260905-sword-contact/retail-skinning-verification.json`.

The producer at `0x28AE4C` passes array `0xA7AC28` to `0x2C0D20`. That producer
uses `0x2C5A38`, whose call at `0x2C5A74` invokes the existing visible-palette
function `0x2C561C` with destination entry `+0x0C`. Each entry begins with the
render tag, object handle and a third metadata word, before its node matrices.

The renderer at `0x2958A7` addresses the same array. At `0x295BA9` it passes the
entry's render tag in EDX, node matrices at entry `+0x0C` in R8, object handle
in ECX, and selected mesh indices in R9 to `0x266838`. It sets the sixth
argument to true; this takes the branch at `0x26689A` that preserves the supplied
palette instead of asking the object subsystem for another node bank.

Within `0x266838`, the loop starting at `0x266990` calculates the live matrix
address as `palette + index*0x34` and the inverse address as
`renderNodes + index*0x60 + 0x28`. At `0x2669AD`, those are respectively RCX and
RDX for the existing native matrix product. The following stores fold scale
into the basis and pack 48-byte shader matrices, matching the official H3EK
consumer. This proves which palette and authored inverse drive the visible
weapon's skinning; it does not yet establish the sword's live tag identity,
permutation selection or blade-off state.

Read-only disassemblies are in `retail-palette-consumer.txt` and
`retail-skinning-builder.txt` beside the verification JSON. No additional hook
is needed merely to obtain this transform: the existing palette capture already
sees its live-node input, and the loaded render tag supplies its inverse.

## Required runtime work

The schema-3 variant audit maps authored permutation names to actual render
meshes. The render tag has exactly one `default` permutation for blade (mesh 0)
and handle (mesh 1). Model variant `default` requests both, with an additional
empty blade permutation of probability zero. Model variant `noblade` requests
the handle's default and an empty blade permutation of probability one. That
empty name has no matching render permutation. This correspondence is not proof
that the engine hides the blade: neither a variant-name check nor an absent
permutation-name lookup can substitute for the actual runtime selection.
In particular, do not implement a speculative `variant == noblade` collision
gate based only on these exports. The current audit intentionally reports the
missing correspondence as null, not as an invisible mesh.

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

## Forge equip controls, September 5, 19:16–19:22 PDT

Stationary normal-controller run on source 9988fa3:
`out/debug-openxr/20260906-021614133Z-controller-contact-result.json`, log
SHA-256 `3B4089E4911368C2E5543B92CD2CC32A8E80C73173F9DCE178689A4CEAF9BBC6`.
The generic controller validation passed, but no sword was equipped and no
sword-contact result is claimed. A focused 500 ms Escape opened the native
Forge pause page; short addressed Escape/Tab did not establish that transition.
Its controls showed `[-] Play / Edit`, `[2] Tools`, and `[E] Toggle Rotation Axes`.
Minus input did not visibly enter the editor; the dash is not established as
an actual assigned key. No bindings or map files were edited or saved.
Screenshot: `out/debug-openxr/20260906-022002427Z-forge-menu-native-held/0000-022002655Z.jpg`.
The harness exited normally and original normal SteamVR settings were verified
restored, with MCC and SteamVR stopped.

The following diagnostic candidate extends the already opt-in
`HALOMCCVR_DEBUG_KEYBOARD_GAMEPAD` bridge from F13–F19 to F13–F21, adding native
X and Y buttons. The validation harness explicitly enables it only with
`-KeyboardGamepad`, and now preserves/clears/restores that process environment
variable with the other test flags. Normal launches do not enable the bridge.
`tools/send-mcc-gamepad-button.ps1` verifies the current live log advertises the
new bridge, focuses the unique MCC window, and sends isolated virtual-key
pulses with release in finally. It rejects a lost foreground before another
button. Input delivery is not treated as confirmation of a game-state change.
This is test-control scaffolding, not a change to weapon physics or sword
geometry. The existing keyboard sender also accepts Tab, E, Minus and Tools;
their mappings alone do not establish native gameplay actions.

### Live bridge result, 19:24–19:31 PDT

Diagnostic source `3afef90ecafd9e0af2885f234a68300799fd805e`, package
`out/candidates/3afef90-h3-physical-contact-20260906-022418871Z`, independently
verified installed DLL SHA-256
`B348B0E219E164EF45E62A9C582F91A44D74617F182C42299A9F5AA245D01302`.
Release build/core tests passed. The prior launcher/configuration are unchanged;
only E: Steam is installed. No sword physics was added.

The opt-in bridge was observed opening the native pause page with Start,
returning with B, switching to monitor/editor with Up, and opening the native
weapon inventory with X. Sword selection was visibly reached at 19:31:03:
`out/debug-openxr/20260906-023103752Z-forge-sword-selection/0000-023103965Z.jpg`.
The automatic hold ended before placement; the subsequent A sender found no
MCC process and sent no input. No sword palette was captured and no sword demo
is claimed. Short pulses were often missed and some longer pulses repeated;
neither a fixed key count nor successful SendInput proves menu selection.

Result `out/debug-openxr/20260906-022449029Z-controller-contact-result.json`
correctly reports **failed**, because the session ended in monitor/editor mode
and lost its on-foot controller-contact pass condition. The final log had
stage=visible-pose, weaponTriangles=0, nativeSamples=0; this is not a weapon
collision regression test. Preserved log SHA-256:
`E2F86AF9917BA34A849826350578D9F27AF58473A5A86C409D399B7178FBA726`.
The observed input actions above are supporting tool evidence only.

The sender's initial ReadAllText failed against the live writer's sharing mode
before input. It now uses Get-Content's shared read and additionally confirms
the isolated key's Windows down-state during the pulse, retaining release in
finally. Successful actions used 750 ms holds with screenshot verification.
The harness now permits up to 900 seconds of explicitly requested post-pass
hold, prints the absolute UTC expiry and records the requested duration. It
still rechecks the pass condition at expiry and restores settings in finally;
an equip investigation must return to the on-foot weapon path before claiming
a controller-contact regression pass.

MCC and SteamVR were confirmed stopped after completion. Original real-headset
settings hash `175C79EDD6BBD58D1B7638BFFF7AAFF784625710BBEBE2A574BE76E4AC8BA89E`
was independently verified restored. Accepted pointer unchanged.

## Equipped retail sword observed, September 5, 19:33–19:45 PDT

Source remained `3afef90ecafd9e0af2885f234a68300799fd805e`, DLL SHA-256
`B348B0E219E164EF45E62A9C582F91A44D74617F182C42299A9F5AA245D01302`.
Validator source was `16aeb0686e809bafd244708969d7dd15c607fb51`; the opt-in
keyboard gamepad and 600-second hold were enabled, fresh-region off. Steam MCC,
Construct Forge, SteamVR OpenXR 2.17.8 / Null Model Number. Result:
`out/debug-openxr/20260906-023350117Z-controller-contact-result.json` (passed).
Log SHA-256 `100CA0D57557919897A18A2870B7BFF6643C2103FDD65D33FF85D348187AC953`.
The pass proves sampling returned after editor use and weapon changes; it is
not blade-collision acceptance.

### Native equip route now demonstrated

The programmatic gamepad Up entered Forge editor; X opened the weapon inventory.
After screenshot-verified Energy Sword selection, A spawned the sword, another
A dropped it, and Up returned to the player. The menu row was observed at
`out/debug-openxr/20260906-023649735Z-sword-highlight/0000-023649977Z.jpg` and
placement at `out/debug-openxr/20260906-023708844Z-sword-placement/0000-023709058Z.jpg`.
Normal W movement approached it. A focused E hold picked it up once close
enough, with the native "Picked up an Energy Sword" message and active sword
visible at `out/debug-openxr/20260906-024020844Z-sword-pickup-close/0000-024021058Z.jpg`.
The earlier X attempts on foot did not establish pickup. No map was saved and
no game-file or process-memory write tool was used.

Menu pulses remain state-verified: ten 650 ms Down pulses advanced fifteen rows
in this run, and a 400 ms Up returned one row to the sword. Do not encode a
fixed pulse count as a reliable selection. Runtime input-source switching and
repeat behavior were not separately attributed.

### Live identity, inverse bind, and selection

The equipped sword's observed handle was `0xE46900B6` and its render tag was
`0xEF060D90`. These are observed session identities, not cross-map constants.
The corresponding first-person palette has two nodes. Its second loaded
inverse has identity basis/unit scale and translation
`(-0.11773931980133057, 0, 0)`; the first inverse is identity. This matches the
official sword's handle/blades rest skeleton within the export's decimal
precision. Samples under `out/research/20260905-sword-contact/`:

| Snapshot | SHA-256 | Sword draw observations |
| --- | --- | --- |
| `forge-sword-equipped.json` | `C78D5B8859073BBAEFB860EBFC2F9CD1CF3F582787784CC42DDE89D490505723` | 10 |
| `forge-sword-tilted.json` | `A1D544EBD9E0ABA4F12B922A7A9396F76D08FC18CF0064A7961CB28C92FF9952` | 8 |

Both select region meshes `[0, 1]`, corresponding to the official blade/handle
meshes. The second sample followed an accepted analytic controller pitch change
to -30 degrees. Their loaded inverses differ from the audited export by at most
3.20e-7. Transforming the authored blade rest position through each live root
predicts the child position within 6.54e-7 world units; child/root bases agree
in these two settled poses. Comparison report `live-sword-rest-comparison.json`,
SHA-256 `A985ED03A26BAAEB312D388F963C8392ECEB3D97981EB96BF7E8FF79593818B8`.
This verifies this live mapping at two settled poses, not arbitrary animation
or equality of every retail vertex to the exported resource.

The earlier `forge-equipped-first.json` filename is misleading: its screenshot
still shows the rifle before pickup. Do not use it as a sword sample.

A 750 ms Y pulse switched to the rifle; its palette replaced the sword entry
with render `0xECA10B2B`, five nodes, matching screenshot and
`forge-after-switch.json`. Re-equipping restored the sword and selected meshes
`[0, 1]` in 19 observed records. Transition capture
`forge-sword-reequip-transition.json`, SHA-256
`69DDC0468A80EA0D0A7C6C8CDF52DF25E4BB2E3E3012B8915D2CA12AEE3CA468`.
**No blade-off mesh selection was observed.** Sparse external sampling cannot
establish that no such transition exists. The stable `[0,1]` result must not
be substituted for a per-frame visibility gate in production.

### Remaining gap and recording scope

With the sword visibly active, the normal contact logger reports
`shapeSource=1 weaponTriangles=12 nativeSamples=33`: the handle-only authored
collision mesh is still being used. No runtime blade geometry was added in
this run. The next implementation must preserve the verified inverse/live-node
transform and source active selection from a synchronous render consumer;
the external record array is not a safe negative visibility test.

`out/demos/20260906-024059-forge-sword-geometry-diagnostic/raw.mp4`, SHA-256
`4952AF8A703DD527026D2235253A692937CEDE8DF2A2F44FD009DD1D8A2B24E4`, is a
30-second live geometry diagnostic. Six sampled frames show the equipped sword
and final controller tilt, not wall contact or a successful shove. It is not a
functionality showcase. The diagnostic hand pose returned to default afterward.

The harness completed normally on foot, MCC/SteamVR stopped, and original
real-headset settings SHA-256
`175C79EDD6BBD58D1B7638BFFF7AAFF784625710BBEBE2A574BE76E4AC8BA89E` was
independently verified restored. Installed DLL/configuration and accepted
pointer unchanged.

## Synchronous selection-capture candidate, September 5

The pinned retail leaf at `0x2667AC` consumes the render tag in ECX and selected
mesh-index pointer in R8. Its complete body through `0x266834` overwrites EDX
before use, reads no stack arguments and returns a matrix count in EAX. It
does not alter the selection list. The first-person caller at `0x295B57`
passes the current palette's render tag and the finalized region selections
from its draw record, calls this leaf at `0x295B62`, then stores AL and allocates
the skinning buffer. This is before skinning/submission; a captured selection
is not proof that later allocation/submission succeeded.

Read-only disassembly is preserved as `retail-selection-matrix-count.txt` under
the sword research directory. The extended `verify-h3-sword-skinning.py`
checks the pinned hash, unique exact code sequences, count-call target and
palette-count producer store. The probe's four wildcard signatures were also
checked across the full mapped pinned image and each has exactly one match:
leaf `0x2667AC`, caller `0x295B57`, producer `0x28AE4C`, consumer `0x2958A7`.
Results: `retail-selection-verification.json`, `selection-probe-signatures.json`.
The producer and consumer independently resolve palette array `0xA7AC28`;
the producer's returned count is stored at `0xA7AC24`.

`halo3_selection_probe.inl` is an opt-in diagnostic through
`HALOMCCVR_H3_SWORD_SELECTION_PROBE=1` / harness `-SelectionProbe`. It forwards
the native count result unchanged and observes only the verified first-person
return address and current primary prepared render tag. It copies bounded
region selections and the unique matching palette, records generation/weapon
identity, and byte-compares that palette with our last-drawn publication.
Sixty-four immutable selection-change records maximum are drained by the cold
logger. Rendering performs no logging, allocation, I/O, locks or scanning.
Observation faults disable the probe alone; missing/ambiguous bindings or hook
failure do not alter camera/contact ownership. The hook is registered with the
existing title teardown registry. No matrix-product hook is added.

The harness requires at least one exact palette-paired record and zero probe
faults, rather than treating its generic contact pass as probe verification.
This candidate does not enable sword blade collision. Runtime capture of a
blade-off selection and a production geometry/selection policy remain pending.
