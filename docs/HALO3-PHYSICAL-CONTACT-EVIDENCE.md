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
| `objects_update` simulation owner | `0xA52920` | `0x34067C` | 1 | retained H3EK `objects.cpp` assertion metadata identifies the body containing the `object_update_absolute_index` transaction; the retail homolog preserves the TLS object table, active-object loops, `object_update_absolute_index` writes, and update-in-progress byte, and the complete runtime signature is unique |
| `unit_melee_effects` (rejected for damage) | `0xA63390` | `0x35A194` | 1 | official disassembly proves the eight arguments and authored effect selection, but the body only emits melee contact effects; it is not used as the damage entry point and is reached only through Halo's stock wrapper |
| authored melee tag selector | `0xA5DE20` | `0x35A9A4` | 1 | the official and retail bodies resolve the active weapon and select its ordinary/clang damage and response tag pair; H3EK's own constant-string table maps `0x0A` to `melee`, and both selector bodies route that value to the first-hit pair without entering lunge selection |
| `damage_owner_from_object` | `0xAA0120` | `0x384A88` | 1 | official assertion/source metadata and both bodies prove the object-handle plus 0x0C-byte owner-output ABI used by Halo's stock melee caller |
| melee damage application helper | `0xA59860` | `0x35BEFC` | 1 | official assertions name the `damage_owner` and `damage_target` arguments; the retail body copies those records into native damage data, sets the melee damage flags, and enters the engine's damage application path |
| stock melee effects/response wrapper | stock caller sequence following `0xA596DA` | `0x35BCA0` | 1 | the retail stock melee caller passes the selector's damage/response tags, exact target index, material, point, and normal; the wrapper invokes `unit_melee_effects` and the authored impact response path |
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
surface/node/region/material sentinels, and scale 1.0, matching the stock
zero-initialized melee record where contact metadata is unavailable. The command
also carries the weapon handle; the simulation consumer revalidates that the
same weapon remains active before selecting tags. No grip, trigger, animation,
melee action, contest, or lunge input is synthesized.

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
The exact final visible right-wrist transform
is published by the first-person palette path through a bounded atomic snapshot.
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
flags, validated target/player/weapon handles, world velocity, contact
point/normal, generation, timestamp, and serial. The unique `objects_update`
hook consumes that command immediately before the authoritative object update.
The hook performs no allocation, logging, file I/O, locking, or signature
scanning and always calls the original update routine.

The closest native surface blocks its sample. BSP and instanced geometry receive
no impulse or damage. Player, held weapon, attached/first-person-only objects,
stale handles, and invalid values are rejected. Every exact object hit publishes
one native velocity command per continuous contact above 0.05 m/s; the
simulation owner validates and applies it before object update. The capsule
fallback admits only the engine object kinds that can own movable physics; the
native velocity path remains the final authority for whether the exact object
actually owns a rigid body. The velocity delta is proportional to swing speed
and clamped. At or above the configured threshold, an independently debounced
command selects the equipped weapon's native melee tags, builds native damage
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

## Verification boundary

The pure regression suite covers translation and rotation sweeps, tunnelling,
grazing misses, the noise floor, slow pushes, exact melee threshold crossing,
finite-value rejection, movable/static classification, impulse clamping,
per-target overlap debounce, separation rearming, reset on weapon/tracking
change, 250 ms cooldown eligibility, timestamp-underflow rejection, and capsule
fallback. The cumulative Release build and complete `ctest` suite must pass
before packaging. Headset acceptance (specific weapons,
materials, enemies, loose weapons/crates, walls, pause/loading/death, and rapid
motion) remains intentionally pending; the feature therefore defaults off.
