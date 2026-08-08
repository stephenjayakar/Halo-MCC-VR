# Halo 3 physical weapon contact evidence

Date: 2026-08-06

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
bits above 14. The executable's official enum metadata names low bit 0
`_collision_test_structure_bit` and low bit 3
`_collision_test_objects_bit`; its initializer at `0x140014040` emits low
`0x9`, independently proving that both enable bits are required. The first
native-query candidate incorrectly used high `1`, selecting one object type,
then the first mask correction still omitted the low object-enable bit.
Physical contact now combines low `0x9` with the proven high `0x7FFE` mask.

## Collision-shape decision

Candidate `a644d2c` used a bounds-derived capsule against every object's broad
bounding sphere. The 2026-08-06 campaign runtime rejected it: roughly 95% of
sweeps reported a hit, almost all stopped at `static-block`, and neither Forge
nor campaign testing produced visible object movement. The hit distribution
proves that broad spheres are not a usable surface-contact proxy. Commit
`2f4940b` disables that failed behavior before the replacement candidate.

The replacement uses Halo 3's native swept-vector collision query. Five fixed,
allocation-free samples cover previous-to-current grip, midpoint, and tip plus
the previous and current weapon spines. Each sample resolves the engine's exact
authored BSP, instanced-geometry, or object surface and returns the first blocker;
the query ignores the player unit and held weapon. The weapon extent remains a
bounded proxy derived from the held weapon's authored radius (0.30–0.75 world
units, with a 0.65-unit invalid-bounds fallback), but targets are no longer
approximated by bounding spheres. The exact final visible right-wrist transform
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

The closest native surface blocks its sample. BSP and instanced geometry receive
no impulse or damage. Player, held weapon, attached/first-person-only objects,
stale handles, and invalid values are rejected. Every exact object hit is passed
to the native velocity setter once per continuous contact above 0.05 m/s; the
native is deliberately allowed to decide whether that object owns a movable
rigid body, avoiding a brittle object-kind whitelist that excluded Forge
scenery and machines. The velocity delta is proportional to swing speed and
clamped. The high-speed melee threshold/debounce remains present but fail-closed
for this collision/rigid-body candidate because the previously bound native was
effects-only. Existing grip melee and all normal input paths remain unchanged.

## Verification boundary

The pure regression suite covers translation and rotation sweeps, tunnelling,
grazing misses, the noise floor, slow pushes, exact melee threshold crossing,
finite-value rejection, movable/static classification, impulse clamping,
per-target overlap debounce, separation rearming, reset on weapon/tracking
change, and capsule fallback. The cumulative Release build and complete `ctest`
suite must pass before packaging. Headset acceptance (specific weapons,
materials, enemies, loose weapons/crates, walls, pause/loading/death, and rapid
motion) remains intentionally pending; the feature therefore defaults off.
