# Floodgate headset feedback, 2026-09-05

The user tested Campaign on source `16c8bd47cdf6358b9ab94c1125d7afa610c4954b`.
Installed DLL independently verified as
`6D6C07D4140D9890560834C65E4453E26625F2B104FC971784C9F637539AFC60`.
The preserved log is `out/headset-sessions/20260905-floodgate-user-feedback/halo3xr.log`,
SHA-256 `DCB70A5D1D2E4A25A96CE3B7FD0B196C7A789E7F26488F0B1A9F217696C4C2C8`.
It identifies Steam, SteamVR/OpenXR 2.17.8, headset `SteamVR/OpenXR : oculus`,
120 Hz. The log does not identify the physical Quest model.

User results: enemy melee worked well; floor nudging seemed good; some Floodgate
rocks still clip; weapons stick far from the hand and flicker between positions;
energy sword collision uncertain; gameplay laggy. This is partial feature
feedback, not acceptance of the cumulative candidate. CURRENT-STATE is unchanged.

## Measured findings

- Seven native enemy melee applications, zero rejected/faulted/no-damage requests.
- 330 prop impulses recorded. This supports nudging being active, not proof that
  every visual contact is aligned.
- Body displacement reached approximately 1 m. Render code can retain compatible old
  palettes without an age limit; refreshing an approval does not prove that its
  position remains close to the current hand. These are concrete reasons to
  bound final presentation, including render-time target following.
- Gameplay renderWindow p95 is usually about 12–18 ms, with one 35.65 ms window,
  above the 8.33 ms deadline at 120 Hz. The old deployment backup is a null-driver
  session, not a comparable real-headset performance baseline. No specific
  optimization is proven by this comparison. One contact-render check peaked
  at 13.994 ms; this identifies an outlier to investigate, not its cause or
  typical cost (six of the first 190 samples exceeded 250 microseconds).
- Decorator instances peaked at three; plane counters stayed zero. Some decorator
  draw counters were nonzero. This does not identify which reported rocks are decorators;
  Floodgate geometry coverage remains unproved.
- Official H3EK energy-blade collision XML contains one region named `handle`,
  one BSP, and a small box with eight vertices. There is no blade region in that
  collision tag. File: `out/h3-contact-re/weapon-collision-xml/objects_weapons_melee_energy_blade_energy_blade.collision_model.xml`,
  SHA-256 `93AF66FC628922BDE83282361C2E2280A41A191C66D3FACAA605595FA89A8870`.
  The held-weapon contact reader uses the collision model. This is a specific
  explanation to verify against the equipped retail sword, not proof of runtime
  blade coverage. A future blade implementation must use proven visible geometry.

## First candidate: recover excessive hand separation

Limit contact displacement to 30 cm relative to the same weapon's uncorrected,
hand-tracked root. This avoids confusing barrel length or configured grip offsets
with separation from the hand. Capture the exact correction used during palette
reconstruction, including reuse of the stereo solve cache. Check after all final
render mutations, including old approved/cached palettes and target following.
On excess displacement, restore the full tracked palette and ask the gameplay
worker to clear constraints, approvals, target following and motion history.
Pause contact correction for 500 ms so the old constraint cannot immediately
reattach. Invalidating the stereo cache on recovery avoids carrying the stuck
solve into the other eye. The worker logs reset count and cooldown state.

This is deliberate temporary contact fallback: during recovery the gun follows
the hand and can pass through an obstacle. Melee speed remains 1.50 m/s. Clearing
motion history prevents the recovery jump itself from becoming a melee swing.

Build and core tests pass before packaging; tests cover radial distance, world
scales and invalid inputs. Headset behavior is unaccepted. Rock coverage, sword
blade geometry, remaining flicker below the leash and performance remain open.
Keep this behavioral candidate separate for the next headset result before
stacking changes to collision geometry or scheduling.

Packaged and installed source `6ea7e86ba2465a32fc08c69a4f8994c584823617`,
package `out/candidates/6ea7e86-h3-physical-contact-20260905-194613384Z`,
DLL `90FB7305ADE7D56EA9B8E707E15420D38059794D91058044CB2C914170EAFB9D`.
Release build, core tests and Reach consistency check passed. Steam on E: was
the only present installation; other Steam/Store locations were absent.
The package preserved the previous runtime and headset log. No headset result
exists for the reset candidate yet.

## Return from upstream trial

The user tried Pancreations' September 4 interaction work, rejected it, and
explicitly requested resuming this Halo 3 development line. On September 5 the
manifest installer restored the exact `6ea7e86` package above. The original
configuration was restored byte-for-byte (SHA-256
`C570089F47A17AE8645310C02688CA1454E1A02C9239BC24C5CC316E4DA94946`).
The desktop `Halo MCC VR - Interaction Candidate` shortcut now targets
`halo3xr_launcher.exe` again. Normal SteamVR headset mode was verified.

Upstream binaries, configuration and the user's upstream log were preserved
under `out/upstream-setup-backups/20260905-upstream-rejected/`; upstream binaries
were removed from the active mod directory by moving them to that backup after
hash verification. No upstream gameplay changes were merged into this branch.
The upstream result does not constitute a test of this branch's 30 cm recovery.
Independent sword geometry work is in `HALO3-SWORD-CONTACT-EVIDENCE.md`.

## Opt-in automated recovery diagnostic

The Forge replay fixture bypasses controller reconstruction, so its successful
gap sessions had zero valid hand-leash checks. A new explicit
`HALOMCCVR_H3_CONTACT_DEBUG_HAND_RECOVERY=1` diagnostic uses the exact replay's
uncorrected requested root as its synthetic hand reference, transforms the full
node palette consistently, and injects one 40 cm separation after the final
contact solve. The existing production leash must restore the whole reference
palette before publication. Normal sessions do not enable this diagnostic.

`run-h3-contact-validation.ps1 -Test visible-weapon-gap -TestHandRecovery`
sets that process-only variable, restores its prior environment in cleanup, and
requires nonzero recovery and valid-check counters in addition to the gap pass.
Release build, core tests and Reach consistency pass. Runtime validation of this
diagnostic is pending; it does not replace headset acceptance of the reset.

### Recovery diagnostic runtime result

Source `096f7cb20ab632b01595afe62e4d0583409c14fa` was packaged as
`out/candidates/096f7cb-h3-physical-contact-20260905-210215800Z` and installed
to the only present edition, Steam. Installed DLL SHA-256:
`34CEA629D7BB32D552D0424B2F21E3D554E7F792F617BF9F35B78DDDE7BD2D71`.

The controlled Forge `visible-weapon-gap -TestHandRecovery` null-driver run
passed on September 5, 21:02-21:04 UTC. Its result is
`out/debug-openxr/20260905-210237359Z-visible-weapon-gap-result.json`; the adjacent
log has SHA-256 `11652FE9BA743B7402DE77B5D397D5036B66C2ADE2A9225D3A00B1C81D72353D`.
At 14:04:40 local time, the preserved log recorded 8,093 valid leash checks, four recoveries,
two missing-reference samples, worker reset reason 32, and 2,660 exact geometry
separations with zero recorded geometry or solid overlaps. One excessive
separation was injected; the additional three recoveries were not injected
directly and have not been individually attributed. The diagnostic establishes
that the production recovery path runs and the fixture regains sustained gap
validation, not clean single-event behavior or headset acceptance.

The harness completed and closed MCC/SteamVR. The original normal-headset
settings hash was restored to
`175C79EDD6BBD58D1B7638BFFF7AAFF784625710BBEBE2A574BE76E4AC8BA89E`.
Null driver is disabled; forcedDriver is empty; requireHmd is true. The private
recovery environment switch is not enabled for a normal launcher session.
The accepted-build pointer remains unchanged.

Recovery timeline analysis of that preserved log finds increases at telemetry
windows 14:03:53.697, 14:03:55.717, 14:03:57.737 and 14:04:05.817. The last
window at 14:04:40.155 still reports four recoveries, while checks rose from
1,911 to 8,093: 6,182 subsequent checks over 34.338 seconds without another
reported recovery. These are two-second observation windows, not exact event
times. Target identity stays `0xE2900021` across the increases, but the logs lack
the per-event reference/displayed roots needed to attribute the extra resets.
Do not claim that target switching caused them, or that the test was a persistent
reset loop. The extracted timeline is
`out/research/20260905-sword-contact/hand-recovery-timeline.json`.

The saved success frame was visually inspected: it looks above the contact
area and does not show the requested interaction clearly. It is diagnostic
evidence only, not a demo. Existing F8/F9 controls adjust process-local pitch by
0.035 radians per press (clamped to +/-0.8); using those controls to frame a
future recording needs visual confirmation and does not require another camera
hook. No new demo video was created during this analysis.

### Framing experiment, September 5 21:08-21:12 UTC

The normal gap fixture (recovery injection disabled) passed on `096f7cb` while
testing view framing. Result: `out/debug-openxr/20260905-210826281Z-visible-weapon-gap-result.json`;
preserved log SHA-256 `DE45A84F148F5242E8B432AEC5118730122A13A0A170D3AED947483D196F53E4`.
Fifteen F8 presses, then eight more and a 600 ms backward input, changed the
view to the floor. Frames in `20260905-211000469Z-contact-framing` and
`20260905-211034572Z-contact-framing-back` were visually inspected: partial
weapon/hand silhouettes appear at the bottom, but no clear contact point.
The logged target also changed from `0xE2740005` to `0xE2900021` during the
session. That correlation does not establish the precise retargeting cause.
The fixture's anchored target, most recent contacted object and camera framing
need to be distinguished before recording a demonstration.

No video was recorded because the still frames did not meet the user's request
for visible functionality. MCC and SteamVR closed normally; the original normal
settings hash `175C79EDD6BBD58D1B7638BFFF7AAFF784625710BBEBE2A574BE76E4AC8BA89E`
was verified after cleanup. The pitch adjustment is process-local and expired
with MCC. This test is not a failed production collision candidate; it is an
unsuccessful demo framing attempt with passing diagnostic contact checks.

Follow-up source inspection found a concrete diagnostic selection defect:
`PhysicalContactRankDebugTarget` applied the acquisition minimum squared camera
distance (`0.25` world units squared) even to an already anchored target. Its
caller excludes every other handle while an anchor exists, then drops the
anchor when that target fails selection. Thus approaching a live, otherwise
valid anchored prop can discard it. The previous run does not record camera
distance at the transition, so this is not asserted as its proven cause.

The next diagnostic candidate restricts that minimum distance to unanchored
selection. Negative/non-finite distances, invalid mass/speed, native liveness,
parent, dynamic-body and generation checks remain intact. A new core regression
check covers a near anchored target versus near unanchored acquisition, plus
negative and NaN distances. Only the diagnostic rig calls this helper; normal
contact and NPC targeting do not. Runtime framing validation is pending.

## Fixed controller admission (September 5, 21:25 UTC)

Validator `9ecc10a` ran installed `f1351cd` in Steam Construct Forge through
SteamVR / Null Model Number with only `HALOMCCVR_H3_AIM_DEBUG_POSE=1`.
Result: `out/debug-openxr/20260905-212551749Z-controller-pose-result.json`.
Preserved log SHA-256:
`7388A4D42D0DFD257752B6BFF8E2A9407380F3BA4A99509643CE1DE1F14AEBD9`.
The fixed aim pose reaches normal right-hand reconstruction without a left
controller: 16,703 checks, zero resets and zero missing poses at 14:28:49.090.
No synthetic prop replay was enabled. Native contact remained at `stage=motion`
with zero samples: `VR_GetRightControllerMotion` still reads the absent real
controller publication, while the fixed pose only overrides `VR_GetAimPose`.
The two inspected still frames show partial dark weapon geometry at the bottom,
not a useful contact demonstration. No video was recorded.

The next diagnostic change supplies a matching stationary LOCAL-space motion
snapshot under that same explicit environment opt-in: identity orientation,
shared fixed position, zero velocities, and GetTickCount64 freshness/serial.
Normal controller tracking is unchanged. The validator gains a separate
`controller-contact` admission check requiring nonzero native samples; this is
not proof of impact, nudging, melee, or headset acceptance. The baseline mode
remains available for comparison. The harness restored the exact normal SteamVR
settings hash `175C79EDD6BBD58D1B7638BFFF7AAFF784625710BBEBE2A574BE76E4AC8BA89E`
and closed MCC/SteamVR. The accepted-build pointer is unchanged.
