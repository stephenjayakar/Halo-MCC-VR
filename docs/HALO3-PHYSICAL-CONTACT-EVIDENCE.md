# Halo 3 physical weapon contact evidence

Date: 2026-08-08

## Pinned binaries

| Binary | Size | SHA-256 |
| --- | ---: | --- |
| MCC retail `halo3.dll` | 11,127,768 | `B209D8454B12DC77E54CCD2C9924EC8D44B8619D21CF98E36FFAF601E67EFB63` |
| Official Halo 3 Mod Tools `halo3_tag_test.exe` (Steam app 1695791, build 13160128) | 27,525,168 | `59A78F2C96034D7CEB5D710505B2B36813AA141FC81A083E3F952973DBCE4602` |

The expected RVAs below are evidence and logging cross-checks only. Runtime
binding scans the loaded image and requires exactly one match. A missing or
second match disables physical contact without touching the established camera,
render, input, ODST, or Reach paths.

## Native identities

| Purpose | Official H3EK RVA | Retail RVA | Retail match count | Evidence |
| --- | ---: | ---: | ---: | --- |
| `collision_test_vector_internal` | `0x652A10` | `0x1FD748` | 1 | the official public wrapper at `0x6529D0` proves the eight-argument core ABI; official callers and the structurally identical retail prologue prove the 0x68-byte result fields used here: type `+0x00`, fraction `+0x04`, point `+0x08`, plane normal `+0x2C`, and object handle `+0x40` |
| `object_set_velocity` native | `0xAD8050` | `0x39BAD0` | 1 | the official script wrapper at `0x7A88F0` and retail wrapper at `0x1E380C` call their respective native with object handle plus three local velocity floats |
| `object_set_velocities` world native | `0xA4EAA0` | `0x3411A4` | 1 | both `object_set_velocity` bodies transform the three local floats into a world vector, then call this routine with object handle, world-linear pointer, and null angular pointer; contact already owns a validated, clamped world vector and uses this lower authoritative physics/network path directly |
| `object_get_velocities` | `0xA43EA0` | `0x345480` | 1 | the official full-symbol accessor and its retail homolog return both world-linear and world-angular velocity for the exact object datum; physical contact uses both rather than reading the old raw `object+0x74` approximation |
| `object_get_center_of_mass` | `0xA426C0` | `0x34523C` | 1 | the official/retail accessor writes the authoritative rigid-body center used to evaluate `linear + angular x (contact-center)` at the exact hit point |
| `havok_component_apply_point_impulse` | `0x480750` | `0x15FBC4` | 1 | the official full-symbol wrapper and retail homolog take component, body index, world point, and impulse; both resolve the exact body and dispatch its native point-impulse method, which uses authored mass and inertia |
| `objects_update` simulation owner | `0xA52920` | `0x34067C` | 1 | retained H3EK `objects.cpp` assertion metadata identifies the body containing the `object_update_absolute_index` transaction; the retail homolog preserves the TLS object table, active-object loops, `object_update_absolute_index` writes, and update-in-progress byte, and the complete runtime signature is unique |
| `unit_melee_effects` (rejected for damage) | `0xA63390` | `0x35A194` | 1 | official disassembly proves the eight arguments and authored effect selection, but the body only emits melee contact effects; it is not used as the damage entry point and is reached only through Halo's stock wrapper |
| authored melee tag selector | `0xA5DE20` | `0x35A9A4` | 1 | the official and retail bodies resolve the active weapon and select its ordinary/clang damage and response tag pair; H3EK's own constant-string table maps `0x0A` to `melee`, and both selector bodies route that value to the first-hit pair without entering lunge selection |
| `damage_owner_from_object` | `0xAA0120` | `0x384A88` | 1 | official assertion/source metadata and both bodies prove the object-handle plus 0x0C-byte owner-output ABI used by Halo's stock melee caller |
| melee damage application helper | `0xA59860` | `0x35BEFC` | 1 | official assertions name the `damage_owner` and `damage_target` arguments; the retail body copies those records into native damage data, sets the melee damage flags, and enters the engine's damage application path |
| stock melee effects/response wrapper | stock caller sequence following `0xA596DA` | `0x35BCA0` | 1 | the retail stock melee caller passes the selector's damage/response tags, exact target index, material, point, and normal; the wrapper invokes `unit_melee_effects` and the authored impact response path |
| point-dependent collision-material remap | `0x446DA0` | `0x14B324` | 1 | the official body and retail homolog consume raw collision material plus exact hit point and return the global material expected by damage/effects |
| global material validation lookup | - | `0x14B274` | 1 | the stock retail melee caller validates the remapped material through this lookup before invoking its effects wrapper; the adjacent unique callsite also resolves the same global-material data pointer without a fixed address |
| `game_is_cooperative` native | `0xCFB7D0` | `0x0F000C` | 1 | official/retail script wrappers call the native; its body first requires game-options byte `+0x10 == 1` (campaign), then returns whether the authoritative player count is greater than one |

The retail signatures embedded in `game.cpp` wildcard only relocation/call
displacements. An independent offline rescan of the pinned retail image found
one match for each complete pattern at the RVAs above. The collision pattern's
single file-offset match is `0x1FCB48`, which maps to RVA `0x1FD748`.

The active held weapon is read through the already-proven Halo 3 object table:
the player unit's current weapon slot is `unit+0x262`, handles begin at
`unit+0x268`, object definition datum is `object+0x00`, and bounds are
`object+0x1C/+0x28`. The earlier `+0x1B4/+0x22C` damage-effect selection was not
proven by the native body and is rejected. The authored selector instead reads
the active weapon's melee parameter blocks at definition offsets `+0x24C`,
`+0x26C`, `+0x28C`, `+0x2AC`, and `+0x2CC`, with clang data at `+0x2EC`;
each block supplies damage at `+0x0C` and response at `+0x1C`. It falls back to
the weapon's default pair and finally the unit's authored melee damage.

The selector's second argument is a global animation string ID, not a motor
action enum. The official H3EK constant-string table at RVA `0x1017538` begins
with empty string ID 0 and places `melee` at ID `0x0A`. The same table names the
selector's other comparisons: `0x79`/`0x7B`/`0x7D` are `melee_1sthit`,
`melee_2ndhit`, and `melee_3rdhit`; `0x25A..0x25C` are melee-lunge variants;
and `0x28B..0x28E` are `melee_strike_1..4`. Official `0xA5DE20` and retail
`0x35A9A4` have identical value-to-block branches. Physical contact therefore
passes only `0x0A` (`melee`), selecting authored ordinary first-hit damage while
never requesting a lunge or synthesizing a motor action.

Halo's damage helper consumes a 0x0C-byte owner and a 0x3C-byte exact-target
record. Retail reads target object handle `+0x1C`, damage section `+0x2C`,
material `+0x30`, scale `+0x34`, and contest/flag bytes `+0x38/+0x39`. Physical
contact supplies the collision point/normal, exact datum handle, invalid
surface/node/region sentinels, and scale 1.0. The collision result's raw
material at `+0x28` is carried through the bounded command; on the simulation
thread it is remapped at the exact hit point and validated through the same
global-material lookup as the stock melee caller. Bounds-only fallback contact
uses authored global default material zero. An invalid material rejects melee
alone while preserving any valid impulse. The command also carries the weapon
handle; the simulation consumer revalidates that the same weapon remains active
before selecting tags. No grip, trigger, animation, melee action, contest, or
lunge input is synthesized.

The mass-aware candidate follows the component path used by
`object_get_center_of_mass`. Object data `+0x9C` holds the Havok component
datum. The unique retail accessor loads the component-array global at function
`+0x6C`, then resolves `elements + index * 0xC0`. The root body index is the
signed byte at component `+0x0C`; body count is `+0x28`; body records are at
`+0x20` with stride `0x60`; the body wrapper is record `+0x50`; and authored
inverse mass is wrapper `+0x1DC`. Official `hkpRigidBody::getMass` at
`0x078AF0` reads its motion's inverse-mass field at `+0xEC` and returns
`1.0 / inverseMass`. The runtime values close the identity: the Mongoose body
reports `0.002`, or about `500 kg`, while a loose object reports `2.615`, or
about `0.382 kg`. Missing, stale, non-root, invalid, or non-finite mass data
rejects only that contact command.

The same official point-impulse wrapper at `0x480803` passes
`bodyWrapper + 0xF0` as the `hkpRigidBody`. Official motion construction and
the getter at `0x06E430` prove the one-byte motion type at body `+0x10`:
values `1` through `5` and `8` are dynamic motion classes, `6` is keyframed,
and `7` is fixed. Keyframed and fixed bodies can retain authored inverse-mass
data even though an impulse cannot move them. The contact response therefore
requires a dynamic target motion type before publishing a point impulse. Native
melee remains independent. The automated Forge rig uses the same check so a
fixed weapon spawn cannot masquerade as a loose test prop.

The rejected response used one bounded inelastic collision impulse. In the
headset it felt like a hit instead of continuous contact. It also had no
tangential force, so it could not scoop or carry a light object.

The replacement is a sustained point-contact constraint. Each valid overlap
matches 20 percent of inward relative speed. Penetration adds a bounded normal
correction. A Coulomb limit of `0.80 * normal impulse` supplies tangential
friction only while the shapes press together. The target velocity change is
capped at `0.08 m/s` per sample. A separating weapon produces no impulse, so
the body keeps its last native linear and angular velocity when released. The
result is converted from metres to Halo world units and sent to the native
point-impulse wrapper at the exact authored target support point. Halo applies
the target's own mass and inertia. No guessed weapon or target mass is used.

The collision query's first argument packs two 32-bit flag sets: collision
flags in the low dword and object-type flags in the high dword. The official
H3EK generated initializer at `0x140014060` writes low `0` and high `0x7FFE`
for its all-object set. Other generated initializers independently validate the
split by modifying each dword and rejecting collision bits above 21 or object
bits above 14. Most importantly, the official assertion text inside
`collision_test_vector_internal` reads
`TEST_FLAG(flags.object_flags, _collision_test_objects_bit)`: the master
object-query enable belongs to the high object-flags dword. Thus high `1`
enables object queries but selects no object types, while high `0x7FFE`
selects every type but does not enable object queries. Physical contact uses
low structure bit `1` and high `0x7FFF` (master enable OR all type bits).

## Collision-shape decision

Candidate `a644d2c` used a bounds-derived capsule against every object's broad
bounding sphere. The 2026-08-06 campaign runtime rejected it: roughly 95% of
sweeps reported a hit, almost all stopped at `static-block`, and neither Forge
nor campaign testing produced visible object movement. The hit distribution
proves that broad spheres are not a usable surface-contact proxy. Commit
`2f4940b` disables that failed behavior before the replacement candidate.

The replacement uses Halo 3's native swept-vector collision query for exact BSP
and instanced-geometry blockers. Five fixed,
allocation-free samples cover previous-to-current grip, midpoint, and tip plus
the previous and current weapon spines. Each sample resolves the engine's exact
authored static surface and returns the first blocker;
the query ignores the player unit and held weapon. The weapon extent remains a
bounded proxy derived from the held weapon's authored radius (0.30–0.75 world
units, with a 0.65-unit invalid-bounds fallback), but targets are no longer
approximated by bounding spheres in the native branch. The live 2026-08-08 Forge
probe proved that branch returns BSP hits but does not expose a loose kind-2
weapon's rigid-body surface. Movable root objects therefore use the approved
bounds-derived capsule fallback, restricted to physics-capable object kinds; an
earlier exact native structure hit still wins unless the proxy begins at the
same surface, such as a weapon resting on a floor. Unlike rejected candidate
`a644d2c`, scenery and machine bounds are not treated as interactive surfaces.

Headset testing rejected that interim capsule/sphere approximation because
contact did not match the visible weapon. The official H3EK `physics_model`
postprocess at `0x52D8B0` proves the exact replacement path. It walks rigid
bodies at root block `+0x58` with stride `0xC0`, resolves each serialized shape
reference, and writes the immutable Havok shape pointer at rigid body `+0x58`.
The resolver at `0x52E830` proves these authored blocks and returned shapes:

| Shape | Root block | Stride | Returned shape |
| --- | ---: | ---: | ---: |
| sphere | `+0x70` | `0xA0` | element `+0x50` translated shape |
| multi-sphere | `+0x7C` | `0xD0` | element `+0x20` |
| pill | `+0x88` | `0x70` | element `+0x20` |
| box | `+0x94` | `0xE0` | element `+0x60` transformed shape |
| triangle | `+0xA0` | `0x90` | element `+0x20` |
| polyhedron | `+0xAC` | `0xA0` | element `+0x20` |
| list | `+0xDC` | `0x60` | element |
| MOPP | `+0xF4` | `0x40` | element |

Constructors in that same official routine prove internal type `3` sphere,
`5` triangle, `6` box, `7` capsule, `8` convex vertices, `12` convex
translate, and `13` convex transform. The official list constructor at
`0x03C2E0` proves type `10`, child storage `+0x30`, count `+0x38`, and a
`0x20` child stride. Radius is shape `+0x20`. Capsule
endpoints are `+0x30/+0x40`. Box half extents are `+0x30`. Transform child,
rotation rows, and translation are `+0x30`, `+0x40/+0x50/+0x60`, and `+0x70`.
A convex-vertices shape stores its packed four-vector pointer at `+0x50`, group
count at `+0x58`, vertex count at `+0x60`, and plane pointer at `+0x68`.
Official assault-rifle XML confirms four exact vertices and a `0.009` rounded
radius instead of the interim long capsule.

The new candidate reads only that resolved immutable shape pointer. It copies
at most eight disjoint convex children and 256 finite vertices per child into
fixed stack storage. It supports the proven sphere, triangle, box, capsule,
polyhedron, list, translate, and transform types. Multi-body, multi-sphere,
MOPP, invalid, and ambiguous shapes stay non-interactive. A bounds sphere
performs broad-phase rejection only. A bounded
GJK sweep decides contact. It interpolates the final visible weapon translation,
orientation, and scale. It samples at one-centimetre swept spacing, up to 32
poses, then performs nine binary refinements at the first overlap. Broad bounds
never create a hit. Unsupported geometry never falls back to capsule/sphere
contact.

Official extracted H3EK tags close the player melee-weapon cases. The energy
sword physics tag (SHA-256
`8B34EFF8A6F525E50085AE94A6E41A459DB988E698A4BCBF97A7AB8AD25F3FDD`)
uses one four-vertex polyhedron. The gravity hammer physics tag (SHA-256
`D7EDC3609E0603A98D41B57CF912D87C2DAEECC3BACC79A174946FE76C80C24D`)
uses one list with separate eight-vertex haft and 12-vertex head polyhedra.
Sweeping each child pair preserves the authored empty space. A convex hull over
both hammer parts would create false contact. The bounded eight-by-eight,
12-vertex benchmark measured `0.0691 ms` p95 on this machine. The common case
remains below `0.022 ms` p95. Both stay below the `0.25 ms` contact budget.

An official H3EK XML census covered all 44 weapon physics tags. Forty-two use
one rigid body. The two multi-body exceptions are detached missile-pod tripod
or vehicle garbage, not held weapons. Root shapes are 32 polyhedra, three
boxes, two pills, one sphere, and four lists. The largest list has four box
children. Every active held-weapon root therefore fits the bounded compound
reader. Unsupported multi-body data cannot silently become approximate contact.

The user's 2026-08-09 headset test rejected the physics hull as the visible
contact solid. The assault-rifle example explains the mismatch. Its rounded
physics hull has four vertices. Its separate authored collision model has 20
vertices, 36 edges, and 18 surfaces. The physics hull remains authoritative for
mass and inertia. It is not detailed enough for visible contact.

The replacement reads the held weapon's authored collision model. Official
H3EK `guerilla.exe` field tables prove the loaded model collision datum at
`+0x1C`, collision regions block at `+0x20`, region stride `0x10`, permutation
stride `0x28`, BSP stride `0x64`, BSP vertex block at `+0x58/+0x5C`, and vertex
stride `0x10`. Official XML exports cover 43 available weapon collision tags.
They contain 1,454 vertices across one to three BSPs per tag. The largest tag
has 117 vertices. Every region has one unambiguous permutation. These bounds
fit fixed storage.

Each collision BSP becomes one authored convex child. This is the approved
convex fallback. The official world-shape query allocates a temporary phantom
for every call, so triangle-accurate native queries are unsafe in the camera
hot path. A missing, ambiguous, invalid, oversized, or custom collision tag
falls back loudly to the existing authored physics shape. Runtime telemetry
reports `shapeSource=1` for the detailed collision model and `shapeSource=2`
for the physics fallback.

The earlier wrist-target publication was runtime-rejected because the desired IK
wrist is not necessarily the transform Halo ultimately skins. The replacement
publishes only after the final visible-palette consumer returns. It accepts a
bounded right-wrist-descendant render model with at most 16 validated nodes
(covering the H3EK ordinary weapon, sword, and hammer first-person models) and
publishes every finite final node, root, and scale through an atomic snapshot.
Collision BSPs bound to moving render nodes are baked into root-local space
from that exact final palette before the continuous sweep.
This is the same matrix space as the visible weapon pixels, not a
controller/wrist estimate.
OpenXR pose, linear/angular velocity, timestamp, and serial use a separate
bounded atomic snapshot.

The original final-palette selector was still too broad. It accepted every
small render model whose node zero mapped into the primary wrist subtree.
First-person attachments use the same interpolated bank and can satisfy both
conditions, so a later attachment submission could replace the correctly
published weapon pose.

The replacement uses the engine's exact active render-model identity. Official
H3EK `halo3_tag_test.exe` (SHA-256
`59A78F2C96034D7CEB5D710505B2B36813AA141FC81A083E3F952973DBCE4602`)
function `+0xABB1B0` selects the authored player-interface element from the
weapon definition's first-person block; its caller at `+0x9663FD` reads the
element's first-person model datum at `+0x0C`. The pinned retail
`halo3.dll` (SHA-256
`B209D8454B12DC77E54CCD2C9924EC8D44B8619D21CF98E36FFAF601E67EFB63`)
first-person builder at `+0x2C0D20` prepares the model set beside the proven
interpolation bones at `+0x4A4`. Runtime probes described below correct the
initial field interpretation: `+0x4C` is the separately submitted 37-node
first-person body, while the held weapon is the datum at `+0x44`. The
interpolation hook therefore snapshots the low 16-bit tag index from slot
`+0x44`, and the final palette hook publishes contact only when its submitted
tag is identical.
Missing or invalid identity withholds contact; weapon changes invalidate the
previous pose before the new model can publish.

## Runtime safety and behavior

Contact requires all of the following: the opt-in setting, current Halo 3
generation, unique native bindings, live validated object table, fresh right
controller tracking, final visible weapon pose, alive player with a valid held
weapon, authoritative `Gameplay`, proven on-foot state, unpaused engine,
player-controlled camera, and either campaign mode with one authoritative
player or multiplayer mode with the exact authoritative role used by Forge.
Candidate `b82fbaa`, running Steam Halo 3 Construct Forge through the
anti-cheat-disabled launcher on 2026-08-08, measured the retail game-options
bytes as mode `2`, simulation `5`, cooperative `0`. The official simulation
enum names `5` as `distributed-server`; the earlier `simulation == 1` local
assumption was false and prevented every Forge sweep. Synchronous and
distributed client roles remain disabled. Any failure resets contact state and
performs no native write.

Collision remains on the camera callback, but object mutation and damage do not.
The camera callback publishes a bounded atomic command containing operation
flags, validated target/player/weapon handles, world impulse, contact
point/normal, generation, timestamp, and serial. The unique `objects_update`
hook consumes that command immediately before the authoritative object update.
The hook performs no allocation, logging, file I/O, locking, or signature
scanning and always calls the original update routine.

The closest native surface blocks its sample. BSP and instanced geometry receive
no impulse or damage. Player, held weapon, attached/first-person-only objects,
stale handles, and invalid values are rejected. Every exact object hit publishes
one native point-impulse command per sample above 0.05 m/s; the simulation
owner re-resolves the target component and body before applying it. The earlier
candidate also used a hard-coded engine object-kind allowlist before resolving
physics. That omitted dynamic bodies carried by other object kinds. The
all-rigid-body candidate instead admits every validated root whose native Havok
component resolves to an official dynamic motion type (`1`-`5` or `8`). Fixed,
keyframed, unresolved, attached, player, and held-weapon objects remain out. The
native point-impulse path remains the final authority for whether the exact
object owns a rigid body. Authored masses, inward speed, penetration, and
tangential speed determine the bounded contact constraint. Classification maps
the exact current weapon contact point into
the previous full visible transform. This includes translation, pitch, yaw,
roll, and scale over the exact bounded timestamp delta. It subtracts target
surface velocity from the two native accessors, including
`angular x (contact-center)`, for the sustained physics response. Melee uses
the weapon contact point's own measured speed. A fast-moving or rotating target
can therefore produce a physical response but cannot turn a slow hand into a
melee. At or above the configured weapon-speed threshold, an independently
debounced command selects the equipped weapon's native melee tags, builds native damage
ownership for the player, applies damage to that exact target, and invokes the
stock effects/response wrapper. Missing or ambiguous melee signatures leave
only high-speed damage stock while slow rigid-body contact continues; a runtime
melee exception likewise disables melee alone. Existing grip melee and all
normal input paths remain unchanged.

## Forge runtime proof

Steam Halo 3 Construct Forge was run through the anti-cheat-disabled launcher
with the repository's headless OpenXR test runtime and the opt-in contact debug
rig. The D3D11 OpenXR path rejected the synthetic adapter with `XrResult(-9)`,
so stereo correctly disarmed while the isolated contact rig continued in the
flat game. This is a diagnostic-only configuration and does not bypass Easy
Anti-Cheat.

The debug rig also has a one-shot scoop mode. After choosing a settled light
dynamic target, it aligns the exact authored weapon support surface `2 cm`
below the target, lifts `0.35 m`, carries sideways `0.35 m`, then separates
downward by `0.30 m`. Smooth-step segments cap the measured trajectory below
`0.61 m/s`, so this diagnostic cannot cross the `1.50 m/s` melee threshold.
The existing target position and native velocity readback then distinguish
lift, carry, release, and retained motion. This environment-only path never
runs in normal play. Live Forge proof remains pending.

Candidate `1da8395` reported an applied command whose value predated the
camera-thread publish and was reverted. Candidate `24b0acd` moved the call to
the XInput thread, where the native setter raised a structured exception, and
was reverted. Candidate `0eccdbdc29b1cab8ffdf1f3af1612c17e91a648d` installed
the unique `objects_update` hook with MinHook status `0/0`. In authoritative
Forge mode (`game=2`, `simulation=5`, `cooperative=0`, `options=1`), the rig
selected the exact loose kind-2 weapon handle `0xE27C000D`. A slow `0.43 m/s`
contact published and applied one native impulse (`command=1`, `applied=1`,
`commandStatus=2`) without a melee event or fault. Readback moved the target
center from `(-1.844, -0.857, 3.116)` to `(-1.841, -0.860, 3.116)`, a measured
displacement of `0.004` world units. The preserved log is
`out/debug-openxr/0eccdbd-forge-success-slow.log`.

This proves headless Forge rigid-body interaction for the slow-contact path. It
does not constitute headset acceptance and does not prove the still fail-closed
high-speed native melee path.

Candidate `b841db244fed9b32854b65370278aee3ad1ee545` uniquely bound the
selector (`+0x35A9A4`), damage helper (`+0x35BEFC`), owner builder
(`+0x384A88`), and stock effects wrapper (`+0x35BCA0`) and ran in the same
authoritative Construct Forge rig. High-speed commands reached and were consumed
by the simulation hook without an exception, but calling the selector with
melee type `0` returned damage and response datums `0xFFFFFFFF`. The guard
therefore applied no damage (`meleeStatus=5`, `melees=0`), as designed. The
runtime log is `out/debug-openxr/b841db2-forge-no-authored-tags.log`. This is a
failed behavioral candidate: the implementation remains in-tree but is disabled
in commit `0450cb4` before the corrected candidate. H3EK evidence subsequently
proved that the call supplied empty string ID 0 rather than ordinary `melee`
string ID `0x0A`; the corrected candidate changes only that selector argument
and re-enables the independently isolated authored-melee transaction.

Candidate `d1f91429b2d0382c075b446cba7dbdcc1dda50e6` proved the corrected
selector argument in the same authoritative Construct Forge rig. It returned
real authored AR datums (`damage=0xEBF10A7B`, `response=0xEAEA0974`) instead of
the previous invalid sentinels. A later call inside the contained native
damage/effects transaction raised a structured exception, reported
`meleeStatus=3`, and disabled melee alone; MCC remained responsive and the
slow-contact impulse path remained active. The preserved log is
`out/debug-openxr/d1f9142-forge-authored-tags-contained-fault.log`. This is a
failed behavioral candidate and is disabled again before downstream-call
isolation. The selector value is now proven; the remaining fault is after tag
selection.

The next diagnostic candidate preserves the same fail-open transaction but
records the exact post-selector fault boundary: status `6` while constructing
the native damage owner, `7` while applying native melee damage, and `8` while
invoking the stock effects/response wrapper. These are bounded atomic status
writes only; no logging, allocation, locks, or additional engine calls are
introduced in `objects_update`.

Candidate `9a815b298a0970ae34cbb87dcafda244d01a2df3` reported status `8`
with the same valid AR damage/response datums. This proves that native damage
owner construction and `unit_apply_melee_damage` both returned; only the stock
effects/response wrapper faulted. Retail call sites at `+0x08AE5C` and
`+0x35BC48` pass a full object datum in `r9`, and the wrapper forwards the full
value unchanged to `unit_melee_effects` after using only its low word for a
bounded table lookup. The candidate instead passed only `handle & 0xFFFF`,
discarding the datum salt. The preserved log is
`out/debug-openxr/9a815b2-forge-effects-wrapper-handle-fault.log`. The failed
probe is disabled before correcting that argument.

The corrected candidate passes the exact validated target datum handle to the
wrapper, matching both stock callers. The wrapper may use the low word for its
own object-table address calculation, but `unit_melee_effects` receives the
unchanged salted handle. No target substitution, trigger input, animation
request, or lunge is introduced.

Candidate `888987a06e923f92fd569691fc3647b6a3debddc` verified the final
visible-palette contact space and contact-point velocity classifier in live
Construct Forge, but exposed a validation-rig defect: the rig advertised a
synthetic `2.25 m/s` controller speed while pinning its synthetic visible pose
to the selected target. The new classifier correctly measured `0.00 m/s`, so
the run applied one impulse but requested no melee. This was a failed validation
candidate; production controller handling was unchanged.

Candidate `60893a70790742314bcb20647d4ea620afc6978b` made the rig's visible
weapon pose follow the same bounded one-second waveform as its synthetic motion
snapshot, reaching `2.25 m/s` and separating once per cycle. In authoritative
Halo 3 Construct Forge (`game=2`, `simulation=5`, `cooperative=0`, `options=1`),
the installed candidate selected loose kind-2 weapon `0xE2A00031`, measured
contact-point speed above the configured `1.50 m/s` threshold, and completed
the native authored-melee transaction without a fault. Status reported
`melees=2`, `meleeStatus=2`, `command=7`, and `applied=7`, with authored AR
datums `damage=0xEBF10A7B` and `response=0xEAEA0974`; the same transaction moved
the target `0.061` world units. MCC remained responsive. The preserved log is
`out/debug-openxr/60893a7-forge-high-speed-success.log` (SHA-256
`0FE3DD5187A63CDD70A13A1F1DE3244A0BCEF3E0A005DFFF011C936F266609B7`).
This proves the installed headless Forge high-speed impulse-plus-native-melee
path. It is not headset acceptance.

The user's first headset result on the `60893a7` line proved that weapon
contacts select and move other objects, but rejected the response quality:
objects "go flying", contact feels collision/force-offset, and the interaction
reads as a one-shot "spank" rather than a gentle continuous push. Commit
`7e34710` therefore leaves the exact target, sweep, and authored-melee paths
intact but disables the rejected first-overlap velocity kick. Candidate
`6c8accd` replaces it with a bounded contact constraint: on every overlap it
matches at most 25% of inward hand speed, caps desired target speed at
`0.30 m/s`, caps each correction at `0.08 m/s`, preserves tangential velocity,
and stops adding velocity once the target follows the contact. This replacement
was installed but remained an interim approximation.

Candidate `515a40a` replaces capsule/sphere final contact with the immutable
authored Havok convexes described above. The next mass-aware candidate replaces
the interim whole-object velocity correction with the proven native point
impulse. It preserves the exact contact point, uses the held weapon and target
body masses, and lets Halo compute angular response from target inertia. It
awaits headset acceptance.

Review of continued authored overlap found one deterministic force-direction
fault. GJK returns a separating plane immediately before first impact. If the
previous pose already overlaps, no separating plane exists. The old path then
invented a center-to-center normal. That normal changed as the prop moved. It
also used the prior weapon support point to measure the current rigid velocity.

The overlap-normal candidate stores the first reliable swept plane per target.
It reuses that plane during continuous overlap. Separation or reset clears it.
A contact first observed inside a target waits for separation. The path now
measures velocity at the matching current weapon support point and applies the
native point impulse at the exact target support surface. Pure tests distinguish
a swept plane from existing overlap and verify normal retention and cleanup.

The sustained-contact candidate keeps that plane orientation during release.
It does not flip the normal to oppose a separating velocity. Pure tests cover
gentle inward motion, a target already following the weapon, clean separation,
penetration-only load, loaded and unloaded tangential motion, friction limits,
light props, a `500 kg` vehicle, invalid mass, and the `0.08 m/s` per-sample
target-delta cap. Runtime telemetry now records the exact target handle and
kind, penetration depth, normal impulse, tangential impulse, and converted
weapon and target masses.

The fixed-body candidate then selected a real dynamic loose weapon in the
headless Construct Forge rig: handle `0xE29A002B`, kind `2`, motion type `4`,
and native mass `4.869 kg`. The installed candidate applied native point
impulses and moved it `0.040` world units. The same nominally slow `0.90 m/s`
run incorrectly reported weapon speeds up to `3.09 m/s` and repeatedly invoked
melee. The visible weapon transform includes Halo idle and authored animation,
so transform-delta velocity was not controller velocity. The preserved log is
`out/debug-openxr/45d3eb4-forge-dynamic-slow-false-melee.log` (SHA-256
`F5D14B2F7EBF57DB45C8918D587944B0250C9417835BE4DFEE9B5F9A4273636B`).

The tracked-motion correction retains the final visible palette and exact
authored convexes for collision location. Force and melee classification now
come only from the bounded lock-free OpenXR controller snapshot. Its linear and
angular velocities are rotated through the same on-foot tracking-to-game axes
as controller displacement. Angular velocity is evaluated at the exact weapon
support point. The exact target surface velocity, including target angular
motion, is subtracted there. Halo animation no longer creates force or melee.
An unavailable optional angular sample becomes zero; invalid required linear
motion rejects only physical contact. The validation rig now uses the signed
derivative of its exact one-second waveform, so a `0.90 m/s` run cannot cross
the `1.50 m/s` melee threshold.

The first one-shot scoop run exposed a debug-rig mismatch: the exact authored
weapon geometry completed the lift/carry path and native point impulses were
accepted, but the rig still published the unrelated forward sinusoid velocity.
The selected `4.869 kg` loose weapon therefore received 101 impulses without
moving from `(4.292, 6.489, 11.641)`. The corrected rig publishes the analytic
derivative of the same smooth lift, lateral carry, and downward separation
trajectory. This changes only the environment-gated validation path; normal
tracked contact remains driven by the OpenXR controller snapshot.

The corrected-velocity run then selected a `2.764 kg` loose kind-2 weapon and
reported the expected vertical path speed (`0.26 m/s` at the sampled point).
It still stayed at `(-1.843, -0.858, 3.117)` after 88 accepted point impulses.
The previous per-sample cap was `0.08 m/s`, below one 60 Hz gravity step, so
the floor solver removed every upward correction before the following sample.
The sustained constraint now adds `target mass * 9.81 m/s^2 * dt`, projected
onto the exact upward contact normal, while the weapon is not separating. The
existing velocity correction remains capped separately. Side contacts add no
weight support, and downward separation removes it immediately.

The next null-driver run still reported zero target displacement after 90
accepted native point impulses. Runtime status now includes the last published
world impulse vector and its exact GJK contact normal. Both values already live
in bounded atomic command storage; the worker only reads them while producing
the existing two-second status line. No logging or extra work enters the render
or authoritative object-update hooks.

That telemetry proved the scoop alignment itself was wrong: the last normal was
`(0.8955, 0.4412, 0.0582)` and the impulse was
`(-0.00379, -0.00811, -0.00094)`, both nearly horizontal. Matching one extreme
weapon support point to one extreme target support point joined unrelated
corners on irregular authored shapes. The rig now matches the two authored
compound centroids in the horizontal plane and uses the exact support points
only for vertical clearance. Its carry direction is projected onto the world
horizontal plane. Normal tracked contact geometry and force are unchanged.

Centroid alignment produced the intended exact vertical contact:
`normal=(0.0063, 0.0003, -1.0000)` and an upward native impulse of `0.09579`
world units. The selected `1.246 kg` map weapon still did not move. Construct
contains map-authored weapons whose root Havok body reports dynamic motion but
which remain constrained to their Forge spawn. The validation rank now gives a
higher tier to settled weapons from `1.8` to `2.2 kg`, selecting the repeatedly
proven loose `2.019 kg` weapon before constrained lighter map weapons. This is
limited to environment-gated target selection; product contact still follows
native dynamic-body evidence for every contacted object.

The next point-impulse run selected that exact previously movable `2.019 kg`
weapon, aligned an exact vertical normal, and applied 99 upward point impulses.
The body still moved `0.000` world units. Candidate `f830ce6` changed only the
environment-gated scoop response to the already bound authoritative
`object_set_velocities` path. In the same Construct setup it moved the same
handle `0xE2740005` by `0.124` world units under a sub-melee trajectory, with
111 applied responses and zero melee events. This isolates the failed floor
response to the point-impulse path or its placement in the object-update
transaction; target selection, native mass, readback, and the whole-object
physics/network path are live. The preserved log is
`out/debug-openxr/f830ce6-forge-scoop-world-velocity-moves.log` (SHA-256
`AE2E369EA3BB7572534B87A395985556445AE3B096C729C03FBA6AF20C72603B`).

The user's `f830ce6` Steam / SteamVR OpenXR / Quest headset session at 120 Hz
independently confirmed native campaign melee, but reported that gentle nudging
did not work and that contact with vehicles often destroyed things by accident.
Its log recorded 971 exact shape hits, 536 accepted point responses, 27 native
melee events, target masses from `1.246 kg` through `17045.13 kg`, and tracked
weapon spikes up to `25.03 m/s`. The preserved log is
`out/debug-openxr/f830ce6-headset-forge-120hz-user-session.log` (SHA-256
`7A9AEF4F4B0FFCAB138BA912C11915B9CB386D5B187DBC35D186C40164269909`).

The mass-correct replacement keeps the bounded impulse calculation for contact
load and haptics, divides it by the target's authored mass to obtain only that
body's velocity change, adds the result to Halo's native linear-velocity
readback, and publishes the absolute value through `object_set_velocities`.
The native call receives a null angular pointer, preserving the body's current
angular velocity. A loose prop therefore follows contact while a 10,000 kg body
gets a proportionally tiny change instead of the old mass-amplified point call.
Native melee remains a separate command and is unchanged. This replacement is
source- and pure-test-verified but awaits the controlled Forge matrix and
headset acceptance.

The first installed mass-correct run selected a different `2.008 kg` map
weapon. It accepted 112 velocity responses but moved only `0.010` world units,
below 5 cm at the live world scale, and returned to rest. Mass and motion type
alone therefore still cannot distinguish a truly loose body from a Forge
placement constraint. The validation rig now measures the full scoop result:
after six seconds it rejects any candidate that moved less than 5 cm, clears
the anchor and contact state, and continues through a bounded 32-handle reject
set until it finds a body that actually responds. Normal gameplay selection is
unchanged. The failed-target log is
`out/debug-openxr/191fb95-forge-scoop-constrained-target.log` (SHA-256
`1DF65C1B24A83187D1987AEE00EF90D2360DB65FEE78B7E526F5B96A544CD541`).

The installed mobility-selection candidate rejected that exact constrained
handle, then selected the previously proven loose `2.019 kg` weapon. It passed
the 5 cm check at `0.053` world units with 199 applied responses and zero
melees. Its exact retained normal was nearly vertical and the published value
contained an upward component, but the body ended `0.027` world units lower on
the floor instead of lifting. This proves the pre-update native velocity write
is still overwritten vertically during Halo's original object update. The
preserved log is
`out/debug-openxr/1d8898d-forge-mobility-selection-success-no-lift.log`
(SHA-256
`02D0F7506B3629A56C0506C073459D1AFBE5B29BBCFD96ED9CFB86FEA69B1506`).
The next candidate keeps native melee at its headset-proven pre-update point
but defers only the validated whole-body velocity write until immediately after
the same authoritative `objects_update` call. It revalidates the salted handle,
dynamic motion type, generation and finite velocity before the post-update
write; failure still disables physical contact alone.

Installed candidate `01143d8` proved that post-update placement fixes the
overwritten response. The automated rig rejected the first anchored map weapon,
selected a loose `2.008 kg` weapon, lifted it `0.094` world units, and carried
it `0.098` world units sideways. The body returned to the floor after
separation. The run applied 94 mass-correct responses with zero melee events
and no crash. Its preserved log is
`out/debug-openxr/01143d8-forge-post-update-lift-carry-success.log` (SHA-256
`7E8D6A8C9348CBB92F501F564422B25068A6AC57C88473CA0E29B9D7563A3E52`).
The original debug path stopped sideways before separation, so this run proves
lift and carry but not retained toss velocity. The next debug-only trajectory
separates downward while its lateral smoothstep is still moving. Native object
velocity readback records peak lift, horizontal carry, and post-separation
lateral speed. A target passes only at `0.02 m` lift, `0.05 m` carry, and
`0.05 m/s` release speed; the whole path remains below the `1.50 m/s` melee
threshold.

Installed candidate `a36a0a0` made the missing release momentum measurable.
The known loose handle `0xE2740005` lifted `0.342 m` and carried `0.075 m`,
but its maximum lateral velocity in the release window was only `0.002 m/s`.
The rig correctly rejected it instead of reporting a visual movement as a
successful toss. Three constrained Forge weapons were also rejected first.
The preserved log is
`out/debug-openxr/a36a0a0-forge-scoop-release-failed.log` (SHA-256
`C2FE68115ACC3A053BEF8870E26C80646694575D0D88161B515FF84A4E86A2A6`).
Halo's next object update removes most of the previous small correction from a
floor-loaded body. The sustained solver therefore now resolves the measured
relative velocity in one sample instead of 20 percent. Native reduced mass,
Coulomb friction, and the existing `0.08 m/s` per-sample cap remain in force.
Pure tests require at least `0.10 m/s` lateral correction for the representative
`2 kg` carried body while retaining more than a tenfold light-to-`500 kg`
vehicle response ratio.

Installed candidate `c994570` passed the complete automated slow-contact
transaction in Halo 3 Forge. It selected a loose authored `2.008 kg` weapon,
lifted it `0.229 m`, carried it `0.202 m`, and retained `0.541 m/s` lateral
speed after separation. All 120 accepted responses stayed below the configured
`1.50 m/s` melee threshold and produced zero melee events. The preserved log is
`out/debug-openxr/c994570-forge-scoop-toss-success.log` (SHA-256
`4161F4219A9A31C79953783275777152E40C95E349AD06967CB4B12810B6A7FC`).

The same installed candidate passed the independent fast-contact transaction.
The automated authored-shape sweep produced 360 exact hits, 289 mass-correct
physics responses, and 14 successful native melee events through damage tag
`0xEBF10A7B` and response tag `0xEAEA0974`. The melee count advanced while the
weapon crossed the threshold near `2.25 m/s`, then remained fixed after the
rig settled to `0.45 m/s`. All 326 published commands were applied and the
title stayed running. The preserved log is
`out/debug-openxr/c994570-forge-native-melee-success.log` (SHA-256
`F3C540B07721B3907CA91F1713A5375E53FFE564E5BE04F4043946AA62102124`).

The headset trace for `f830ce6` recorded isolated weapon-speed samples up to
`25.03 m/s`. These are not plausible hand swings and can falsely turn ordinary
contact with a vehicle into destructive native melee. Physical contact now
keeps samples through `8.00 m/s` eligible for melee, but classifies faster
samples as impulse-only. The normal mass-aware response and its existing
`0.08 m/s` per-sample target-velocity cap still run, so the guard removes only
the false damage event. Runtime telemetry counts these decisions as
`rejectMeleeSpike`; pure tests cover the exact limit, the first rejected value,
the preserved `25.03 m/s` case, and non-finite input.

Installed candidate `a7ddb49` then passed the complete guarded slow-contact
transaction. The exact visible Assault Rifle shape selected a loose authored
`2.008 kg` weapon, lifted it `0.287 m`, carried it `0.205 m`, and left it moving
at `0.441 m/s` after separation. It applied 130 post-update mass-correct
responses and published zero melee events. The independent fast transaction
still invoked Halo 3's native melee near `1.99 m/s`; synthetic samples above
`8.00 m/s` stayed physics-only. This proves the spike guard removes the
headset-observed accidental-damage path without removing normal physical melee.

The installed `575c868` build also ran a kind-10-only Construct Forge sweep.
It produced more than 2,378 exact authored-shape hits and more than 1,081
successful sub-threshold responses with zero melee events. Dynamic crate-class
bodies from `6.354 kg` through `300 kg` moved under contact. Handle
`0xE3AB013C`, with authored mass `6.354 kg`, moved `0.096` world units; heavier
`247.787 kg` and `300 kg` bodies retained only small bounded velocities. Several
map-constrained crates failed the rig's stronger scoop-and-toss acceptance, but
the measured displacement proves that gentle nudging is no longer erased by
Halo's next object update. The preserved log is
`out/debug-openxr/575c868-construct-kind10-slow-response-success.log` (SHA-256
`7C2059FC51EF7BFE6B1ADBE1896CCDBEFB1AC9C42612BEC525D0BDE66B57CB3D`).

The same installed build ran a kind-1-only High Ground Forge sweep against
Halo 3 vehicle bodies. The exact Assault Rifle solid (`shapeSource=1`, 20
authored vertices) produced 459 exact hits and 292 applied slow responses
against several dynamic `464.835 kg` targets. The observed bodies moved only
`0.017-0.083` world units per test pass and the largest logged final target
velocity change remained below the fixed `0.08 m/s` per-sample cap. The entire
run published zero melee and damage events. The `normalImpulse` status value is
the mass-aware contact load used for response and haptics; the native write is
the divided, bounded target-velocity change, so it does not reproduce the old
mass-amplified point impulse. The preserved log is
`out/debug-openxr/575c868-high-ground-kind1-vehicle-slow-response-success.log`
(SHA-256
`85C9349834AE8304AFD7EEAF81FD0DE384F0358A32BC1DA44853D3BD3D59037A`).

The earlier wall candidate traced only the grip and a bounds-derived tip. That
did not represent the visible authored solid. The exact wall candidate traces
every authored convex vertex from the camera through Halo 3's native structure
query. It keeps each returned surface point and normal. The normal is oriented
toward the camera-side free space. A bounded four-pass plane solver finds one
rigid translation that clears all hit planes, including corners. Each child uses
its exact authored rounded radius plus `5 mm` visual clearance. Release remains
bounded at `1.5 m/s`. The 44-tag census proves every held weapon uses at most
20 vertices. Runtime storage allows 64 vertices without allocation.

A camera-to-current-vertex ray alone does not prove continuous wall contact. A
fast sideways motion can cross thin structure and end with the vertex visible
again. The continuous-wall candidate retains the previous unconstrained weapon
transform. When a current camera ray is clear, it sweeps that same authored
vertex from its previous position to its current position through the native
structure query. A motion hit contributes the same camera-facing surface plane
to the rigid solver. Motion below `5 mm` per sample skips the extra query; it
cannot tunnel a thicker surface during that sample, and current occlusion still
handles penetration. One vertex contributes at most one plane, so fixed storage
remains bounded at 64 planes. The log separates `wallRays` and
`wallMotionRays` for headset performance verification.

The first exact-wall implementation passed only low structure bit `1`. That
explains the headset split where map and instanced walls blocked correctly but
Forge scenery walls did not: native type-4 object collision was never queried.
The static-object candidate adds the already proven high `0x7FFF` all-object
flags to the same camera and motion rays. A validated root object with a dynamic
Havok motion type stays in the impulse path. A fixed, keyframed, or unresolved
root object contributes Halo's returned surface point and normal to the rigid
wall solver and receives no impulse or damage. Invalid, attached, player, and
held-weapon handles do not constrain the weapon. `wallObjectPlanes` records
accepted native type-4 planes. Headset acceptance remains pending.

### Valhalla decorator rocks are render-only

The headset report that one Valhalla rock did not block the held weapon has a
separate authored-data explanation. The official H3EK archive contains
`riverworld.scenario_decorators_resource` alongside the BSP and scenery
resources. Its shared `rocks.decorator_set` references only the custom render
model `levels\shared\decorators\rocks\rocks` and its bitmap. H3EK's own
`print-tag-to-xml` output exposes four visual meshes (`%rock_a` through
`%rock_d`), random rotation, normal alignment, and per-type scale ranges. The
tag has no collision-model or physics-model reference. The embedded official
tag definition independently lists only the render model, instance names,
texture, render/light flags, fade/cull data, and decorator types.

Therefore the native BSP/object collision query cannot report these visible
decorator instances. Expanding BSP or scenery bounds would not be visual-contact
evidence and would reintroduce the rejected broad-proxy behavior. Complete
coverage requires a distinct immutable decorator transaction: resolve the
loaded scenario's decorator resource outside hot callbacks, resolve each
decorator set's render-model mesh and exact placed transform, build a bounded
spatial index, and query those triangles from the existing authored weapon
vertices. Until that data path is proven and uniquely bound in retail, native
BSP, instanced structure, and object walls remain exact while render-only
decorators remain a stated gap.

The 2026-08-10 read-only retail probe then verified that the ordinary loaded-tag
path is not a substitute for the resource path. Visible menu capture confirmed
Halo 3 Forge, Valhalla, and the built-in Forge game type before launch. The
probe matched the already shipped unique vehicle type accessor at retail RVA
`0x396D84`, decoded the same tag-instance-table and tag-data-base globals, and
walked every bounded readable loaded-tag root without writing process memory.
It found no root whose block data satisfied the official BSP-cluster bounds,
stride, and runtime-decorator record invariants. The tag-group table exposed
the `sbsp` definition entry only as its invalid-address sentinel. This negative
result is consistent with the official pageable BSP/decorator resource layout:
an exact fix must bind the streamed resource or the renderer's prepared
instance data, not guess an inline `sbsp` offset. The preserved probe report is
`out/h3ek-valhalla/retail-valhalla-decorator-probe.json` (SHA-256
`9FCDFC49B1366F5F3BAF345BE0FF30325A94449D02652DCD660AF6CA27FA3AF7`).

The same visible-state Valhalla run passed the existing exact native wall
transaction under the SteamVR null driver. It remained Halo 3 Forge throughout
and never entered Escalation Slayer. The preserved log is
`out/debug-openxr/20260810-222821041Z-wall.log` (SHA-256
`A4635F95D94C1C04785B7342AC7BA2186D79CC0250C103B0F81EDFB80EED1CA8`).
This is binding and regression evidence only, not headset acceptance.

### Dynamic-body query-gap hold

The headset report also identified a separate failure after a successful
nudge: the visible weapon could briefly clip through the moved body. The exact
body solver previously began its `1.5 m/s` visual-offset release on the first
clear camera sample. Halo advances a dynamic body in the physics update after
the camera query, so a single clear sample does not prove separation. The next
candidate keeps the last exact dynamic-body correction for at most `50 ms`
after the last validated hit, then uses the unchanged bounded release. Exact
hits still replace the correction immediately. Tracking loss, weapon changes,
title teardown, invalid clocks, and all normal contact resets clear the hold.
The unit transaction covers 49 ms, the inclusive 50 ms edge, release at 51 ms,
immediate re-contact, and invalid time. Runtime telemetry reports
`bodyGapHolds`; no mass, impulse, geometry, or melee rule changes in this
candidate.

Source `b154546` packages behavior commit `8e56ca1`. Its Steam candidate DLL is
SHA-256 `2666B42D9326132AB31452B319BC01EFF5CD812E0C0FFB5EE412687CE212D54A`.
A live High Ground dynamic-body transaction exercised the new path with
`bodyGapHolds=1`, real `464.835 kg` mass, detailed target geometry, bounded
impulse/release, and zero melee.

Visible menu capture then found a defect in the old unattended validator: its
three Right presses in the game-type carousel selected Escalation Slayer. The
validator now explicitly enters the map carousel, selects Construct 1/27, High
Ground 4/27, or Valhalla 11/27, and confirms the built-in Forge type. A second
visible replay on 2026-08-10 proved the final transition exactly: accepting
Forge options advances directly to Launch Game with its Start row selected,
and one Enter starts loading. The intervening Right and duplicate Enter could
leave MCC in the shell without starting Halo 3. Earlier fixed-sequence results
are not Forge evidence. With the corrected route, the installed `b154546` DLL
passed:

- Valhalla exact structure and placed-object wall transaction:
  `out/debug-openxr/20260810-231804734Z-wall.log`, SHA-256
  `349D7A3B6B7D54FAD3F99564021A954031706D4D0A134156B40EA1D19069F063`.
- High Ground Mongoose dynamic nudge/release with detailed geometry and zero
  melee: `out/debug-openxr/20260810-232213720Z-vehicle-nudge.log`, SHA-256
  `EF57C56BC594BF79CA2A798983336E36CB6E8F482148F1F136B33A498BA3655A`.
- Construct loose-weapon authored-geometry scoop/constraint with zero melee:
  `out/debug-openxr/20260810-232902359Z-weapon-scoop.log`, SHA-256
  `A58A143E1095EA289192F075220DB03D34166B788DB1328983F8E1E2FCEB746F`.
- Construct native fast melee response and haptics:
  `out/debug-openxr/20260810-233057407Z-melee.log`, SHA-256
  `E4B09EE63E5D8072FC44242F69F8075AE0934BE8943ED6AC2AC24D695F670728`.
- Construct exact visible-palette overlap/separation replay:
  `out/debug-openxr/20260810-233244424Z-visible-weapon-gap.log`, SHA-256
  `0C420AFABA1EAED27160B5025B3F5011C6B21511843E640DA9998642E4719694`.

All are SteamVR null-driver binding/regression evidence, not headset
acceptance.

The official H3EK reflection definitions additionally prove
`scenario_decorator_block` is `0x84` bytes, its decorator count is at `+0x34`,
palette/set blocks are at `+0x6C/+0x78`, each scenario set is `0x10` bytes,
and each authored placement is `0x18` bytes. Riverworld contains 25,971
placements, 25,963 in the current BSP, three palettes, and 11 sets. A corrected
read-only retail probe applied the engine's exact packed-address conversion
without the earlier invalid high-bit cutoff and read 11,243 live tag roots in
visibly confirmed Valhalla Forge. No root retained that exact scenario
decorator block. The preserved report is
`out/h3ek-valhalla/retail-valhalla-scenario-decorator-probe.json` (SHA-256
`66A145F754C26D4EAB7104BEB39721237021D353185BF77CC5FF365AD281CDFA`). This closes
the ordinary inline-tag route: render-only rock coverage still requires the
streamed decorator resource or renderer-prepared instance data.

Installed diagnostic candidate `cba2427` closed the automated wall-evidence
gap in Halo 3 Construct Forge. The null-runtime transaction forced the exact
visible Assault Rifle collision solid `0.12-0.16 m` through discovered native
surfaces. `shapeSource=1` supplied all 20 authored vertices. Native structure
type 1 and fixed Forge object `0xE2D10062` both produced rigid wall constraints;
the run reached 1,987 successful wall solves, 39,420 current camera rays,
12,925 continuous-motion rays, and 7,675 accepted static-object planes without
a crash. Both `structureValidated` and `objectValidated` latched true. The
preserved log is
`out/debug-openxr/cba2427-forge-native-wall-success.log` (SHA-256
`184879EFDE37AE84E580CB51ED8B2E4598319DE0DED6236937E1808C990F4BE9`).
This proves the production solver covers authored BSP/instanced structure and
fixed/keyframed Forge scenery with the same visible weapon geometry. Headset
feel and pixel alignment remain acceptance items.

### Authored weapon-face wall coverage

Review of the exact wall transaction found a geometry gap independent of map
content. It traced every collision-convex vertex, but a narrow static surface
can cross the middle of a broad weapon triangle while every outer vertex ray
passes beside it. The target wall can be exact and native yet still never reach
the rigid plane solver.

The replacement keeps every authored convex vertex and adds exact triangle
centres and edge midpoints from the already prepared held-weapon collision
mesh. The current and previous-transform native queries use the same local
point, so both present overlap and sideways tunnelling remain continuous. The
Assault Rifle's 20 vertices plus all 36 triangle centres and eight edge
midpoints fit in a fixed 64-sample budget. An edge midpoint covers the case
where a thin wall cuts a broad triangle while its endpoints and centre remain
clear. Larger held meshes keep every outer vertex and rotate evenly distributed
subsets of both feature kinds across fresh controller samples; no allocation or
new target proxy is introduced. Invalid or absent triangle data leaves the
existing vertex-only path unchanged.

The edge-midpoint extension passes the cumulative Release build, pure tests,
and the shared Reach consistency gate. Its exact installed candidate remains
headset-pending; the accepted-build pointer does not advance.

### Rotating-body visible-gap validator

The existing exact visible replay moves a final rendered weapon into a live
loose object's authored surface, but an ordinary resting object cannot prove
that the render-side constraint follows target rotation. The debug-only
`rotating-body-gap` transaction preserves the selected loose weapon's linear
velocity and applies a bounded 1.5 rad/s world-up angular velocity through the
already verified post-update `object_set_velocities` binding. The normal exact
replay then rebuilds that target's current authored geometry and applies the
same zero-penetration judgement to the final displayed weapon. A separate
counter proves that hundreds of rotation commands ran; the test cannot pass by
silently exercising the older stationary path. This environment-gated path is
absent from normal headset play and makes no production behavior claim.

This improves narrow BSP, instanced-structure, and fixed-object coverage
without naming a map or wall. It does not pretend that Valhalla's render-only
decorator rocks have native collision; those still require the separately
documented streamed decorator-resource or renderer-instance binding. Pure
tests cover the fixed budget, complete Assault Rifle centre set plus bounded
edge samples, even large-mesh sampling, phase rotation, and invalid indices.
Source `dc03a63` then
passed a visible-state Valhalla Forge wall transaction under the SteamVR null
driver. The assault rifle published all 56 samples, reached 13,660 native wall
rays, and validated both exact map structure and a fixed Halo object. The
preserved log is `out/debug-openxr/20260811-015156116Z-wall.log` (SHA-256
`98E7D648BC21BE9781C646BE150C05EC3CC93CC02A61B959E5F62FE87CC7E6F2`).
This is binding and regression evidence only; headset acceptance is still
pending.

Installed candidate `01f9401` extended only the environment-gated validation
selector so a run can require one Halo object kind. With kind `3` required, the
Construct Forge scoop transaction rejected 11 map-constrained equipment
objects before selecting live handle `0xE2BE004F`: a dynamic motion-type-4 body
with an authored runtime mass of `0.382 kg`. The exact visible weapon shape
(`shapeSource=1`) lifted it `0.088 m`, carried it `0.158 m`, and left it moving
at `0.106 m/s` after separation. The run applied 203 mass-correct responses,
published no melee or damage event, and kept the same target handle alive for
the extended observation window without an explosion. This proves gentle
scoop-and-toss behavior for the engine's equipment/grenade class; it does not
identify the exact frag, plasma, or other equipment tag. The preserved log is
`out/debug-openxr/01f9401-forge-equipment-scoop-success.log` (SHA-256
`605B119D8E50F8303A6363F8D3CA8D3F03A34A1E688D368A0E4658988C9F11AF`).

Animated bipeds do not expose one root convex. The earlier branch traced three
bounds-derived motion points and two non-temporal spine lines through Halo 3's
native object query. Fractions from those different lines were not comparable.
The authored-sample candidate replaced them with every exact weapon vertex and
one centre per disjoint child. All fractions then represented motion time.

An offline comparison against exact convex sweep proved that vertex and centre
samples still missed broadside contact. Against the official assault-rifle
shape, 52,580 random motions produced an exact hit. Vertex and centre lines
missed 3,970 of them and reported 289 false hits. Adding all vertex-pair
midpoints still missed 2,965 and reported 342 false hits. Point sampling is
therefore retained only as native material evidence. It cannot create or
override object contact. Unsupported target shapes remain inert. It is not the
final biped contact decision.

The official H3EK `physics_model` tags for Master Chief, male and female
marines, brutes, and elites each contain ten node-bound rigid bodies. The five
tags use authored pills and polyhedra for the head, torso, pelvis, arms, and
legs. Pill endpoints and polyhedron vertices are local to each named render
node. The already proven `g_halo3InterpolatedNodes` provider returns the exact
world-space animated node matrix used by the visible model. The exact biped
candidate reads that fixed body block, resolves each Havok child, transforms it
through its animated node, and runs the same continuous compound GJK sweep used
for props. The earliest body hit supplies the exact weapon child, target child,
surface point, normal, and temporal fraction. A provider fault rejects only
that contact sample. It does not change VR ownership or the node binding.

### Detailed movable-target collision geometry

The installed `2909e2d` product uses detailed authored collision geometry for
the held weapon, but movable target objects still use their Havok physics
shapes. The official H3EK Mongoose tags prove why that target fallback is not
precise enough for vehicle contact. `mongoose.physics_model` contains one root
polyhedron with 28 vertices. `mongoose.collision_model` instead contains six
regions: bumper, hull, and four wheels. The default permutation combines ten
simultaneous BSP children and 269 vertices across the visible root, fender, and
animated wheel nodes. The collision tag has 14 render nodes. Its largest child
has 70 vertices. The official XML exports are preserved under the ignored
`out/h3-contact-target-geometry` directory. Their SHA-256 values are
`1707ABC5361063888AF481844B328FC048482AB78E6C45C898084E36D6351F76`
for the physics model and
`6CE1F148706335F6A5581D9E7E1DF10F515F25869F43DBE78FD186D0F5B83689`
for the collision model.

The failed `d71955d` detailed-target experiment raised the fixed compound limit from eight to
sixteen children. This holds the complete ten-part Mongoose while keeping the
worst supported held-weapon/target pair at 4 x 16 = 64 convex tests, the same
pair-count ceiling as the former 8 x 8 limit. The proven 44-tag held-weapon
census limit of four children is checked before every sweep. It reads the same
official collision-model blocks already proven for the visible held weapon,
maps every child through Halo's live interpolated node bank, and sweeps the
two detailed compound shapes. A complete detailed shape owns the contact
decision; a miss
does not fall through to the larger physics proxy. If the detailed tag or node
bank cannot be validated, only that target uses the existing physics fallback.

Vehicle collision regions have default, medium, major, and destroyed
permutations. The official tag stores the default first in every Mongoose
region, but no verified retail binding currently exposes the selected live
damage permutation. Therefore a proposed surface from a multi-permutation tag
must also be confirmed by Halo's live native object query against the exact
target and point. Failed confirmation rejects the contact rather than trusting
the default shape. Status telemetry reports `targetShapeSource=1` for detailed
collision, `2` for the physics fallback, and `3` for animated physics bodies.
It also reports the current detailed/fallback candidate counts and native
confirmation rejects, so a missed contact can be separated from unavailable
geometry without hot-path logging.

The Forge result disproved the vehicle part of that implementation. A loose
kind-2 weapon used `targetShapeSource=1` and passed its detailed scoop test. On
High Ground, kind-1 Mongoose candidates instead reported
`targetShapeSource=2`, `targetDetailed=0`, and `targetFallback=1`. Halo reported
the expected `464.835 kg` vehicle mass, but the authored collision-model reader
did not produce a usable detailed shape. The failed run is preserved at
`out/debug-openxr/20260810-063345019Z-vehicle-nudge.log` (SHA-256
`11A0EF28EF602DA2827BE29A5621D95C78A583C2DD4EBFE752FF3EF185E6E3BA`).
The behavior was disabled before the next experiment.

The next candidate adds a debug-rig-only read probe. It does not use the
candidate geometry for contact. It walks the live target node bank and loaded
tag blocks once when the rig anchors a vehicle, then publishes the first failed
stage in the normal two-second status log. Stages 1-4 cover the node provider,
5-7 the object/model/collision tag chain, 8 the region block, 9 the first live
permutation, 10 the BSP block and fixed child bound, 11 the node mapping, 12 the
vertex block, 13 finite vertices, and 100 a complete ten-part walk. This probe
exists only to replace the failed implementation with runtime evidence instead
of another guessed fix.

The installed `683d9cc` probe stopped at stage 2 for every selected High Ground
vehicle: the interpolated-node provider returned false and supplied no bank.
The preserved run is
`out/debug-openxr/20260810-070234987Z-vehicle-nudge.log` (SHA-256
`6BF60AF90C114D835B33038D7BD27788C90FA891EA8C09B92DA5CD558E076300`).
This is not a provider fault. Halo's already verified visible-object renderer
uses the raw object node bank when the same provider reports false. The next
read-only probe copies that exact bounded fallback and reports `nodeSource=2`
for raw or `1` for interpolated before it continues through the collision tag.

The installed `77486ce` probe completed at stage 100 with `nodeSource=2`.
It read all 14 Mongoose render nodes, all six collision regions, and all ten
simultaneous default-permutation BSP children. The final child used wheel node
13 and contained 18 finite vertices. The preserved run is
`out/debug-openxr/20260810-092001942Z-vehicle-nudge.log` (SHA-256
`DBCA7E9B71E638294D7C38066CDAAF096B8CF5A0579DE50406049CD5FFBABA8C`).
This closes the failed vehicle geometry path: detailed target contact may use
the renderer-proven raw bank when the interpolated provider reports false,
while a provider fault still rejects only that contact sample.

The first installed product run loaded one detailed target on every vehicle
sweep (`targetDetailed=1`, `targetFallback=0`) but the debug rig produced no
hit. The product reader was not the failed stage. The rig still aligned its
synthetic weapon to the vehicle's old 28-vertex physics hull while the contact
decision used the ten-part collision model. The next validator-only change
aligns its scoop trajectory to the same detailed target shape and transform.
Normal tracked-controller contact is unchanged.

That whole-compound validator alignment failed and is disabled. Installed
`1ba6663` still loaded `targetDetailed=1` with no fallback and completed the
ten-child probe, but it produced zero exact hits. The preserved run is
`out/debug-openxr/20260810-094547644Z-vehicle-nudge.log` (SHA-256
`21B9D71247DE7D111DEA3130758E55BA78CCB89AA2D41B435C2F6A1C5FDFCD11`).
The geometry is disjoint: a global lowest support point can lie on a wheel
while the compound centroid lies in empty space between vehicle parts. The
next validator must align one concrete convex child to one weapon child, not
mix extrema from the whole compound.

Installed `810eb00` passed the High Ground vehicle transaction after the rig
aligned its largest weapon child to the largest Mongoose child. Runtime contact
reported `targetShapeSource=1`, `targetDetailed=1`, `targetFallback=0`, all ten
collision children, the authored `464.835 kg` vehicle mass, and zero melee
events. The exact contact moved the vehicle `0.922 m`; the rig measured
`0.383 m` peak lift, `2.770 m` peak carry, and `1.666 m/s` release speed. The
preserved log is
`out/debug-openxr/20260810-095653341Z-vehicle-nudge.log` (SHA-256
`814990296F3E3EF717DF64471CEE59BAAD3B1D88130FFDA8CC5658B67ABBB041`).
This null-driver result proves the automated geometry/physics transaction. It
is not headset acceptance.

The bounded synthetic Release benchmark uses 70 vertices per target child,
matching the largest official Mongoose child. Four held-weapon children against
all ten Mongoose children measured `0.0655 ms` p95. The full four-by-sixteen
storage bound measured `0.1044 ms` p95. Both remain below the fixed `0.25 ms`
contact budget. The Forge validator now rejects weapon and vehicle results
unless the runtime log proves `targetShapeSource=1` and at least one detailed
target candidate. Physics-fallback movement cannot pass as precision-contact
evidence in a future candidate.

### Post-separation velocity restoration

The first headset result for sustained response proved that physical contact
could move objects and that native melee worked in Campaign, but gentle nudging
was usually lost and touching a vehicle could still cause a destructive event.
Four bounded experiments identified the missing behavior. A tiny native wake
impulse before or after the bound velocity setter did not survive separation.
Replaying an absolute setter write after separation published and applied the
commands but left the target nearly stationary. Reusing the last solver impulse
was unstable because that correction shrinks while a target follows the weapon.
Using only the last relative velocity had the same fault. Those experiments are
preserved by commits `b60e971`, `ccbdbc6`, `dab6f9e`, and `70cba51`; each failed
behavior was reverted before the next candidate.

Installed product commit `2909e2d` instead retains the tracked weapon velocity
at the exact contact point. After exact shape separation, the authoritative
simulation hook waits until `objects_update` completes. It then rereads the
target's real linear and angular velocity, recomputes target velocity at the
same contact point, and sends only the missing velocity as one native point
impulse. The command revalidates the datum generation, exact handle, dynamic
motion type, finite values, bindings, and authored target mass. The native
impulse is capped at `1 kg m/s`. Fast melee contact clears this slow-release
latch, so it cannot receive both paths. Release adds no second haptic pulse.

The installed DLL for these runs has SHA-256
`0B105ACDD7303AAD9D658C43C72EA57DC492B0F6DC6F3E542614F6BBBB0D9919`.
SteamVR's null driver exercised the following independent transactions against
that exact product commit:

- A dynamic kind-3 equipment body with authored mass `0.382 kg` used
  `shapeSource=1`, lifted `0.254 m`, carried `0.310 m`, and retained
  `0.068 m/s` after separation. It produced 55 release commands and no melee.
  The preserved log is
  `out/debug-openxr/20260810-034729106Z-equipment-scoop.log` (SHA-256
  `D6373408E63278F1D50753E62D7719B9FEE3DBCB0B295E01DFAF73A3FB0EA08C`).
- A dynamic kind-2 loose weapon with authored mass `2.764 kg` used the same
  exact shape, lifted `0.048 m`, carried `0.351 m`, and retained `0.260 m/s`.
  It produced 19 releases and no melee. The preserved log is
  `out/debug-openxr/20260810-035042017Z-weapon-scoop.log` (SHA-256
  `C9F684A7ABF68A7B49B71D53544A9FD00DE1FC7C9B0BEEC9CB32AC4151A34AB8`).
- A dynamic kind-1 vehicle with authored mass `464.835 kg` received 33 slow
  responses and five post-separation releases with zero melee. Its last native
  release impulse was `0.34753 kg m/s`, below the fixed cap. The preserved log
  is `out/debug-openxr/20260810-035810179Z-vehicle-nudge.log` (SHA-256
  `92C66BF8E1692FF4D510EAD58E385CFB892A36264C300E7AEEA3B5086E6456E3`).
  The original result file rejected this run because the two-second status
  sample missed its one-frame haptic. Validator commit `f5dd5e5` replays the
  log as valid by requiring durable applied-command counters, exact vehicle
  type and shape, positive authored mass, zero melee, and a release impulse no
  larger than `1 kg m/s`. Its negative checks reject an oversized impulse,
  melee, a wrong object kind, or an unapplied command.
- A dynamic kind-10 crate with authored mass `6.354 kg` accepted exact slow
  contact at `0.20 m/s`, produced a bounded `0.07223 kg m/s` native impulse,
  and produced no melee. The preserved log is
  `out/debug-openxr/20260810-040602136Z-crate-nudge.log` (SHA-256
  `55BC999E138F0EDD072C97F1DC3C0E5550D9C092FA31CFD7D3A9DD2E8CDE0AA5`).
- The wall transaction forced the 20-vertex authored Assault Rifle solid
  `0.158 m` into native structure and fixed-object surfaces. Both
  `structureValidated` and `objectValidated` latched true. The preserved log is
  `out/debug-openxr/20260810-040805553Z-wall.log` (SHA-256
  `9E567529F86A0D2E19856E22B1A3BD2DF282E6F79E5B1CEA73BFA19D8BCC446A`).
- The independent fast transaction crossed the threshold at `1.99 m/s`, used
  `shapeSource=1`, and completed exactly one native melee response with
  `meleeStatus=2`. The preserved log is
  `out/debug-openxr/20260810-041342707Z-melee.log` (SHA-256
  `D05943331BBEFEDEC6CA77ED47C0E51B740507451C5098F480E9BB004D180062`).

These null-driver runs prove the installed command paths and their readback.
They do not prove headset feel, natural hand tracking, visual alignment, or
vehicle safety in ordinary play. Those remain headset acceptance items. The
debug validator also now stops SteamVR before restoring the byte-exact user
settings and leaves it stopped; commit `316dcf9` removed an intermittent late
null-driver rewrite. One later navigation attempt selected Escalation Slayer
instead of Forge and was rejected before Halo 3 gameplay validation. That menu
failure is not product evidence.

## Rejected exact render-model identity

Candidate `e91f451` read the primary prepared first-person slot's datum at
`+0x4C` and required its low 16 bits to equal the tag passed to the final
visible-palette submission. The automated exact-visible Forge replay reached
Halo 3 gameplay, but `palettes=0`, `shapeSource=0`, and the contact state
remained at `visible-pose`. The strict equality therefore rejected every final
weapon palette instead of separating the weapon from its attachments. The
preserved log is
`out/debug-openxr/20260810-151337682Z-visible-weapon-nudge.log` (SHA-256
`134262C419761FFC7D53604EC60D4CB45E1174BB6DC707FAFEE66C26DF2B3650`).
The equality is disabled before any replacement experiment; the proven slot,
field, and submission hook remain leads, not a proven cross-hook identity.

Probe `bfc08ce` restored the earlier bounded wrist filter and recorded identity
only. The final hook consistently saw `0x19FB` with 37 nodes (the first-person
body) and `0x0B2B` with 5 nodes (the weapon) from the slot-0 interpolation
source. The prepared-slot lookup missed 48 of 48 calls because the interpolator
returns a distinct output buffer, not the slot's `+0x4A4` prepared bone array.
The visible replay still passed through the bounded filter. Its preserved log
is `out/debug-openxr/20260810-152353775Z-visible-weapon-nudge.log` (SHA-256
`01C557A12D94488F0EFCC88313C2664B20E1043E81CDA6E9A68E1AAB27624B83`).
The hook already receives the authoritative slot number, so the next probe
reads slot 0 directly rather than inferring it from an unrelated buffer.

Probe `71dea3c` then read the prepared slot directly on every call. It reported
`prepared=0xFB9319FB`, 44 slot matches, zero misses, and the same two final
submissions. Low tag `0x19FB` is therefore the 37-node first-person body; it is
not the 5-node held weapon (`0x0B2B`). The earlier interpretation of `+0x4C`
as the held weapon's render identity is runtime-rejected. The passing broad
visible replay is preserved at
`out/debug-openxr/20260810-152904233Z-visible-weapon-nudge.log` (SHA-256
`7C00335429F9C9F34E04F2208098EDEB047EC14405D252AEFC8584C98464BC16`).

Probe `0ebfd26` performed one bounded scan of the fixed prepared-slot header
before the `+0x4A4` bone bank. For final weapon tag `0x0B2B`, it found exactly
one matching datum: `0xECA10B2B` at slot `+0x44`. The live `+0x4C` datum
remained `0xFB9319FB`, matching the separate 37-node body submission. The
one-time scan is disabled after discovery; the product path reads only the
proven `+0x44` field. Its passing broad-filter replay is preserved at
`out/debug-openxr/20260810-153652702Z-visible-weapon-nudge.log` (SHA-256
`027674855EB715DE4EFB1E624DA228BBA79EDE019DA40FB0E602EA745EA60790`).

## Rejected first-person root composition

The exact visible replay proved that the contact publication and the pixels do
not yet share one verified transform. Candidate `77011f0` commanded a visible
surface gap from about -6 cm to +2 cm while retaining the local-palette contact
publication. Contact instead measured `1.7213 m` to `4.5880 m`; its preserved
log is `out/debug-openxr/20260810-121701541Z-visible-weapon-gap.log` (SHA-256
`BDBC587117559E3985994F9D20CFFA735AE1D329FB4307954049862ADB5F8AA0`).

Candidate `e9975ee` then tried publishing `root * destination[node]` and
converting the debug replay back through `inverse(root)`. Its installed exact
Forge replay also failed: the measured gap expanded to `-88.4815 m` through
`0.9001 m` rather than the commanded centimetre range. The preserved log is
`out/debug-openxr/20260810-122923424Z-visible-weapon-gap.log` (SHA-256
`6BDC11C62C913F3DB7420018C02B746B0EBE9ADEC90EC281C297A567EAC96494`).
Candidate `e9975ee` is therefore rejected; its behavior is disabled before any
alternative. Neither failed transform is evidence of pixel/contact alignment.

## Exact-contact haptics

Physical contact previously published no haptic event. The only feedback came
from Halo's ordinary XInput rumble, including native melee effects, so it could
not identify the exact authored contact point or the successful point-impulse
transaction. The exact-contact candidate carries one bounded amplitude in the
existing atomic simulation command. The authoritative `objects_update` hook
raises a lock-free right-hand peak only after the native point impulse or native
melee event succeeds. It performs no OpenXR call, allocation, lock, logging, or
file access. The normal VR frame loop consumes the peak and mixes it with stock
rumble: game feedback remains on both hands, contact adds only to the right, and
the stronger value wins. Slow feedback follows the square root of the exact
mass-aware normal/tangent impulse, capped at `0.60`; native melee has a `0.75`
minimum. The universal `haptic_intensity` still scales both sources. Tracking,
focus, menu, pause, and title-capability gates drop pending contact feedback.
Headset amplitude acceptance remains pending.

## Triangle-accurate collision replacement

The per-BSP convex fallback is not precise enough to remain the contact
authority. Reconstructing every official surface loop from the extracted H3EK
XML proves that the Assault Rifle's one 20-vertex BSP occupies
`0.000231689` cubic world units. The convex hull of those same authored
vertices occupies `0.000271197`, 17.05 percent more volume. Across the 44
available weapon collision tags, ordinary weapons are worse: the plasma pistol
is 123.43 percent larger, the rocket launcher is 123.25 percent larger, and the
plasma rifle is 117.18 percent larger under the old fallback. Vehicle/turret
garbage can exceed 900 percent. This is direct evidence that one convex child
per BSP creates contact in authored empty space.

The Mongoose also disproves the assumption that its ten BSP children are
individually convex. Its 502 authored triangles span ten default-permutation
children. Weighted by triangle count, the old convex children add about 60.62
percent extra volume. The hull child alone adds 152.31 percent; the bumper adds
52.97 percent. This explains a contact being reported beside the visible
vehicle even after its live animated collision model was loaded correctly.

Official `guerilla.exe` field-definition tables close the loaded runtime
layout instead of guessing it. Three collision-BSP field arrays list the same
ordered blocks: `bsp3d nodes`, `planes`, `leaves`, `bsp2d references`,
`bsp2d nodes`, `surfaces`, `edges`, and `vertices`. With the proven 12-byte
loaded tag block and 0x64-byte BSP, the block offsets are respectively `+0x04`,
`+0x10`, `+0x1C`, `+0x28`, `+0x34`, `+0x40`, `+0x4C`, and `+0x58`.
The surface field table proves a 12-byte record whose first-edge index is the
second 16-bit field. The edge table proves six consecutive 16-bit fields:
start vertex, end vertex, forward edge, reverse edge, left surface, and right
surface. No retail address or copied engine layout is used for discovery.

The replacement walks each closed half-edge surface and triangulates it into
fixed storage. The full official weapon census fits at most 222 triangles and
three BSP groups. The complete Mongoose fits 502 triangles and ten groups. The
runtime limits are 768 triangles and 32 groups; malformed, open, oversized, or
non-finite geometry rejects only physical contact and falls back loudly through
the existing `shapeSource` telemetry. A two-level group/triangle bounding-sphere
test removes distant pairs before GJK. The actual surface decision uses the
triangle mesh, a 2 mm continuous-sweep step, nine binary refinements, and a
1.25 mm one-sided surface skin to prevent tunnelling between adjacent samples.
The former convex child remains only for broad phase, support points, native
material samples, and the already documented physics fallback.

Pure tests cover fast thin-surface crossing, a grazing miss, separation between
disjoint triangle groups, malformed group bounds, mesh-to-mesh contact, and
mesh-to-convex fallback contact. Live Forge performance and headset acceptance
remain pending for the replacement candidate.

### Valhalla headset result and dynamic separation constraint

Source `6a7ae5a` was tested in Halo 3 Valhalla Forge on the Steam edition,
SteamVR/OpenXR 2.17.6, Oculus/Quest path, at 120 Hz. The installed DLL was
`F83AA191034F3E56D1CE3EB1976B6CC277E4750050A98E9F083684D4C769489A`.
The preserved log is
`out/test-runs/6a7ae5a-h3-valhalla-headset-rejected-20260810-131849PDT/halo3xr.log`
(SHA-256 `4F5ECD45D5DD15DBE34E18CA84E7101BBEA7520BC5054C50A4BA0D007F131DDA`).
The user reported that exact weapon/object geometry was a large improvement,
so the triangle replacement remains active. The same run rejected response
behavior: the weapon could clip through a body after a nudge, lifting therefore
failed, and sustained Mongoose pushing caused accidental melee damage.

The log agrees with that distinction. Exact held meshes remained active at 36
and later 80 triangles, while exact targets ranged from 12 to 438 triangles.
The response nevertheless recorded end-pose penetration from centimetres to
more than half a metre, and 42 native melee events during the session. An
impulse can change target velocity but cannot by itself constrain the rendered
kinematic weapon, especially against a floor-loaded or very heavy body.

The next response candidate therefore keeps the exact triangle sweep and
mass-correct impulse, but adds a separate visual dynamic-body constraint. It
recovers the controller-intended pose by removing the prior published wall and
body translations, sweeps from the last rendered constrained pose to that
intent, and rejects only travel into the exact target-facing normal. Tangential
motion remains free for sliding, scooping, and carrying. End-pose penetration
covers rotation about the grip; a 1.25 mm clearance and one-metre bound preserve
finite behavior. Blocking engages in one update and releases at the existing
bounded 1.5 m/s rate. `bodySetback` reports the independent live correction.
Pure tests cover tunnelling, tangential sliding, rotational penetration,
clamping, and invalid normals. Headless Forge and headset acceptance remain
pending.

Source `53a9c6a` adds cumulative constraint-count and peak-setback telemetry to
that same behavior; it does not change the constraint or impulse calculations.
Its installed DLL is
`541A52B48043DB57681EC1CF551783679489F978D313D40BE84E00044587BC1D`.
The first vehicle validation incorrectly reused the loose-prop scoop path and
crossed the vehicle deeply, so its one-metre clamped peak is rejected as a
validation-rig result. Validator source `6f4aab2` instead keeps one vehicle and
uses the bounded slow nudge path.

That corrected High Ground Forge run is preserved at
`out/debug-openxr/20260810-210910821Z-vehicle-nudge.log` (SHA-256
`F9F3F86FD8E7B10CA310F2CABED99107FE88C21A89BEBBE68AA2947A0E8F845D`).
It selected one live Mongoose, used its 502-triangle authored mesh and
`464.835 kg` native mass, applied 164 impulses and 21 releases with zero melee,
and recorded 170 constrained frames with a `0.317 m` peak. The exact target had
moved `0.006 m` at validation. The same installed DLL then passed the loose
weapon scoop at
`out/debug-openxr/20260810-211200237Z-weapon-scoop.log` (SHA-256
`0A08905A1AF3C380CF226D1E10BEC9EF6DECAD30402BA6FE1BC61E4FB085CCD2`):
the `2.008 kg` target reached `0.738 m` lift and `0.389 m` carry with a
`1.195 m/s` release, 116 constrained frames, and zero melee. Both runs used
the SteamVR null driver; they prove the installed bindings, exact geometry,
native mass, bounded impulses, and constraint transaction, but not headset
alignment or feel. Real-headset acceptance remains pending.

### Stable melee separation

Candidate `ef64b38` first changed melee classification from total weapon speed
to the inward normal speed of a fresh exact contact. It correctly made
tangential sliding, separating motion, and continued pressure physics-only,
but its one-frame overlap debounce rearmed whenever triangle contact flickered.
The deliberate-hit rig consequently produced repeated native melee events
during one apparent interaction. The failed behavior was reverted by
`c48527e` before its replacement.

Installed product source `d435237` keeps the fresh inward-impact rule and
retains each target's contact state through gaps shorter than `100 ms`. Only a
real separation rearms that target; weapon/title/tracking resets still clear it
immediately, and the independent `250 ms` native-melee cooldown remains. Its
Steam DLL is
`470E8C78907E864EE4BE955ADA070F34B3166CEB6AA791EC16758E4F398FC626`.
The corrected slow Mongoose transaction is preserved at
`out/debug-openxr/20260810-214106637Z-vehicle-nudge.log` (SHA-256
`EF731D2BBBA928FF466EA03858107D79F9B9EFB89B2EB39BDF2A9233A5B24547`).
It applied 166 impulses and 32 releases against the `464.835 kg`, 502-triangle
target with zero melee, including samples where target motion raised relative
speed above the melee threshold. The independent one-second sinusoidal rig
then continued to produce native melee on fresh inward impacts; its preserved
log is `out/debug-openxr/20260810-214328397Z-melee.log` (SHA-256
`F3C16DE207E008FDC954CD7CC6EA06A7E434F3232756439C03BD8967A374DF86`).

The same installed DLL passed exact static walls, loose-weapon scoop, and
visible-palette alignment. Those logs are respectively
`20260810-215254860Z-wall.log` (SHA-256
`582AE4DB1783583EE77DB08D07DDE6CF990810AACB2A70D67A43C816491219FE`),
`20260810-215448515Z-weapon-scoop.log` (SHA-256
`F50E5BBBA6108A34EA467C6088F23D1782D4C8EEEDB9425730143655EC328C7F`),
and `20260810-215644102Z-visible-weapon-gap.log` (SHA-256
`0092139F3FB2F01D459495FD840B2BC4583CE33A52B59F5CC017F052DCE6FAB9`).
All are null-driver evidence, not headset acceptance.

### Headset feedback: enemy-only first-contact swing speed

The user tested installed source `b154546` in Halo 3 Campaign and Forge on the
Steam edition through SteamVR/OpenXR 2.17.7 and the Oculus/Quest streaming
path. The preserved log is
`out/test-runs/b154546-headset-feedback-20260810-171722PDT/halo3xr.log`
(SHA-256
`D0F30EB72957D2994F1A5E968A766A11D7BB31FEC755DCBDA6B093C08799F83E`).
The user accepted object interaction and nudging as working well enough, so
this candidate does not change impulse strength. The same run recorded 5,623
exact hits, 3,232 impulses, 245 releases, 35 native melee events, and 865
animated-body hits. The user nevertheless reported that meleeing Campaign
enemies was too difficult.

The existing classifier measured only first-contact velocity closing into the
exact target-facing normal. That remains correct for vehicles and props: it is
what stops a fast tangential shove from becoming melee. Animated enemy body
normals, however, turn sharply across limbs and can reduce a deliberate weapon
swing to almost zero normal speed. The next candidate therefore keeps the
normal rule for every non-enemy kind. On a fresh exact biped, creature, or
giant contact only, it uses the greater of normal closing speed and tracked
weapon-point speed. It excludes target velocity, so a moving enemy cannot turn
a stationary weapon into damage. Existing per-target separation rearming,
the 250 ms cooldown, the configured 1.50 m/s threshold, and the 8 m/s tracking
spike rejection remain unchanged. Pure tests cover enemy tangential hits,
stationary weapons, sustained contact, and unchanged vehicle/prop behavior.
Headset acceptance remains pending.

### Exact animated-body impulse target

The same `b154546` headset run recorded 865 exact animated-body hits, while
the user reported that physical interaction with bodies still felt poor. Code
review found a deterministic mismatch after collision: the animated sweep
returned `rigidBodyIndex` for the exact authored head, torso, arm, pelvis, or
leg body, but the closest-hit record discarded that index. Mass lookup and the
simulation command then selected the component root body at `+0x0C` instead.
Thus collision could occur on one visible limb while response was applied to a
different rigid body.

The replacement carries the selected index through the bounded command. It
validates that index against the runtime component body count, reads that
body's wrapper, inverse mass, and motion type, and calls the already proven
`havok_component_apply_point_impulse` with the same exact body index and
surface point. This path is used only for target shape source `3`, the fixed
H3EK-authored animated multi-body sweep, and only for body indices `0..31`.
Single-body props and vehicles keep their headset-proven whole-object velocity
path. Dynamic ragdoll bodies receive their own mass/inertia response;
keyframed living bodies remain non-movable while their independent native
melee path continues. Telemetry now reports `targetBody` and `commandBody`.
Pure tests cover the exact-body source/index boundary. Runtime and headset
acceptance remain pending.

### Left-hand physical pickup candidate

The pickup candidate reuses only the already proven Halo 3 object table,
authored collision-model extraction, Havok mass/motion lookup, object center,
and post-`objects_update` velocity binding. OpenXR now publishes the left aim
pose, linear velocity, angular velocity, grip value, timestamp, and serial as
one bounded lock-free snapshot. The camera callback never enters the legacy
controller lock.

The palm is a `5.5 cm` sphere at the same configured wrist-to-visible-palm
point used by two-hand aiming. A bounds sphere is only the broad phase. A grab
candidate must overlap the target's fixed-storage authored triangle mesh or,
when that is unavailable, its proven Havok convex. Candidates are limited to
root dynamic weapon, equipment/grenade, garbage, and crate-class objects with
a native mass from `0.01` through `25 kg`. The player, held weapon, bipeds,
vehicles, scenery, projectiles, parented attachments, keyframed bodies, and
invalid/stale data are rejected.

Left grip uses `0.65` acquisition and `0.45` release hysteresis. Acquisition
stores the current center-to-palm offset, so the object does not snap to the
controller. Each later sample adds the tracked palm velocity to a bounded
position correction (`12/s`, at most `2.5 m/s`) and caps total follow/release
speed at `8 m/s`. Halo applies that linear and tracked angular velocity after
its own object update; the object's native body, mass, inertia, contacts, and
gravity remain intact. Release publishes the tracked palm velocity once for a
bounded toss. The left-bumper/grenade input is withheld only while an exact
candidate or active hold was published within `120 ms`; everywhere else the
stock input remains available.

Pickup owns a separate runtime binding and failure atom. A pickup fault clears
only pickup and cannot disable right-hand contact, native melee, the camera,
or OpenXR. Pure tests cover kind/motion/mass eligibility, exact-overlap
requirement, grip hysteresis, no-snap follow correction, world-scale
conversion, correction and toss clamps, and invalid scale. Runtime and headset
acceptance remain pending.

Source `95047a6` adds an environment-gated scripted left-palm transaction to
the existing Forge validator. It reuses the proven movable-object selector,
but returns before the right-hand sweep so the test cannot move the target by
weapon contact. The scripted palm must overlap authored target geometry,
acquire with grip, lift and carry through the normal camera-to-simulation
handoff, then release with measured velocity. Production controller input and
pickup behavior are unchanged when the environment variable is absent.

The exact installed candidate is
`out/package-worktrees/95047a6/out/candidates/95047a6-h3-physical-contact-20260811-032955950Z`.
Its DLL is
`C5CA7A67802971C88F34413608791E68068ED931BC2448D67CE7D25E05A10048`
and its launcher is
`659F4A16A53459B779C03D443DDC4AA59CCD95668986169D39FF1434A2474FAB`.
The clean Release build and full `ctest` passed before automatic Steam install.

The corrected scripted run passed at
`out/debug-openxr/20260811-033900709Z-left-grab.log` (SHA-256
`13EAC59F5A381ED522D8A923C83097490778EF5901B9F90AC2E99674143F5705`).
It acquired one exact `0.226 kg` equipment object, published 194 commands,
applied 147 after native object updates, released once, lifted `0.274 m`,
carried `0.346 m`, and retained `0.235 m/s` release speed. The isolated
right-hand path reported zero sweeps, hits, impulses, haptics, and melee. This
proves the pickup bindings and semantic transaction under the SteamVR null
driver; palm alignment, grip feel, and natural toss still require headset
acceptance.

### Visible Forge launch recovery

Two visible-state runs of installed source `dc03a63` reached the correct Halo 3
Forge Launch Game panel with Construct, Forge, and the Start row selected, but
MCC dropped the first synthetic Enter while the shell was settling.  A later
visible Enter started the title without changing any game option.  The melee
run then passed at
`out/debug-openxr/20260811-030307988Z-melee.log` (SHA-256
`C36549285C4E1AEF92FE853569DB3999C23DD24AF714DD951D0296EF36DE6979`),
and the equipment/grenade run passed at
`out/debug-openxr/20260811-030926727Z-equipment-scoop.log` (SHA-256
`EA3521703D8021C0DCBCCBB8D83D753B9BD547F6A0796146B04C69F0C03A4904`).

The internal validator now waits up to 25 seconds for the runtime log to prove
that Halo 3 loaded.  It sends one recovery Enter only when that proof is still
absent.  This avoids both the dropped-launch failure and an unconditional
second input landing inside a successfully loading game.  The first unattended
run of that recovery passed the exact visible-weapon gap scenario at
`out/debug-openxr/20260811-031621916Z-visible-weapon-gap.log` (SHA-256
`B148C42D9F607E3C2D6267EF3DDE11179C8DF0BA34AF13D761DA5A3B6EF2F4AE`).
The tool change does not alter the installed runtime candidate.

### Retained renderer geometry and cumulative Forge validation

Installed source `bc44c6c229f2ec0ad8e65b100f9259def3200e57` retains the
renderer-owned decorator placement and geometry buffers in bounded DLL-owned
storage. Placements use a 4 MiB pool and potential geometry uses a separate
64 MiB pool. The capture hook remains behind the strict Halo 3 placement
decoder and physical-contact feature gate. The contact worker copies immutable
validated data, rejects flat foliage, expands exact triangle strips, culls
distant placement blocks, and publishes only solid decorator planes. No
renderer resource or game pointer crosses into the worker.

The exact package is
`out/package-worktrees/881d012/out/candidates/bc44c6c-h3-physical-contact-20260811-095415663Z`.
Its installed Steam DLL is
`205DADDDAA2AB6FED8C1164DCDC8DB691BF4519A1FD608A149A832382181CEAA`
and its launcher is
`9D96956573FBF455697AFE3DDE087FCFC496E0FFC11CBC6D06B40A626C8803CC`.
The Microsoft Store edition was not present on this machine, so only Steam was
installed. The clean Release build and complete `ctest` suite passed before
automatic installation.

The environment-gated exact decorator self-test passed in a real Valhalla
renderer capture. It selected a captured placement block, copied retained
geometry, decoded a non-flat solid mesh and one placement, then swept the real
authored weapon triangles through the transformed decorator mesh. The
preserved log is
`out/debug-openxr/20260811-095432524Z-decorator-wall.log` (SHA-256
`130E124FDBBF039B2A5C695060FE43B377C0114F8777EF160FD0FA9773CC1FAC`).
This closes the previous render-only Valhalla-rock geometry gap without naming
or hardcoding a map object.

The same installed bytes passed a fresh normal Valhalla wall transaction at
`out/debug-openxr/20260811-143547087Z-wall.log` (SHA-256
`9C75780F3EA3C464C5AB1EEE0DDF67B31F77BA9B994D88082AE318B28EC1AA9C`).
That run independently validated both native type-1 structure walls and
type-4 placed-object walls. It therefore covers the authored native wall path
as well as the separately proven decorator path.

Three fresh dynamic-object transactions also passed with the same DLL:

- Loose-object scoop and toss:
  `out/debug-openxr/20260811-143832244Z-weapon-scoop.log`, SHA-256
  `EA9C82DA43AC71FAE16F772C6249AC0B4F28C707B91AF39116649A318668826B`.
  It reached `0.384 m` lift, `0.178 m` carry and `0.354 m/s` release, with
  150 applied impulses and zero melee.
- Mongoose nudge:
  `out/debug-openxr/20260811-144244039Z-vehicle-nudge.log`, SHA-256
  `DFD2467F15C20EE780F8D4E337296ABE6FCF4A4ABB545FCB56594EDE36C0514E`.
  It selected the `464.835 kg` vehicle and its 502-triangle authored target
  mesh, applied gradual point impulses, and produced zero melee events.
- Native melee:
  `out/debug-openxr/20260811-144521014Z-melee.log`, SHA-256
  `4E7BAD2A1B31D070251F07B256C8DD616AF960AD992584A3F4D635E03AE9922F`.
  It selected the detailed 88-triangle target mesh and invoked the authored
  damage/effect transaction while preserving the independent physics path.

All five runs used Steam / SteamVR null / Null Model Number and visible
external menu control. They prove bindings, exact geometry selection, native
mass use, contact constraints, impulses, and melee routing. They do not prove
headset alignment or feel.

The current DLL's left-grab regression then passed with clean validator source
`7ac6bbe60c43a91c93465b431b6a6ab32e8337fa`. The preserved log is
`out/debug-openxr/20260811-150713469Z-left-grab.log` (SHA-256
`C9E7B6426907FA6494A35049E2B6E1EEE4470B778D228BD8F33350AC98B19613`).
It acquired one exact `0.382 kg` equipment object, published 195 commands,
applied 131 after the native object update, released once, lifted `0.300 m`,
carried `0.213 m`, and retained `0.607 m/s` release speed. The isolated
right-hand path again reported zero sweeps, hits, impulses, haptics, and melee.

The clean result followed two invalid launch attempts. One exited during the
MCC shell transition and one remained in the documented Halo 3 level-load gate;
neither entered Forge and neither was counted as a pickup failure. Tool commit
`c6589a8` now requires one unique visible MCC window, waits for the contact
runtime rather than treating `halo3.dll` admission as success, and restarts MCC
once when the frozen-camera gate exceeds 45 seconds. A later reproducibility
run exposed 15 abandoned SteamVR Home `steamtours.exe` processes from earlier
null-driver sessions. They reserved about 5.5 GiB of private memory and caused
a recorded PowerShell `System.OutOfMemoryException`. Tool commit `7ac6bbe`
adds SteamVR Home to the bounded shutdown set. The clean pass above ended with
zero MCC, validator, SteamVR, or SteamVR Home processes and restored the exact
normal SteamVR configuration, SHA-256
`F95C7F261AD27423D1FAE287E2950E3471110C1DA969A4022E8298915801348C`.
Natural palm alignment, grip feel, and toss feel still require headset
acceptance.

Two further clean Valhalla runs used the same installed `bc44c6c` DLL and
validator source `61462b92d7cecc8cd2a6bc2e0c1491cd6a511f53`:

- Right-hand equipment/grenade-class scoop and toss passed at
  `out/debug-openxr/20260811-151156042Z-equipment-scoop.log` (SHA-256
  `896F6BE5B4FB675E2581AFC666AF8410DC7D6611F01A0527C4C01CE5304745F5`).
  The exact 8-triangle target had native mass `0.382 kg`. It received 68
  impulses and one release with zero melee, reached `0.271 m` lift and
  `0.122 m` carry, and retained `0.272 m/s` release speed. A live exact contact
  published right-hand haptic amplitude `0.234`.
- The rendered-weapon gap replay passed at
  `out/debug-openxr/20260811-151438607Z-visible-weapon-gap.log` (SHA-256
  `91CC0FD4F4CDBCE57E73DEA82B50B4EA38F00DE458C04090C562044AB73A067A`).
  It consumed 2,706 exact visible palettes and swept the 36-triangle weapon
  against an 88-triangle detailed target through 462 direct overlaps and 884
  direct separations across the scripted `-0.0600..+0.0703 m` surface range.
  The normal contact path recorded 496 authored hits, 297 impulses, 484 body
  constraints, 67 short gap holds, zero melee, and haptic amplitude `0.301`.
  This proves that contact, correction, and haptics share the final visible
  weapon geometry in the replay; headset pixel alignment and perceived haptic
  strength remain acceptance items.

Both runs ended with zero MCC, validator, SteamVR, and SteamVR Home processes
and restored the exact normal SteamVR settings hash. The saved visible replay
screenshot also exposed one stale `vrwebhelper.exe` crash dialog left by the
earlier memory exhaustion; it was dismissed after its owning process was gone.

### Current crate and Valhalla decorator revalidation

The same installed `bc44c6c` DLL passed a fresh crate-class nudge transaction
at `out/debug-openxr/20260811-152036869Z-crate-nudge.log` (SHA-256
`35F0F1317AA04EC37B1B69EF31BEC741D5458907F27FCF524682AADF1E5856DA`).
It selected a detailed 12-triangle kind-10 target with native mass `1.819 kg`
and the detailed 36-triangle held weapon with native mass `2.764 kg`. The
scripted slow contact produced 76 authored hits, 62 point impulses, 76 body
constraints, `0.260 m` peak correction, haptic amplitude `0.290`, and zero
melee events. This is current-build proof that a medium loose body uses its
authored geometry and mass while remaining on the gradual contact path.

One subsequent legacy fixed-sequence launch selected Halo: Reach because the
Forge title picker initially focused Reach. It was stopped immediately and was
not counted as Halo 3 evidence. Validator commit `abee7eb` disables that blind
menu path, requires `-ExternalMenuControl`, records
`external-visible-state` in the result, and aborts if a non-Halo-3 gameplay
module wins the transition. The no-switch path rejects the run before changing
SteamVR settings or launching MCC, and the title classifier passed its Reach,
ODST, and Halo 3 cases.

Using that visible-only path, Valhalla and the built-in Forge game type were
confirmed on captured screens before launch. The decorator-wall transaction
then passed at `out/debug-openxr/20260811-152730096Z-decorator-wall.log`
(SHA-256
`A8709469F13D23BC47D5837E2987B29F3811C50A4D9CD4DF68C241CE2A91D991`).
The runtime validated native type-1 structure geometry and native type-4
placed-object geometry, and `decoratorSelfTest=1` proved the separately retained
render-only decorator mesh path. The run ended with zero MCC, validator,
SteamVR, or SteamVR Home processes and restored the user's exact normal
SteamVR settings, SHA-256
`F95C7F261AD27423D1FAE287E2950E3471110C1DA969A4022E8298915801348C`.

### Current requirement audit

- Missing walls: native structure planes, placed-object planes, and retained
  decorator meshes all pass exact Valhalla runtime checks. The remaining test
  is whether the weapon appears pixel-aligned against many real walls in a
  headset.
- Mongoose: the 502-triangle authored mesh, native `464.835 kg` mass, gradual
  impulses, and zero accidental melee pass. Perceived force still needs a
  headset check.
- Loose weapons, equipment, and grenade-class bodies: exact authored-target
  scoop, carry, release, and zero-melee transactions pass. The null driver
  cannot judge whether a natural hand motion feels easy.
- Crates: exact authored geometry, native mass, gradual impulses, constraints,
  and haptics pass. The current proof does not replace freeform headset play.
- Visible alignment and clipping control: the replay consumes the final visible
  weapon palette, sweeps exact weapon and target triangles, and exercises
  direct overlap, separation, body constraints, and short gap holds. Only a
  headset can confirm that no visible gap or penetration remains during
  unscripted motion.
- Melee: authored damage/effect routing passes separately from slow contact.
  Enemy difficulty and the threshold's feel in Campaign remain headset items.
- Left-hand pickup: exact mass, acquisition, lift, carry, release, and strict
  right-hand isolation pass. Palm alignment and grip/toss feel remain headset
  items.
- Campaign bodies: exact animated-body routing and safety rules have regression
  coverage, but living/ragdoll body feel still needs a Campaign headset pass.
- Test control: unattended checks now require inspected visible menu state and
  have a wrong-title guard. Blind menu navigation is no longer an allowed
  validation path.

These null-driver runs prove bindings and deterministic transactions. They do
not advance the accepted-build pointer; real-headset play remains the player
acceptance gate.

### Read-only headset-session audit

`tools/analyze-h3-contact-session.ps1` turns one preserved `halo3xr.log` into a
short console summary and optional JSON. It reports source, edition, runtime,
headset, observed collision paths, contacted object kinds, native mass, wall
sources, impulses, constraints, haptics, melee, animated bodies, and left-hand
pickup. It labels missing counters as `not observed`, not failed, because a
player may simply not have exercised that path. It also marks null-driver runs
separately and states that logs cannot prove visual alignment, clipping, force
feel, haptic feel, or player acceptance.

The embedded self-test passes. A replay against the preserved `b154546` Quest
headset log correctly identifies Steam, SteamVR/OpenXR, the Oculus headset,
exact visible-weapon and authored-target contact, gradual response, gap holds,
haptics, structure and placed-object walls, animated bodies, and native melee;
it correctly leaves the later decorator and left-grab paths unobserved. A
second replay against the current `bc44c6c` Valhalla null-driver log identifies
the null device, structure, placed-object, and decorator wall paths without
claiming headset acceptance.

`tools/run-h3-contact-headset-session.ps1` is the real-headset capture wrapper.
It refuses null/debug SteamVR settings, identifies the exact installed package
from both installed hashes and a unique candidate manifest, verifies that
physical contact is enabled, and supports either Steam or Microsoft Store. It
can attach to an already open matching MCC process or start the installed
edition-aware launcher. It never drives MCC menus. When MCC closes it preserves
the log, verifies that installed bytes did not change, runs the analyzer, and
writes a reproducible session manifest. It does not edit SteamVR, the game,
`halomccvr.cfg`, or Easy Anti-Cheat. The settings safety self-test and a
read-only `-VerifyOnly` check pass against installed `bc44c6c`; the latter
matched both hashes, normal SteamVR settings, `physical_weapon_contact=1`, and
the `1.50 m/s` threshold without launching any process. Full record-until-exit
behavior intentionally awaits the next real-headset session.

### Enemy melee admission diagnosis

The preserved Steam / SteamVR OpenXR 2.17.7 / Oculus headset session from
source `ed81db3accd8baa509d621ac218c76f48fe3a2ff` is
`out/deploy-backups/17fc2dc-steam-before-f35706f-20260812-004854715Z/halo3xr.log`
(SHA-256
`6DE3AD969E10D7EA866187BF0D370562767D24B7588F188CAAAC3F880FE1D2D9`).
It ran at 120 Hz and recorded 88 exact animated-body hits, 8 completed native
melees, and 235 rejected authored overlaps whose sweep supplied no reliable
separating normal.

A read-only counter-delta audit found three biped status intervals that expose
the old admission error. At `16:11:57`, four new animated-body hits coincided
with an `8.97 m/s` tracked weapon sample; at `16:12:05`, two more coincided
with `9.58 m/s`; and at `16:14:45`, six more coincided with `10.38 m/s`.
All three reported `firstContact=0`, `meleeImpactSpeed=0.00 m/s`, and no new
melee. By contrast, first-contact biped samples at `16:14:06` and `16:14:37`
admitted `8.47 m/s` and `7.30 m/s` and each led to a native melee. These are
two-second status snapshots rather than per-collision traces, so the audit does
not attribute every counter increment to the displayed target. It does prove
that the running classifier discarded fast, armed biped samples solely because
the overlap had begun on an earlier frame.

The same log contains an interval at `16:18:21` where 14 new animated-body
hits and 14 new unreliable-normal rejects advanced together while the current
candidate reported `targetShapeSource=3` and `candidateNormal=0`; the next
interval added 9 and 9. This proves a second exact animated-body admission gap.
The status line's last accepted `target` differs from its current `candidate`,
so it does not prove the candidate's object kind. The session analyzer now
requires those identities to match before assigning candidate geometry to the
last accepted target.

The pending enemy-melee candidate changes only admission. A biped, creature,
or giant whose per-target melee latch is still armed may qualify on a later
sample in the same overlap using tracked weapon-point speed. Vehicles and props
retain first-contact, inward-surface speed. An exact animated-enemy overlap
without a separating plane may use the opposite tracked weapon motion only as
the normal for native damage/effects; that fallback cannot publish an impulse
or a visible-weapon constraint. After one melee, the existing per-target latch
still blocks repeats until its existing rearm path, and the global 250 ms
cooldown remains intact. Runtime counters distinguish later-sample admissions
(`enemySustainedMelees`) and no-plane admissions
(`enemyFallbackNormalMelees`). Headset acceptance remains pending.

### 2026-08-12 enemy-only threshold follow-up

The user tested installed source `68e7b56` in Halo 3 Campaign and reported that
meleeing enemies was still too hard. The exact animated-body path, later-sample
armed admission, unreliable-normal fallback, and native melee routing were all
already present in that source. The remaining deliberate difficulty control was
the enemy threshold: the default `1.50 m/s` configuration became `1.00 m/s` for
bipeds, creatures, and giants.

The next candidate halves the configured threshold for those three enemy kinds,
so the default becomes `0.75 m/s`, with the existing `0.50 m/s` lower clamp.
Vehicles, loose objects, and props still require the full configured threshold
and retain their strict first-contact inward-normal rule. The exact target latch,
`250 ms` global cooldown, tracked-weapon-speed requirement, and `8 m/s` spike
rejection are unchanged. Pure coverage proves that a `0.75 m/s` biped hit is
melee while the same Mongoose contact remains physics-only. Headset acceptance
remains pending.

### 2026-08-12 minimum enemy threshold follow-up

The `0.75 m/s` enemy-only threshold above was still an estimate rather than a
headset-accepted result, while the user's requested outcome was that campaign
enemy melee should be *much* easier. The next isolated candidate reduces only
biped, creature, and giant melee to one third of the configured threshold. At
the default `1.50 m/s` setting this reaches the existing `0.50 m/s` safety
floor. Vehicles, loose objects, and props remain at the full configured
threshold, so a slow Mongoose shove cannot become melee.

The exact animated-body contact path, later-sample admission, tracked weapon
speed requirement, per-target armed latch, `250 ms` global cooldown, and
`8 m/s` spike rejection are unchanged. The `0.50 m/s` enemy threshold remains
ten times the `0.05 m/s` tracking-noise floor. Pure coverage also proves that a
larger explicit configuration still scales (`3.00 m/s` becomes `1.00 m/s`) and
that the same `0.50 m/s` contact remains physics-only for a vehicle. Headset
acceptance remains pending.

### 2026-08-12 rotating-body render follow-up

The user reported that a weapon could still clip through an object after a
nudge. Source `68e7b56` followed the contacted body's surface velocity across
the short simulation-to-render delay, but it applied that velocity as one
translation to every weapon node. That is exact for linear body motion and only
first-order exact for one contact point during rotation. A long visible barrel
kept its old orientation while a crate, loose weapon, or vehicle rotated, so a
different part of the moving surface could still enter it before the next exact
solve.

The replacement publishes the native body's linear velocity, angular velocity,
and authoritative `object_get_center_of_mass` pivot with the exact approved
pose serial. The palette consumer advances
every visible weapon node by the same bounded rigid transform: translation plus
axis-angle rotation about the body pivot. It also rotates every node basis, so
the visible weapon remains rigid instead of merely moving its node positions.
The existing `50 ms`, `0.25 m`, and pose-serial limits remain; rotation adds a
`0.35 rad` cap. Invalid, stale, mismatched, or unconstrained publications do
nothing. Pure tests cover combined translation and rotation, the full weapon
basis, caps, stale data, and non-finite rejection. Exact runtime geometry and a
real headset remain the final acceptance tests.

### Dynamic-body separation diagnosis

The same preserved Quest headset session proves that the old fixed 50 ms body
gap was not a safe separation rule. At `16:11:21`, target `0xE2FC008D` had a
`0.408 m` visible setback after 63 exact constraints. At `16:11:23`, the same
target and candidate remained reported with `0.4653 m` penetration and no
current reliable normal, but the setback had already drained to `0.021 m`.
The run had reached the `1.000 m` correction cap and counted only 13 short gap
holds. By `16:11:25` the correction was zero. This matches the headset report
that a weapon could clip through a body after initially nudging it.

The pending separation candidate removes elapsed time as proof of clearance.
It remembers the exact constrained target and classifies each later scan as:

- blocked: a new exact constraint replaces the correction immediately;
- uncertain: the live target remains in the weapon broad phase but its exact
  geometry or separating normal is temporarily unavailable, so the last safe
  correction and contact latch are preserved;
- separated: the target was removed, fell outside the conservative broad
  phase, or resolved exact geometry was found clear, so the correction may use
  the existing bounded release.

Title, weapon, tracking, and gameplay-state resets still clear the correction
immediately. Runtime telemetry records `bodyUncertainHolds`; the analyzer also
accepts historical `bodyGapHolds` logs. Regression coverage proves indefinite
uncertain retention, confirmed-clear release, removed-target release, immediate
reblocking, invalid-state rejection, and no melee rearm during uncertainty.
This candidate does not change impulse strength, geometry, or melee rules.
Headset acceptance remains pending.

### Pre-existing-overlap visual blocker

The same real-headset run counted 235 exact authored overlaps whose sweep could
not supply a reliable separating plane. The previous path rejected a new
overlap completely when no earlier reliable normal was cached. That protected
native physics from a guessed force direction, but it also withheld the visual
body constraint and allowed the rendered weapon to remain inside the target.

The pending visual-blocker candidate keeps the safety boundary while separating
the two uses of a normal. For an exact overlap, the sweep already returns a
finite geometry-based fallback direction and the exact weapon/target convexes.
Their opposing support points provide a bounded penetration correction for the
rendered kinematic weapon. That fallback can publish only the visual blocker;
it is explicitly ineligible for native rigid-body impulse. Ordinary targets
return after the blocker is published. The already isolated animated-enemy
path may continue only to native melee, using tracked weapon motion for effects
orientation, and remains ineligible for impulse. Runtime telemetry records
`bodyFallbackNormalConstraints`.

The regression suite begins with an already intersecting rifle and prop. It
proves the sweep marks the normal unreliable, the exact support-point depth
produces an outward visual correction, and neither an unreliable sweep normal
nor an absent cache can authorize a physics impulse. Headset acceptance remains
pending.

### Whole-visible-weapon dynamic blocker

The first strict corrected-palette replay on source `a69430e` disproved the
claim that the visual body blocker prevented all clipping. Its preserved log is
`out/debug-openxr/20260812-015656688Z-visible-weapon-gap.log` (SHA-256
`D24C68470E7F83567595FAAE6BEAA3CF9776400D7EF1D29C12EFA3409A6F1B57`).
The visible Assault Rifle used all 36 authored triangles against all 88 target
triangles. The palette hook applied the published correction 95 times in the
first measured interval, but the corrected full meshes still overlapped 34
times; later totals reached 78 overlaps after 239 corrected palettes. The
validator correctly failed because it requires zero direct overlaps.

The old depth calculation explains that result. It measured only the one held-
weapon triangle or convex selected by the earliest exact hit. Moving that one
piece outside the contacted target plane did not move a different, deeper part
weapon outside it. Candidate `a260be9c85b03ad7bdc90e22a75345e4dbf5fd03`
tested a full-mesh support point against that one local plane. Its exact package
was `out/candidates/a260be9-h3-physical-contact-20260812-021410036Z`, with DLL
SHA-256 `B13B0C6DD64EA232F7D030E8208AEF737C6E3E037CC5CCF7749815C5AA0CC1DC`.
The strict visible Valhalla Forge replay failed again. The preserved log is
`out/debug-openxr/20260812-022244630Z-visible-weapon-gap.log` (SHA-256
`DA7B82DE14793C28B8692998721DDF397E9D3702F7370EDC5138D4D43AE6F7CC`).
It reached exact 36-triangle-versus-88-triangle contact, applied corrected
palettes, and still accumulated direct overlaps; early samples already showed
17 overlaps after 96 corrected palettes and 51 after 270. The single-plane
full-mesh behavior is therefore rejected and reverted. A support plane can be
conservative along one normal while still failing against a different target
face; the next candidate must prove its final translated pose directly against
the complete exact target geometry. SteamVR was restored to the byte-identical
real-headset settings hash
`F8B2C009AE05A2AC796D3458B9AA8B072A5BEA97CA8EC5A74FAA5DD585DBAB9E`.
Headset acceptance remains pending.

Candidate `f72a54581f290a7a271368fb7ff8701ee259d086` then used the
contact normal only as a search direction and checked each candidate pose with
the complete exact weapon-versus-target query. Its package was
`out/candidates/f72a545-h3-physical-contact-20260812-024244071Z`, with DLL
SHA-256 `268FFC6E83BB59FA8BCE96F1A40A5857F25EE7960E3DF86D16E70F9B0F145575`.
The preserved Valhalla Forge replay is
`out/debug-openxr/20260812-024325812Z-visible-weapon-gap.log`, SHA-256
`5A82B2F6D0A778FE079AF51313DF63DA7D87318797C07DAF5EAEA985F43A379E`.
It disproved that solver too: the first report had zero overlaps after only 27
palettes, but the cumulative count rose to 103 overlaps after 1,911 palettes
and 651 corrected palettes. The validator incorrectly marked the run passed
because it matched that early zero-overlap report. The runtime behavior was
reverted by `f7e4e13`; its result is rejected regardless of the stale JSON
`passed` field.

The strict gate now reads only the newest cumulative replay report and does not
pass until at least 900 exact palettes, 250 corrected palettes and 400 direct
separations have run with zero cumulative direct overlaps. A transient clean
prefix can no longer approve a solver that clips later. SteamVR was restored to
the byte-identical real-headset settings hash
`F8B2C009AE05A2AC796D3458B9AA8B072A5BEA97CA8EC5A74FAA5DD585DBAB9E`.
Headset acceptance remains pending.

Candidate `c529df334f422bf2080f104063bd6b9ddd1f033c` replaced the final
triangle query with the complete authored weapon and target solid compounds.
Its package was
`out/candidates/c529df3-h3-physical-contact-20260812-032132824Z`, with DLL
SHA-256 `6D53F76FA232AF37B71A4E39E091C1964A8CB8722DB48E0A3C2505667D7F65EF`.
The strict Valhalla Forge replay still failed. Its preserved log is
`out/debug-openxr/20260812-033603362Z-c529df3-visible-weapon-gap-failed.log`,
SHA-256
`FB7908A61A91E2C79DA41AA53570EAF2251F0342419B0348A9A14FDB640224EB`.
The final cumulative report recorded 9,613 exact palettes, 3,383 corrected
palettes, 4,262 direct separations and 725 direct overlaps, with a worst gap of
`-0.3075 m`. The solid solver ran only after a triangle surface sweep reported
a hit. A weapon already contained inside a closed target can overlap both
solids while crossing no triangle at that sample, so the code released its
body constraint. This post-hit-only behavior is rejected and reverted. Normal
SteamVR settings were restored with the byte-identical hash above.

Candidate `2e23800ece6e357fa49fca05eb8b9f084bf97124` admitted final-pose
solid containment before running that same clear-pose solver. Its package was
`out/candidates/2e23800-h3-physical-contact-20260812-034818802Z`, with DLL
SHA-256 `871759BB0BE6FFA6C6D2554CB4F289C7CB3E2040979036EFB932CF02F19C4263`.
The strict Valhalla Forge replay failed fast as designed. Its preserved log is
`out/debug-openxr/20260812-034906156Z-visible-weapon-gap.log`, SHA-256
`D2F1671206BC58413878A7752ECE99727FA94A3BE85AD78469F7C88F33528F51`.
At 216 exact palettes and 72 corrected palettes it had already recorded 15
direct overlaps. Runtime status showed only 131 contact sweeps during the same
interval, while the null headset rendered at 90 Hz. The worker can prove a
clear correction after seeing a palette, but intervening render palettes can
move into the object before the next worker sample. Final-pose containment in
the worker is therefore still reactive rather than preventive. This behavior
is rejected and reverted. The next candidate must stop unapproved visible
motion at the render boundary, while leaving all collision discovery and
native physics work outside the render hook. Normal SteamVR settings were
restored with the byte-identical hash above.

The next null-runtime replay is diagnostic only. Alongside the existing exact
triangle overlap counter, it records the complete authored solid-compound
overlap at the same final corrected palette. This distinguishes a stale
worker/correction publication from a solid query that disagrees with the
rendered triangle geometry. The counters are environment-gated, lock-free and
allocation-free in the existing debug path; they do not alter a pose, physics,
melee, input, or production feature state.

That diagnostic ran as source `e8a5351926074cf9c53e723f00bb09306167c57e`
from package
`out/candidates/e8a5351-h3-physical-contact-20260812-040607392Z`, with DLL
SHA-256 `4CCE0AD8DBAC6A26E852785B140F824F03D787B45B383FFD33ACF6AE5E0F9303`.
Its preserved log is
`out/debug-openxr/20260812-040658061Z-visible-weapon-gap.log`, SHA-256
`5145B164D5432FE14D49CAC95D91F4180FB6D47F1B4CDFC186C7A0C3A8D06D4B`.
The first failing cumulative report recorded four direct triangle overlaps but
only one solid-compound overlap after 217 exact palettes. A solid hull is
therefore not an exact substitute for the rendered collision surface. A final
pose is safe only when both the complete triangle surface and every authored
solid child report separation. SteamVR was restored to the normal settings
hash above.

Candidate `413719253eb27a0e65206548b65886cd580ee8ab` tested both the exact
triangle surfaces and solid compounds before accepting each new blocked pose.
Its package was
`out/candidates/4137192-h3-physical-contact-20260812-041955039Z`, with DLL
SHA-256 `C76405BC6FC9378C8CA719BDEB5C0AB8C9A1B5C642386E7AC27EC95E2C514E3E`.
The strict Valhalla replay still failed; its preserved log is
`out/debug-openxr/20260812-042049214Z-visible-weapon-gap.log`, SHA-256
`5C9D83387EEC9914927076DA826963933303E89DD9C908C701DB7D04863E2459`.
It recorded three direct surface overlaps after only 26 palettes and 13
corrected palettes, while the solid counter remained zero. The accepted blocked
poses were checked, but the separate dynamic-body release path still eased the
published offset toward zero without checking each intermediate pose. That
unchecked release can cross the target surface. The candidate is rejected and
reverted. Normal SteamVR settings were restored with the byte-identical hash
above.

Candidate `2df47e5772693a967ffdfc4451dc8f121d093a9e` added direct release
after the same combined triangle-and-solid proof. Its package was
`out/candidates/2df47e5-h3-physical-contact-20260812-042842233Z`, with DLL
SHA-256 `23E5DE9CC2E9F7E0C97905B6CCDA1DDDE44E048710C544AF8002B2D020EB3391`.
The strict Valhalla replay failed before the first reporting interval. Its
preserved log is
`out/debug-openxr/20260812-042938323Z-visible-weapon-gap.log`, SHA-256
`9F90C94455B67F8CF272CA00D0071A7DCFB4FB3F2EB575246F03DFC168EBC613`.
It recorded seven direct triangle overlaps and five solid overlaps after 25
palettes, including ten corrected palettes. Therefore unchecked release alone
does not explain the defect. Worker-side correction remains reactive: the
renderer can display a newly proposed palette before that same pose has been
checked and corrected. This behavior is rejected and reverted. The next path
must use a lock-free render proposal and worker approval, while keeping all
collision work outside the render hook. Normal SteamVR settings were restored
with the byte-identical hash above.

The next candidate implements that render-boundary ownership change. The final
primary-weapon palette is first published as a bounded lock-free proposal. The
camera/gameplay worker retains all tag reads, object-table access, geometry
construction, triangle/convex collision, and native physics work. It publishes
a full corrected 16-node-or-smaller palette only after the final pose has been
proven clear against both the rendered triangle surface and authored solid
children. The render hook performs only bounded atomic reads and a fixed-size
matrix copy: while a newer proposal is unchecked or blocked, it displays the
last fresh approval for the exact same weapon handle, render tag, and node
count. Weapon changes, tracking loss, runtime teardown, stale timestamps, and
identity mismatches invalidate approval rather than crossing feature state.
Animated multi-body targets hold the preceding safe pose during overlap because
one struck limb is insufficient proof that the whole moving body is clear.

Unit coverage now rejects stale, future, wrong-tag, wrong-handle, and wrong-node
approvals, and confirms that the bounded exact-separation search returns only a
directly clear final pose. Release build and full `ctest` pass. Runtime replay,
artifact identity, and headset acceptance remain pending until this paragraph
is updated with the committed candidate and preserved log.

Candidate `ca628b1e9f326d70521ca3c3630e27bf1081b865` was built and installed
from `out/candidates/ca628b1-h3-physical-contact-20260812-050424278Z`; its DLL
SHA-256 was `CDEFAA79F54027FAB83365A5BCD7F0AE65B5F0375AF720848C7A6B99D45A6615`.
The strict external-visible-state Halo 3 Forge replay failed after recording
one displayed triangle overlap and one solid overlap. Its preserved log is
`out/debug-openxr/20260812-050516592Z-visible-weapon-gap.log`, SHA-256
`43AC2E68179411BCE4DC3456A086E22634CF52617C9A57D62B42EDC7B35DBFD6`.
The same report proves the new handoff was live (`approvedPalettes=42`,
`heldPalettes=28`), but only eight held palettes carried correction. The
remaining hole is the dynamic-body `Separated` branch: it still eased the
offset toward zero and approved that unchecked intermediate. This behavior is
rejected and reverted by `1e4c204` before the next candidate. Normal SteamVR
was restored and MCC closed.

The next candidate preserves the proposal/approval gate but changes exact
dynamic-body separation to release directly to the already checked controller
pose. `Separated` is produced only when the constrained target was removed or
its resolved geometry was tested and found clear; `Uncertain` continues to hold
the last exact correction. This removes every unchecked intermediate release
pose from the approval stream.

Candidate `49fac411a56a884585572cccb4a3054395da8a72` was built and installed
from `out/candidates/49fac41-h3-physical-contact-20260812-051540012Z`; its DLL
SHA-256 was `D3050F752B8A290129FB41F654181EC7814A51900A6E40277B1CD417B2996FF2`.
The strict visible-state Valhalla Forge replay failed with two displayed
triangle overlaps and two solid overlaps. Its preserved log is
`out/debug-openxr/20260812-051604413Z-visible-weapon-gap.log`, SHA-256
`C31E09E95A0911AB79EE2AE62FE495E1A4498A3CEA80501AFCCAB8D17B6AFE62`.
Direct release removed the earlier smoothed-release hole, but the target itself
moved about 10 mm during the first report. A corrected pose that is clear at
worker time can therefore be crossed by a moving dynamic body before the
renderer consumes it. The candidate is rejected and reverted by `cb84883`.
Normal SteamVR was restored and MCC closed.

The next candidate treats contact as a closed gate: a blocked dynamic target
never produces a new visible approval. Rendering holds the last clear palette
for the duration of contact. Only the complete object sweep's exact no-hit
branch advances to a newer proposed pose. This removes dependence on a target
remaining still between worker proof and render consumption.

Candidate `2a324add3ad59938954a2ef1d062a22af05763fb` was built and
installed from
`out/candidates/2a324ad-h3-physical-contact-20260812-053001778Z`; its DLL
SHA-256 was
`92AA96ED92C646C7F01DEB12670C1DBB586B4C2B230C04E38FC87AC7725CF82F`.
The strict external-visible-state Valhalla Forge replay failed after 410 exact
palettes with one displayed triangle-surface overlap and zero solid overlaps.
Its preserved log is
`out/debug-openxr/20260812-060431892Z-visible-weapon-gap.log`, SHA-256
`CB0EEB74CB034959AA1F0EBE649C64E568CA74A9AA5F00B3ECF1CE91E76E1A8A`.
The gate was live (`approvedPalettes=123`, `heldPalettes=403`), but a loose
target moved into the last clear pose after contact began. This behavior is
rejected and reverted by `b3c47db`.

Candidate `4ac1b5fba588ab42ed37b8edabc2e59c612066ba` then added clearance
based on the target contact point's measured linear and angular velocity. It
was built and installed from
`out/candidates/4ac1b5f-h3-physical-contact-20260812-061812760Z`; its DLL
SHA-256 was
`14EE1BAD5E026328664C3F39EB65ACE8B3CA19BF96B8F3CED467762B5EAECEB2`.
The strict replay also failed, with three triangle overlaps and two solid
overlaps by the second report. Its preserved log is
`out/debug-openxr/20260812-061843191Z-visible-weapon-gap.log`, SHA-256
`ADC105784EEADAD5EBFC4D6DCA1B1A0172577D492E6059CDB844B12B5BABC2E7`.
The target had moved only 3 mm while the solver had reached a 108 mm peak
setback. The failure therefore occurs before a post-hit motion reserve can
protect the displayed pose; increasing distance along a contact normal is not
a preventive contact boundary. This behavior is rejected and reverted by
`b637557`. Both failed runs closed MCC and restored the byte-identical normal
SteamVR settings hash
`F8B2C009AE05A2AC796D3458B9AA8B072A5BEA97CA8EC5A74FAA5DD585DBAB9E`.

The next candidate adds a separate visual-only guard around exact authored
triangle geometry. Physical contact, impulse timing, melee timing and the
1.25 mm physical surface remain unchanged. The worker begins constraining and
approving the visible palette when the complete moving weapon mesh comes within
8 mm of a movable target, then directly proves a final pose outside that guard
plus 4 mm clearance against both triangle surfaces and solid children. The
render hook still performs only bounded atomic reads and a fixed matrix copy.
During real contact, every newly published correction must pass the same guard;
an unresolved guard holds the preceding approval. This creates the safety band
before the first impulse or target rotation rather than trying to recover after
penetration. The strict validator requires sustained corrected approvals and
zero displayed overlaps.

Candidate `4f4f25e4355cd693c226b3ff1738cda7de49fb91` was built and
installed from
`out/candidates/4f4f25e-h3-physical-contact-20260812-063042103Z`; its DLL
SHA-256 was
`70D7211B6DB03BDCFF6B3EDE77A1920DEFD1BB30826FEFD0D26E18A405B73EF8`.
The strict visible-weapon replay passed with zero displayed triangle and solid
overlaps after 2,230 exact palettes, including 879 corrected palettes and 827
approved palettes. Its preserved log is
`out/debug-openxr/20260812-063104028Z-visible-weapon-gap.log`, SHA-256
`367FCEAAC9CB700F8EDF678F480E36600B723B24E5B8F41F05AC5E6E40FF2A07`.
The same installed candidate passed visible weapon nudge and exact loose-weapon
lift/carry/release. Their preserved logs are
`out/debug-openxr/20260812-063650725Z-visible-weapon-nudge.log`, SHA-256
`BA3DECFD8AA0173C3BA8884A7068F7A7638E70CECF3EDD5451E3FFF202DDBBAF`,
and `out/debug-openxr/20260812-063936948Z-weapon-scoop.log`, SHA-256
`FCFB4BEBF84135E1B793074703E72004ED37F259277537334A3E0D0215329032`.
The scoop resolved a native 0.382 kg weapon and measured 1.906 m peak lift,
0.212 m carry and 0.474 m/s release. These null-driver results validate the
guard and deterministic rig, not headset acceptance.

The later High Ground Mongoose run exposed a separate melee-classification
defect. Its preserved log is
`out/debug-openxr/20260812-064236047Z-vehicle-nudge.log`, SHA-256
`362A77D02712D3BFBB6FAFEFDE26B99C9FC6C817B24A919AF92B68740CEA7451`.
The exact 502-triangle, 464.835 kg Mongoose had been moving from prior pushes.
At the decisive contact the tracked weapon point moved only 0.88 m/s, but the
target-relative speed was 1.70 m/s and queued a 1.68 m/s native melee. The
validator incorrectly accepted an earlier completed impulse line before that
queued melee executed. This proves target rebound must remain part of physics
response but cannot contribute to melee classification.

The next candidate therefore classifies all native melee from tracked weapon
point velocity. Props and vehicles retain the configured threshold and strict
first-contact surface-normal rule. Animated enemy kinds retain exact target
selection and the armed sustained-contact allowance, but use a bounded
one-third threshold reduction: the default 1.50 m/s setting becomes 1.00 m/s
for bipeds, creatures and giants. The vehicle validator also requires the
latest target status to have no pending melee, so it cannot pass on stale
evidence again.

Candidate `e87caf451b2840da84e963bb62ba859fc8580398` was built and
installed from
`out/candidates/e87caf4-h3-physical-contact-20260812-065533207Z`; its DLL
SHA-256 was
`A5FA74EF0CEDC878B9028B1AF9E8B95FEB1D1ECFE543890EB6171772BE017356`.
The extended High Ground Mongoose transaction passed and stayed clean through
its 30-second post-pass window. Its preserved log is
`out/debug-openxr/20260812-065544043Z-vehicle-nudge.log`, SHA-256
`C70D3E25ADE8114CE13FEDD9000960598108E0FD08169C2091B769ECEEBAA227`.
It applied more than 617 impulses to the exact 502-triangle, 464.835 kg vehicle
while native melee and queued melee status remained zero. The exact fast-hit
transaction also passed with one completed native melee response; its log is
`out/debug-openxr/20260812-070800172Z-melee.log`, SHA-256
`021EDD2030C3A135325348FF32A60D00C73201925D35433C555C566331F536BE`.
The same installed DLL passed exact left-palm acquisition, native-mass carry,
release and toss in
`out/debug-openxr/20260812-071146836Z-left-grab.log`, SHA-256
`C600CDAA140F338650F649A1E185F27C6DB9542F125006CE16FE37881BAB4011`,
and passed Valhalla structure and placed-object wall validation in
`out/debug-openxr/20260812-071522684Z-wall.log`, SHA-256
`8EF8647E1D5542BC3EDEA13C3FA7D862AC97E79F6686BE74E956B892CE5AC32A`.

The first decorator replay was rejected only by its validator. Runtime reached
`decoratorSelfTest=3`, with 8,454 wall blocks, 472,228 wall rays, 272,142
placed-object planes, 56 wall vertices and 38 wall planes. The validator
incorrectly required the cumulative self-test counter to equal exactly one.
Its preserved rejected log is
`out/debug-openxr/20260812-072354299Z-decorator-wall.log`, SHA-256
`B2C2EEF0BD8DFCBDCD7EEC8335702BEA19668C2A2D81B02EAD548EA7B29F0AD0`.
The corrected validator accepts any positive cumulative self-test count; no
runtime behavior changed.

### 2026-08-13 animated-enemy sampling catch zone

The user's next Campaign result on source `23e9664` still found physical melee
too difficult even though that source already used the minimum `0.50 m/s`
enemy threshold, later-sample admission, and exact node-bound animated rigid
bodies. Lowering the speed again would not repair a contact the sweep never
observed.

The animated-body sweep used the same `1.25 mm` surface radius as rigid-body
physics. A fast tracked weapon and an animated limb are sampled on different
render/game clocks, so both exact shapes can visibly cross between samples
without ever entering that very thin band. The next isolated candidate adds a
`30 mm` sweep radius only when the target kind is biped, creature, or giant and
only after the exact animated limb shape has resolved. A hit admitted by this
outer band may request the existing native melee transaction, but it cannot
publish a visual constraint or a rigid-body impulse. Props and vehicles retain
their exact `1.25 mm` surface and their full configured melee threshold.

Runtime telemetry records `enemyAssistHits`. Pure coverage proves the radius is
`30 mm` for the three animated enemy kinds, unchanged for vehicles, and never
shrinks a larger exact radius. Headset acceptance remains pending.

### 2026-08-13 animated-body render-frame repair

The same Campaign report said interaction with bodies was poor and the weapon
could clip after a nudge. Inspection found a concrete frame mismatch in the
render-follow transaction. The worker correctly selected one H3EK-authored
rigid body and saved that limb's node transform, but the render callback always
compared it with interpolated node zero. For a biped, creature, giant, or
ragdoll, node zero is the character root, not the contacted limb. The resulting
root-versus-limb delta could move the held weapon incorrectly and the final
guard could not rebuild the selected animated collision shape.

The isolated repair carries the proven rigid-body index through the existing
bounded lock-free publication. The render callback resolves that body's own
authored node and Havok shape, follows the same node, and runs the final 5 cm
render reserve against that exact convex body. Single-body props continue to
use node zero. The enemy-only 30 mm melee sampling band does not publish a body
index to the render guard, so it remains damage admission only and cannot form
an invisible wall. A final render solve that cannot prove separation now refuses
that mutation and restores the worker-approved palette instead of publishing a
known intersecting correction. Runtime validation and headset acceptance remain
pending.

The first strict rotating-body replay of source `6855b72` rejected that repair
after 1,253 same-frame comparisons found one confirmed penetration. The final
guard recorded zero timed samples, proving it never reached its geometry
predicate. The new body index had been read from the general contact target:
ordinary props report body zero for native mass and impulse handling, but their
root physics body authors node `-1`. Treating that index as an animated
render-node binding therefore made the final guard return unresolved. The
preserved log is
`out/debug-openxr/20260813-045336454Z-rotating-body-gap.log`, SHA-256
`E763DC6F5CACCDFB5171C8BFB3F6F08894D398453FD27AE6A186E093808D450B`.

The follow-up carries a body index into the visible guard only for target shape
source `3`, the exact animated-body path. Detailed and physics-fallback props
continue to use their authored node-zero collision transaction while retaining
the native rigid-body index for mass and impulse. Pure coverage locks that
source-to-index policy. A fresh strict replay is required before the candidate
can survive.

Three strict rotating-body replays of installed source `2a247b7` then proved
the root-space repair: the longest run made 11,710 same-frame comparisons and
26,629 final-render checks with zero confirmed overlap and zero render
separation failure. The preserved log is
`out/debug-openxr/20260813-052236359Z-rotating-body-gap.log`, SHA-256
`BC3E73617A8FAA5E536DF34F6570F9E4086FE40D0474197C963C1D4A7D0E23E8`.
That run did not pass the complete gate because 1,589 checks (5.97%) exceeded
0.25 ms, above the 5% p95 boundary. The two earlier preserved runs are
`20260813-050401368Z-rotating-body-gap.log` and
`20260813-051228044Z-rotating-body-gap.log`; neither recorded a penetration or
separation refusal, and neither met the timing gate.

The timing path immediately reads the target's interpolated render bank three
times to catch a bank change during the callback. Logs showed the common case
still paid three complete intersection queries for three bit-identical target
transforms. The next isolated candidate retains every distinct observed pose
but collapses exact duplicates before geometry testing. This does not merge
nearby or approximately equal poses: any float component change keeps the
observation. The final guard therefore proves the same set of target poses
while avoiding redundant exact geometry work. Pure coverage locks exact-equal
and one-component-different cases; fresh runtime timing is required.

## Verification boundary

The pure regression suite covers translation and rotation sweeps, tunnelling,
grazing misses, the noise floor, slow pushes, exact melee threshold crossing,
contact-fraction point velocity, rotational tip speed, target angular surface
velocity, world-scale conversion, invalid timing/data,
finite-value rejection, movable/static classification, mass response,
point-impulse clamping, sustained normal contact, loaded tangential friction,
clean separation,
per-target overlap debounce, separation rearming, reset on weapon/tracking
change, 250 ms cooldown eligibility, timestamp-underflow rejection, exact
convex translation/rotation/tunnelling, grazing rejection, and unsupported
shape failure. The cumulative Release build and complete `ctest` suite must pass
before packaging. Headset acceptance (specific weapons,
materials, enemies, loose weapons/crates, walls, pause/loading/death, and rapid
motion) remains intentionally pending; the feature therefore defaults off.

Candidate `af89f28` launched and injected without a crash. SteamVR could not be
used for this run because a pre-existing unsigned `WTSAPI32.dll` in the MCC
binary directory shadows the Windows DLL and exports only the wide query while
the current signed SteamVR `vrclient_x64.dll` imports
`WTSQuerySessionInformationA`; project safety forbids patching game files. A
process-local `XR_RUNTIME_JSON` override proved the installed Oculus 1.201.0
runtime initializes, but no headset was connected, so this is launch evidence
only and not physical-contact acceptance.

## Bounded visible-palette gaps

The clean `1ac8a37` replay in
`out/debug-openxr/20260812-195824089Z-visible-weapon-gap.log` stayed clear for
8,388 palettes, then recorded two confirmed overlaps. The reset diagnostic was
reason 2: a missing or stale visible-palette publication. Controller tracking,
gameplay gates, the active weapon, and the constrained target were still
valid. Resetting all contact state on that render-only gap erased the last
geometry-approved pose and exposed the uncorrected replay root. The approved
palette consumer already rejects approvals older than 100 ms. A render-palette
gap can therefore safely stop contact work without clearing the approval; true
tracking, gameplay, lifecycle, vehicle, and weapon failures retain their
immediate resets.

## Rotating replay time-alignment diagnosis

The motion-reserve candidate `8cb8d34` also failed and was reverted. It stayed
clear for 2,107 checks before two confirmed overlaps, despite continuously
publishing corrected poses and reaching a one-metre bounded setback. More
clearance therefore did not address the measurement. The replay checker reads
the last final weapon palette, then compares it with the target transform read
on the following gameplay sample. A rotating target can advance between those
timestamps even though the weapon and target were separated when rendered.

The next diagnostic candidate keeps gameplay unchanged. During the opt-in
rotating replay only, it uses the exact displayed-palette timestamp plus native
linear velocity, angular velocity, and centre of mass to rewind the target
transform to the same time. It reports the existing current-time overlap and a
second time-aligned overlap. The validator remains unchanged until runtime
evidence proves whether this is a cross-frame false positive.

Candidate `cdbaf53` supplied that proof. Its preserved rotating replay is
`out/debug-openxr/20260812-232601457Z-rotating-body-gap.log`, SHA-256
`5B88DAC107B414B48C3EBD27B5D8FDA1D1513F2254B3862B8FF00F5F8065AD57`.
The current-time comparison eventually counted three confirmed triangle and
solid overlaps. The timestamp-aligned comparison of the same displayed weapon
samples counted zero geometry intersections, zero confirmed penetrations, and
zero solid overlaps through 10,852 exact palettes and 6,818 aligned
separations. Two aligned samples touched only the 1.25 mm physical skin. The
old rotating result is therefore a cross-frame validator false positive, not
visible clipping.

The first corrected-validator replay on candidate `c670ba3` exposed a second
validator mistake. The exact 36-triangle weapon against the exact 88-triangle
target stayed at zero aligned geometry intersections and zero confirmed
penetrations, while the deliberately conservative compound safety proxy
reported two overlaps. The normal non-rotating gap validator already treats
the authored triangle test as the visible-geometry authority and retains the
compound counter only as a diagnostic. The rotating validator now applies that
same rule: aligned confirmed penetration must remain zero; aligned compound
overlap remains logged but cannot fail an exact-triangle sample. The preserved
failed-validator log is
`out/debug-openxr/20260812-234014616Z-rotating-body-gap.log`, SHA-256
`B11CCFE27CBAE1B43C2E69D444719C59F91E73FD6F41BF6A165F03E5945223FA`.

Candidate `e60f3cf` then ran long enough to record one aligned confirmed
triangle penetration after 2,320 displayed palettes. That does not establish a
visible overlap: the aligned pose is reconstructed from the target's *later*
physics pose and its *current* velocity. A collision impulse can change that
velocity between the displayed-palette timestamp and the gameplay check, so
rewinding the new velocity cannot reproduce the earlier rendered pose. The
preserved log is
`out/debug-openxr/20260812-234549819Z-rotating-body-gap.log`, SHA-256
`C5082673A1706166F6AD354D707BABF957C6D09E737730D985847F1E00EC9714`.

The replacement validator measures both rendered objects at the same time.
Inside the opt-in final visible-weapon palette callback, it reads the target's
interpolated node bank—the same bank Halo's object renderer consumes—and
compares the exact final weapon triangles against those exact target
triangles. The callback publishes only bounded atomic counters. It performs no
allocation, logging, lock, file I/O, or signature scan. The two eyes share one
world-space pose, so the validator takes at most one sample per millisecond and
requires at least 8,000 successful same-frame comparisons with zero confirmed
penetrations. The current-time, velocity-rewound, and conservative-compound
counters remain in the log as diagnostics but no longer decide rotating
visible-geometry acceptance.

The ordinary non-rotating gap validator remains unchanged and continues to
judge the live transform directly. This changes no production pose, geometry,
impulse, melee, input, or render behavior. Exact packaging and a full replay
under the same-frame validator remain pending.

Candidate `7f16699` proved that the remaining gap was real. The same-frame
checker recorded six exact confirmed penetrations within its first 263 valid
render comparisons. At that point the worker had already issued 451 dynamic
body constraints, but the final palette still advanced the held weapon from a
linear/angular velocity prediction. Halo renders the loose target from an
interpolated node bank; that visible transform can diverge from a physics
velocity prediction after collision impulses or interpolation changes. The
preserved failure log is
`out/debug-openxr/20260812-235409406Z-rotating-body-gap.log`, SHA-256
`C65C0F679657FF555FCD5417FF98798DED59E45C667A4398CA5F3D31B291A93F`.

The replacement body follow publishes the exact target root used when the
safe weapon palette is approved. When that palette is consumed, the render
callback reads the target's current interpolated root and applies the exact
root-to-root rigid delta to every held-weapon node. This preserves the proven
weapon/target separation through translation, rotation, collisions, and render
interpolation. The older velocity follow remains a feature-local fallback if
the optional visible target root cannot be read. Both exact follows and
fallbacks are counted in the normal status log. The render callback adds no
allocation, logging, lock, file I/O, or scan. Packaging and the 8,000-sample
same-frame replay remain pending.

Candidate `a024e8e` proved root-only follow was necessary but insufficient. It
applied 404 exact visible-root follows with zero velocity fallbacks, yet the
same-frame checker found one confirmed triangle penetration in its first 125
comparisons. The selected loose weapon exposed 12 authored triangles. This
means at least one final target shape changed relative to its root, or its
interpolated node bank advanced between the root read and final palette. The
preserved failure log is
`out/debug-openxr/20260813-000242194Z-rotating-body-gap.log`, SHA-256
`2705489B8B75694A35F4068F88958435FE6D7B479FF7397E7D53BF872F456799`.

The next candidate keeps exact root follow and adds a final exact-triangle
guard in that same render callback. During an active dynamic-body constraint,
it builds the already bounded authored weapon and target meshes and performs
one authored-triangle guard test using the existing 8 mm visual reserve. Most
palettes end there. Only a guard overlap starts the fixed-count verified-
separation search; the weapon moves by the smallest directly proven clear
offset plus 1 mm, capped at one metre. Exact
render separations and failures are counted. A separation failure prevents the
rotating validator from passing. This path still performs no allocation,
logging, lock, file I/O, or scan. Packaging, runtime cost measurement, and the
8,000-sample replay remain pending.

Candidate `e04ec81` showed why a zero-margin render guard is insufficient. Its
guard read was clear on all 1,132 samples, but the immediately following
same-frame validator saw one confirmed penetration. The target's interpolated
bank can therefore advance between two reads even inside the final palette
transaction. Cost was already safe: only one of 1,132 guard samples exceeded
0.25 ms (0.09%), and the measured peak was 335.1 microseconds. The preserved
failure log is
`out/debug-openxr/20260813-001220523Z-rotating-body-gap.log`, SHA-256
`9D38CBA09CE32AFA0C990E52CC55B64EAF1427597BB4567820269ABC3FBEFEF1`.
The failed zero-margin guard is disabled. Its code remains dormant as required
by the candidate discipline; the next candidate will enable the 8 mm authored-
triangle reserve as a separate behavior. Packaging and replay remain pending.

The replacement candidate enables that dormant path with the existing 8 mm
visual reserve around the same exact authored triangles. The 1 mm verified-
clear padding remains unchanged. This covers one concurrent interpolation
update without switching to a bounds proxy. Packaging and replay remain
pending.

Candidate `5df1f1c` also failed. It corrected 64 exact render-time overlaps,
but the same-frame validator still recorded one confirmed authored-triangle
penetration after 2,056 valid comparisons. The status line changed the target
from a 12-triangle authored shape to no published target triangles near the
failure, so a fixed spatial reserve cannot make separately read target banks
transactional. Its measured cost stayed within budget: 46 of 4,061 samples
exceeded 0.25 ms (1.13%), with a 782.7 microsecond peak. The preserved failure
log is `out/debug-openxr/20260813-002019939Z-rotating-body-gap.log`, SHA-256
`F841BA5EF9301CA974E32873766700DBDB06E6B0663CEFA6B3BD3E2D36BE95FC`.
The failed 8 mm guard is disabled; its implementation remains dormant.

The next candidate keeps the same exact authored-triangle correction but reads
the target's bounded visible geometry three times in the final weapon-palette
callback. Each pass validates the palette produced by the prior pass and can
move it to the smallest directly verified clear position. This targets the
observed target-bank transition without hardcoding a shape or wall and without
adding allocation, logging, locks, file I/O, or scanning to the hook. Runtime
cost and the 8,000-sample same-frame replay remain pending.

Candidate `41eba17` failed after 2,010 valid same-frame comparisons, recording
three confirmed penetrations. It exceeded 0.25 ms on 145 of 4,284 samples
(3.38%, still below the 5% p95 limit), with an 844.4 microsecond peak. The
important state correlation is that 4,425 visible palettes were submitted but
only 4,299 were corrected: the exact guard was nested inside the approved-body-
follow branch, so a final unapproved palette could bypass every repeated check
during a contact-state transition. The preserved failure log is
`out/debug-openxr/20260813-002907551Z-rotating-body-gap.log`, SHA-256
`925235985F7CE16ACD12D8B04C882B5FDAC26DB42B024C966197E1A348594B03`.
The failed repeated-read behavior is disabled. Its code remains dormant.

The next candidate performs one authored-triangle correction after final
palette selection, immediately before the visible pose is published and drawn.
It covers both a worker-approved body-follow palette and the transient current
proposal that previously bypassed the nested guard. The target root is read
twice around that last check so a concurrent interpolation update is carried
into the final weapon palette. This adds no hardcoded geometry and retains the
bounded, allocation-free render path. Packaging, runtime cost measurement, and
the 8,000-sample same-frame replay remain pending.

Candidate `0838dce` checked every final contact palette and kept render cost
safe (32 of 3,671 samples over 0.25 ms, 0.87%; 529.4 microsecond peak), but one
confirmed penetration still appeared after 1,766 same-frame comparisons. There
were 3,670 submitted palettes and 3,671 final guard samples, so no palette
bypassed this placement. Halo's target provider can therefore expose a
different interpolation bank between consecutive bounded reads. Sequentially
clearing only the latest bank can re-enter an earlier bank. The preserved log
is `out/debug-openxr/20260813-003607504Z-rotating-body-gap.log`, SHA-256
`04201CEADC324B93D85563529DC42F72A6804B230E99476AE4F7FE556A3981A1`.
The failed single-bank final guard is disabled; its code remains dormant.

The next candidate captures up to three bounded authored target meshes and
transforms during the final callback, then finds one weapon position that is
verified clear of every captured bank at once. Unlike repeated corrections,
the final search predicate cannot move the weapon out of one bank and back into
another. The existing 8 mm reserve remains part of every exact triangle test.
Packaging, render-budget measurement, and the 8,000-sample replay remain
pending.

Candidate `d209679` was rejected on both correctness and cost. At 2,122 final
guard samples it recorded three separation-search failures; the independent
same-frame validator then recorded two confirmed penetrations after 927 valid
comparisons. Rebuilding three target meshes also exceeded 0.25 ms on 1,101 of
2,122 samples (51.9%), with a 6.63 ms peak. The preserved failure log is
`out/debug-openxr/20260813-004259922Z-rotating-body-gap.log`, SHA-256
`4F44AD74A8DF122277BB32FB92AB86C17B640C5A83528D9A7DAF7FDCBEDFFBA8`.
The failed multi-mesh behavior is disabled and remains dormant. The next
candidate will decode one authored mesh, sample only the changing rigid root,
and try multiple exact, verified escape directions only when overlap exists.

The replacement candidate decodes the authored target triangles once per final
palette and records three bounded root transforms. The final predicate requires
the same exact mesh to be clear at every observed transform. If it overlaps,
the fixed search tries triangle-centre, object-centre, and weapon-local axis
directions, accepting only the shortest candidate directly verified clear of
all banks. The extra searches run only on an overlap. Packaging, cost
measurement, and the 8,000-sample replay remain pending.

Candidate `4ec4241` passed the exact rotating-body replay under SteamVR's null
driver. The final sample contained 8,039 same-frame authored-triangle checks,
zero same-frame geometry overlaps, zero confirmed penetrations, 70 final render
separations, and zero separation failures. Only 101 of 19,341 final guard
samples exceeded 0.25 ms (0.52%, safely below the 5% p95 boundary). The high
one-off peak was 11.78 ms and did not affect p95. The preserved log is
`out/debug-openxr/20260813-004927658Z-rotating-body-gap.log`, SHA-256
`7D89273EE20601CB90447834955AAD415CFCE933875BB628B33224E2E5B1134F`.

The same exact code candidate also passed these visible Forge regressions:

- loose-weapon scoop/carry/toss: `20260813-005613062Z-weapon-scoop.log`,
  SHA-256 `3F048DCBC899710627E3579FB47BCCDDFEFF8337932834C94C9F4EB622CC5824`;
- left-hand acquire/carry/release: `20260813-005906604Z-left-grab.log`,
  SHA-256 `681A24A41CA8394DB707AE2B84A1D330C1FBA66A9EF2740842D09E61425E1ADF`;
- native melee plus contact haptic: `20260813-010134499Z-melee.log`,
  SHA-256 `C6E1DAE5DB3B6077B400DE48EEE77A39B63FD4826FDA083D141137F5C6878F88`;
- High Ground vehicle nudge with no melee: `20260813-010405319Z-vehicle-nudge.log`,
  SHA-256 `FDD283B3EC5031260FBCE63CD0D749751230C14913FA1D60D042481806CC8802`;
- Valhalla structure and object wall validation: `20260813-010657984Z-wall.log`,
  SHA-256 `37CE5F91548CDC0F4A95ACCBABA3F0EA1712B430D23356987B0259C70FABB7AE`;
- Valhalla decorator/rock self-test: `20260813-010958706Z-decorator-wall.log`,
  SHA-256 `647D3408F781E5859883A27C8C66CC6308D718F239966F17B21E35375FC9B00D`.

These null-driver results are automated evidence, not headset acceptance. The
accepted-build pointer remains unchanged until the user verifies this behavior
in a real headset.

The exact packaged identity `b15c7da` exposed an 88-triangle loose weapon that
the earlier 12-triangle rotating target did not cover. It failed after 823
same-frame comparisons with one confirmed penetration. It also exceeded 0.25
ms on 791 of 1,904 final checks (41.5%), with a 37.15 ms peak. The preserved
failure log is `out/debug-openxr/20260813-011503090Z-rotating-body-gap.log`,
SHA-256
`FBD00808A6813BD1310840A6932C956CA789E76BAA5BE26EF229CAB04307246A`.
The root-only exact-target solver is disabled. This meets the plan's explicit
condition for using the authored convex fallback on complex target geometry;
the held weapon remains triangle-accurate.

The replacement keeps the held weapon's authored triangle mesh for every
final check. Targets reporting more than 48 authored triangles skip target
triangle decoding in the hot callback and use the conservative authored convex
compound already built from the same collision BSP. Simple targets remain
triangle-versus-triangle. Both paths sample three target roots and retain the
multi-direction verified-clear search. The rotating validator now requires at
least 2,500 fallback samples, at least 8,000 timed final samples, and no more than 5% of
those samples above 0.25 ms, in addition to its existing 8,000 independent
same-frame exact-triangle comparisons and zero-penetration requirement.

Candidate `652e6b6` exercised the complex-target convex fallback for 7,138 of
8,269 timed final samples and stayed inside budget (118 samples over 0.25 ms,
1.43%), but the validator found one confirmed penetration after 3,865
same-frame comparisons when the rig changed back to a 12-triangle exact target.
The preserved failure log is
`out/debug-openxr/20260813-012325269Z-rotating-body-gap.log`, SHA-256
`E8B12B3D4A27EFB159CB72115D1F848BFBF50C158350D808B9E5E6A5CFC21C31`.
The mixed exact/convex target behavior is disabled and remains dormant.

The replacement uses one consistent model for the final visual constraint:
the held weapon's exact authored triangles against the target's conservative
authored convex collision compound. The independent same-frame validator still
judges both objects using exact triangles, so a pass proves that the
conservative fallback prevented real visible penetration. The runtime no
longer changes constraint models when the target changes complexity.

Candidate `54bd633` used the convex target consistently and stayed within the
render budget (42 of 7,123 samples over 0.25 ms, 0.59%), but still recorded one
confirmed penetration after 3,334 same-frame comparisons. The preserved log is
`out/debug-openxr/20260813-012921442Z-rotating-body-gap.log`, SHA-256
`0C591F64B90F3F99B8BEDC6EC4A4CB4AE73B7D75F225BDA6AD07D921AF236239`.
The 8 mm consistent-target guard is disabled and remains dormant.

The replacement keeps the same exact-weapon/convex-target model and raises its
bounded motion reserve to 5 cm. This intentionally prefers a small visible gap
over any weapon penetration when Halo changes target interpolation banks after
an impulse. The final headset test must judge whether this safety distance is
acceptable for precise interaction.

Candidate `6c43d6b` had zero same-frame penetrations in its first 827 exact
comparisons, but 212 of 1,933 render checks exceeded 0.25 ms (10.97%). Every
overlap ran the multi-direction search, so the candidate was stopped and
rejected on cost before the long threshold. The preserved partial log is
`out/debug-openxr/20260813-014011409Z-rotating-body-gap-aborted-performance.log`,
SHA-256
`76AF78A16FF6A4EDE79E0BC98C06D45D058B36C60445D70D3A220BDF4F2921C4`.
The expensive 5 cm search is disabled and remains dormant.

The replacement retains the 5 cm motion reserve but runs one directly verified
target-centre escape direction. The conservative target compound makes that
direction well-defined; the existing fixed expansion and binary search still
refuse any result that is not clear of all three observed target roots.

Candidate `7358ca0` kept its first 846 same-frame comparisons clear, but 219 of
2,095 render checks exceeded 0.25 ms (10.45%). The single-direction binary
search was still too expensive when the 5 cm reserve activated. The run was
stopped before the long threshold and the behavior is disabled.

The next candidate replaces that repeated search with a direct conservative
bounding-sphere exit calculation. The exact held-weapon triangle mesh and the
authored target compound already have bounded root-relative radii. Solving the
ray/sphere exit for each of the three observed target roots takes fixed scalar
work, after which one complete triangle-versus-compound predicate must prove
the proposed visible pose clear. Failure still leaves the correction disabled
for that frame and increments the existing loud failure counter.

### Direct-bound validation (`4cf1be3`)

The installed Steam candidate from source `4cf1be3990595dd682661eb809df7804f7d6a9a6`
(`halo3xr.dll` SHA-256
`EAEDEA32735D579EB01A95A3A50103792C99A8FDEC7634504B4F79424609EB7D`)
passed the long rotating-body test in visibly confirmed Halo 3 Forge on
Construct. The final status recorded 19,302 render checks, 27 over 0.25 ms
(0.14%), 19,302 conservative target fallbacks, 2,815 visible separations, and
zero separation failures. The independent exact-target same-frame comparison
recorded 8,006 samples and zero confirmed penetrations. Preserved log:
`out/debug-openxr/20260813-015531560Z-rotating-body-gap.log`, SHA-256
`D446AC3343ADADAEBDFCD2AAF709761E7159FF6160419921BA4EE72E5EB28D90`.

The same installed candidate also passed all focused regressions through
visible Halo 3 Forge navigation:

- loose weapon push, lift, and release: `20260813-020501796Z-weapon-scoop.log`
  (`9CE04C32009E8D5E384135779F57DA83011FF6F1D0DBFED287CFC18397DEDB67`);
- left-hand pickup with native mass and release velocity:
  `20260813-020846495Z-left-grab.log`
  (`1F607BAAC0218E2FA67D305BF30F92EF645EBF596F85C79D693D0E864955E6FD`);
- native melee plus contact haptic: `20260813-021159426Z-melee.log`
  (`6FB16A949B574A2483975FB40717AD45C34C2C808098F8001C07CA65D93E6F0D`);
- heavy vehicle nudge without melee on High Ground:
  `20260813-021501426Z-vehicle-nudge.log`
  (`B123BB96C0D11A9963231284F459D9BF67C35B46FE47C37CF5FDDD94017794AD`);
- structure and placed-object wall contact on Valhalla:
  `20260813-021826281Z-wall.log`
  (`18298496980715636523129AAB82ACB2D74761A4F88710282EE3DAC2E772BD6D`);
- Valhalla decorator/rock collision: `20260813-022200192Z-decorator-wall.log`
  (`B12C03C005C6943471350A3252B22B04F458AC566A3F7869FC548A792F59B81A`).

Every run used the SteamVR null driver only for automation and restored the
original settings hash
`F8B2C009AE05A2AC796D3458B9AA8B072A5BEA97CA8EC5A74FAA5DD585DBAB9E`.
These passes are strong runtime evidence, but are not headset acceptance.

The documentation-only descendant `2007344` was rebuilt and rerun to verify
its exact installed identity. That longer run caught one confirmed same-frame
penetration after four `bodyRenderSeparationFailures`. The coarse sphere exit
reached its one-metre safety cap and could not prove a clear result for those
frames. Preserved log:
`out/debug-openxr/20260813-022740574Z-rotating-body-gap.log`, SHA-256
`0D3C682078B084893341F0CA3BEB65570DC7014DED599949F5A7B3C8C00C3503`.
This overrules the earlier pass: the direct-sphere behavior is rejected and
disabled before the next candidate.

The replacement measures the exact held-weapon mesh's minimum support point
and the authored target compound's maximum support point along the selected
outward direction. Their projected difference plus the 5 cm render reserve is
the smallest translation that makes that plane separating. Only observations
that actually overlap the intended weapon pose contribute, the result is
bounded to four metres, and one complete triangle-versus-compound query must
still prove all observed target poses clear. This keeps constant query count
while avoiding the sphere bound's false one-metre refusal.

Candidate `c34dad4` disproved the whole-compound version. Its first run found
one confirmed same-frame penetration after eleven render separation failures.
The target collision model contained a distant authored child, producing a
43 m root-space span even though the actual overlapping child was local. A
support plane over every child therefore exceeded the four-metre guard and
correctly refused to move, but left the visible weapon intersecting. Preserved
log: `out/debug-openxr/20260813-023617291Z-rotating-body-gap.log`, SHA-256
`567D3A5F3B4DBFEE2ABE7C6C12BE661B5CCD874973A51FAC7C2C48C7CADA26E3`.
The whole-compound behavior is rejected and disabled.

The next candidate takes the triangle and target-child indices returned by the
actual expanded intersection. Its direction runs from the overlapping target
child's transformed bounds centre to the overlapping weapon triangle centre.
Its support plane includes the complete held weapon but only target children
that truly overlap in one of the three observed target poses. A final complete
compound query still rejects any motion that would meet another child. This
removes object-root and distant-child offsets from the correction calculation.

Candidate `a43d7f2` still failed its first long run. The first selected child
was cleared, but two render corrections later failed when the translated
weapon met another child or another one of the three observed target poses;
the independent checker then recorded one confirmed penetration. Preserved
log: `out/debug-openxr/20260813-024350832Z-rotating-body-gap.log`, SHA-256
`DECFD5B8F6CBA4F50194B5C1AC1479F151F212BDF3EBD22B2A8538B65417601E`.
The one-step child behavior is rejected and disabled.

The replacement fixes the outward axis from the first real primitive pair and
moves only forward on that axis. Up to eight fixed steps inspect the three
observed target poses, separate the whole held-weapon mesh from the exact child
encountered, and repeat if that forward motion exposes another child. Because
every step advances on one axis, a child already cleared cannot be re-entered.
The loop is fixed and allocation-free; a correction is published only after a
complete pass over all observed poses finds no remaining overlap.

Candidate `307fb22` showed that eight steps are insufficient. A 16-child
target observed at three poses can expose more than eight distinct child/pose
pairs along the fixed outward ray. The run accumulated 58 refused corrections
and then four confirmed same-frame penetrations. Preserved log:
`out/debug-openxr/20260813-025144090Z-rotating-body-gap.log`, SHA-256
`7FA782FFCB8B9D5D64EFD3E1168959E4FC0552864E917CA37E1B4629A5992F75`.
The eight-step behavior is rejected and disabled.

The replacement uses the structural maximum: 16 authored compound children
times three observed target poses, or 48 forward-only steps. It remains fixed,
bounded, allocation-free, and monotonic. Runtime validation must prove both
zero refused corrections/penetrations and that fewer than 5% of callbacks
exceed the 0.25 ms budget before this can survive.

Candidate `48d6ba4` reached all 48 steps but still accumulated 44 refused
corrections before two confirmed penetrations. The observed target rotation
required more than the four-metre cumulative forward ceiling; callback cost
remained safe (zero over-budget samples at the failure). Preserved log:
`out/debug-openxr/20260813-025816999Z-rotating-body-gap.log`, SHA-256
`E167CD15708CB99DCB30B689314ABB85721020C940D3654FD5AF862E13F45AF1`.
The four-metre-ceiling behavior is rejected and disabled.

The next candidate retains the 48 structural steps and permits up to 16 m of
cumulative forward escape for the deliberately extreme rotating replay. This
is a fail-closed upper bound, not an ordinary setback: every step requires a
current expanded-geometry intersection, and the result is published only after
all three observed poses are proven clear. Headset testing must still reject
any visible jump in ordinary push/lift use even if the stress test passes.

Candidate `f358e89` did not record a confirmed same-frame penetration, but it
still accumulated 388 render separation failures in 26,796 checks and could
not reach the strict pass condition. The 16 m ceiling was therefore not the
limiting factor; a fixed outward support direction is not sufficient for every
rotating compound pose. Preserved log:
`out/debug-openxr/20260813-030435875Z-rotating-body-gap.log`, SHA-256
`C2296390F4B084B3CFA5D3AC152C3AC1E38416331D2F3BFE886D0227BE5D257D`.
The behavior is rejected and disabled before the next candidate.

Candidate `ee38ad0` added a rare authored-triangle proof for conservative-hull
refusals. The proof correctly accepted nine hull-only overlaps, but three
other frames remained real authored-surface overlaps and the same-frame check
confirmed one penetration. Preserved log:
`out/debug-openxr/20260813-031515820Z-rotating-body-gap.log`, SHA-256
`44F02A10614EB6D0AA62765152EFFBBDE1112332C4D6934D10EF6A309C62055E`.
The conservative-first behavior is rejected and disabled.

Candidate `49c392c` used authored target triangles in every final-palette
callback. It was both insufficient and too expensive: after 1,251 checks it
had 66 unresolved separations and one confirmed same-frame penetration, while
565 callbacks (45%) exceeded 0.25 ms. Preserved log:
`out/debug-openxr/20260813-032325066Z-rotating-body-gap.log`, SHA-256
`53127F4B9A2CCABD92D321F984AA940D1D6B2F2B555423DB817909B92F9880FF`.
The always-exact behavior is rejected and disabled.

Candidate `56785cb` eliminated all solver refusals and kept callback cost low,
but one exact same-frame penetration still appeared after 9,839 render checks.
The rare escape itself worked (744 exact clears); the remaining hole was a
conservative correction that reported clear without a final authored-mesh
proof. Preserved log:
`out/debug-openxr/20260813-032957152Z-rotating-body-gap.log`, SHA-256
`ECD145FB50F42AF27F22A2E83E42DB7CD96C0E4A253F131D17BDC35E9E0EFD1A`.
The unverified-correction behavior is rejected and disabled.

### 2026-08-13 campaign living-enemy admission regression

The headset-rejected `a3efc86` run installed the native melee bindings and
tracked the right controller, but every contact report remained at
`eligible=0` and no animated-body sweep or melee ran. Preserved log:
`out/debug-openxr/20260813-092138-headset-rejected-a3efc86.log`, SHA-256
`C0890B9EDF39A57D9A0FA12F383311B46C00E2F4FECB50DB09B9BDDFF8849142`.

The object enumerator required the root Havok body to be dynamic before it
called `Halo3ContactSweepAnimatedBodies`. That contradicts the already proven
H3EK behavior above: living bipeds and creatures are normally keyframed, while
their exact animated bodies are still valid native melee targets. The fixed
admission rule therefore accepts either a validated dynamic root object or an
enemy melee kind (biped, creature, or giant). The later impulse path is
unchanged and still requires the exact selected body to be dynamic, so living
enemies do not receive synthetic rigid-body impulses. Pure tests cover dynamic
props, keyframed and unresolved enemy kinds, fixed non-enemy objects, vehicles,
and excluded objects. Headset acceptance remains pending.

### 2026-08-13 indexed-instanced solid decorators

The rejected campaign report also identified a visible rock that the weapon
passed through. The preserved `a3efc86` log accumulated native wall rays but no
blocking plane for that contact. Existing exact decorator capture covered only
non-indexed `DrawInstanced` triangle strips. D3D11 and the already bounded
diagnostic hooks show that Halo also submits instanced geometry through
`DrawIndexedInstanced`; production did not observe that draw family at all.
This is a general renderer boundary, not a map or rock allowlist.

The production capture now also observes the bound index buffer and
`DrawIndexedInstanced`. It accepts only the already proven 20-byte compressed
decorator vertex format, 16-byte placement format, exact 48/96-byte constants,
triangle-list or triangle-strip topology, and 16- or 32-bit indices. Initial
vertex and index bytes are copied once into fixed owned storage at buffer
creation. Draw callbacks publish only pointers, sizes, constants, and bounded
draw parameters into the existing fixed frame snapshot; they perform no
allocation, logging, GPU readback, COM call, file I/O, or lock.

The contact worker bounds index count to the triangle-mesh structural maximum,
copies at most 16 KiB of vertex data and 1.5 KiB of indices to fixed stack
storage, validates every signed base-vertex result, decodes exact indexed
triangles, and retains the existing three-axis solid-mesh rejection so planar
foliage cannot become a wall. Pure tests cover a 16-bit indexed solid mesh and
out-of-range-index rejection. Existing non-indexed Valhalla capture is
unchanged. The specific campaign rock and indexed runtime path remain pending
headset acceptance.

### 2026-08-13 enemy root-proxy bypass

The next campaign headset result reported no physical melee. Inspection found
that enemy objects could resolve an object-level collision model before the
animated-body path ran. That root-space proxy is not the visible animated head,
torso, or limb bank already proven from official H3EK, so its presence could
prevent exact animated-body contact from being considered at all.

Bipeds, creatures, and giants now always use the official animated multi-body
collision bank. Props and vehicles keep their existing authored object-level
triangle/convex path. The exact 1.25 mm surface remains the only source of
visual blocking and physics impulse. Enemy melee alone receives an 8 cm sampled
catch zone to cover the render/physics clock split; it cannot constrain the
weapon or create a vehicle/prop melee. The existing armed latch, 250 ms global
cooldown, tracked-weapon speed, and native exact-target melee call remain.
Headset acceptance is pending.

### 2026-08-13 indirect instanced decorator draws

The indexed-instanced candidate still missed the reported campaign rock. The
production renderer boundary observed direct `DrawInstanced` and
`DrawIndexedInstanced`, while its already existing diagnostic boundary showed
that D3D11 also exposes `DrawInstancedIndirect` and
`DrawIndexedInstancedIndirect`. Those two production hooks were absent, so an
otherwise valid 20-byte solid decorator mesh and 16-byte placement stream could
remain invisible to contact solely because Halo selected an indirect draw.

Production now observes both indirect draw families. Buffers carrying D3D11's
`DRAWINDIRECT_ARGS` flag are copied into an existing bounded 512-byte lock-free
snapshot when Halo supplies or CPU-updates their contents. The hot draw hook
only reads that owned snapshot and decodes the documented 16-byte or 20-byte
argument record; it performs no COM call, GPU readback, allocation, lock,
logging, or file I/O. Missing, stale, GPU-only, zero-count, or out-of-bounds
arguments fail closed for that draw. Decoded draws still must pass every
existing decorator invariant: exact vertex/placement strides, topology and
index format, owned geometry bounds, finite shader constants, and the solid
three-axis mesh test. Pure coverage checks the indexed indirect layout,
negative base vertices, nonzero offsets, and truncation rejection. The exact
campaign-rock headset check remains pending.

### 2026-08-13 comprehensive animated-enemy catch

The next campaign headset report still found no physical melee. The existing
enemy-only catch had two remaining limits: its 8 cm radius did not cover a full
fast 120 Hz weapon step plus independent limb animation, and the second pass
ran only when triangle weapon geometry was present. A weapon using the approved
authored convex fallback therefore remained millimeter-exact.

The animated-body sweep now gives both weapon geometry paths the same 18 cm
enemy-only sampled-contact skin. The triangle path uses its existing rounded
surface query. The convex path temporarily increases the copied convex radius
before its bounded continuous sweep, leaving the immutable authored shape
unchanged. Both paths still return the exact contacted animated rigid-body
index. A catch-only hit can request native melee, but it still cannot publish a
visual constraint or physics impulse. Props, vehicles, static objects, and
dynamic nudging keep exact geometry and their existing thresholds. Pure tests
cover the convex fallback gap, exact child selection, enemy-kind radius, and
unchanged vehicle radius. Headset acceptance remains pending.

### 2026-08-13 fixed-object exact wall sweep

The next campaign report still identified a rock that the weapon could cross.
The wall transaction had three providers: native camera-to-weapon point rays,
captured decorator meshes, and motion rays for thin-wall tunnelling. It did not
directly compare the complete weapon solid with fixed object collision. A
placed scenery or machine object could therefore own valid authored collision
but remain absent when its native ray provider did not return the needed
surface and its renderer did not use a captured decorator draw.

The wall worker now enumerates the same bounded live Halo object table already
used by dynamic contact. It admits only root objects that the proven Havok
motion classification identifies as fixed/keyframed, or whose body cannot be
resolved and must therefore fail closed as a blocker. A swept bounding-sphere
test rejects distant objects. Nearby candidates resolve through the existing
H3EK-authored detailed collision reader, with the native Havok compound as a
fallback, and receive a continuous triangle/convex weapon sweep. An exact hit
adds its target surface, weapon support point, and camera-facing normal to the
existing multi-plane visual constraint solver. Dynamic bodies remain in the
separate impulse path. The player, held weapon, attachments, invalid handles,
and non-finite geometry are rejected. No map, scenario, tag, or rock identity
is hardcoded. The cumulative Release build passes; the reported campaign rock
remains pending headset acceptance.
