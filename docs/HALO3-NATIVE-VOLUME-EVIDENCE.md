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
