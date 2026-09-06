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

### Retail whole-cover result

Source `068d808a886a6d0378a1f2a4e871704f7aabecec`, package
`out/candidates/068d808-h3-physical-contact-20260906-073340007Z`.
Installed DLL SHA-256 independently rechecked:
`4EB9241331E742A685E11F5C81CA81355008D6700C95B73986A9DB0F83D63064`.
Launcher SHA remains `D489C5763E21FC339999DC734CED09A2068AAC6C035EA8B7BF4339B6810FA450`.
Release, both CTest suites and packaging checks passed. E: Steam installed;
alternate Steam and Store roots absent. Existing config unchanged.

First attempt `out/debug-openxr/20260906-073357821Z-wall-result.json` failed
during MCC menu startup. Log SHA
`FAC039C3F8F4FE3C27A82DF023C14C9FB63F7FC21EF13C3621ABD50BC4623303`.
The captured dialog says Fatal error. The minidump has execute-access violation
at address zero with `chrome_elf.dll+247CB` at the fault thread's stack top;
`halo3.dll` is absent from its loaded modules. No volume binding or probe ran.
The dump is preserved under
`out/research/20260906-native-volume/startup-chrome-elf.dmp`. This identifies the
observed null-call site, not the root cause of that MCC startup failure. The
prior preserved deployment log reached its native probe; this attempt did not.
The harness closed MCC and restored VR settings before a same-artifact retry.

Retry `out/debug-openxr/20260906-073809913Z-wall-result.json` passed. Log SHA:
`13CA2EC6398902474D62F8BFB157CC58478E0A42B7E51FE978D8659EA2F58689`.
Steam / SteamVR OpenXR 2.17.8 null / Null Model Number / 90 Hz / Construct Forge.
The map was verified in lobby image
`out/debug-openxr/20260906-073911136Z-forge-menu-ocr/0000-073911325Z.jpg` before
requesting Start. Parsed observations are in
`out/research/20260906-native-volume/whole-runtime-observations.json`.

All 32 covers built with eight spheres; 31 seeds were proved clear, and one
object retreat seed was unknown and skipped. All 31 admitted sweeps were valid
and none exhausted the 192-query budget. The current weapon geometry is the
existing authored collision shape (`shapeSource=1`, 36 triangles in the contact
status), not a proof that every rendered weapon triangle is enclosed. No live
sword coverage is claimed by this run.

- Structure type 1: two inward, two inward/tangential and two yaw/inward cases
  blocked. Minimum reference-plane clearance was 5 mm, matching the skin.
  Both retreat controls completed their full motion.
- Object type 4: six inward, six inward/tangential and six yaw/inward cases
  blocked; five admitted retreats completed. Some final covers crossed the
  original reference plane by up to 5.14 cm. A plane extended beyond the hit
  object's finite face is not an object-overlap test. These cases need actual
  shape/feature checks; neither penetration nor correct nonpenetration is
  established by the reference-plane number alone.
- Maximum total observation cost, including cover construction and seed tests,
  was 354.2 us. Cast counts were 8 for retreat, 16 for translation contact and
  up to 48 for yaw/inward contact. This is a small local eight-sphere fixture,
  not a worst-case budget or evidence that normal headset lag is fixed.

The harness completed its cleanup. MCC and SteamVR are closed. The restored VR
settings SHA independently matches
`298D6E805F90CADD0BD2564459AD19DAC15DF0634A5D2431F65506A3898C4D44`:
null disabled, forcedDriver empty, requireHmd true. No normal weapon rendering
policy changed. Sliding toward an obstructed hand target, clear-pose lifetime,
recovery, dynamic impulses, latency and headset acceptance remain unfinished.
The accepted pointer is unchanged, and no new functionality video is claimed.

## Current-pose integration: verified math boundary and sliding

The normal worker-approved palette adds measured pose age even in free space
(see HALO3-CLEARANCE-QUERY-EVIDENCE). Moving the new volume solver into that
worker without changing consumption would preserve that delay. The intended
integration therefore gathers nearby world geometry on the simulation worker
and evaluates rigid motion against a bounded immutable snapshot for the current
rendered controller pose. This is an implementation direction, not installed
behavior or proof that snapshot lifetime/coverage is already correct.

`tools/verify-h3-feature-math.py` pins both existing module identities and the
matched first-contact ABI, then checks full reviewed retail unwind ranges and
hashes for `24B8B0`, its sphere/cylinder/prism helpers `24AFF0`, `24B1F8`,
`24B5B8`, and normalization helper `212AC`. All direct calls stay in that closure
except thunk `6F5612`, verified as the `sqrtf` import through
`api-ms-win-crt-math-l1-1-0.dll`. All RIP-relative data references are read-only
`.rdata`; the projection helper's static axis table is at `768EF0`. There are
no indirect branches inside the five reviewed functions, no engine TLS accesses,
no world/object lookup calls, and no allocation or logging calls. The reviewed
argument flow reads the supplied feature records and point/vector inputs, uses
local scratch, and writes the supplied result. Output is recorded in
`out/research/20260906-native-volume/verified-feature-math.json`.

That permits testing a copied-feature query independently of the gather's
engine context. It is not a render-thread runtime result. Complete snapshots,
internal feature-count/index validation, exact query radii, swept-region
coverage, scene/weapon identity and clear starting poses remain required.
The gather and point-interior functions must stay on the simulation worker.

`PhysicalContactSlideVolume` now proposes rigid destinations along up to four
encountered contact planes, then recasts each changed movement before accepting
it. Planes live only for that solve. Tests independently check one-wall sliding,
two-wall corner sliding, immediate retreat, and rejection of a correction beyond
the caller's hand-distance limit. It returns the last swept pose plus a recovery
flag for an unreachable target; it never teleports to the inside-wall hand pose.
An outer rendering policy still has to implement that recovery. Both CTest
suites pass; this helper has not yet been used by the installed runtime.

### Snapshot machinery prepared for integration

`Halo3CastVolumeFeatures` now separates the audited first-contact math from
`Halo3CastNativeVolume`, which retains the worker-only gather and active-mask
checks. The new helper takes caller-owned validated feature memory. It has no
world gather, active-mask read or object lookup. It is still only reached from
the opt-in worker probe; it is not called by rendering yet.

`Halo3VolumeFeaturesValid` bounds the three outer categories and the nested
prism accesses used by the reviewed primitive math: projection word at `+28`,
orientation byte at `+2A`, polygon count at `+2C`, and embedded 2D points from
`+30` in each 112-byte record. It admits projection 0..2, orientation 0..1 and
0..8 points, so neither the read-only axis table nor embedded polygon can be
indexed out of the reviewed range. All referenced geometry floats must be
finite and bounded; sphere/cylinder radii must be nonnegative. Saturated outer
categories remain unknown. These are conservative admission checks for copied
snapshots; the new nested checks still need a retail probe result.

`PhysicalContactSnapshot<T>` supplies three fixed slots with pinned immutable
reads, bounded writer reservations, and monotonic publication versions. A slot
is published before its writer reservation is released, closing the window in
which a second producer could overwrite unpublished data. A reader pins a slot
and rechecks its publication descriptor before accessing its payload. Slot
exhaustion or contention returns no snapshot instead of blocking or overwriting
a reader. RAII handles release reservations on early returns; no geometry is
copied on the reader path and no allocation is performed by the exchange.

Tests cover empty/truncated/saturated/malformed feature storage, nested polygon
and projection bounds, readers held across successive publications, exhausted
slots, late old versions, and two concurrent readers with two concurrent
producers. Release compilation and both CTest suites pass. The core suite also
passed 20 repeated runs to vary the new concurrency test's scheduling. This is
implementation and stress evidence, not a proof of every possible schedule or
an in-game rendering result. No package was installed for this preparation;
installed source remains `068d808...` and normal headset settings are unchanged.

Remaining integration obligations, not verified findings:

- Gather complete world/instance features on the simulation worker, grouped by
  exact expanded sphere radius, within a region containing every accepted sweep.
  The point-inside/empty-feature seed stays on that worker too.
- Tie each cache to scene generation/active structure, object-update epoch,
  weapon identity, scale and bounded local node deformation. A copied feature
  set alone gives neither lifetime nor whole-weapon coverage permission.
- Run the first-contact math on pinned immutable features for the current
  tracked pose. Keep a last clear pose and never accept an untested fractional
  rotation, expired region or inside-wall hand reset.
- Constrain the proposal before the worker's movable-object contact test, then
  constrain the final palette again after body-follow mutations. Otherwise a
  stopped visible gun could still apply forces to a target behind the wall, or
  a later body-follow correction could move it through world geometry.
- Preserve nudging/melee as separate object transactions. Current fresh-region
  admission includes world geometry; changing it to object-only clearance is
  valid only while a working world-volume guard owns the final palette. Existing
  worker-approved free-space palettes must not continue adding the measured age.
- Resolve unreachable hand targets without leaving a gun far away or resetting
  it inside a solid. A hand more than the permitted distance inside the wall
  cannot have both a visible solid weapon outside it and an unrestricted reset
  to the hand; that fallback still needs implementation and headset evaluation.

## Current-pose world-volume candidate (opt-in, September 6)

`HALOMCCVR_H3_CONTACT_WORLD_VOLUME=1` now connects the previously probed
native first-contact math to normal controller rendering. The validation
harness exposes this as `-WorldVolume` for `controller-contact` only. Normal
headset launches leave it off until this integration has runtime evidence.

The simulation worker builds an enclosing cover from the existing authored
collision compound, groups spheres by child radius, and gathers structure and
instance features with flags 9. Each immutable cache includes exact radii,
active structure, scene/reset/weapon identity, reference nodes and object epoch.
Rendering pins that cache and sweeps/slides the current rigid weapon pose, with
a maximum of 192 casts per solve. It never invokes native world gathering or
point queries from the palette hook. Cache admission requires the current
object epoch and at most 20 ms age; a cast must fit entirely inside the region.

A 10 mm geometry reserve encloses small numerical/animation differences. All
nodes must pass the existing strict local-pose check plus an explicit lever
and scaled-basis deformation bound using at most half that reserve. Substantial
animation changes rebuild the cover and require a new clear seed. This is an
initial conservative policy that may reject more poses than necessary.

The raw controller palette is published separately. World correction precedes
worker proposals (preserving only the paired legacy body/object translation),
and a second sweep follows the final body-follow mutations. While a seeded,
current world cache owns this weapon, legacy native structure hits are ignored;
fixed objects and decorator handling remain. Object-only empty-region admission
is permitted only alongside the world guard, to avoid retaining the measured
worker approval delay in otherwise clear space.

The last swept root is retained for the same world/shape identity. An expired
cache holds it briefly; an unproved changed shape, a cache older than 100 ms, or
a hand farther than 30 cm hides the draw. Physical history remains separate
from the collapsed draw palette. A far-away hand can reset only after worker
point/empty-feature checks independently establish its entire cover as clear.
This clear-only recovery is a teleport, not a claim of a swept path through the
intervening wall. The existing raw inside-wall leash reset is bypassed while
the new guard owns the proposal. Map/weapon/contact resets invalidate ownership.

Cold status counters report admitted sweeps, blocking, holding, hiding, shape
rejections, unknown queries and native faults. QPC mean/max solve cost measures
the math path separately. The harness requires actual blocked current-pose
sweeps before passing; it still does not establish visible nonpenetration,
contact feel, prop impulses, all maps, sword animation, or headset acceptance.
Those are runtime tests, not conclusions from compilation.

### First current-pose retail result

Candidate source `52a813f67d10abbca901d107eb93fa706486861a`; package
`out/candidates/52a813f-h3-physical-contact-20260906-082800602Z`.
Installed DLL was independently checked as
`6B246D06DA13FAF32D99D23A041DD3AF4CA9B9B5ACC6E688A3BD96982ABDB2ED`.
Launcher SHA remains `D489C5763E21FC339999DC734CED09A2068AAC6C035EA8B7BF4339B6810FA450`.
Existing configuration remains `C570089F47A17AE8645310C02688CA1454E1A02C9239BC24C5CC316E4DA94946`.
Release, both CTest suites and packaging checks passed; Steam E: installed,
alternate Steam and Store roots checked absent. Prior deployment is preserved
under `out/deploy-backups/4eb9241-steam-before-52a813f-20260906-082801716Z`.

Run `out/debug-openxr/20260906-082818973Z-controller-contact-result.json`
passed its bounded current-pose blocking condition. Log SHA:
`E6B154214324A49F3FFAC4D80CC31E1DD0E284A4CABB9A46AED343AB2F64FE46`.
Steam / SteamVR OpenXR 2.17.8 null / Null Model Number / 90 Hz panel, 60 fps
reported gameplay / Guardian Forge. Guardian was verified before Start in
`20260906-082951834Z-world-volume-launch/0000-082952034Z.jpg` (see actual
capture directory for timestamped image); gameplay was the enclosed blue room
with the ceiling lift. The prior 068d808 preserved log reached its worker-only
probe; this run binds and exercises current-pose rendering instead.

Last cold counters: 14,439 geometry caches; three clear seeds; 83,080 admitted
solves; 4,526 blocked solves; 3,525 held submissions; 8,365 hidden submissions;
one shape rejection; zero unknown casts, exhausted budgets or native faults.
Solves are pre/post palette calls, not unique displayed frames or separate
contacts. The mean math-query cost was 3.4 microseconds, maximum 264.7 us;
this excludes worker gathering and the rest of the legacy contact pipeline.
The runtime nested-feature validation admitted the gathered geometry with no
reported query failure in this room.

A lateral controller sweep to X=1.3 m reached the right wall and exceeded the
30 cm leash. The weapon draw was hidden while held there. Retreat to X=.18 m
restored the visible gun: compare `20260906-083119925Z-world-volume-contact2`
with `20260906-083143647Z-world-volume-retreat`. A subsequent -65 degree yaw,
lateral/down/forward sequence increased blocked solves from 4,082 to 4,526
without increasing hidden submissions; retreat again restored the forward
pose. These are observed constraint/recovery transactions. The mirror often
cropped the contact point, so visible nonpenetration and sliding accuracy are
not established by the screenshots. No prop was contacted: impulses and
melees were zero, so this run says nothing about nudging or enemy regression.

Timing analysis is saved in
`out/research/20260906-native-volume/current-pose-timing.json`. Of 12,726 sampled
uncorrected palettes, 12,358 (97.1%) used fresh-region admission with zero
proposal age and zero recorded root gap. The residual 363 worker-approved
uncorrected samples had window p50 15-32 ms and maximum age 47 ms. This confirms
that the new path removes the worker approval age for most admitted samples in
this room, but it is neither an exact same-motion A/B nor sensor-to-photon
measurement. Deliberately constrained/hidden physical roots are recorded
separately as corrected; their gap is not free-space tracking error. Overall
60 fps behavior remains unresolved; recording also changes frame cost.

Three local MP4s under `out/demos/20260906-083048-guardian-world-volume-wall-test`,
`20260906-083253-guardian-volume-turn-contact-retreat` and
`20260906-083406-guardian-volume-wall-slide-view` are explicitly marked
DIAGNOSTIC-NOT-ACCEPTANCE. They are not finished functionality demonstrations.
The final clip crosses harness shutdown after about 14 seconds; its tail is
not gameplay and must not be presented as a demo.

Harness cleanup completed. MCC and SteamVR are closed; the restored settings
SHA independently matches `298D6E805F90CADD0BD2564459AD19DAC15DF0634A5D2431F65506A3898C4D44`.
Null disabled, forced driver empty, real HMD required. Normal launches still
leave WorldVolume off. The accepted pointer is unchanged. Clearer visible
contact tests, Campaign/instance coverage, sword animation, props/NPCs and
headset acceptance remain open; this is a successful initial integration
probe, not a ready-for-headset or completed-goal claim.

## Paired submitted-mesh audit (September 6)

The world-volume experiment now records up to 32 independent worker audits of
exact paired tracked/submitted palettes, captured immediately before the draw
is hidden for recovery. At most four unconstrained and four hidden samples
consume the bounded array; visible corrected poses retain the remaining slots.
The worker reconstructs the authored collision triangle mesh for each palette,
tests every vertex and triangle centroid with the native point-inside query,
and tests every triangle edge in both directions with the native line query.
Flags 9 select structure and instances. Active structure/reset identity is
checked around the query sequence. No result changes a rendering permission.

The validator now requires two visible (not hidden recovery) paired observations
where the requested mesh has points inside native solids and the submitted mesh
has neither sampled point interiors nor edge crossings. A native fault or a
valid visible submitted-mesh intersection fails the run. The checks are not a
complete triangle-vs-world overlap algorithm; unsampled face interiors and the
difference between authored collision and fully rendered/skinned geometry
remain limitations. They are independent evidence beyond the solver's own
blocked counter, with exact sampled counts and query cost logged.

Inspection also found existing opt-in turn controls: `send-mcc-keys.ps1 -Keys
TurnRight` / `TurnLeft` enqueue 15-degree steps consumed by ordinary VR turning.
The null controller pose continues through the same yaw reference. Mouse look
is not needed to frame the test and no new camera override has been added.

### Paired audit runtime result and wall recording

Source `143f3a3bba9eea6b39435131c0443ea48ab493c9`, package
`out/candidates/143f3a3-h3-physical-contact-20260906-084548468Z`.
Installed DLL SHA independently verified:
`53873266C69C3C2B555F42057AC9942908115E960E91BF53DC90A046B0716713`.
Launcher/config unchanged from 52a813f. E: Steam installed; alternate Steam and
Store roots absent. Backup:
`out/deploy-backups/6b246d0-steam-before-143f3a3-20260906-084549542Z`.

Run `out/debug-openxr/20260906-084605323Z-controller-contact-result.json`
passed the new paired-mesh criterion. Log SHA:
`F80A1BBBEF47B0B945814E2D4133E2A5569DCBB27D56B6D65038B5662266DF68`.
Steam / SteamVR OpenXR 2.17.8 null / Null Model Number / 90 Hz panel / Guardian
Forge. Lobby verification is under `20260906-084807344Z-world-audit-launch`.
This spawn was the covered outer ramp, not the previous blue-room spawn.
The installed predecessor's preserved log had only blocked counters; the new
log adds independent point and vector checks on submitted geometry.

All 32 audits were valid and fault-free, each with 36 authored collision
triangles for both palettes. Of 28 visible samples, 22 had requested-mesh
points inside native world solids; all 28 submitted meshes had zero sampled
point interiors and zero edge crossings. Four hidden samples also retained a
clear physical mesh but are excluded from visible success. The stable visible
contact held approximately 15.2 cm root correction: 14 requested samples were
inside the wall and 40 requested directed edges crossed it, versus zero of
both for the submitted mesh. These counts include repeated triangle vertices
and both edge directions, not unique vertices or distinct contacts. Maximum
full paired audit cost was 2,672.2 microseconds on the worker. Parsed records:
`out/research/20260906-native-volume/paired-mesh-audit.json`.

The obstacle was the structural support beside the covered ramp. Programmatic
turning correctly rotated the view. Native keyboard W/A/S/D movement did not
follow the diagnostic view yaw in this run: it retained its original game
heading, so stepping toward the turned view required matching the original
axes. After returning along S and approaching on D, a far-forward hand was
hidden beyond 30 cm; retracting Z from -.65 to -.48 m produced the visible
15.2 cm constraint and the positive paired audit. This control-space detail
must be accounted for in later scripts rather than navigating blindly.

A second oblique sequence at the same obstacle extends the controller, moves
sideways/vertically, then retracts. Local 15-second captioned recording:
`out/demos/20260906-085322-guardian-wall-oblique-controller-sweep/guardian-wall-contact-demo.mp4`.
SHA: `D7E551A9DF157B85B2CCDABBC9D7B155AD73AEC78ACC69191040E51E03440B7B`.
The source MP4 is preserved alongside it. The edit trims seconds 9-24, lifts
visibility with gamma 1.45 / contrast 1.08 / brightness +.01, and captions the
command phases. Geometry, motion and timing are not generated or replaced.
Representative contact/release frames and the caption render were inspected.
The scene remains dark; this is a limited experimental wall demonstration.
`demo-evidence.json` identifies all edits and limitations. The positive paired
audits precede this clip at the same obstacle; they are not frame-by-frame
verification of every later movement. The earlier frontal 40-second recording
is preserved under `20260906-085138-guardian-native-wall-contact-slide-release`.

After final retreat the blocked counter remained at 12,597 while subsequent
uncorrected fresh poses had zero recorded root gap, supporting release rather
than a persistent displaced gun in that final state. Last totals were 130,311
admitted pre/post solves, 5,779 held submissions, 6,500 hidden submissions,
zero unknown casts/shape rejects/budget exhaustion/native faults. No object
contact was attempted, so this run does not establish props or NPC regression.

Harness cleanup completed and MCC/SteamVR are closed. Restored settings hash
independently matches `298D6E805F90CADD0BD2564459AD19DAC15DF0634A5D2431F65506A3898C4D44`;
null disabled, forced driver empty, real headset required. The accepted pointer
has not advanced and WorldVolume remains opt-in. Campaign, instance/rock
coverage, sword animation, physical items, NPC shove/melee and headset feel
remain necessary before enabling this as the normal interaction path.

### Floodgate setup attempt: no contact positive control

Run `out/debug-openxr/20260906-090054092Z-controller-contact-result.json`
used the same installed 143f3a3 DLL, Steam / SteamVR null / Null Model Number,
with WorldVolume enabled and BladeGeometry disabled. Log SHA:
`9B7150270FB39B2DAE25A9E76505C9B6879813CA8443A9A11C924208BCB8BCA0`.
The existing Resume automation loaded the Floodgate outdoor rock area holding
the sword. A 350 ms diagnostic Y pulse did not visibly switch weapons. The
screenshot after movement still showed the sword, so a rifle test must not be
inferred from the delivered input. The audited mesh had 12 triangles: this run
cannot establish blade coverage.

The harness timed out without its required two visible positive paired audits.
Last preserved counters: 84,980 admitted solves, zero blocks, 3,758 holds,
341 hidden submissions, 745 shape rejects, 96 seeds, zero unknown queries,
budget exhaustion or native faults. This is an inconclusive setup attempt,
not a passed Campaign collision test or evidence of rock penetration. Shape
rejections during this run merit a separate animation/recovery investigation;
the counts alone do not establish their cause or headset visibility.

Before launch, the current profile and three paired Campaign save files were
copied and hash verified under
`out/test-runs/20260906-090053-before-world-volume-campaign/manifest.json`.
After harness cleanup closed MCC, post-test files were preserved there under
`post-test/`, then all four pre-test files were restored and independently
hash verified. `post-test/restoration.json` records both versions. SteamVR
settings independently matched the previous real-headset hash, with null off,
forced driver empty and requireHmd true. No code behavior or accepted pointer
changed for this attempt.

The immediate retry with BladeGeometry enabled did not reach gameplay:
`out/debug-openxr/20260906-090915485Z-controller-contact-result.json`, log SHA
`74234DF8C2C91221A8DF9F524DB9C1715FFF1D4B9EAF216AD38E57143881EEF3`.
The Halo 3 Campaign screen exposed Quickstart/Missions/Playlists but no Resume
(`20260906-091105136Z-menu-ocr/0000-091105286Z.jpg`). The observer closed MCC
through CloseMainWindow without choosing a mission; the harness exited through
its cleanup path. This is a menu/setup failure, not a blade runtime result.

All three checkpoint files still matched the pre-test snapshots. The profile
binary had changed, but read-only zlib decoding at byte 44 and recursive JSON
comparison found no semantic differences from the pre-test profile. Therefore
neither successful byte restoration nor the historical difficulty explanation
proves Resume availability in this attempt. No older profile was substituted
and no profile was reconstructed. The cause remains unresolved.

The retry's files and observed Steam remote-cache metadata were preserved under
`out/test-runs/20260906-090053-before-world-volume-campaign/post-menu-retry/`.
All four current pre-test originals were then restored and hash verified again;
the restoration report is in that directory. MCC/SteamVR are closed. Normal VR
settings again independently match
`298D6E805F90CADD0BD2564459AD19DAC15DF0634A5D2431F65506A3898C4D44`, null disabled,
forced driver empty, requireHmd true. Before further Campaign tests, resolve
Resume availability and expand the save-isolation procedure if another file or
cached state is shown to participate. Do not claim the restored menu works.

### Steam cache recovery and full sword/Floodgate failure

Steam's cloud log showed cached checkpoint SHA mismatches after restoration,
then refreshed/uploaded the restored files on the next MCC shutdown. Before
the following launch, cached checkpoint sizes/SHA-1 matched the files. On that
launch Resume returned (`20260906-091533141Z-menu-ocr/0000-091533286Z.jpg`) and
loaded Floodgate (`20260906-091547309Z-floodgate-full-blade-start`). No profile
contents or remote-cache metadata were edited. This supports stale Steam
metadata as the missing-Resume cause; a causal API trace was not performed.
Byte restoration must be followed by metadata refresh and menu verification.

The full-blade run used the same 143f3a3 artifact, Steam / SteamVR null /
Null Model Number. Result:
`out/debug-openxr/20260906-091431848Z-controller-contact-result.json`.
Log SHA: `3582A9DCB7AB865ADFCE625D7DEB2FF90E85F27DC0AD077DF8F1A67F74326A7B`.
Blade appends were active and audited meshes had 256 triangles. D movement
toward the nearby rock produced blocks, then recurring unknown queries and
hidden submissions. At one interval caches froze at 1,048 while unknowns grew;
in another, caches grew while admitted solves froze and unknowns grew. These
distinguish failed gathers from failed render casts in the code, but do not
identify each rejection reason. Retreat/reposition and respawn restored
updates. Enemy combat and death were present, so this is not an isolated
kinematic test. Sampled audit costs reached about 19 ms on the worker; that
probe is itself material to timing and must not be treated as cost-free.

Last totals: 6,000 caches, 18 seeds, 30,714 admitted solves, 93 blocks,
13,089 holds, 12,462 hidden submissions, 5,230 unknowns, two shape rejects,
804,388 queries, zero budget exhaustion/native faults. Mean sweep cost was
24.4 us, maximum 417.6 us. The bounded audit did not establish the required
two visible positive counterfactuals. The observer closed MCC through its
window; the harness recorded failure and restored real-headset VR settings.
This is a failed interaction candidate, not Campaign acceptance.

Diagnostic recording (not a successful demo):
`out/demos/20260906-091716-floodgate-sword-gather-failure-diagnostic/raw.mp4`,
SHA `589D631F98878B70E1CF6A982F6A69B00BE77C893272908A2090EC45F3C243C2`.
The snapshots show the sword's effects at the rock during recovery. They do
not establish that hiding the physical draw hides every sword effect.
All four current pre-test save/profile files were restored and hash verified,
with intervening files preserved in `post-full-blade-failure` under the current
Campaign snapshot. Steam metadata/menu verification is still required after
this latest restoration. The world-volume experiment is disabled as a separate
behavioral revert; its code is retained for an observation-only diagnosis.

### Observation-only world gather diagnosis

After behavioral revert 9792aab, `HALOMCCVR_H3_CONTACT_DEBUG_WORLD_GATHER=1`
(`-WorldGather` in the harness) observes the same raw-pose, full-cover world
gather without enabling the disabled world-volume behavior. Rendering only
publishes the raw request and returns before reading a world pose. The worker
returns after gathering, before seed tests or cache publication. World pose
ownership and the paired mesh audit are explicitly bypassed. FreshRegion stays
off. No current-pose world correction or hiding is performed by this probe.

Four success controls and sixteen rejection records are reserved independently.
Cold records identify reason (1 active mask, 2 feature validation, 3 false
return with nonempty features, 4 canary, 5 changed active mask, 6 exception),
native return, feature counts, group, query center/radius/expansion and shape
bound. The optional validator diagnostic encodes category in the high word
(1 storage, 2 capacity, 3 sphere, 4 cylinder, 5 prism) and offending index in
the low word. Existing validation rules remain unchanged. This probe diagnoses
the gather side only; it does not yet classify the separate render-cast
rejections. Its pass condition requires successful and rejected observations,
zero pose/seed/hide counters, no native faults and no mesh audit. Passing means
the failure was observed with that isolation, not that collision was fixed.

#### Capacity rejection reproduced without world pose ownership

Source `0b4eebf120429aece12daaf81af8c36df516fb1d`; package
`out/candidates/0b4eebf-h3-physical-contact-20260906-092523568Z`.
Installed DLL independently checked:
`EAA624C0A1CBBDA24E91A80207AEAC852BF874557E247B3E06B8AEED1F27C0BD`.
Launcher/config unchanged. Both CTests passed. E: Steam was installed; alternate
Steam and Store roots were absent. Prior artifact/log preserved in
`out/deploy-backups/5387326-steam-before-0b4eebf-20260906-092524624Z`.

Run `out/debug-openxr/20260906-092644623Z-controller-contact-result.json`
passed the observation-only criterion, Steam / SteamVR null / Null Model Number,
Floodgate, sword blade enabled. Log SHA:
`350C767A91598FEAA9C0539D33C8F86888041F2D3B6B8381F25654D29F2105EC`.
The scene screenshot is
`20260906-092847351Z-floodgate-gather-only-rock/0000-092847618Z.jpg`.
The same 1,200 ms D approach reached the nearby rock. All sixteen recorded
rejections were reason 2, validation `0x00020000`: feature-category capacity.
Native return was true, group zero, active structure `0x18`. Counts were
128-143 spheres, 144-153 cylinders and exactly 256 prisms. Search radius was
0.581567-0.581570 world units, expansion 0.035001, bound approximately 0.381919.
This proves the capacity guard rejected a full prism array, not that a specific
surface beyond that array was omitted. Never accept a full/truncated array as
complete coverage merely to clear this guard.

The four success controls occurred during initial handle-only preparation and
had empty arrays; they do not establish successful full-blade gathering at the
rock. Subsequent full-blade appends and 256-triangle status were recorded. Final
totals: 3,512 successful gathers, 759 rejected gathers, zero seeds, solver
frames, blocks, holds, hidden submissions, cast queries, shape rejects, budget
exhaustion and native faults. No paired mesh audit ran. Retreat/respawn restored
successful gathering without further rejected counts in the final interval.
Parsed records: `out/research/20260906-native-volume/gather-capacity-audit.json`.
The next design must bound spatial query size while retaining coverage for
every admitted swept sphere; smaller queries are a direction, not yet a fix.

For save restoration, a start-screen-only launch followed by ordinary window
closure refreshed all four Steam metadata entries to the original file sizes
and hashes. The subsequent menu showed Resume and loaded the original
checkpoint. `out/test-runs/20260906-resume-cache-check/after-menu-refresh.json`
records the match. That refresh run intentionally stopped before title entry:
`20260906-092542315Z-controller-contact-result.json` reports menu-control
failure and is not an interaction test. The resume helper now supports
`-MenuOnly` to verify Resume without entering Campaign; a recognized menu
without Resume reports an immediate failure without choosing Quickstart.

After the observation run, the current four originals were restored again
(`post-gather-observation/restoration.json`). A start-screen-only refresh run
(`20260906-093054800Z`) synchronized Steam's cached metadata. The final
`-MenuOnly` check exited successfully with Resume visible at
`20260906-093351192Z-menu-ocr/0000-093351341Z.jpg`; that actual frame was also
inspected. No mission was entered. The surrounding harness
(`20260906-093237472Z`) intentionally reports menu-control failure because the
observer then closed MCC; it is not an interaction-validation result.
Final `out/test-runs/20260906-resume-cache-check/final-menu-cache.json` proves
all four live files still match the original SHA-256 values and Steam's cached
sizes/SHA-1 values after closure. Normal VR settings independently match
`298D6E805F90CADD0BD2564459AD19DAC15DF0634A5D2431F65506A3898C4D44`.
MCC/SteamVR are closed, null is disabled, forced driver empty and requireHmd
true. Resume restoration is now verified. WorldVolume remains disabled;
normal launches do not enable WorldGather or BladeGeometry. The accepted
pointer has not advanced.

### Partitioned swept-sphere candidate

The 0b4eebf probe established that the large full-weapon query reaches the
native 256-prism capacity near the Floodgate rock. The replacement experimental
backend is enabled separately by `HALOMCCVR_H3_CONTACT_WORLD_PARTITIONS=1`
(`-WorldPartitions` in the harness). The old whole-weapon backend remains
disabled; normal launches still opt into neither path.

`physical_contact_volume_regions.h` builds bounded balls around each cover
sphere's short motion segments, using the existing shortest rigid arc. The
segment bound includes its chord, analytic arc error, exact expanded sphere
radius and a 40 cm motion reserve. Center travel steps are at most 20 cm;
merging is limited to an additional 8 cm over the per-radius bound. Up to 96
regions may be emitted. A plan that exceeds its bounds returns no partial
coverage. Each region is gathered independently using the same verified
flags-9 native query and existing capacity/nested-field/canary guards. A
capacity failure never becomes a clear result. The previous global gather and
observation-only probe remain available as dormant/reference code.

All regions must complete in the same active structure before the immutable
cache can publish. The renderer selects a region only when its expansion
exactly matches the cast radius and both endpoint spheres are wholly inside
the region. Convexity of the query ball then covers the entire swept capsule.
It queries only cached native math. No feature copy, gather or allocation was
added to the render hook. Missing containment and invalid native math have
separate counters, as do region-plan failures and capacity rejections. Worker
planning/gather time is measured independently of render sweep time.

The previous scene/shape/epoch/age guards, sweep/slide algorithm and clear-only
30 cm recovery are retained. Beyond that leash, the worker plans at the raw
pose proposed for recovery, but the existing independent clearance tests must
still establish the seed. An uncleared raw pose is not a teleport permission.
The ordinary world-gather observation mode still cannot publish seeds or take
pose ownership.

Offline coverage tests sample complete reserve balls along coupled translation
and rotations up to a half-turn, including two-prong cover geometry. They also
check bounded merging, region-budget failure, changed scale, mismatched query
radii, nonfinite bounds and capsules escaping a region. Both existing CTests
passed before packaging. Runtime capacity, timing, visible collision and
recovery acceptance remain unproven for this candidate.

#### 4500848 Floodgate partition test: contact evidence, overall failure

Source `4500848e99e8d4b9449826798ebd749f01f047c2`; package
`out/candidates/4500848-h3-physical-contact-20260906-094857046Z`.
Installed DLL independently verified as
`05D62920B16436075613F3B5BE3061D93B6926B1EE7F96F98CDE012EB1EC4393`.
Both CTests passed. E: Steam was installed; the alternate Steam and Store
roots were absent. Launcher and configuration remained unchanged.

Run `out/debug-openxr/20260906-095009422Z-controller-contact-result.json`
used Campaign / Floodgate, Steam / SteamVR null / Null Model Number, with
WorldPartitions, BladeGeometry and KeyboardGamepad. Preserved log SHA-256:
`3686A80D28D769501146C7162CD1E7AF77C9F183EC4188E9B247713CF0356877`.
A 500 ms D approach reached the adjacent rock. Of 32 visible paired mesh
audits, 20 found requested geometry inside the world and submitted geometry
with zero tested interior points and zero tested edge crossings. Full sword
samples contained 256 triangles in each palette. Stable contact samples had
136-139 requested interior points and 84 directed edge crossings, versus zero
for both submitted counts. Maximum positive correction was 0.25718 m. All
audits were valid and fault-free. Parsed records are in
`out/research/20260906-native-volume/partitioned-sword-audit.json`.
This is sampled geometry evidence, not exhaustive surface or headset acceptance.

The completed partition run recorded 10,072 gathers across 90,341 regions,
zero capacity rejections, invalid gathers, plan failures or invalid native
casts. Peak native counts were 15 spheres / 34 cylinders / 36 prisms, below
the previous whole-query 256-prism failure. Mean gather time was 271.5 us,
maximum 2,755.5 us. Final solver totals were 55,593 admitted solves, 4,961
blocks, 4,803 holds, 318 hidden submissions, 452 unknown results, 1,261 shape
rejects and zero native faults or exhausted query budgets. There were 452
region misses. Those later recovery/shape failures remain unexplained; the
successful capacity result does not establish reliable tracking.

The run **failed overall**: the final post-pass hold lost its pass condition.
The last contact status was `base-gate`, with no native samples or weapon
triangles; the failure screenshot shows the world without first-person weapon
or HUD after combat. That is insufficient to establish the exact cause. Do
not relabel this a passing run or relax the final check. A 500 ms A retreat
preceded that failure, but combat confounded the visual release observation.

The actual blade visibly stayed displaced from the rock while its electric
effects remained near the original hand pose. This is an unfinished visual
defect. The paired mesh audit also cost up to 25,485.2 us on the worker; it
must become an explicit diagnostic before production enablement. These data
do not establish a headset latency improvement. Props, NPC shoves, melee and
Guardian regression were not tested with this artifact.

Actual footage is preserved in
`out/demos/20260906-095156-floodgate-partitioned-sword-rock-test/raw.mp4`
(SHA `D96D4380E5AAE5B17FE559EF62CD3B2DDA1C89CDF6D7CD7F2ABD2516A7E1CBF0`).
`blade-contact-partial-demo.mp4` in that directory trims seconds 6-24 and
adds captions explicitly identifying the unfinished effects. Its SHA is
`DF8435525A40E7835FD3C221305F2D7DF1576EF2FDB984647E3B413B612AD0FB`.
It shows approach/contact, not retreat, nudging or a fully working system.
An encoded contact frame was inspected. No synthetic geometry, motion or
timing was added. Details and limitations are in `demo-evidence.json`.

Original Campaign files were restored from the current pre-test snapshot,
recorded in `post-partitioned-sword-test/restoration.json`. A start-screen
launch/ordinary close refreshed Steam metadata. The final MenuOnly check
showed Resume at `20260906-100433027Z-menu-ocr/0000-100433184Z.jpg` and entered
no mission; that screenshot was inspected. Its surrounding harness
`20260906-100005420Z` intentionally failed menu control when MCC was closed,
and is not an interaction test. All four original file hashes and Steam
metadata match in
`out/test-runs/20260906-resume-cache-check/final-partition-menu-cache.json`.
MCC and SteamVR are closed. Normal VR settings independently match
`298D6E805F90CADD0BD2564459AD19DAC15DF0634A5D2431F65506A3898C4D44`:
null disabled, forced driver empty, requireHmd true. The partition and blade
experiments remain off in ordinary launches. The accepted pointer is unchanged.

### Separate the measured mesh audit from ordinary collision work

The 4500848 contact run measured the paired sword audit at up to 25,485.2 us;
the actual current-pose solve recorded a mean of 18.0 us and maximum 580.0 us
over 56,045 attempts. These are different tasks. No causal attribution of all
headset lag or of later shape/region rejects follows from those timings.

`HALOMCCVR_H3_CONTACT_DEBUG_WORLD_MESH=1` now separately opts into the existing
paired draw audit. It defaults off and only arms with successful world bindings,
world pose ownership requested, and observation-only WorldGather disabled.
Both the render-side diagnostic palette publication and worker-side native
mesh queries return before touching their snapshots when this flag is off.
The observation implementation is retained. No solver, seed, cache coverage,
age, shape, recovery, or visibility permission consumes the audit flag.
This is a diagnostic scheduling change, not a new collision solver candidate
stacked over the incomplete Floodgate run.

The validation harness explicitly sets/restores this flag for WorldVolume and
WorldPartitions and requires the enabled marker as well as its existing paired
mesh criterion. It records the audit request in the result JSON. Qualification
is not weakened to pass without geometric evidence. Audit-enabled run timings
must not be presented as ordinary tracking performance. A live audit-off
comparison and headset latency acceptance remain outstanding; the experimental
world and blade paths still default off.

The diagnostic scheduling change is source
`f9ddf32aef7032630c734fd341922747a582fd8d`, packaged as
`out/candidates/f9ddf32-h3-physical-contact-20260906-100951472Z`.
Release build and both CTests passed; PowerShell parsed the updated harness
without errors. Installed DLL independently verified:
`43CC1F19215538F13D0579C95FDE382A4558E0D2E14F9CC4B71E588FC4D1FAE2`.
E: Steam installed; alternate Steam and Store roots absent. Prior install is
preserved in `out/deploy-backups/05d6292-steam-before-f9ddf32-20260906-100952384Z`.
Launcher/config hashes are unchanged. No MCC or headset session has run on
this source yet. MCC/SteamVR remain closed and normal VR settings match the
previous restoration hash. The accepted pointer has not advanced.

#### Next diagnostic lead: legacy cached-pose handoff

Re-reading the preserved 4500848 log changes the next investigation. At
`02:52:38.446`, proof-2 cached poses report origin age up to 1,031 ms and a
peak root gap of 4.1201 m. In the same reporting window the blade append is
inactive, weapon geometry drops from 256 to 12 triangles, shape rejects rise
from 2 to 162, and body render separation failures reach 165. These are
co-occurring observations, not an established causal chain or per-frame proof.
The pose timing record precedes the optional hidden-draw scale, so it alone
does not prove that every distant physical pose was visibly drawn.

The code contains two separate world constraint calls around the legacy
approval/body-follow path. The legacy final leash is skipped when the first
call returns nonzero. The second call independently reads the cache and may
return zero before applying any transform if the cache has no seed or matching
safe pose. Hidden drawing then depends only on that second return being 2.
A between-call ownership/shape transition is therefore a specific candidate
handoff gap to instrument, not a verified explanation of the logged frame.
The next observation must pair both return values, final gap, hide disposition
and cache identity on the same frame before selecting a behavioral fix. Do
not treat audit opt-out as resolving this older cached-pose problem.
