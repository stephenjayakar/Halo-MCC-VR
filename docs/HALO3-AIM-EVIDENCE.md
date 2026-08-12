# Halo 3 VR aim evidence

This document records Halo 3-only aim changes. A source change or passing unit
test is not headset acceptance. `docs/CURRENT-STATE.md` remains the accepted
build pointer.

## Weapon-owned firing angle

The established Halo 3 visible-weapon path takes the mount-trimmed
`VR_GetAimPose` in `ControllerWorldPoseEx`, converts it to the game frame with
`BuildTrackedGameBasisFromFrame`, and uses basis column 0 as the weapon forward
axis. `DesiredWristWorld` then aligns the authored barrel-in-wrist axis to that
same column before applying one rigid transform to the visible palette and the
marker/muzzle consumers.

Before this candidate, `Game_ComputeAimStick` did not preserve that direction.
It built a point `crosshair_distance_m` along the hand ray, subtracted the room
head position, and steered Halo toward the resulting head-to-point vector.
Therefore translating the head while holding the weapon still changed the
requested firing angle. This was a deterministic VR-side mismatch; no new
engine address or structure inference is involved.

The Halo 3 path now gives the normalized mount-trimmed weapon-forward vector to
the existing game-owned stick/aim integration. Weapon position, head position,
and crosshair distance cannot rotate it. The existing passenger-seat
re-origining remains later in the path because a personal weapon fired from a
vehicle still needs the proven engine-eye/rendered-eye correction. ODST and
Reach keep their previous behavior.

`ComputeVrWeaponAimRay` is allocation-free, lock-free, finite-checked, and unit
tested for:

- exact normalized weapon direction;
- invariance under independent weapon and head translations;
- preservation of the legacy convergence mode for other titles;
- rejection of invalid tracked data.

Runtime acceptance still requires a Halo 3 headset test proving that the
visible barrel, floating reticle, muzzle effects, and shot direction remain on
one line while the player moves their head around a stationary weapon.

## Direct weapon-owned projectile base ray

The stick path above makes Halo's normal unit aim approach the visible weapon,
but it cannot make it arrive immediately. Official H3EK proves why: function
`unit_euler_aiming_update` (`halo3_tag_test.exe` RVA `0xA5EC80`) integrates the
unit's aim under authored angular-velocity and acceleration limits. Official
`unit_get_aiming_vector` (RVA `0xA60D80`) then copies the integrated vector from
unit `+0x1AC..+0x1B4`. A fast hand movement can therefore leave the shot on the
torso-owned vector for several frames even though the visible weapon has
already moved.

Official H3EK names the narrow firing helper at RVA `0xA55D90`
`unit_adjust_projectile_ray`. Its eight arguments are the firing unit handle,
origin, forward vector, inherited velocity, first-person weapon offset, and
three booleans for origin offset, aim offset, and origin verification. Its sole
direct caller is at `0xA84C33` inside the official weapon-barrel projectile
creation function. The helper obtains the unit camera position, conditionally
copies the unit aiming vector, and performs Halo's authored origin adjustment
and verification before the caller continues into targeting and aim assist.

Pinned retail RVA `0x3524B0` is the exact optimized homolog. It has the same
eight-argument ABI, the same camera/seat alternatives, the same unit aiming
vector fields, and one direct caller at RVA `0x368B92` in the retail weapon-fire
function. The following entry signature occurs exactly once in the complete
pinned `halo3.dll` (file offset `0x3518B0`, RVA `0x3524B0`):

```text
48 8B C4 48 89 58 08 48 89 70 10 48 89 78 18
55 41 56 41 57 48 8D 68 C1 48 81 EC C0 00 00 00
44 8B 15 ?? ?? ?? ?? 48 8B FA 0F 29 70 D8
```

The optional detour always calls Halo's original helper first. It may then
replace only the final `forward[3]` with a fresh, finite, normalized direction
published from the same world yaw/pitch that places the visible weapon. It
does not change origin, spread, projectile data, weapon tags, animation, input,
or trigger state. Because the hook is before the caller's native targeting and
aim-assist work, those systems remain downstream of the corrected base ray.
The normal right-stick servo also remains active so the character and weapon
animation catch up naturally.

The replacement is limited to the exact output-user-0 unit, on foot, while VR
head tracking and VR aim are active, when the native call requested aim offset,
and when the lock-free publication matches the current Halo 3 generation and
is at most 100 ms old. AI, vehicles, stale samples, title transitions, tracking
loss, and every failed identity check keep Halo's original direction. A runtime
fault disables only this optional replacement while the detour continues to
call the original helper. Unit coverage proves world-vector construction,
normalization, the freshness boundary, and every local-player/lifecycle reject.

Headset acceptance remains required. Fire the assault rifle while rapidly
moving the controller sideways and while moving the head around a stationary
weapon. The shot and muzzle should follow the visible barrel immediately;
target adhesion should remain present. The log must contain both the installed
line and the first-local-shot line.

## Always-scoped VR auto-aim

### Pinned evidence

| Artifact | SHA-256 |
| --- | --- |
| Official H3EK `halo3_tag_test.exe` | `59A78F2C96034D7CEB5D710505B2B36813AA141FC81A083E3F952973DBCE4602` |
| Retail `halo3.dll` | `B209D8454B12DC77E54CCD2C9924EC8D44B8619D21CF98E36FFAF601E67EFB63` |

ManagedBlam loaded the official assault-rifle weapon tag
`objects\weapons\rifle\assault_rifle\assault_rifle.weapon`, checksum
`0xBF2A5048`. Its root definition gives these exact offsets:

- magnification-level count: `+0x31E`;
- magnification range: `+0x320`;
- auto-aim angle/range/falloff: `+0x328/+0x32C/+0x330`;
- magnetism angle/range/falloff: `+0x334/+0x338/+0x33C`;
- deviation angle: `+0x340`.

This directly disproves the earlier community `+0x378` lead; it was off by
`0x50` and is not used.

Official H3EK RVA `0x0040DE60` is
`aim_assist_build_query_parameters`. Its ABI is unit handle in `ECX`, query
flags in `RDX`, signed magnification level in `R8W`, and the `0x40`-byte output
in `R9`. It treats `-1` as unscoped. Its call to
`weapon_get_magnification` at H3EK RVA `0x00A8A0D0` accepts levels
`0 <= level < magnification_levels`, making zero the first authored scoped
level. It then applies the returned magnification to all seven weapon fields
above: angles use the reciprocal and ranges use magnification. The function
also tests the weapon's `aim assists only when zoomed` flag against the
unscoped level before it builds the query.

Pinned retail RVA `0x0013C518` is the exact homolog. It has the same ABI,
unscoped gate, magnification helper, and field reads. Two independent byte
identities are each unique in the complete pinned retail file:

- the 50-byte query-builder entry signature matches once at file offset
  `0x13B918`, mapping to RVA `0x0013C518`;
- the 87-byte ordered field-consumer signature matches once at file offset
  `0x13BA8E`, exactly builder `+0x176`.

The retail builder also proves the loaded-definition route used by the hook. It
gets the unit's current weapon, reads the weapon object's first word as the
definition index, resolves the tag-instance entry at `index * 8 + 4`, and adds
the stored address in dwords to the loaded-tag-data base. The two RIP-relative
global slots are decoded from this matched function at builder `+0x94` and
`+0xAC`; no absolute global address is shipped. The definition-index load is
the exact `0F B7 11` instruction at builder `+0x9B`, immediately after the
seven-byte `48 8B 05 disp32` instruction at `+0x94`. The first headset build
incorrectly checked `+0x9F`, which is four bytes into the following compare;
its log therefore reported `layout=0` and left this optional feature on stock
fallback. The corrected install check uses the pinned instruction boundary at
`+0x9B`.

### Runtime policy

The optional Halo 3 hook changes only an unscoped (`-1`) query while VR head
tracking is active and the held weapon authors a positive magnification-level
count. It passes level zero to Halo's original builder, so Halo itself applies
that weapon's first scoped auto-aim, magnetism, range, falloff, deviation, and
`aim assists only when zoomed` policy. It does not alter zoom state, input,
weapon tags, or the output query.

Weapons with no scope, calls that are already scoped, and all non-VR calls keep
their original level. The hook validates object generations, weapon kind,
loaded-tag pointers, both unique signatures, their exact relationship, the
definition-index load, and both RIP-relative global instructions. Runtime
object/tag reads are bounded, allocation-free, lock-free, logging-free, and
inside SEH because Halo can invalidate them during transitions. Any install or
runtime failure falls back to the original level for this feature alone; it
cannot disarm the camera, firing-angle fix, or OpenXR session.

Unit coverage proves the policy for unscoped one- and two-level weapons,
weapons without zoom, invalid counts, already-scoped levels, and VR-off calls.
The remaining acceptance test is in a Halo 3 headset: compare the same scoped
weapon unscoped versus scoped and confirm that target adhesion is unchanged,
while the visible barrel, reticle, muzzle effect, and shot still share one line.
