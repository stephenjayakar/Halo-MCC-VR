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
| `unit_melee_effects` (rejected for damage) | `0xA63390` | `0x35A194` | 1 | official disassembly proves the eight arguments and authored effect selection, but the body only emits the melee contact effects; it is not a damage-applying entry point and remains unbound |
| `game_is_cooperative` native | `0xCFB7D0` | `0x0F000C` | 1 | official/retail script wrappers call the native; its body first requires game-options byte `+0x10 == 1` (campaign), then returns whether the authoritative player count is greater than one |

The retail signatures embedded in `game.cpp` wildcard only relocation/call
displacements. An independent offline rescan of the pinned retail image found
one match for each complete pattern at the RVAs above. The collision pattern's
single file-offset match is `0x1FCB48`, which maps to RVA `0x1FD748`.

The active held weapon is read through the already-proven Halo 3 object table:
the player unit's current weapon slot is `unit+0x262`, handles begin at
`unit+0x268`, object definition datum is `object+0x00`, and bounds are
`object+0x1C/+0x28`. The earlier `+0x1B4/+0x22C` damage-effect selection was not
proven by the native body and is rejected. `unit_melee_effects` actually reads
weapon-definition fields `+0x8C` and conditionally `+0x49C`; because effects
alone do not satisfy native melee damage, high-speed contact stays stock until
the damage-applying path is separately established. No grip, trigger,
animation, or lunge input is synthesized.

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

Collision remains on the camera callback, but object mutation does not. The
camera callback publishes a bounded atomic command containing the validated
target handle, world velocity, generation, timestamp, and serial. The unique
`objects_update` hook consumes that command immediately before the authoritative
object update. The hook performs no allocation, logging, file I/O, locking, or
signature scanning and always calls the original update routine.

The closest native surface blocks its sample. BSP and instanced geometry receive
no impulse or damage. Player, held weapon, attached/first-person-only objects,
stale handles, and invalid values are rejected. Every exact object hit publishes
one native velocity command per continuous contact above 0.05 m/s; the
simulation owner validates and applies it before object update. The native is
deliberately allowed to decide whether that object owns a movable rigid body,
avoiding a brittle object-kind whitelist that excluded Forge scenery and
machines. The velocity delta is proportional to swing speed and clamped. The
high-speed melee threshold/debounce remains present but fail-closed for this
collision/rigid-body candidate because the previously bound native was
effects-only. Existing grip melee and all normal input paths remain unchanged.

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

## Verification boundary

The pure regression suite covers translation and rotation sweeps, tunnelling,
grazing misses, the noise floor, slow pushes, exact melee threshold crossing,
finite-value rejection, movable/static classification, impulse clamping,
per-target overlap debounce, separation rearming, reset on weapon/tracking
change, and capsule fallback. The cumulative Release build and complete `ctest`
suite must pass before packaging. Headset acceptance (specific weapons,
materials, enemies, loose weapons/crates, walls, pause/loading/death, and rapid
motion) remains intentionally pending; the feature therefore defaults off.
