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
`+0xAC`; no absolute global address is shipped.

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
