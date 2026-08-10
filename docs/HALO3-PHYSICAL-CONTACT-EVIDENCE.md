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

## First-person root composition

The first visible replay exposed a space error that the earlier synthetic rig
could not see. Halo's final weapon palette is still local to the first-person
root passed to `FpVisiblePaletteHook`; the renderer consumes
`root * destination[node]`. The existing arm and marker paths already prove
this relation by composing a palette node with `root` before comparing it with
a world-space controller target, then applying the inverse root before writing
back to the local palette.

Candidate `77011f0` moved the drawn weapon with exact authored support-point
placement while intentionally retaining the old contact publication. The
commanded surface gap swept from -6 cm to +2 cm, but the contact-side measured
gap ranged from `1.7213 m` to `4.5880 m`; 5,470 current-pose intersection tests
reported overlap and only two reported separation. This is direct evidence
that publishing `destination[node]` without the first-person root does not put
contact in the space Halo draws. The preserved failed probe is
`out/debug-openxr/20260810-121701541Z-visible-weapon-gap.log` (SHA-256
`BDBC587117559E3985994F9D20CFFA735AE1D329FB4307954049862ADB5F8AA0`).
The failed placement behavior was disabled by `616acfd` before the product
alternative. The alternative publishes `root * destination[node]` for every
bounded weapon node and converts debug replay world transforms back through
`inverse(root)` before writing the local render palette.

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
