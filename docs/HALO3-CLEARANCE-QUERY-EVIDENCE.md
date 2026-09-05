# Halo 3 clearance-query investigation

September 5, 2026. Offline official H3EK evidence only. No new retail binding,
runtime hook, or headset acceptance. This investigation began on affb3cb;
the separately validated startup-readiness candidate d8f10b6 is now installed.

## Problem and intended behavior

The recorded ramp case in `HALO3-FLOODGATE-20260905-FEEDBACK.md` shows an old,
uncorrected approved palette displaced from the current tracked weapon by about
.333 m, with no wall blocks or consumed collision offset. The renderer currently
selects every compatible approval before the fresh proposal, including an
uncorrected approval. Compatibility has no age limit. Merely selecting a fresh
proposal would not prove that it remains clear of a newly approached wall.

The intended improvement is responsive tracking within a proven clear volume,
while retaining collision constraints at its boundary. This is a proposed design,
not an implemented or verified clearance guarantee.

## Verified official executable observations

Pinned `halo3_tag_test.exe` SHA-256:
`59A78F2C96034D7CEB5D710505B2B36813AA141FC81A083E3F952973DBCE4602`.
Reproduce with `tools/audit-h3-clearance-query.py`; local output is under
`out/research/20260905-clearance-query`. The auditor checks identity, nine direct
call targets, three assertion expressions, and coverage/capacity instructions
before writing disassembly.
It also rejects an unrelated input file before interpreting any address.

- The spawn diagnostic explicitly naming `collision_test_point()` has a call
  at RVA 5AECA6 to 652090, whose wrapper calls 6520B0. This point test alone
  does not establish whole-weapon clearance.
- Routine 64EC20 constructs a center from a position, half a motion vector,
  and a vertical half-size. At 64ED6A it calls 64D6E0 with a radius assembled
  from half the motion length plus two size terms. A false return takes the
  branch at 64EDB7 which writes original position plus motion to its output.
  A true return calls 64EE60 at 64EDAD with collected feature storage.
- 64D6E0 initializes that storage through 71B590. The initializer zeros six
  bytes. Its return path at 64DDEE tests three 16-bit counts at offsets 0, 2,
  and 4; it returns false only when all three are zero. This is a count test,
  not proof that every enabled geometry category was successfully queried.
- Routine 64DE60 contains assertions named `center`, `new_center`, and
  `new_radius`, all attached to official `physics/collisions.cpp` records.
  Its positive-radius branch calls 64D6E0 at 64E03B with zero auxiliary size
  terms and then processes the collected features. Describing this as a
  sphere-adjustment candidate is an inference from those arguments and
  assertions; its exact API name and complete contract are not established.
- Unwind ranges can start in the middle of these functions. The range at
  64EEB4 chains through 64EE8B and 64EE84 to primary entry 64EE60. The radius
  assertions around 66FA9C belong to primary entry 66F9C0, which operates on
  virtual shape objects; they are not evidence of a standalone sphere query.

## Required before use

Trace feature overflow and disabled geometry flags; establish treatment of
interior solids and required active BSP state. Verify the exact argument and
output layouts, excluded-object behavior, and how any dynamic geometry affects
the lifetime of a clearance result. Match the resulting official semantics to
the pinned retail module using unique signatures before any invocation. Measure
query cost on the worker. Do not put an unaudited native query in a render hook.

A possible conservative certificate would bound the entire weapon and permitted
motion, not just its root. Zero features cannot be treated as that certificate
until the preceding conditions are proven. No production behavior changed in
this investigation, and the earlier floating-gun reproduction remains unresolved.

## Coverage audit and instance flag correction

The lower BSP query 7CF010 initializes four collection counts at offsets 0,
804, 1008, and 180C. Its return at 7CF126 considers only the first three.
The standard tree walker 7CB590 records reached non-sentinel leaves in the
fourth list when its mode bit 1 is enabled; the negative sentinel -1 exits at
7CB79F. Therefore an empty returned surface-feature set does not by itself
certify free space. A solid-interior check remains necessary. Dynamic object
features are also separately gated by high object flag bit 0 in 64D6E0, then
filtered by 6547D0, so their coverage cannot be inferred from BSP results.

This audit exposed a separate verified production bug in the existing vector
query. H3EK 651790 receives packed query flags in R8; 6517BA shifts its low
32 bits by three and 6517D2 skips the instanced-geometry path when that bit is
clear. The enabled path includes the structure instance list at +1C0 through
its cluster records, and also the newer representation through 800E00.
The public vector core calls this routine at 65364D.

Pinned retail has the matching branch at 1FE3D8-1FE3F0 in routine 1FE3B8:
R8D is shifted by three and the zero case branches to 1FE5BE, skipping both
instance representations. Verified callers from the existing vector core are
1FDD33 and 1FDE05. The 30-byte gate sequence is unique across executable
sections. `tools/verify-h3-instance-query-flag.py` checks both module hashes,
these call edges, the official extraction, and that unique retail gate.
Result: `out/research/20260905-clearance-query/instance-flag-verification.json`.

Production previously used low flags 1, high flags 7FFF. Low 1 enables BSP
but does not enable this instance path. The candidate changes low flags to 9
(BSP plus instances) for the wall solver, normal native contact samples, and
its opt-in structure test. The object-only debug isolation remains object-only.
No new hook or guessed binding is installed. This corrects prior documentation
that described low 1 rays as covering instanced geometry; that claim was too
broad. It does not prove that the user's particular Floodgate rock is an
instance, nor does it provide collision for render-only decorators.
The proposed clearance certificate and uncorrected-palette lag remain separate,
unresolved work.

## Result-type proof and first live instance result

The instance path calls H3EK 654590 at 65194F. After its exact geometry query
succeeds, 65474D writes result.type=3, and 654798 writes objectHandle=-1.
The matched retail path calls 1FCF3C at 1FE53C; 1FD0F2 writes the same type 3.
The verifier now checks both call edges and both type writes in pinned modules.
Thus type 3 in the wall-fixture result is specifically instance collision.

The b0426fe Forge wall fixture passed at 23:11:41-23:13:04 UTC:
`out/debug-openxr/20260905-231135817Z-wall-result.json`.
Log SHA `818B5B23C13B236F8F9CE8C07D371E3A69C73911BACC262065D00496836D260E`.
At 16:12:52.114 and 16:12:54.134, nativeType=3, object handle FFFFFFFF,
structureValidated=1, objectValidated=1, requested penetration .083-.105 m.
Steam / SteamVR null driver / Null Model Number. This proves the installed
candidate exercised instance contact and fixed-object contact in the fixture.
It does not prove an exact rendered controller-path nonpenetration result.
No video from that fixture was represented as a functionality demonstration.
Cleanup restored the byte-exact normal SteamVR settings before the next test.

## Output capacity and engine exclusions (September 5 follow-up)

Official base append paths were traced through their callers:
719F90 -> 71AF10 (vertex), 7196E0 -> 71A950 (edge), and
719CF0 -> 71B0E0 (surface). For the base append operation each reads its
16-bit count at output offsets 0, 2, or 4, respectively; it appends only
below 256 and increments that count. When full it skips the append and
retains the nonzero count. These base paths cannot wrap a full output set
into an empty one. This is not a proof of intermediate BSP collection,
auxiliary extrusion paths, or all exclusions.

The corresponding base arrays start at output +8 (36-byte vertex records),
+2408 (48-byte edge records), and +5408 (112-byte surface records).
The surface path at 71B155 increments count +4 and 71B15A multiplies the
old index by 70 hex; 256 records end at C408. Layout observations are
for this official executable only, not a retail ABI authorization.

Two global byte tests at 64D702 and 64D770 can clear requested categories
before collection: 64D760 clears low flag bit 3 (instances), and 64D779
clears high flag bit 0 (objects). Their retail identity/state and purpose
have not been established. A requested mask alone therefore cannot certify
coverage. The auditor now verifies these exact instructions and the three
base capacity branches, retaining complete append disassembly. It passed
against the pinned H3EK and rejected CLAUDE.md as an unverified module.

Next binding work must establish intermediate-query completeness, interior
solid treatment, category coverage, and dynamic freshness. This evidence
changes no production collision or rendering policy. The stale-palette
movement reproduction remains unresolved.

## Solid-interior classification and retail match

The named official point-test core 6520B0 asserts the structure flag at
652138, iterates structure indices 0..15, and calls 2D36C0 to test each active
bit. Its leaf traversal call 6521AA -> 4AB6F0 distinguishes the -1 sentinel
from a valid leaf index. The -1 branch sets hit=true and result type 1;
a non-sentinel leaf clears that hit before instance/object classification.
The traversal handles both packed 23-bit and ordinary 31-bit leaf indices.
If no structure is active and no separately requested boundary test hits,
the initial false hit value is returned. Thus point-test false also requires
an independent active-structure readiness check before it can mean clear.

Matched pinned retail traversal 16A4C4 has the same two representations:
packed children shift by 8, leaf classification tests bit 23 and preserves
-1; ordinary children test the sign bit and clear bit 31 only for a real leaf.
The 27-byte packed classification sequence at 16A53F is unique in executable
sections. Retail point core 1FCAC0 tests the active mask at 1FCB20, calls this
traversal at 1FCB65, and sets hit=true/result type 1 for its -1 result. This
19-byte call/result sequence is also unique. The instance path calls 1FC79C
at 1FCCA5; that transforms the point to instance-local coordinates before
calling the same leaf traversal at 1FC8B2. The object path calls 1FC8E0 at
1FCDC1. These are matches of official constructs, not retail-first guesses.

The full retail core examined here does not contain the two official global
flag-clearing switches described above. That is a difference in this point
core, not evidence that the still-unmatched retail feature gather has no such
exclusions. `tools/verify-h3-clearance-point.py` pins both file hashes, checks
these call edges, both unique retail sequences, and the official named
structure assertion. Output: out/research/20260905-clearance-query/point-verification.json.
No runtime binding or invocation has been added. A valid point is not proof
of clearance around the entire weapon; feature-gather coverage and freshness
remain required before changing render approval behavior.

## Retail feature-gather match and call-shape difference

The official movement caller 64EC20 matches retail 1FFB64: both allocate
C490 stack bytes, compute a center from position plus half motion and a
vertical half-size, construct a bounding radius, and branch between a feature
solver and unchanged position-plus-motion output. Official 64ED6A -> 64D6E0
matches retail 1FFC58 -> 1FE800; solver calls match 64EDAD -> 64EE60 and
1FFC9B -> 1FEF30. These call edges and the retail radius/call/branch sequence
are checked by tools/verify-h3-clearance-gather.py.

Retail 1FE800 zeroes the three output counts at 1FE855/1FE859, enumerates
active structures using the same mask consumed by the matched point core,
and returns whether any output count is nonzero at 1FEEE4. That 26-byte
count sequence is unique in executable sections. The direct BSP collection
and append calls are 1FE967 -> 25583C and 1FE9AC -> 24BC10. The object-feature
call is 1FEE13 -> 1FE64C. Both official and retail add exactly 0.0625 world
units to the requested gather radius; the verifier checks both RIP-relative
float constants. This inflation is native behavior, not a mod tuning value.

The retail routine differs from the official two-ignore-argument narrative:
its object path reads the first ignore argument at original entry RSP+28
(1FEDB4), while the second argument's entry RSP+30 slot is overwritten with
an instance index at 1FEB90 and reused as local storage. The retail movement
caller explicitly supplies -1 for the first and does not initialize the
second. Never assume the retail gather excludes two objects just because
the official call does. Output remains the eighth argument, entry RSP+40.
The full retail body has no counterparts of the two official debug switches
that clear instance/object category flags. Its object-cache path can broaden
flags by OR at 1FEA63-1FEA89; complete cache/filter coverage remains unproven.

Verification output: out/research/20260905-clearance-query/gather-verification.json.
Both pinned inputs passed. No runtime invocation, hook, clearance certificate,
or render-policy change has been made. Before relying on this query, a probe
must validate call/output shape and cost, and the intermediate collection and
object filtering still need coverage review. The independent solid-interior
and active-structure checks remain mandatory.

### Opt-in clearance probe candidate

The next candidate adds HALOMCCVR_H3_CONTACT_DEBUG_CLEARANCE=1, disabled in
normal launches. It binds both uniquely matched query entries, samples only
from the existing gated simulation contact worker, and records 32 queries
at 200 ms spacing. Radii cycle 0.1-0.4 m; masks alternate structure+instances
and that mask plus all object categories, with the player ignored. Only the
verified three counts are read from oversized sentinel-filled storage; writes
beyond the official C408 extent or counts above 256 fail the probe. A fault
also disables the probe alone. Cold status logging drains immutable records.
No result changes weapon collision, melee, or render approval. The harness
-ProbeClearance option requires all 32 observations and no bound/fault failure.
This is a query-call/output/cost probe, not whole-weapon clearance proof.
