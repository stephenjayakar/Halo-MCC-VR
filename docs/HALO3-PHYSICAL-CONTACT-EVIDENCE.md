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
| generic swept collision core | `0x652A10` | closest homolog `0x1FD748` | not bound | H3EK assertions include `_collision_result_object`; the large `0x14C3` body and private result layout are not safe to call without symbols for every retail field |
| `object_set_velocity` native | `0xAD8050` | `0x39BAD0` | 1 | the official script wrapper at `0x7A88F0` and retail wrapper at `0x1E380C` call their respective native with object handle plus three local velocity floats |
| native melee contact response | `0xA63390` | `0x35A194` | 1 | structurally matched official/retail bodies; callers pass unit, authored damage-effect datum, exact target handle, melee type, material, direction, point, and normal |
| `game_is_cooperative` native | `0xCFB7D0` | `0x0F000C` | 1 | official/retail script wrappers call the native; its body first requires game-options byte `+0x10 == 1` (campaign), then returns whether the authoritative player count is greater than one |

The retail signatures embedded in `game.cpp` wildcard only relocation/call
displacements. An independent offline rescan of the pinned retail image found
one match for each complete pattern at the RVAs above.

The active held weapon is read through the already-proven Halo 3 object table:
the player unit's current weapon slot is `unit+0x262`, handles begin at
`unit+0x268`, object definition datum is `object+0x00`, and bounds are
`object+0x1C/+0x28`. The ordinary authored melee effect is the unit definition's
`+0x1B4` datum. When weapon-definition flag bit 9 at `+0x18C` is set, the weapon
override at `+0x22C` is used; this preserves energy-sword and gravity-hammer
authored contact effects. No grip, trigger, animation, or lunge input is
synthesized.

## Collision-shape decision

Triangle-accurate collision was attempted first by identifying H3EK's native
swept-collision core. It is rejected for this candidate because the function's
private query/result ABI is large, the retail homolog cannot be proven field for
field from available symbols, and no immutable first-person triangle stream can
be prepared without entering unproven renderer/tag ownership. Calling it would
violate the project's fail-closed retail-binding contract.

The selected fallback is therefore a bounds-derived capsule. Its length and
radius are clamped from the held weapon object's authored bounding radius; an
explicit 0.65 m by 0.06 m capsule is used only when those bounds are invalid.
The prior and current grip, midpoint, tip, and capsule spines are swept against
validated object bounding spheres. This is bounded, allocation-free, lock-free,
and contains no logging, signature scan, file access, or GPU readback in the
camera callback. The exact final visible right-wrist transform is published by
the first-person palette path through a bounded atomic snapshot. OpenXR pose,
linear/angular velocity, timestamp, and serial use a separate bounded atomic
snapshot.

## Runtime safety and behavior

Contact requires all of the following: the opt-in setting, current Halo 3
generation, unique native bindings, live validated object table, fresh right
controller tracking, final visible weapon pose, alive player with a valid held
weapon, authoritative `Gameplay`, proven on-foot state, unpaused engine,
player-controlled camera, and either campaign mode with one authoritative
player or multiplayer mode with the exact local/offline simulation role used by
Forge. Synchronous/distributed client and server roles remain disabled. Any
failure resets contact state and performs no native write.

The closest object blocks the sweep. Static scenery receives no impulse or
damage. Player, held weapon, attached/first-person-only objects, stale handles,
and invalid values are rejected. Movable object kinds receive a native velocity
update once per continuous contact above 0.05 m/s; the velocity delta is
proportional to swing speed and clamped. At the configured threshold (default
1.50 m/s), that same exact target additionally receives one native authored
melee response. Separation rearms a target, the below-half-threshold dwell is
100 ms, and the global melee cooldown is 250 ms. Existing grip melee and all
normal input paths remain unchanged.

## Verification boundary

The pure regression suite covers translation and rotation sweeps, tunnelling,
grazing misses, the noise floor, slow pushes, exact melee threshold crossing,
finite-value rejection, movable/static classification, impulse clamping,
per-target overlap debounce, separation rearming, reset on weapon/tracking
change, and capsule fallback. The cumulative Release build and complete `ctest`
suite must pass before packaging. Headset acceptance (specific weapons,
materials, enemies, loose weapons/crates, walls, pause/loading/death, and rapid
motion) remains intentionally pending; the feature therefore defaults off.
