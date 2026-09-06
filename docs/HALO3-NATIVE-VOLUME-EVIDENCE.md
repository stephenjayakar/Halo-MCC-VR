# Halo 3 native volume movement

September 6, 2026 UTC. This follows the user's request for comprehensive world
collision, with player collision as a reference, instead of isolated surface
patches. No replacement weapon solver or headset acceptance is claimed here.

## Pinned source and verified call shape

The official H3EK `halo3_tag_test.exe` SHA-256 is
`59A78F2C96034D7CEB5D710505B2B36813AA141FC81A083E3F952973DBCE4602`.
Retail `halo3.dll` SHA-256 is
`B209D8454B12DC77E54CCD2C9924EC8D44B8619D21CF98E36FFAF601E67EFB63`.
`tools/verify-h3-volume-movement.py` first runs the existing point/gather
verification, then checks the movement/solver instructions and a unique retail
solver entry. Output: `out/research/20260906-native-volume/verification.json`.

Official `6506C0` moves its incoming radius from XMM3 to the fifth stack
argument, zeroes XMM3, then calls `64EC20` at `650717`. This is the zero-height
(sphere) form of the previously identified movement query. Direct call sites
to the wrapper are `4DF7FC`, `5DFD6C`, `646E92`; direct calls to the full query
are `5DFDA2` and the wrapper. The `5DEE...` caller includes explicit
`physics/collision_debug.cpp` source records. The scanner checked call
instruction boundaries inside their unwind ranges and saved disassembly under
the output directory. These are direct-call observations, not an exhaustive
indirect-call graph. **A player movement connection is not established.**

The full query gathers around position plus half displacement and vertical
half-height. Search radius is half displacement length plus half-height plus
sphere radius. Gather receives height and sphere radius as separate arguments;
zeroing both, as in the empty-region probe, is not a swept-sphere query.

Official `64EE60` and retail `1FEF30` consume those gathered features. Official
assertions name `old_position`, `old_velocity`, `new_position`, `new_velocity`,
and the three feature categories sphere/cylinder/prism. The caller's empty
gather branch directly copies position + displacement and unchanged input
displacement to the two outputs. The matched solver call shape is:

```
uint16 solve(position*, displacement*, features*,
             new_position*, new_velocity*, uint16 maximum_count, records*)
```

The returned collision-record stride is 48 bytes in both modules. The official
debug caller supplies 14 slots; another native caller supplies 16. The probe
supplies 16 and checks a sentinel beyond all 16 records. The record fields are
not otherwise interpreted by this probe.

## Retail exclusion difference

Retail movement `1FFB64` writes -1 to the gather's first ignored-object argument
at `1FFC45`, unlike the official caller forwarding it. Invoking that convenience
wrapper near the player's own collider would therefore be inappropriate for a
weapon query that must exclude its owner. The probe calls the already verified
gather separately with explicit player exclusion, using the native construction
of search center/radius and the sphere radius argument, then calls the matched
solver. It uses structure + instance + object flags already evidenced in
`HALO3-CLEARANCE-QUERY-EVIDENCE.md`.

## Opt-in bounded native probe

`HALOMCCVR_H3_CONTACT_DEBUG_VOLUME=1` / harness `-Test wall -ProbeVolume` enables
32 observations at 250 ms spacing on surfaces discovered by the existing native
wall fixture. All work occurs on the gated simulation contact worker. The probe
does not change weapon poses, object movement, melee, camera ownership or hooks.

It tries four radii (3, 8, 16, 24 cm), and four motion cases: crossing from the
free side, moving away, crossing with tangential motion, and starting 2 cm inside
the reference plane. The normal is oriented toward the camera that found the
surface. Nearby geometry can constrain motion sooner than that reference plane;
the signed reference-plane clearance is evidence, not a complete pass metric.
Point interior status, gathered counts, solved position/velocity, collision
count, sentinel/finite checks, and query time are published into fixed records.
The existing cold logger formats them. Invalid bounds or a fault disables this
probe only. Normal launches leave it off.

The harness requires 32 bounded, fault-free observations. That is admission and
memory-layout validation, **not** proof of correct sweep response. Review the
positive, negative, sliding and interior cases before using results in the
weapon solver. Rotating whole-weapon coverage, latency, dynamic target response,
Guardian/Floodgate behavior, videos and headset acceptance remain outstanding.

## First retail query result

Source `29c3c51165873a6f62c1bae4a07102c77e1f0eb8`, package
`out/candidates/29c3c51-h3-physical-contact-20260906-065418368Z`.
DLL SHA-256 independently verified after installation:
`B25D1DD886B3BD73F4AFCC6E28C4069C268822732A81FE19787670439B9168B9`.
Launcher SHA remains `D489C5763E21FC339999DC734CED09A2068AAC6C035EA8B7BF4339B6810FA450`.
Release, both CTest suites and Reach consistency passed. E: Steam installed;
alternate Steam and Store roots absent. Existing config was not changed.

Run `out/debug-openxr/20260906-065445170Z-wall-result.json` passed its probe
admission condition. Log SHA-256:
`C36315DB8366A116671DC12DFA9FC25021686F4AD21D6E7EA3253A78A51FCF4C`.
Steam / SteamVR null / Null Model Number / **Construct Forge**, verified from
the lobby image `out/debug-openxr/20260906-065537712Z-forge-menu-ocr/0000-065537855Z.jpg`.
All 32 records were bounded and fault-free. Parsed observations are in
`out/research/20260906-native-volume/runtime-observations.json`.

Observed native response:

- Instance (type 3), five frontal crossings: one collision each, final clearance
  equals the requested sphere radius within the logged float precision. Four
  tangential crossings likewise retain radius clearance and tangential motion.
- Fixed object (type 4), three frontal crossings: one collision each and final
  clearance equals requested radius. Tangential cases contact other geometry on
  that object; distance to the original reference plane alone cannot classify
  those outcomes as sphere penetration.
- Eight retreat controls: zero collisions, full requested motion, final reference
  clearance approximately 0.8 m.
- Four interior starts in the instance: point test reports inside, but solver
  returns zero collisions and continues to -0.92 m reference clearance. Therefore
  the sweep **cannot** be used as an overlap-recovery or interior-clearance proof.
- Four interior starts in the object: 1–16 returned collisions and variable
  output. The 16-contact case reaches the supplied capacity. This also does not
  establish a robust overlap recovery policy.

Observed costs include point test, gather and solve. Free-side instance crossing
peaked at 55.4 us, fixed-object crossing at 93.1 us, retreat at 41.4 us; the
interior object case peaked at 144.6 us. These are 32 queries around two local
surface classes, not a general performance bound or a whole-weapon benchmark.

The next behavioral experiment should maintain a known clear starting pose,
cover the weapon's volume through rotation/translation, and use this native
gather/solve path for world response. It must not blindly query from a hand pose
already inside a solid, and it must distinguish contact-capacity exhaustion from
a proved safe result. Existing impulse/melee behavior is a separate transaction.

The harness closed MCC/SteamVR and restored the independently checked settings
hash `298D6E805F90CADD0BD2564459AD19DAC15DF0634A5D2431F65506A3898C4D44`.
Null driver disabled, forcedDriver empty, requireHmd true. No production weapon
response changed, no functionality video is claimed, and the accepted pointer
in CURRENT-STATE remains unchanged.

## Whole-weapon coverage foundation (not installed behavior)

`src/common/physical_contact_volume_logic.h` constructs an enclosing union of
at most 64 spheres from all authored convex children. Each child is divided
into slabs along its longest local axis; each sphere encloses a complete slab,
including the child's authored round padding. Therefore the construction covers
the hull's faces and interior, not just vertices or selected triangle rays.
The output remains in local coordinates. Callers must transform centers and
scale radii consistently with the actual weapon palette.

Centers and radii account for stored-float rounding. Invalid geometry, invalid
slab size, or exceeding the cumulative sphere budget returns no cover, never a
partial cover. The helper also bounds rotation arc/chord error using a stable
sine expression and rejects rotation subdivision beyond the caller's budget.
This is only a geometric bound: it does not itself implement rotation
interpolation, collision response, or a safe starting-pose policy.

Release core tests exercise 194,481 padded box points over all three long-axis
orientations, plus 98,820 points on the actual 244 sword blade triangles under
nine animated scale/rotation cases. All were enclosed. The largest blade-only
cover in these fixtures used 36 spheres. Tests also cover large-coordinate
center rounding, invalid input, cumulative capacity rejection, and rotation arc
coverage. An initial test incorrectly expected a two-meter half-turn to fit 16
steps at 5 mm tolerance; the helper correctly rejected it. The test now verifies
that rejection and checks admissible subdivision with an explicit larger budget.
Both CTest suites pass after that correction.

These are enclosing volumes, not an exact mesh decomposition. They can cause
early contact and can enclose empty space, including parts of the sword-prong
gap, despite retaining separate child identities. Tighter coverage and native
query cost need measurement before this can be a polished player-facing solver.
No runtime code includes this new helper yet; no candidate was installed for
this foundation, and the existing installed identity above remains unchanged.

## Verified first-contact output for a rigid weapon

Read-only matching found the feature query called by both movement solvers:
official `71BCC0`, retail `24B8B0`, with call shape
`bool(features*, start*, displacement*, collision_record*)`. The verifier now
checks a unique 52-byte retail entry, the official/retail argument flow, the
closest-fraction comparisons, and these output writes:

- `+0x10`: first contact fraction. Both functions select a smaller candidate
  fraction and write it here. The movement solvers subtract it from one before
  scaling remaining displacement.
- `+0x14`: three-float center position, calculated explicitly as start plus
  displacement times that fraction in both modules.
- `+0x20`: normal xyz and plane distance. Official `64F3DE` names the field
  `&collision->plane` in its plane-validation assertion; both modules copy the
  corresponding 16-byte value here.

Retail's no-hit branch writes fraction one and start plus full displacement;
its plane is unspecified on that branch and must not be read. The comparison
also rejects contacts whose normal does not face against movement. This remains
a first-contact query, not an initial-overlap or interior recovery method.

Verification output is
`out/research/20260906-native-volume/first-hit-verification.json`, against the
same pinned official and retail hashes. The earlier 34-byte entry prefix was
nonunique; verification rejected it. No runtime binding used that prefix. The
52-byte entry is unique in the pinned retail executable sections. The function
has not yet been directly called by the mod; the previous native movement probe
used it indirectly inside the engine solver.

This provides the first-contact information needed to constrain one rigid
weapon by the earliest of all its enclosing volumes. Applying each sphere's
independent final slide position would not define a single rigid weapon pose.
Integration must still maintain a clear start, cover the actual rotation path,
recast any changed motion, bound query work, and keep movable-object impulses
and melee as separate features. The native map geometry does not establish
collision for purely visual meshes that the engine never gave a collider.

## Rigid sweep kernel and opt-in retail integration

`physical_contact_volume_sweep.h` now consumes a bounded enclosing cover and a
first-contact callback. It extracts the relative world rotation, follows its
shortest rigid arc, and subdivides by the fixed skin's arc/chord error. All
spheres share one accepted transform. A blocked fractional rotation is recast
from the last proved pose before it can be accepted. Query failures and work
budget exhaustion never admit the untested remainder. Scale changes and skewed
bases require a new proof. The caller must separately prove the starting cover,
including the fixed skin, is clear in the current world.

Tests use analytic half-spaces and arbitrary independent axis-angle rotations.
They check whole-body stopping at the independently calculated first sphere
contact, tangential movement, immediate retreat, exact unobstructed endpoints,
budget exhaustion, query failure, and a 120-degree rotation whose endpoints are
both clear but whose middle passes through the wall. Every sampled point on the
accepted portion of that rotation stays clear. Release and both CTest suites
pass. This kernel currently stops at contact; an outer slide/hand-target policy
and normal palette ownership are not implemented by this change.

The opt-in `-ProbeVolume` path now binds the verified first-contact function and
calls this kernel with the current weapon's entire authored convex cover. It
attempts a conservative seed with point-outside plus empty gathered features
for every sphere, including the skin. Nonempty broadphase is unknown and skips
that observation. Native casts reject a saturated feature category, validate
canaries before the first-hit call, validate returned fraction/point/normal,
and check active-structure continuity. They remain on the simulation worker.

For each of the existing 32 observations, the whole-weapon fixture starts with
its nearest covered point 60 cm in front of the reference plane and requests
90 cm inward travel, 20 cm retreat, inward/tangential travel, or inward travel
with a 90-degree yaw. It uses 12 cm local-axis slabs converted through live
scale, 5 mm skin, and a maximum of 192 first-hit queries per observation. The
`H3 whole volume PROBE` record reports cover construction, seed admission,
validity, blocking, work exhaustion, sphere/query counts, rigid progress,
reference-plane clearance and total cost including seeding.

The harness now requires both a seeded inward block and a seeded full retreat,
in addition to the original 32 bounded native sphere observations. A seeded
query failure fails the run. These conditions do not prove universal scene
coverage or performance; rotation results, costs and skipped seeds still need
inspection. This remains an opt-in probe without production pose changes.
