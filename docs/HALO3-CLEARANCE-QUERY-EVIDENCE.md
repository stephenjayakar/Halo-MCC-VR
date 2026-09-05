# Halo 3 clearance-query investigation

September 5, 2026. Offline official H3EK evidence only. No new retail binding,
runtime hook, or headset acceptance. Installed candidate remains affb3cb.

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
`out/research/20260905-clearance-query`. The auditor checks identity, six direct
call targets, and three assertion expressions before writing disassembly.
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
