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

### 2026-08-12 missing-publication diagnosis and repair

Opt-in diagnostic candidate `8d863b6` counted only bounded rejection gates in
the firing detour; it did not change aim behavior. A visible Halo 3 Forge run
fired the assault rifle through ordinary Windows mouse input. The detour saw 18
native firing calls and rejected all 18 at `sample`, with every later gate at
zero. The preserved log is
`out/debug-openxr/20260812-110455498Z-vehicle-nudge.log`, SHA-256
`C5700BC6C4E770E9ECC3A224E1DF3AC8C75B11ED064951951C3D6D8E76316CC8`.
The diagnostic was explicitly reverted by `390618e` before this repair.

The missing sample had a concrete code cause. Publication existed only inside
`Game_ComputeAimStick`, which is reached through MCC's XInput polling path.
Keyboard/mouse firing can call the verified native projectile helper without
polling that path, leaving the otherwise-correct firing detour with no sample.

The final visible right-hand path already computes the mount-trimmed controller
basis in `ControllerWorldPoseEx`. Basis column zero is the same forward axis
used to align the authored barrel and visible weapon. The repair publishes that
normalized column through the existing lock-free snapshot whenever the exact
Halo 3 generation is alive and on foot. This adds no input synthesis and does
not change projectile origin, spread, tags, aim assist, or firing state. Unit
coverage checks exact column selection, normalization, and invalid-basis
rejection. Runtime acceptance still requires the installed and first-local-shot
log lines from the same build, followed by the headset alignment test above.

### 2026-08-12 torso-frame overwrite diagnosis and repair

Headset candidate `68e7b56` installed the verified firing detour, but the user
reported that on-foot aiming still felt relative to their torso while native
aim assist remained active. Code inspection found two concurrent writers for
the one direct-shot snapshot. `ControllerWorldPoseEx` published the final
visible barrel basis, but `Game_ComputeAimStick` could run afterward and replace
it with a direction reconstructed from the body-turn servo's desired yaw and
pitch. The firing detour could therefore report a valid override while consuming
the torso-frame approximation instead of the visible weapon ray.

The direct-shot snapshot now has one owner: the final visible right-hand basis.
The XInput path retains the complete native right-stick/body-turn servo but no
longer writes or clears the projectile snapshot. The verified
`unit_adjust_projectile_ray` detour, native origin correction, spread, targeting,
and aim-assist stages are unchanged. This candidate remains unaccepted until an
exact-build headset test confirms that shots follow the visible barrel while
aim assist remains present.

### 2026-08-13 remaining stock-direction gate

The next headset report still described aim as torso-relative. Inspection found
one remaining deliberate fallback in the verified projectile hook: it required
the native `offsetAim` boolean to be true. Official H3EK proves that boolean
only tells `unit_adjust_projectile_ray` whether to copy the unit's integrated
aim into the call's forward vector. It is not a projectile identity flag; the
function's sole caller is already the weapon-barrel projectile creation path.

The isolated follow-up therefore applies the fresh visible-barrel direction in
both native `offsetAim` modes, while retaining every stronger gate: exact local
unit, on foot, VR active, current Halo 3 generation, finite unit vector, and a
sample no older than 100 ms. Halo still owns origin correction and verification,
targeting, aim assist, spread, ballistics, and all non-local or vehicle shots.
Pure coverage exercises both boolean modes. Headset acceptance remains pending.

### 2026-08-13 downstream targeting overwrite

The next headset report against `a3efc86` still described shots as relative to
the torso even though the installed log recorded a local direct-ray override.
Pinned retail disassembly identified the missing downstream writer. The sole
caller of `unit_adjust_projectile_ray` at `halo3.dll+0x368B92` later calls the
projectile-targeting helper at `+0x368DFE`. That helper's mutable seventh
argument is the same `[rbp-0x40]` direction overwritten by the direct-ray hook.
It writes the direction again before the caller copies it into the projectile
transaction at `+0x369118`. The caller applies authored random spread only
after that copy.

The targeting helper is uniquely identified at pinned retail RVA `0x13BAD0` by
its 48-byte entry signature. It has exactly one direct caller in the module.
The 47-byte caller signature is also unique at RVA `0x368DD8`; install-time
validation decodes its `E8 rel32` edge and requires it to target the matched
helper. The helper's seven-argument ABI and ordering are homologous to the
official H3EK firing path.

The direct-ray transaction now carries its validated local-shot direction in
thread-local state from `unit_adjust_projectile_ray` to that exact downstream
helper. The helper still runs normally, preserving its target bookkeeping and
other effects, then the detour restores only the visible-barrel direction. The
thread-local state is cleared at every projectile-ray call and consumed once,
so AI or a later shot cannot inherit it. Halo's authored spread and ballistics
remain downstream. Either signature, caller-edge, or hook failure leaves the
entire optional direct-ray transaction on stock fallback. Pure tests cover the
final normalized restore and its local-shot, pointer, finite-value, and
unit-length rejects. Headset acceptance remains pending.

### 2026-08-13 alternate campaign targeting branch

The next campaign headset result still described shots as torso-relative.
Reviewing the complete official H3EK weapon-barrel function found that the
previous repair covered only one of two mutually exclusive targeting branches.
The first calls official RVA `0x411F60`. The other calls official RVA
`0xD8E920` with the same mutable forward vector as argument 3, then both paths
join before authored spread. A restore attached only to the first helper could
therefore never cover a shot that selected the second branch.

Pinned retail has the exact alternate helper at RVA `0x5B15A4` and one direct
caller, at `0x368E55` inside the same verified weapon-barrel function. Its
48-byte entry signature is unique in the complete pinned module. A separate
53-byte caller signature is unique at `0x368E25`; install-time validation
decodes its `E8 rel32` at `+0x30` and requires it to target the matched helper.

The direct-ray transaction now hooks both native targeting helpers. Each calls
Halo first, consumes the same thread-local local-shot context, and restores only
the validated visible-barrel direction. The branches are mutually exclusive,
and the context is still reset at every new projectile-ray call, so no later or
non-player shot can inherit it. All three hooks install as one optional
transaction; any missing signature, bad call edge, or hook failure leaves
firing stock without affecting the camera or VR session. Authored spread and
ballistics remain after the restored direction. Headset acceptance is pending.

### 2026-08-13 visible shot origin

The next campaign headset report described shooting as still relative to the
torso. The existing detour replaced only `forward`; it deliberately left the
origin produced by Halo's torso-owned first-person weapon path unchanged. The
same bounded render snapshot now publishes the finite visible right-hand world
position with the visible direction. After the official
`unit_adjust_projectile_ray` has completed its stock work, the verified local,
on-foot VR transaction replaces both mutable fields before either targeting
branch consumes them. Halo still owns target selection, authored spread,
projectile type, velocity, damage, and every non-local or vehicle shot. A stale
generation, stale sample, invalid position, invalid direction, or lifecycle
doubt leaves both stock values untouched. Pure tests cover the finite origin
copy and reject. Headset acceptance is pending.

Pinned retail also proves that this is the final origin input to the projectile
transaction. Immediately after the call at `halo3.dll+0x368B92`, the caller
copies origin XY from `[rbp-0x30]` into nonvolatile `xmm15` at `+0x368C1F` and
origin Z into its private `[rsp+0x70]` cache. The two targeting branches run
later, but neither receives either cache. The joined firing path writes those
cached components into the projectile record at `+0x36912D/+0x369136`.
Direction remains mutable through targeting and is separately restored at the
verified branch hooks before its joined copy at `+0x369118`. Therefore an
additional post-targeting origin restore is unnecessary; candidate `2c9f099`
was reverted by `527d8b8` rather than retaining that disproven mechanism.

The next candidate adds read-only first-shot telemetry at the already hooked
shot boundary. It records the distance from Halo's stock origin to the
published visible origin, the angle from stock direction to visible direction,
and the distance from the published origin to the newest final rendered weapon
root. The hook performs only finite arithmetic and atomic stores; the existing
50 ms worker formats the one-time log. This does not change firing behavior.
It makes the next headset result distinguish a hook/gate failure, a coordinate
space error, and an authored wrist-to-render-root offset without relying only
on visual judgement.

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
