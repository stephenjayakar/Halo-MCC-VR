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

### Same-frame world handoff observation

Both constraint calls now return a diagnostic record alongside their unchanged
integer result. The record captures the actual pinned cache's timestamp,
epoch, shape, reset, active structure and node count, plus seed/safe-pose and
match/freshness state. Exit reasons distinguish inactive, gather-only, missing
cache, identity mismatch, missing seed/safe pose, reset change, invalid
transform, visible and hidden results. No extra cache read reconstructs these
facts after the call.

Before publication and hidden draw scaling, a bounded observer pairs both
records with the frame serial, raw/final roots, origin age, old proof and
actual world decisions. Exclusive categories prioritize lost early ownership,
then visible over-leash without ownership loss, then hidden, then controls.
Each late-failure category reserves sixteen samples independently; hidden and
control categories reserve four each. Atomic publication and a 100 ms
per-category cadence keep formatting, I/O, allocation and native queries out
of this render observation. Cold logging consumes each record once. All
existing collision, pose mutation, recovery and draw decisions are unchanged.
This diagnostic must establish whether the proposed handoff gap actually
occurs before a repair is selected.

#### eec4c0b Floodgate handoff run

Source `eec4c0bfdeaebc34f5d4f9e4f459a842147cddba`; package
`out/candidates/eec4c0b-h3-physical-contact-20260906-101431091Z`.
Release build and both CTests passed. E: Steam DLL independently verified as
`BA6C22D8FD0C735065700B37A6702E2E4D9C129ABFF1613DB7AD261FFE00BBC5`;
launcher/config unchanged. Alternate Steam and Store installations were absent.
Prior artifact is in
`out/deploy-backups/43cc1f1-steam-before-eec4c0b-20260906-101431914Z`.

Run `out/debug-openxr/20260906-101446436Z-controller-contact-result.json`
used Steam / SteamVR null / Null Model Number, Campaign, WorldPartitions,
BladeGeometry and explicit mesh audit. It **failed to reach its pass condition**
in 180 seconds; it never entered the requested 90-second post-pass hold.
Log SHA: `BE69AF97B2933BA5595D5C37BA176CF04BCA7B488A8B37D7A043DB0DD28C0ED1`.
The original checkpoint was loaded. The initial 500 ms D approach was
interrupted by combat; its screenshot had no first-person weapon/HUD. Later
A 500 ms, D 500 ms, D 1,000 ms and A 1,000 ms pulses sampled approach/retreat
but did not reproduce the prior controlled rock contact. The 32 mesh records
contained zero positive visible counterfactuals. Do not claim another collision
acceptance result or demo. Audit maximum was 23,289.6 us.

Final observations: 8,264 complete caches / 62,608 regions, capacity/invalid/
plan failures/cast-invalid all zero, peak counts 61/116/124, mean gather
264.3 us, maximum 1,557.7 us. Solver totals: 39,047 admitted frames, 985 blocks,
10,423 holds, 7,417 hidden decisions across the two calls, 5,410 unknown results
(all region misses), 846 shape rejects, no exhausted budget and no native fault.
There were 44,457 attempts, mean 7.2 us, maximum 197.6 us. Final-draw handoff
categories counted one lost early ownership, zero visible-over-leash without
loss, 4,411 hidden draws and 20,397 controls. These are different denominators.
Parsed records: `out/research/20260906-native-volume/handoff-observation-audit.json`.

The one lost-ownership frame (`serial=4097`) was decisions 1/0, reason 8/4
(final cache identity rejection), proof 1, origin age 16 ms, gap 0.00009 m.
Thus a handoff exists, but this run does **not** establish it as the cause of
the earlier distant cached draw. No visible over-30-cm final pose was observed
by this diagnostic. The record captures cache reset, not the call's requested
reset/generation/tag, so do not name the precise identity mismatch component.

A separate directly recorded timing case is `serial=5786`: decisions 1/2,
matching 1/1, fresh 1/0, gap effectively zero, both seeded and safe, same
shape/reset/active state. The frame timestamp was `96505421`, while the final
cache timestamp was `96505437`. The renderer takes `nowMs` before the two
constraint calls; the worker can publish between them. The final hide predicate
explicitly includes `nowMs < cache.ms`, so this newer-cache/older-frame-time
combination hides even a matching near-hand pose. Re-evaluating freshness after
pinning the cache is a concrete next repair to validate, retaining the age and
epoch guards. This does not explain all region misses or detached sword effects.

Source review also finds that cover reuse currently checks node transforms and
identity but does not compare the incoming compound child geometry. A selection
change with unchanged node transforms is therefore a separate coverage question;
the current observations do not establish that transition's live cache contents.

After the run, all four original files were restored and verified
(`post-handoff-observation/restoration.json`). Start-screen refresh harness
`20260906-101946954Z` synchronized their Steam metadata. Final MenuOnly check
showed Resume at `20260906-102259439Z-menu-ocr/0000-102259591Z.jpg` (inspected),
without entering Campaign. Both surrounding menu-only harnesses intentionally
report closure during external control, not an interaction failure or pass.
After final harness `20260906-102116008Z` closed, all three checkpoint files
still matched the original bytes. AceSettings changed its encoded bytes during
the ordinary menu session, but decoding both zlib JSON payloads proves exact
semantic equality with no setting differences. Its current SHA-256 is
`FFC8C7E64319CC140C50656A8C5E1F259BFCD5DF1DF6A4626F0BFA8973B9C2BE`.
All four Steam metadata entries match their current files. Evidence is in
`out/test-runs/20260906-resume-cache-check/final-handoff-menu-cache.json` and
`handoff-profile-comparison.json`; do not claim all four remain byte-identical.
Normal VR settings independently match
`298D6E805F90CADD0BD2564459AD19DAC15DF0634A5D2431F65506A3898C4D44`.
MCC/SteamVR are closed, null disabled, forced driver empty, requireHmd true.
World partitions and blade geometry remain opt-in, and the accepted pointer
has not advanced.

The partition experiment was subsequently disabled in its own source commit
after the repeated failed Floodgate runs and directly observed false-future
cache rejection. Its implementation and diagnostics remain intact. This
disable precedes the next behavioral candidate; it does not advance acceptance.

### Cache-pinned freshness clock candidate

After disable commit `2718151`, the next opt-in partition candidate samples
`GetTickCount64` after acquiring its immutable cache. Its 20 ms solve freshness
and 100 ms retained-pose age checks use this evaluation clock. This ordering
prevents a concurrently published cache from appearing to be in the future
merely because the caller retained a timestamp from earlier palette work.
The raw request and pose provenance timestamps remain unchanged. Epoch,
identity, shape, seed clearance, query containment and leash guards are retained.
This changes one behavior: the clock used to evaluate the pinned cache's age.

Each paired observation now records both evaluation clocks. An independent
sixteen-record category reserves final-cache-newer-than-frame cases; loss and
visible-over-leash categories retain priority. Cold logging reports the total
number of such clock advances across both calls. This makes it possible to
verify visible corrected cases rather than infer success from build results.
The old whole-query backend remains disabled and normal launches still opt
into neither world partitions nor sword blades. Other open collision/shape,
sword effects, shove and headset latency requirements remain outstanding.

#### 28d2f20 short collision regression

Source `28d2f20ef409be869026cf4725e41c812eb3e822`; package
`out/candidates/28d2f20-h3-physical-contact-20260906-102849861Z`.
Release build and both CTests passed. Installed Steam DLL independently
verified: `0EA65BD4F00D69DEA0590CB7E0CAE490907BA88AADE692FC344EB1EF3592D8FC`.
Launcher/config unchanged; alternate Steam and Store roots absent. Prior
artifact preserved in
`out/deploy-backups/ba6c22d-steam-before-28d2f20-20260906-102850779Z`.

Run `out/debug-openxr/20260906-102903117Z-controller-contact-result.json`
passed its bounded controller-contact/paired-mesh criteria and 30-second
post-pass hold. Steam / SteamVR null / Null Model Number, Floodgate resumed
checkpoint, WorldPartitions / BladeGeometry / mesh audit. Log SHA:
`E3EC97A79D1722A7642D5E4E5F65CD0454A559C3E05FB6862BCA57B7D7CF868B`.
This shorter hold is not equivalent to the earlier 180-second failed run.
The 500 ms D approach produced 20 visible positive counterfactuals among 32
mesh samples. Stable requested geometry had 109 interior points and 80 directed
edge crossings; submitted geometry had zero of both. Maximum positive root
correction was 0.21974 m. All samples were valid and fault-free. Audit maximum
was 23,425.6 us. Parsed records:
`out/research/20260906-native-volume/cache-clock-audit.json`.

The contact screenshot `20260906-103040977Z-cache-clock-rock/0000-103041222Z.jpg`
was inspected: the actual blade is displaced clear while electrical effects
remain near the original pose. The 500 ms A retreat screenshot
`20260906-103108483Z-cache-clock-retreat/0000-103108754Z.jpg` has no first-person
weapon/HUD after combat and does not prove clean release. No new polished demo
or headset acceptance is claimed.

Final preserved-log totals: 2,538 complete caches / 24,186 regions, capacity/invalid/plan/
region-miss/cast-invalid counters all zero, peak native counts 64/119/128,
mean gather 532.6 us, maximum 2,114.7 us. There were 10,357 admitted solves,
2,028 blocks, 851 holds, 63 counted age/shape/leash hide decisions, 322 shape rejects, zero
unknown results, native faults or exhausted budgets. Mean solve 19.1 us,
maximum 377.9 us. Final handoff categories: no ownership losses or visible
over-leash frames, 329 hidden draws and 7,338 controls. The latter includes
early return-2 dispositions not counted by the solver's final hide counter.
These data still
contradict a claim of consistently visible, polished interaction.

Both newer-than-frame counters were zero: this run did **not** exercise the
repaired interleaving. Its collision regression passed, but live proof of the
timestamp repair remains outstanding. The change's ordering follows the
recorded failing case; do not attribute the reduced holds or zero region misses
to it from this shorter, different combat sequence.

Campaign files were restored and verified under
`post-cache-clock-test/restoration.json`. Start-screen refresh harness
`20260906-103152139Z` synchronized Steam metadata. Final MenuOnly check
`20260906-103409961Z` showed Resume at
`20260906-103536559Z-menu-ocr/0000-103536707Z.jpg` (inspected), entered no
mission, then closed normally. These menu-only harnesses intentionally report
closure during external control. All three checkpoint files retain their
original bytes and all four Steam metadata entries match current files in
`out/test-runs/20260906-resume-cache-check/final-cache-clock-menu-cache.json`.
AceSettings matches the previously decoded, semantically identical
`FFC8C7E64319CC140C50656A8C5E1F259BFCD5DF1DF6A4626F0BFA8973B9C2BE`
file, independently hash-checked in `cache-clock-profile-comparison.json`.
MCC/SteamVR are closed. Normal settings hash remains
`298D6E805F90CADD0BD2564459AD19DAC15DF0634A5D2431F65506A3898C4D44`;
null is disabled. Accepted pointer unchanged.

### Incoming geometry proof before cover reuse

The previous reuse predicate checked scene/weapon identity, scale and node
transforms, but did not inspect the supplied compound geometry. A synthetic
handle-to-two-prong transition with unchanged nodes demonstrates why those
conditions alone are insufficient: the handle cover cannot enclose the blade.
This is a verified missing condition in the code, not a claim that the exact
transition was captured in the earlier live cache records.

The next candidate builds the incoming complete slab-sphere cover on the
worker before considering reuse. Reuse additionally requires unchanged child
count and full containment of every incoming sphere, plus 5 mm of deformation
reserve, inside a cached sphere of the same child. The containment arithmetic
uses double intermediates and outward rounding. Centre/vertex samples alone
cannot grant this proof. If the condition fails, the existing rebuild/new-shape
and independent seed paths run; no old safe pose transfers to a different cover.

On successful reuse, the cached cover/radii/shape identity stay fixed, while
the node reference updates to the current geometry's nodes. This matters:
the remaining 5 mm allowance is for deformation from those current nodes.
Retaining a node reference on the opposite side of an earlier allowance would
double-count the margin. Every subsequent worker sample must fit the same
fixed outer cover again, so small changes cannot accumulate unbounded drift.
Changing child count rebuilds even when a smaller shape could fit an older
larger cover, so an inactive blade does not indefinitely retain its old extent.

Regression tests cover handle-to-full-blade and same-child growth, increased
authored padding, small numerical movement, accumulated drift, child-group
mismatch, nonfinite/empty inputs and a larger sphere sharing an accepted centre.
Counters distinguish reuse from compatible-node geometry rejection. Worker
timing measures incoming cover construction and compatibility/containment
checks separately from native gathering. Render hooks gain no geometry copies,
native queries or new loops. World and blade experiments remain opt-in.

#### 83cac52 Guardian contact and repeat-approach failure

Source `83cac52e5309ab59b87c7aa14c8e1691ee80bb4c`, package
`out/candidates/83cac52-h3-physical-contact-20260906-104427104Z`, installed DLL
`A803C95C616CBB2C0944C83FCD0402C98948B5CF285DE9A3F58667B2A941E857`.
Release and both CTests passed. All three runs below used Steam / SteamVR null /
Null Model Number, Guardian Forge requested, WorldPartitions and mesh audit,
without BladeGeometry. These are not headset acceptance.

- `20260906-104448687Z-controller-contact-result.json`: timed out without
  establishing positive contact. Navigation stopped short of the intended wall.
  Log SHA `71C5C158BE36984BB218A9EAD0E009F5BDB58403F48ACCE183E8CE11C0ADC8BF`.
  Cover checks averaged 5.1 us, maximum 58.6 us; no geometry-change rejection.
- `20260906-105152810Z-controller-contact-result.json`: external menu timeout,
  no Halo 3 gameplay. Log SHA
  `AF4C592315DD77A3B9E6D14A95E74FC72EE7EB088C785D0C9023C4B53A375238`.
- `20260906-105809015Z-controller-contact-result.json`: passed the harness's
  cumulative paired-mesh criterion and 60-second hold, but **failed the actual
  repeat-approach visibility check**. Log SHA
  `72BE97A32C8078B617310CC85C2536EC35D48756FEA1037360B1EC4B16E80344`.
  D 600 ms then W 1200 ms approached the support between the overshield room's
  doorways. Visible AR samples had 21 requested interior points / 52 directed
  edge crossings, versus zero of each after correction, at about 18.5 cm gap.
  Controller Z -0.48 and S 500 ms visibly released the gun. W 500 ms then Z
  -0.65 made it disappear. The inspected recontact screenshot is
  `20260906-110134154Z-guardian-cover-retry-recontact/0000-110134379Z.jpg`.
  By the last preserved counters (04:01:40), region misses rose from 2 to 5,744,
  admitted solves stopped at 28,865, and hide decisions reached 5,953. Capacity,
  invalid regions, plan failures, invalid casts, native faults and query-budget
  exhaustion stayed zero. This contradicts an overall pass despite the harness
  returning true: its positive mesh observations are retained cumulatively.

Actual 35-second recording:
`out/demos/20260906-110052-guardian-native-cover-contact-release/raw.mp4`, SHA
`00BFF0F247E8C90883A301914EF32903B7B0814633DB2E3A5708F12331F227BE`.
It records the first contact, pull-away and failed recontact, not a polished
demo. It proves no prop/NPC interaction, sword behavior or headset latency.

Normal SteamVR settings restored and independently verified at SHA
`298D6E805F90CADD0BD2564459AD19DAC15DF0634A5D2431F65506A3898C4D44`;
MCC and SteamVR closed. Campaign checkpoint bytes remained original in the
read-only check before the third Forge run. Accepted pointer unchanged.
The failed partition behavior is disabled in its own commit before repair;
the incoming-geometry containment code is retained.

### Retained-safe-path recovery candidate

The worker previously chose a raw-to-raw region plan once the retained safe
root was more than 30 cm from raw. However, the same function carried that old
safe seed unless independent native clearance established that raw was clear.
The renderer could therefore start from a position absent from its own cache.
This is a verified contradictory path in the code. The Guardian recontact
misses are consistent with it, but no individual late miss recorded its full
query endpoints, so that exact causal attribution remains unproven.

The new candidate always plans from the matching retained safe pose when one
exists. It keeps independent raw-seed clearance, the visual leash, all native
capacity and validation guards, the 96-region/64-step planning caps, and the
192-query render budget. Excessive travel can still exceed those budgets; this
does not grant unchecked relocation or claim arbitrary teleport recovery.

The analytic regression places raw 60 cm from safe inside a wall. A raw-only
cache cannot authorize the first sweep; the retained-safe path reaches contact
within 30 cm of raw and releases immediately on retreat. This synthetic case
proves the coverage correction, not native performance or headset behavior.
The experiment remains opt-in pending a repeat-approach runtime result.

#### fcb4bc0 green-panel runtime check

Source `fcb4bc0e3ee9547af17df5163e4ec54eca8d4cde`, package
`out/candidates/fcb4bc0-h3-physical-contact-20260906-110719783Z`, installed Steam
DLL independently verified at
`317D07ED3E8A875E96E3986A9CB7D046805C31716B37E47484428D35DB9279DF`.
Release and both CTests passed; previous files are preserved in
`out/deploy-backups/a803c95-steam-before-fcb4bc0-20260906-110720791Z`.
Other Steam/Store roots were absent. Launcher and config hashes unchanged.

Run `20260906-110739828Z-controller-contact-result.json`: Steam / SteamVR null /
Null Model Number, Guardian Forge, WorldPartitions and mesh audit, no blade
geometry. Spawn was the blue lift room rather than the prior overshield room.
D 500 ms then W 4000 + 1000 ms approached the green panel. Controller Z -0.95
produced a conservative block before sampled requested mesh penetration;
Z -1.15 deepened contact. Retraction to Z -0.48 plus S 500 ms, then W 500 ms
plus Z -1.15, kept the gun visible in inspected captures. A second retraction
and S 500 ms released again. These are different geometry and controls from
the earlier failure, so this is not an exact regression reproduction.

The harness **timed out without its positive paired-mesh condition**; its
requested 90-second post-pass hold never began. All 32 detailed mesh samples
were used before the deeper contact, with zero positive counterfactuals.
Therefore it does not establish penetration-free submitted geometry during
recontact. Log SHA:
`77B685AA20697A2E214AE447EAA393C11372B8247748E39982773A45B8CFBA33`.
Parsed result: `out/research/20260906-native-volume/guardian-safe-path-audit.json`.

Last preserved counters: 10,806 caches / 26,937 regions; zero capacity errors,
invalid regions, planning failures, missing regions, invalid casts, native
faults, unknown solves or exhausted query budgets. 62,187 admitted solves,
15,480 blocks, 2,649 holds and six early hide decisions / shape rejects; final
handoff hidden counter zero. One ownership loss had a near-hand 0.00001 m gap,
not a distant visible weapon. Cover mean/max 5.2/99.2 us, native gather mean/max
143.7/764.0 us, solve mean/max 14.1/314.2 us, mesh audit max 3,437.3 us. These are
instrumented null-driver measurements, not headset latency or FPS acceptance.
No clock-race or changing-geometry reuse case was captured.

Thirty-second captioned actual-gameplay clip (raw seconds 7-37):
`out/demos/20260906-111141-guardian-green-panel-recovery-check/recovery-check.mp4`,
SHA `710BAABBC11044E0EF0DBDEADDFB630DA5490AEF3A22CB4055E6497AF3756034`.
Raw 45-second source SHA:
`A7AEF6DF56A02FE1A59712367796FD4039D7176CF8A5C5FBE87E94AEB4EFB244`.
Raw frames at 16 and 33 seconds and encoded frame at 22 seconds were inspected.
This demonstrates the visible contact/release sequence only; no prop/NPC,
sword, exhaustive collision, headset tracking or overall completion claim.

MCC and SteamVR closed; independently verified normal settings SHA
`298D6E805F90CADD0BD2564459AD19DAC15DF0634A5D2431F65506A3898C4D44`, null false,
forced driver empty, requireHmd true. No persistent world-partition/audit
environment overrides in process, user or machine scope. The new solver stays
off in normal launches; the existing physical-contact config remains enabled.
Final read-only save check found all four files byte-identical to the original
Campaign snapshot and all Steam metadata matching. No restore was necessary.
Accepted pointer unchanged. Next evidence gaps are the same-doorway failure
sequence and event-spaced mesh samples that survive through recontact.

### Mesh-audit duration correction

The prior 32-record/250-ms sampler exhausted its nontrivial-gap observations
within seconds of first contact. Its four free controls were lifetime quotas,
so withdrawal could also go unobserved after the initial free-hand samples.
The two Guardian recordings demonstrate why the cumulative pass is insufficient.

The diagnostic-only candidate spaces mesh audits by 2.5 seconds and reserves
128 records, covering the configured 180-second validation plus 90-second
recovery hold. Free and hidden quotas apply consecutively: an audited contact
rearms free samples for subsequent withdrawal, and a visible sample rearms
hidden observations. The finite session budget still applies and the audit
remains opt-in; it is not an exhaustive per-frame geometry proof. No solver,
cache ownership, collision bound, hand tracking or native binding changes.

#### dd461a3 paired contact / withdrawal / recontact

Source `dd461a3890a9c41b66385e6d595712178f15f5bd`; Release and both CTests
passed. Package `out/candidates/dd461a3-h3-physical-contact-20260906-111941745Z`;
installed Steam DLL independently verified at
`282C8648B2541A996E43259A3B1E795D3E389D12A64872175EEB273A4ACAA0A4`.
Previous installation is preserved under
`out/deploy-backups/317d07e-steam-before-dd461a3-20260906-111942762Z`.
Other Steam/Store roots absent; launcher and config unchanged.

Run `20260906-112007680Z-controller-contact-result.json` passed the paired
mesh criteria and 90-second post-pass hold. Steam / SteamVR null / Null Model
Number, Guardian Forge, WorldPartitions + mesh audit, no BladeGeometry.
Log SHA `16F5C89E3483F4E2B7D836637182FF86D47A4236DC35CB71A43170A902A440E6`.
Spawn was a different room again, with two blue floor fixtures; W 1600 ms,
A 900 ms then W 1300 ms reached its flat paneled wall. Initial correction
about 0.41 m hid the gun because it exceeded the existing 30 cm limit. This
is an unresolved user-experience limitation, not a polished overall pass.
Controller retraction from Z -0.65 to -0.48 restored visible contact.

The longer sampler retained 43 observations rather than exhausting at 32.
There were 21 positive visible paired samples, maximum gap 0.29990 m. Stable
contact had 31 raw interior points and 44 directed edge crossings, with zero
of both in the submitted mesh. S 500 ms produced clear, near-hand samples
(indices 27-30); W 500 ms recontact produced positive samples again at indices
31-38, still with zero submitted interiors/crossings. A second S 500 ms
produced four more clear, near-hand samples (39-42). Thus both release and
recontact now have paired geometry observations. This remains a bounded mesh
sampling result, not a full triangle-interior/per-frame proof. All native mesh
audits were valid and fault-free; maximum audit cost was 878.3 us.

Last preserved counters: 8,992 caches / 24,273 regions, 53,004 admitted solves,
12,108 blocks, 847 holds, 6,294 hide decisions accumulated during the initial
over-limit approach. No unknown solves, missing regions, capacity errors,
invalid regions/casts, plan failures, shape rejects, exhausted budgets or native
faults. No paired ownership loss or visible-over-leash event. Cover mean/max
5.9/1,990.9 us, gather 41.8/1,767.9 us, solve 3.3/207.5 us. The late counters
did not continue accumulating hidden draws after retraction. These instrumented
null-driver timings do not prove headset responsiveness. Parsed summary:
`out/research/20260906-native-volume/guardian-long-audit-summary.json`.

Actual 45-second recording:
`out/demos/20260906-112335-guardian-wall-paired-recovery/raw.mp4`, SHA
`B2ABA45EC245ADE4A182F2902C7E83DBB694A3A16BCC6FA154148E29EB157CC8`.
Continuous captioned trim of seconds 7-42: `wall-recovery.mp4` in that directory,
SHA `097D3234C6320CA257E5C2FDD42F58C36161067213E6D1FE28FED8F9F9277149`.
The clip shows contact, first withdrawal and recontact; the second withdrawal
occurs after the raw recording and is established by the log. Encoded frame at
27 seconds and live contact/release/recontact screenshots were inspected.

Harness terminated successfully. MCC/SteamVR are closed. Normal settings
independently verified at
`298D6E805F90CADD0BD2564459AD19DAC15DF0634A5D2431F65506A3898C4D44`, null disabled,
forcedDriver empty, requireHmd true. Read-only Campaign check found all four
original files and matching Steam metadata. Accepted pointer unchanged;
ordinary launches still do not enable the world/blade experiments. The exact
overshield-doorway failure remains a separate regression target, as do full
sword shape/effects, moving bodies, NPC interaction and headset latency.

### dd461a3 full-sword ownership gap and diagnostic follow-up

Run `20260906-112946313Z-controller-contact-result.json` used Steam / SteamVR
null / Null Model Number, Guardian Forge, WorldPartitions + mesh audit +
BladeGeometry. Runtime source and DLL are the dd461a3 identity above; validator
source was b828cc7. Preserved log SHA:
`3CC0CD0C59EF981D48D41818F1E4149605C048B6A77C082E2C7EA8E87887464E`.
The harness timed out without a wall-contact pass; its 90-second hold never
started. The sword was spawned through the Forge palette and picked up with
normal player input. Full 256-triangle blade geometry was selected, with four
incoming-cover geometry changes observed and no sword geometry rejection.

During the late approach, admitted solves stopped at 44,891 while caches kept
publishing. Withdrawal restored progress to 45,281 and seeds increased from
14 to 15. Last counters: 10,394 caches, 46,326 regions, zero blocks, 1,229 holds,
17 hide decisions/shape rejects, and zero unknown solves, region misses,
capacity/validation/planning/cast failures, exhausted budgets or native faults.
Existing handoff controls do not identify this late return reason. Missing a
clear seed after a shape change is a hypothesis, not an established cause.
Only nine native mesh audits survived the consecutive-control quotas; none
proved contact. No new demonstration video was produced from this failed test.
Parsed evidence: `out/research/20260906-native-volume/guardian-sword-ownership-summary.json`.

Diagnostic follow-up removes free/hidden quotas from the opt-in mesh sampler:
a zero-correction raw/unowned pose must still receive native geometry checks.
The 2.5-second cadence remains, with a fixed 256-record session budget. A new
independent handoff category reserves sixteen records, at one-second spacing,
for final return reason 5 (neither a clear seed nor a matching safe pose).
Counters continue after records fill. This changes observations only; it does
not claim a sword fix or change solver ownership, collision binding, or normal
headset configuration. Native mesh audits remain worker-only and opt-in.

After the sword run, MCC/SteamVR were closed and normal SteamVR settings
restored to SHA `298D6E805F90CADD0BD2564459AD19DAC15DF0634A5D2431F65506A3898C4D44`.
The read-only Campaign check found all four original files and matching Steam
metadata. No save restoration was necessary. Accepted pointer unchanged.

### 22bf874 sword contact succeeded, reset recovery failed

Package `out/candidates/22bf874-h3-physical-contact-20260906-114644864Z`, source
`22bf874e5ba11b62c16024266e89f7c97dd1f000`. Release and both CTests passed.
Installed Steam DLL independently verified at
`AB43BB7489CEC4BF59CD27D3D10C948037C31D922867B5594B9E8CF415BE9E29`;
launcher/config unchanged, other Steam/Store roots absent. Prior installation:
`out/deploy-backups/282c864-steam-before-22bf874-20260906-114645807Z`.

Run `20260906-114654558Z-controller-contact-result.json`: Steam / SteamVR null /
Null Model Number, Guardian Forge, WorldPartitions + mesh audit + BladeGeometry.
Log SHA `069E1FCD7DC68ECADEEFA12A3D239B97E086C420CDC0D3AB6B30F2751A3A42DB`.
The cumulative harness reported a pass including its 90-second hold, but the
late reset failure below makes this an **overall failed recovery candidate**.
Parsed summary: `out/research/20260906-native-volume/guardian-sword-reset-summary.json`.

Spawn was the outdoor ramp with a blue floor fixture. The Forge palette spawned
an energy sword onto the ramp; W 1100 + 900 ms and ordinary pickup acquired it.
D 450 ms approached the right railing. Controller position (0.18,-0.65,-0.65),
pitch 75 degrees produced native paired contact: twelve positive visible-draw
samples, maximum correction 0.09492 m. Representative requested geometry had
four interior points and sixteen directed edge crossings; submitted geometry
had zero of both. The sword was below the inspected camera frame during this
contact, so a visible draw flag is not a visually useful demonstration video.
Position (0.10,-0.30,-0.75), pitch 35 degrees brought the sword into view and
returned to near-zero correction with clear sampled meshes. No recontact input
was completed before the post-pass timeout.

At log time 04:53:25, reset identity changed from 30 to 31. A paired 1/0 decision
first recorded reasons 8/4 (cache identity changed), then the reserved category
5 recorded sixteen 0/0 decisions with reasons 5/5, seeded=0/0 and safe=0/0.
The fresh cache shape changed from 3419 to 9078 across that reset; this is not
proof that incoming geometry deformation caused it. Missing-seed count reached
3,796 in the preserved log, while admitted solves stopped at 52,194 and caches
continued to 10,341. Thus lost seed ownership is now observed; the initiating
reset reason still needs capture. No native/query/capacity/plan/region errors.
Last preserved counts: 5,149 blocks, 2,128 holds, 22 hide decisions, 16 shape
rejects. Cover mean/max 6.9/301.8 us, gather 164.3/2,411.7 us, solve 16.3/406.2 us.
Maximum worker mesh audit 19,479.5 us; not headset latency acceptance.

The sampler retained 62 records, but a second diagnostic blind spot remains:
its caller publishes only when worldFinal is nonzero. The new unowned category
therefore has no corresponding mesh audits. Correct this call-site restriction
before interpreting absent unowned mesh samples as clear space. Also extend the
harness beyond cumulative successes so a sustained late ownership gap cannot
be reported as a successful recovery hold.

The partition experiment is disabled as its own failure-revert commit before
further changes. Dormant solver code is retained. The accepted pointer remains
unchanged. A late 35-second recording started only eight seconds before the
hold ended and covered shutdown instead of the requested interaction; it is
irrelevant. Removal was requested under the user's existing cleanup instruction,
but automatic approval review rejected it with "blocked by policy" and no more
specific reason. The recording remains on disk and is not a functionality demo.

### Unowned draw and late-stall validation correction

The draw audit publisher now admits guarded tracked submissions even when
worldFinal=0; the existing experiment/mesh-audit flags still gate all diagnostic
snapshots and worker native queries. The paired handoff captures the existing
contact reset reason at constraint evaluation time. Neither change modifies
solver ownership or grants permission to reuse a pre-reset pose.

The harness rejects sampled submitted-mesh penetration for visible draw=0 as
well as draw=1. Its final check also rejects three consecutive cold reports
with increasing cache and missing-seed counters but unchanged admitted solves.
This catches the observed late stall while allowing transient misses followed
by resumed solving. Offline validation extracted only these pure functions
from the parsed PowerShell AST (no game launch): the preserved 22bf874 run was
rejected, the dd461a3 wall run retained, a resumed-solve suffix retained, an
unowned penetrating mesh rejected, and an unowned clear mesh did not satisfy
the positive-contact requirement. Script parsing and git diff checks passed.
The partition behavior remains disabled; this is diagnostic preparation for
the next recovery experiment, not a recovery fix.

After the run, MCC/SteamVR were independently confirmed closed, normal settings
SHA `298D6E805F90CADD0BD2564459AD19DAC15DF0634A5D2431F65506A3898C4D44`, null=false,
forcedDriver empty and requireHmd=true. The three Campaign checkpoint files
remain byte-identical to the original snapshot; all four Steam metadata entries
match their files. AceSettings has changed and must not be called byte-identical.
Both wrappers independently decode as zlib JSON at offset 44. Only 22 highest
completed-difficulty fields in DifficultyTracking[57..68] differ, from 255 to
values 1, 2 or 3; other JSON values match. This Forge test completed no Campaign
missions. The origin of those updates is not established, so do not overwrite
potentially synchronized completion records merely to force the original hash.
Current profile SHA `A1F17DE454DFC8A9CFCBCFA9670BEB57455177D31F77FB354DDB48CC972ADF5B`;
comparison: `out/test-runs/20260906-resume-cache-check/sword-reset-profile-comparison.json`.
No save restoration was performed.

The disabled experiment plus diagnostic/validation preparation is packaged as
`out/candidates/1ee2468-h3-physical-contact-20260906-120038834Z`, source
`1ee2468495cca968e7713f071ec8faf946f3b3b7`. Release and both CTests passed.
Installed Steam DLL SHA
`A55163A073C07B4521C1569B80ACB96017FAAB3F06DC8FBAFF297AA3799A9C30`;
launcher/config unchanged. Backup:
`out/deploy-backups/ab43bb7-steam-before-1ee2468-20260906-120039794Z`.
Other Steam/Store roots remain absent. No game session has used this package;
the partition solver is disabled even when requested. Accepted pointer unchanged.

### Expanded-feature seed-clearance candidate

The reset-gap diagnosis exposed an overly broad seed rule in
Halo3NativeVolumeSeedClear: after an outside-solid point test it rejected every
nonempty gather with expansion zero. As the comment itself stated, nonempty
features mean nearby geometry, not overlap. That rule cannot re-establish a
seed near many walls even when the expanded weapon sphere is clear.

The candidate gathers with the actual sphere radius as the native expansion
argument, retains the independent native outside-solid point test, and checks
whether the sphere center is outside the complete expanded feature union.
It does not reuse a pre-reset approval, skip the reset, or use a directional
zero-motion cast as an overlap proof. Scene/weapon/reset/active-mask, capacity,
nested-count, finite-value and canary guards remain. Gathering and seed tests
remain worker-only. The existing opt-in partition flag is re-enabled for this
single seed-clearance behavior; ordinary launch configuration is unchanged.

Pure point-membership math follows the already reviewed, pinned native feature
helpers. Sphere fields +14/+20 are center/radius. Cylinder +14 is the start,
+20 the complete axis vector and +2C the radius: retail 24B43F..24B486 clips
its longitudinal coordinate to [0,axis length squared], while 24B4C2..24B4F8
removes the axis projection for the radial normal. Prism 24B5FB..24B710 clips
normal-dot-point minus +20 to [0,+24]; 24B712..24B78D projects onto the base
plane, and 24B7C7..24B863 clips against each directed 2D edge halfspace.
The official 71A4B0 and 71C450 helpers retain the corresponding field access
and clipping math. No new engine function binding is introduced.

The full six-row axis table is now independently checked in the official and
retail modules by tools/verify-h3-feature-math.py: official FEFC68 (addressed
at 71C6A7) and retail 768EF0 (24B749). Rows are (2,1,0), (1,2,0), (0,2,1),
(2,0,1), (1,0,2), (0,1,2). Both module identities, reviewed retail math closure
hashes and tables passed verification in
out/research/20260906-native-volume/verified-seed-feature-math.json.

The new helper uses double arithmetic and an outward tolerance of at least
1e-5 world units, scaled for float input magnitude. Tangency, degenerate
cylinders/edges, invalid normals/thickness/polygon counts and malformed storage
cannot grant clearance. Core tests cover sphere interior/tangency/separation,
finite cylinder sides/end disks and the union with vertex spheres, all six
prism projections with face/edge/corner cases, a tilted prism, malformed inputs
and saturated storage. Both CTests passed after recompiling the final fixtures.
Cold counters distinguish accepted nonempty feature sets from rejected ones.
This is source/math evidence only; the next runtime must demonstrate recovery
and retain paired mesh checks. Accepted pointer unchanged.

#### 08d6675 Guardian runtime: improved seed query, failed ownership recovery

Source `08d667597ca6b6aee4223a690db3161a990d3a49`, package
`out/candidates/08d6675-h3-physical-contact-20260906-121407911Z`.
Release and both CTests passed. Installed Steam DLL independently verified at
`2237019DB7205955683F57943E3F4F086F91CFE0DB0F8F423CF9734E53570CC4`.
Other Steam/Store roots absent; launcher and config unchanged.

First attempt `20260906-121416838Z-controller-contact-result.json` closed in
menu control before the world test started. Log SHA
`7CD8A0E1DF6901911B570256FFD83D49872BA5C782E586C000C2C9182A85DBA0`.
Crash dump MCC-Win64-Shipping.exe.7304.dmp reports execute access violation at
zero, with chrome_elf.dll+247CB as the first stack word. This is not a seed-query
runtime result. Read-only analysis: `out/research/20260906-native-volume/08d6675-menu-crash.txt`.
The harness and menu helper terminated before retry; normal VR settings restored.

Retry `20260906-121711978Z-controller-contact-result.json`: Steam / SteamVR null /
Null Model Number, Guardian Forge, WorldPartitions + BladeGeometry + mesh audit.
Log SHA `62FA0F243AC8900A149A124420A1B959811FE6621A14B40AB05E794A1B45D8C1`.
The new harness correctly failed on submitted visible mesh intersections after
completing the 180-second observation hold. Parsed summary:
`out/research/20260906-native-volume/guardian-expanded-seed-summary.json`.

Spawn was the blue lift room. The Forge palette spawned a sword onto the floor;
W 1400 + 450 ms and ordinary pickup acquired it; D 450 ms approached the wall
right of the forward doorway. Controller Z -0.85 produced constrained contact,
Z -1.00 deepened it, and Z -0.48 withdrew. Z -1.00 recontact later lost ownership;
Z -0.48 withdrew again. Final Z -0.60 and a Forge monitor/Spartan round-trip
left the sword visible and controlled near the wall. These are different
geometry and controls from the prior outdoor-railing run.

125 mesh audits, four positive constrained samples, maximum positive correction
0.27738 m. Fourteen submitted intersections were recorded, all draw=0 (world
solver unowned); none occurred in draw=1 or draw=2 samples. First bad sample 56
had requested inside/crossings 8/32, submitted 4/16, correction 0.09595 m.
Thus this is not a working collision candidate even though some contacts passed.
The previously hidden diagnostic gap now yields actual geometry evidence.

Reserved no-seed records show resetReasons=1 (motion gate). The first interval
used reset 33, shape 5957; a later no-seed record used shape 8122 with the same
reset 33. Both lifecycle reset and cover replacement therefore remain relevant.
Missing-seed count ended at 14,980; withdrawal resumed solving. End counters:
18,442 caches, 78,428 solves, 8,366 blocks, 2,175 holds, 32 hide decisions,
17 shape rejects, four geometry changes, no unknown/native faults, incomplete
regions, capacity/plan/cast failures or exhausted queries. Per-sphere seed checks
accepted 12,110 nonempty feature sets and rejected 4,983; these are not counts
of approved whole-weapon poses. Cover mean/max 8.1/2,103.5 us, gather
85.8/2,492.4 us, solve 6.2/1,999.8 us, audit max 5,701.4 us. Not headset timing.

Two actual 50-second recordings are retained as **failed recovery diagnostics**,
with demo-evidence.json sidecars explicitly marking overall_pass=false:
- `out/demos/20260906-122206-guardian-sword-doorway-seed-recovery/raw.mp4`, SHA
  `950096D5A50A9AA897F3D6138AFF4309157B1E4FDD309F4FCCDC2A5C42445A47`.
- `out/demos/20260906-122322-guardian-sword-contact-withdraw-recontact/raw.mp4`, SHA
  `A62247F17BE56FC59CF6ACEE5C4C0757EA3A663D0E485A3A27F5D5AFDDA6FD0A`.
Do not present these as working functionality demonstrations.

The partition behavior is disabled as its own failure-revert commit; dormant
feature math is retained. MCC/SteamVR independently confirmed closed; normal
settings SHA `298D6E805F90CADD0BD2564459AD19DAC15DF0634A5D2431F65506A3898C4D44`,
null=false, forcedDriver empty, requireHmd=true. The three Campaign checkpoint
files remain original; all four Steam metadata entries match. AceSettings still
has the separately preserved completion-record differences and was not restored.
Accepted pointer unchanged.

### Motion-clock underflow correction (world experiment remains disabled)

The observed reset reason 1 led to a source-confirmed arithmetic defect in
Halo3SamplePhysicalContact. Its outer motion gate correctly skips a sample
newer than the worker's nowMs. The nested reset decision then evaluates unsigned
nowMs-sampleMs without the same ordering guard, making a slightly newer sample
look enormously stale and clearing contact with reason 1.
VR_GetRightControllerMotion's null path calls GetTickCount64 itself after the
worker has sampled nowMs; the normal publication can also advance concurrently.
Thus the clock ordering is possible in the actual producers. The 08d6675 records
prove reset reason 1, but do not individually prove which subtype caused it.

The correction retains the outer sample rejection and the 100-ms age limit;
it requires nowMs>=sampleMs before the nested expiration subtraction. Future
samples skip one tick without invalidating contact. Invalid poses, disabled
base gates and truly expired samples still reset immediately. A bounded atomic
counter records future samples skipped, with cold world diagnostic output.
This does not fix cover-replacement seed loss or authorize stale geometry.
The failed partition experiment remains disabled for this separate candidate.

65bf5c5 build/install: source `65bf5c5bc6b657dab7e3c5246cf1ebecaf789fa6`,
package `out/candidates/65bf5c5-h3-physical-contact-20260906-123155495Z`.
Release and both CTests passed; installed Steam DLL independently verified at
`BA1C54BE56FFFE5D455CBECA4A4338F08033E254B237A8AB58A689041319B3DC`.
Launcher/config unchanged; other Steam/Store roots absent. Backup:
`out/deploy-backups/2237019-steam-before-65bf5c5-20260906-123156495Z`.
No game session has used this package yet. The partition experiment is disabled,
so this is not a world-collision headset-test candidate or accepted build.

Final read-only profile comparison independently decoded the current and
pre-run preserved AceSettings JSON: semantic values match exactly. Current
wrapper SHA `FA6FFC8D4AD3FD6395FCADEEFA96F8B75C4F8F3638FF3D10F26FE24174804B49`.
Evidence: `out/test-runs/20260906-resume-cache-check/expanded-seed-profile-comparison.json`.
The existing differences from the much earlier original completion-record
snapshot were preserved, not overwritten. No Campaign save restoration.

Next recovery candidate should independently test a same-weapon historical
safe transform against the incoming full cover/current native world before
using it as a new seed across shape/reset changes. Never inherit its old
approval unchecked. The existing raw-seed test, visual leash and complete
swept-region coverage must remain. This is a proposed next action, not implemented
behavior or evidence of a solved recovery path. Goal and accepted pointer remain
unchanged; Forge/Campaign, sword, props/NPCs and headset performance acceptance
are still required.

#### Historical full-cover seed revalidation candidate (2026-09-06)

The 08d6675 Guardian doorway run recorded new shape identity at unchanged reset,
then missing-seed fallback with submitted blade penetration. This candidate
re-tests a historical same-generation/weapon/tag rigid transform against every
sphere of the incoming cover and current native scene. Shape/reset approval is
not inherited. The candidate must have identical scale and be within 0.60 m of
the raw root (twice the existing visual leash); all complete candidate-to-raw
regions are gathered before a candidate can publish. Each native seed query
must match the gathered active structure, with the existing reset/epoch checks
still required at publication. A rejected candidate cannot seed the new shape.
Raw clearance recovery and the 0.30 m visual leash remain.

Revalidation uses the existing expanded-feature membership proof, on the
simulation worker only. Cold counters distinguish whole-cover tests, clears,
and rejections. Partitions are re-enabled only through the diagnostic opt-in;
normal launches retain the prior contact path. This also exercises the separate
65bf5c5 motion timestamp fix for the first time; no runtime result is yet claimed.

A two-prong analytic wall fixture checks raw blade penetration despite a clear
handle, full new-cover seed approval, rejection after further blade growth or
scene movement, bounded recovery sweep regions, and immediate withdrawal.
The initial Release build and both CTests passed; final packaging must rebuild
a subsequent invalid-radius guard and its negative control before runtime.
This is a recovery experiment, not headset acceptance or comprehensive surface
coverage proof. The accepted pointer is unchanged.

50681c9 runtime result: FAILED in Guardian Forge on Steam / SteamVR null /
Null Model Number. Source `50681c9c3a14b1ef6376ca2f276296a6714d99ed`, package
`out/candidates/50681c9-h3-physical-contact-20260906-124424230Z`, installed DLL
`AE68AE977F68CCEC4223AFB6CBE3D9B23E1DFF6F472AEA8D82489685940E63AF` independently
verified. Final Release build and both CTests passed, including the invalid
radius guard. Normal config and launcher hashes remained unchanged.
Backup: `out/deploy-backups/ba1c54b-steam-before-50681c9-20260906-124425088Z`.

Result: `out/debug-openxr/20260906-124439341Z-controller-contact-result.json`.
Preserved log SHA `395B0E7B9F6765149D9EF11808929F2EB7621AF59A85FA20F3A121BE37312C7D`.
Started 12:44:45Z, finished 12:51:30Z. Guardian's blue-floor-fixture room
spawned successfully. Sword pickup required moving nearer until the native
pickup prompt appeared, then E; earlier out-of-range E presses did not pick it
up. Positive controller pitch raised the rifle barrel, negative pitch lowered
it; do not treat a numeric pitch alone as proof of an on-camera sword pose.

At 12:50:24Z, D450 and controller Z=-1.0 left the blade clear, with zero raw
mesh intersections. At 12:51:08Z, W450 plus Z=-1.25 produced deep doorway
contact. The solver briefly blocked (19 frames), then recorded hidden gap
0.33934m at reset34/shape8287. A new cover at the SAME reset34/shape11904
lost its seed. Revalidation counters were tested16/clear11/rejected5; these
are complete-cover counts. The prior 10 clears preceded this contact, so they
are not evidence of recovery from a wall. Afterward legacy hand-recovery
requests caused resetReasons32, and no-seed fallback continued.

81 preserved mesh audits: zero positive paired contact samples; the last
sample (index80, draw0) contained raw/submitted inside755/755 and crossings
168/168. MissingSeed511; capacity/invalid/planFailures/regionMisses/castInvalid,
query exhaustion and native faults all zero. Cover mean7.0us/max2294.0;
gather mean82.6us/max2464.4; solve mean4.8us/max379.7; diagnostic audit
max11023.3us. These are null-run timings with audits enabled, not headset
tracking latency. futureSamplesSkipped remained0: the separate timestamp fix
ran without that branch being observed.

The harness correctly failed and closed MCC upon visible submitted penetration;
180-second post-pass hold was never reached. A subsequent withdrawal command
found MCC already closed, so this run does not establish withdrawal recovery.
The late capture helper ended at its own 45-second visible-window timeout.
All tool sessions terminal, no remaining MCC/SteamVR processes. SteamVR settings
independently match normal-headset SHA
`298D6E805F90CADD0BD2564459AD19DAC15DF0634A5D2431F65506A3898C4D44`.

Retained video: `out/demos/20260906-125026-guardian-historical-seed-doorway/raw.mp4`,
SHA `B928381AA39FAF5239084CE31D281BD85C7F8FB5E9C038FC1B9D1CAFD2672785`.
50 seconds of actual MCC, mostly clear approach with deep failed contact near
the end. Labelled failed diagnostic, not a working demo. No cleanup attempted.
The partition experiment is disabled again in its own revert commit before
any further behavioral candidate. Accepted pointer unchanged.

Next work must resolve the explicit no-seed fallback after ownership has been
established. Revalidation is necessary but insufficient when new geometry
actually overlaps at the historical pose. A bounded search for a newly clear
pose (using current full-cover clearance) is one possible next experiment;
never call a rejected seed clear or infer player-body clearance proves a long
weapon clear. Preserve complete swept regions and hand-distance limits. The
legacy fallback must not be mistaken for collision ownership. Full surface,
Forge/Campaign, props/NPC and headset latency acceptance remain outstanding.

Disabled-candidate install: source `0cdb9fab28f0819e018f62638b831c9925adbbb1`,
package `out/candidates/0cdb9fa-h3-physical-contact-20260906-125420099Z`.
Release build and both CTests passed. Installed Steam DLL SHA
`4DEE4DA8FE818336544DF888C8AFB93F886A72DD7808EAFB4F3595C86B0EBAFB`;
launcher unchanged, existing config preserved. Other Steam/Store roots absent.
Backup: `out/deploy-backups/ae68ae9-steam-before-0cdb9fa-20260906-125421125Z`.
No session has used this disabled package. Campaign checkpoint bytes still
match the original snapshot; all four Steam metadata entries match their live
files. AceSettings has the same SHA1 as before this run, including the previously
preserved completion data. No save or profile restoration was performed.

#### Bounded depenetration of changed-cover seeds (2026-09-06 candidate)

50681c9 proved that a historical transform is insufficient when incoming blade
geometry overlaps there. After the separate 0cdb9fa disable, this next candidate
searches at most 35 translated poses around a rejected historical seed (five
shells up to 0.10m, preferred direction away from raw followed by six world
axes). Directions only propose poses: every incoming cover sphere must fit a
complete region with EXACT matching expansion, be outside its expanded feature
union, and pass the pinned native solid-interior point query against the same
active structure. Reset and object epoch are still checked before publication.
No radius is reduced and no rejected pose is granted clearance. The existing
historical identity/0.60m admission bound and 0.30m visual leash remain.

The simulation worker reuses complete cached native features instead of doing
new native gathers at every search position. At most 512 sphere callbacks are
allowed; budget exhaustion or failed containment leaves the output unmodified.
The 0.40m region reserve covers a possible 0.10m seed displacement plus the
existing 0.30m solver correction; actual cast containment still enforces this
rather than assuming it. Cold counters record attempts, complete clear seeds,
sphere checks, mean/maximum duration and native query faults. The new native
point helper uses the existing pinned signature and SEH; a fault contributes
to the existing world-volume failure counter checked by the harness.

Unit fixture: both tips overlap two analytic corner faces at the old pose;
a diagonal candidate clears both and fits complete regions. Single-query
exhaustion cannot publish a partial proof, fully enclosed/unknown space rejects
all proposals, and a 0.33 world scale preserves the metric displacement bound.
Initial Release and both CTests passed; final packaging will compile the helper
relocation that makes native faults visible in the existing counter. This is
opt-in partition recovery only. No runtime or headset result is claimed yet;
no-seed fallback still needs verification if all bounded proposals fail.

6c40f90 Guardian runtime: FAILED. Source
`6c40f90c66651303e4293f0deee1ef53565f85e1`, package
`out/candidates/6c40f90-h3-physical-contact-20260906-130226818Z`, installed DLL
`F2DD1024F8A60C6294EDF83624701F6F85D8C0B2EDC8289D80016E8922E45A5A` independently
verified. Final Release and both CTests passed. Backup:
`out/deploy-backups/4dee4da-steam-before-6c40f90-20260906-130227726Z`.
Steam / SteamVR null / Null Model Number, started13:02:54Z, ended13:06:34Z.
Result `out/debug-openxr/20260906-130248977Z-controller-contact-result.json`;
log SHA `67540FF84FB16EF60D54833329E9BDF0B3E7F1AC1CEE383AEA6840803B69B582`.

This spawn was the lower blue-fixture room looking through an angled doorway,
not the preceding upper-room spawn. Up750/X450, weapon palette selection,
A450 twice/Up750 placed the sword. W2100 followed by E1500 picked it up;
screenshot `20260906-130601994Z-depenetration-pickup` confirms the held sword.
D250 plus controller Z=-0.85 at13:06:14.615Z approached the right angled wall.

The first recorded over-leash event had gap0.32767m at reset28/shape1454.
The next missing-seed record changed to shape2857 at the SAME reset28, with
native fields/epochs current. 112 whole-cover rejections ran depenetration:
0 recovered seeds, 47,920 sphere callbacks, mean574.9us/max878.9us. This proves
the search ran and was bounded; it does not explain which feature/region/native
interior test prevented clearance. Do not expand the distance or callback budget
without capturing and inspecting that actual failing geometry.

22 preserved mesh audits, zero positive paired contacts. Last audit(index21)
was draw0, raw/submitted inside2/2 and crossings12/12. Caches2968, seeds5,
frames16785, blocks8, holds207, hidden32, missingSeed335. Gather capacity,
invalid, planFailures, regionMisses, castInvalid, query exhaustion and faults
all0. Cover mean6.6us/max95.1; gather mean69.2us/max915.0; solve mean4.4us/
max93.1; diagnostic audit max3544.3us. These are not ordinary headset latency.
The harness stopped on penetration before any post-pass hold or withdrawal.
No video was started: the run never reached visible paired contact evidence.

The failed partition behavior is disabled in its own commit. MCC/SteamVR
processes are absent and normal SteamVR settings independently match SHA
`298D6E805F90CADD0BD2564459AD19DAC15DF0634A5D2431F65506A3898C4D44`.
The next useful step is a bounded capture of the first rejected historical seed,
full cover, raw and historical transforms, complete gathered native features,
and per-sphere rejection causes. Replay that exact geometry offline to establish
why clearance fails before choosing another recovery radius or direction set.
The renderer's no-seed fallback remains an explicit unresolved failure. No
accepted pointer advancement and no claim of Forge/Campaign/headset readiness.

Disabled 4db21a7 install before replay work: package
`out/candidates/4db21a7-h3-physical-contact-20260906-130952521Z`, source
`4db21a714fdbfe53e8f730aa6e76ca89cfc578d1`, installed DLL independently checked
`3AEDC61388560DC38067B21E1FC9922DB960EE08B4AF5CE09DA6388D9DE2093F`.
Release and both CTests passed. Backup
`out/deploy-backups/f2dd102-steam-before-4db21a7-20260906-130953692Z`.
No game session used this disabled package. Normal config/launcher unchanged;
other Steam/Store roots absent. Profile bytes/Steam metadata remain as before.

#### Exact recovery replay diagnostic (2026-09-06)

A new `-WorldReplay` switch requires `-WorldPartitions` and gives this one
launch a unique `out/debug-openxr/<stamp>-world-recovery.bin` path. Without
that path the failed partition experiment remains disabled. With the explicit
path, the candidate reproduces the failed behavior to capture its first
rejected historical seed; this is diagnostic reproduction, not a headset-test
candidate or a new claim that recovery works. The harness restores the new
environment variable with its other debug variables.

The bounded snapshot contains the complete incoming cover, historical/raw
transforms, all gathered regions and their copied native features, and the
outcome of each recovery sphere callback: missing region, expanded boundary
rejection, native solid-interior/scene rejection, or clear. Source commit,
shape/reset/epoch/active structure, fault counts, result and scene stability
are included. One simulation-worker copy publishes through an atomic state;
no allocation, environment access, logging or disk I/O is added there or in
rendering. The existing 50ms background polling/logger path (game.cpp callers
of LogHalo3PhysicalContactStatus) creates the new binary file exclusively and
writes it once. No overwrite of an existing file is permitted. Capture timing
can perturb this diagnostic run and is not a normal tracking-cost measurement.

`halo3_world_recovery_replay` independently reloads the bounded file, validates
its version/length/identities/features, reruns every coverage and expanded-feature
membership test, and compares its exact callback sequence and result with the
captured native outcomes. It does not manufacture native interior results at
unrecorded points. Invalid, truncated, unstable or faulted snapshots fail.
The standalone executable accepts the capture path as its sole argument;
its output distinguishes rejection categories and query-budget exhaustion.
Initial Release build, both existing CTests, standalone replay target build,
and PowerShell syntax parsing passed. An actual live capture/replay remains
required; no synthetic no-argument CTest is presented as runtime evidence.

fbf4d7f capture/replay result (2026-09-06): diagnostic SUCCEEDED; collision
behavior FAILED as expected. Source `fbf4d7f2b925194085f38ef7392154379c872789`,
package `out/candidates/fbf4d7f-h3-physical-contact-20260906-131718655Z`, installed
DLL independently checked
`3FCAC3DD033DC685CD001C93A98D7D6E6A4EF0BDE4997A7E19C7C56D2BE3B8CB`.
Backup `out/deploy-backups/3aedc61-steam-before-fbf4d7f-20260906-131719502Z`.
Final Release, both CTests and the replay executable build passed; passing a
runtime log instead of a binary correctly returns exit2 for invalid size.

Steam / SteamVR null / Null Model Number, Guardian Forge lift-room spawn,
13:17:46Z to13:23:31Z. Menu helper selected Guardian; three TurnRight commands
faced the nearby wall before sword placement. A450 twice/Up750, W1800/E1500
picked up the sword. Subsequent W400 and controller reach Z=-1.05, then
X=.35/Z=-1.55, remained clear in the sampled mesh. X=.85/Z=-1.55 at
13:22:53.585Z triggered the first rejected historical seed. The background
logger saved it at06:22:54.563 local, stable=1, faults unchanged.

Result `out/debug-openxr/20260906-131740397Z-controller-contact-result.json`,
preserved log SHA `F709EBFCF7155F986139B2282ACC2F9B5F5360C95872F0446EDC5619FF4E7999`.
Snapshot `out/debug-openxr/20260906-131740397Z-world-recovery.bin`, 4,834,240 bytes,
SHA `44276DE7AD7EE3B2755D421D529DB16B4F436203F09CFC4EC30CC420BEABDF9F`.
Running `out/build/release/Release/halo3_world_recovery_replay.exe` with that
snapshot produced an EXACT replay (exit0): 453 callbacks, 25 cover spheres,
7 complete regions, 35 expanded-boundary rejections, 418 clear spheres,
0 missing-region and 0 native-interior rejections. Budget was NOT exhausted.
No recovery proposal cleared the full cover. This rules out those three
alternative explanations for this captured attempt; it is not a global claim.
Saved machine-readable output:
`out/research/20260906-native-volume/recovery-capture-replay-result.json`.

Independent read-only Python inspection of the version1 layout and recorded
feature math: `inspect-recovery-capture.py` / `recovery-capture-boundaries.json`
in that research directory. Historical/raw roots differ by only0.0000082m.
The first rejecting ball was usually sphere12 (blade child1), query radius
0.15625675m. Its containing native prism has normal approximately(0,-1,0),
plane7.8865366 world units, thickness0.15625675m, signed point distance
0.10749765m. Shifting enough to clear that ball still left other blade balls
inside expanded features. Do not confuse one clear sphere with a clear weapon.

`scan-recovery-capture.py` / `recovery-cached-direction-scan.json` tested six
straight translation directions in5mm increments, with exact region expansion
matching and conservative containment slack. The first full-cover cached
boundary-clear result was0.315m along negativeY; other tested axes found none
through0.40m. Native solid-interior tests at those NEW positions are unverified:
this scan proves neither an accepted seed nor a global minimum over rotations
or other directions. It does explain why the0.10m candidate search cannot
solve this captured case and why simply increasing that bound conflicts with
the existing0.30m visible-hand leash for the tested straight recovery direction.

The live run eventually submitted penetrated geometry: audit59 draw0 had
raw/submitted inside10/10, crossings32/32. The harness correctly failed before
a post-pass hold. 60 preserved audits, zero positive paired contacts (some
legacy draw0 corrections temporarily cleared raw8/0, but are not world-solver
success). Caches8816, frames45700, blocks579, holds465, hidden38, missingSeed3276.
Native faults/capacity/invalid/plan/region/cast failures and exhaustion all0.
Final preserved recovery counters1090 attempts, 0 clears,494859 callbacks,
mean373.1us/max2932.6us (includes first snapshot copy). These are diagnostic
null-run timings, not ordinary headset tracking. No video was started or
presented as a working demo.

MCC and SteamVR processes are absent. Normal headset settings independently
match `298D6E805F90CADD0BD2564459AD19DAC15DF0634A5D2431F65506A3898C4D44`;
installed config still `C570089F47A17AE8645310C02688CA1454E1A02C9239BC24C5CC316E4DA94946`.
The three original Campaign checkpoints and all four Steam metadata records
still match; AceSettings bytes retain the pre-run completion state unchanged.
No save restoration or cleanup was needed. All tool sessions are terminal.
The installed diagnostic defaults to the disabled partition path; only an
explicit WorldReplay launch reproduces the failed behavior. Accepted pointer
unchanged. The next behavioral candidate must address no-seed draw disposition
and continuity when a full cover cannot recover within the visible leash,
using this exact replay to avoid another guessed recovery distance. Tighter
cover geometry may also matter, but is not established by this capture alone.

### 2026-09-06 pending candidate: retain ownership while awaiting a seed

The captured fbf4d7f no-seed case previously returned disposition0 and allowed
legacy recovery to publish a penetrating draw (audit59 inside10/10). Change
that single recovery transaction to disposition2: keep raw request publication,
hide the final draw, and preserve the current cache while the worker continues
independent full-cover seed checks. A current unseeded cache still owns world
contact; it is not permission to use the legacy wall/leash fallback. The worker
returns after cover construction/audit while awaiting a seed, invalidating only
previous physical sweep history so an invisible raw pose cannot shove/melee or
become a recovery-tick sweep origin. Missing-seed diagnostics retain priority
over generic hidden/clock records, preserving the harness stall check.

No seed radius, feature membership, native binding, skin, leash, query budget,
or accepted-build pointer changes. Existing bounded recovery remains intact.
Partition mode is enabled only by its existing explicit environment opt-in.
This is a pending behavioral candidate, not a headset acceptance or a completed
collision solution. Required live evidence: sustained visible paired contact,
obstructed no-seed handling, and withdrawal returning to a clear visible pose.

### 2026-09-06 13:39Z no-seed ownership run FAILED; sword draw feedback found

Candidate2d04ac7dc04c61da7a68e2253203a15cf528c72d, DLL
19028D00D015115B3086CCC4AEDA98810BF38E65AAC714A0F8DF4AC7A01F8862,
package out/candidates/2d04ac7-h3-physical-contact-20260906-133905254Z.
Steam / SteamVR null / Null Model Number. Result
out/debug-openxr/20260906-133912537Z-controller-contact-result.json;
log SHA36275A40153CD5529E41998EB7ADE1119C6C36C5BCA60DC1A8AA2AA104C131BD.
Guardian upper blue fixture room; W1800 then W500/E1500 picked up sword.
Controller X-.35/Y-.18/Z-1.25 at13:43:41.335Z, W450 at13:43:57 into
right angled pillar. At13:44:15.687Z withdrew controller to Z-.65.
Audit48 caught visible draw1 raw/submitted inside800/181, crossings144/232,
gap0.24596m, triangles256/256. Harness stopped and restored normal VR.
The attempted subsequent S400/neutral controller command found MCC already
closed; that full withdrawal did NOT execute. No video was made. 49 audits,
zero positive visible pairs. Last preserved world counters: caches7039,
seeds6, frames41666, blocks3873, holds344, hidden7454, missingSeed139,
no faults, unknown casts, region misses, invalid casts, or budget exhaustion.
Independent exact snapshot replay:131 callbacks,35 expanded-boundary rejects,
96 clear sphere checks,25 cover spheres,9 regions,0 missing/native rejects,
no recovery and no exhausted budget. Snapshot SHA
F1F2AF5478E0D83A131526AFCA1E700B9DED5B956C48F196A9C230F2D8FAE76E.
Disabled failed partitions as own commit580d0bb before the next change.

The log proves a separate blade-selection feedback boundary: before hiding,
sword active1/256 triangles; throughout hidden audits40-47, sword active0 and
12 handle-only triangles; on withdrawal audit48, active1/256 triangles return.
Code confirms why: Halo3ObserveSwordSelection memcmp paired the native final
draw against g_halo3LastDrawnWeaponPose, but that publication intentionally
preceded worldFinal==2 scale suppression. This is an exact mismatch whenever
hidden. Halo3AppendLiveSwordGeometry then omitted the blade. Consequently
world seed/cover work during hiding could use handle-only geometry. This
observed state change and code path establish a feedback defect; they do not
prove all remaining native sweep mathematics correct.

Next candidate pairs selection with a separate publication of exact final
draw matrices after suppression. Physical pose/aim publications stay unscaled.
The blade remains contingent on exact tag/weapon/native selection pairing and
freshness. The runtime replay test exercises visible -> hidden -> visible,
retained full-size blade geometry, and rejection of a changed hidden matrix.
Native bindings, collision feature math and visible leash stay unchanged.

### 2026-09-06 13:50Z final-draw pairing: Guardian sword PASS, limited scope

Runtime0bb5f1232d8721ec0b474d13e1c62837875426e2, package
out/candidates/0bb5f12-h3-physical-contact-20260906-134950468Z;
DLL8AF75D573AA3F47EC3D6398D51A250044E5EBA6EC8ABFD852AB6B520C699F8B7,
launcherD489C5763E21FC339999DC734CED09A2068AAC6C035EA8B7BF4339B6810FA450.
Release build and both CTests pass; sword runtime replay131 checks0 failures.
All present editions installed: E:Steam only; N:Steam and N:Store absent.
Backup out/deploy-backups/8b80f85-steam-before-0bb5f12-20260906-134951338Z.

Steam / SteamVR null / Null Model Number,13:50:14.232-13:59:25.562Z.
Result out/debug-openxr/20260906-135008475Z-controller-contact-result.json;
preserved log3E60F7F09E3C724233C72030A426937E28E458C3A266B9F5A74A3440BFD80662.
Started in the corridor toward the upper blue fixture room. First sword spawn
was not picked up; those setup movements prove no blade contact. Spawned a
second sword in the upper room, picked it up after W1300/W600/W250 and E1500,
then faced its flat paneled wall next to the overshield doorway.

Controller X.18/Y-.18: Z-1.05 at13:55:47.468Z gave shallow correction;
Z-1.23 at13:56:01.382Z produced visible positive pairs. Audit73 raw/submitted
inside8/0,crossings32/0,gap0.28769m; sustained visible pairs followed. At
13:56:45.481Z extended to Z-1.5, then withdrew to Z-.65 at13:56:48.081Z.
Hidden audit90 retained256/256 triangles, active sword1, inside8/0,cross32/0,
gap0.38656m; audit91 likewise retained full blades. Unlike2d04ac7, hiding did
not switch to12 handle-only triangles. Visible recovery was checked in the
13:57:05.236Z capture. Recontact Z-1.2 at13:57:07.595Z resumed visible pairs
(e.g. audit107 raw8/0,cross32/0,gap0.23378m). A second deep cycle at
13:57:56.868Z added W100, then S200 and Z-.65 at13:58:00.503Z. Subsequent
visible audits were full256/256,raw/submitted0/0 with near-zero correction;
13:58:20.728Z capture confirms visible sword. These are actual accepted
controller commands and normal keyboard movement, not teleport/memory writes.

Harness pass including the full180s follow-up hold to13:59:07Z.147 audits,
36 positive visible pairs, maximum positive gap0.29726m. No detected visible
submitted mesh intersections/native faults. Final preserved caches21649,
seeds4,frames127714,blocks19633,holds2140,hidden2157,missingSeed0. Historical
seed revalidation13/13 clear; no rejected recovery snapshot was generated.
Sword observations118712,paired118690,appends13332,stale1,rejected0,active1.
One early/final ownership loss at13:57:05 was a reset identity change during
clear withdrawal, gap0.00012m (reason8/4, reset reason33); not a penetration.
308 region misses during the second deeper movement held previous clearance
and recovered;54 query-budget exhaustion events occurred in the first retreat.
Do not erase these concerns because the cumulative harness passed. No cache
capacity/invalid/plan/cast faults. Mean render solver5.8us,max2834.4us;
cover6.6us/max2195.1us,gather85.5us/max2546us; mesh auditmax9530.8us.
These diagnostic null timings include audit work and are not headset latency.

Actual50s recording:
out/demos/20260906-135622-guardian-sword-full-blade-recovery/raw.mp4,
SHA4DBE8ED9290006A65944E97FFF02D1F4343275C8CD8669EB7B614FE65A04CBA4.
Trimmed seconds18-50 without synthetic imagery to sword-contact-and-withdrawal.mp4
in the same folder, SHA92C0B49B905F47F79A6A459927FD2BAA3B3CA9BF03D3EBDE5629F842A72D0946.
Contact and withdrawn frames were inspected. It demonstrates this limited
sword contact/recovery; displaced electrical effects remain visible and are
not fixed. It is not a props/NPC/melee/Floodgate/full-surface showcase.

MCC/SteamVR are closed. Independently verified installed DLL above, config
C570089F47A17AE8645310C02688CA1454E1A02C9239BC24C5CC316E4DA94946 and
normal SteamVR298D6E805F90CADD0BD2564459AD19DAC15DF0634A5D2431F65506A3898C4D44
(null false, forcedDriver empty, requireHmd true). Three original Campaign
checkpoints and all four Steam metadata records match. AceSettings wrapper
changed but decoded content equals the preserved pre-run profile exactly;
current SHA A1F17DE454DFC8A9CFCBCFA9670BEB57455177D31F77FB354DDB48CC972ADF5B,
comparison out/research/20260906-native-volume/blade-pairing-profile-comparison.json.
No save restoration needed. No old-demo cleanup attempted this turn.

Accepted pointer unchanged. World partitions and blades still require their
existing explicit launch opt-ins; ordinary headset launches do not silently
enable this experiment. Pending: original angled-pillar repeat, Floodgate rocks,
prop/NPC/AR regressions with this path, visual effects alignment, region-miss
recovery and ordinary headset tracking. This is progress, not full readiness.
