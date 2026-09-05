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
