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
controller: 16,367 checks, zero resets and zero missing poses at the preserved
log cutoff of 14:28:47.070. A later live tail reached 16,703 checks before close,
but that later line is outside the preserved log cited here.
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

Follow-up `65cbdcd` passed the separate controller-contact admission test in
Construct Forge. Package:
`out/candidates/65cbdcd-h3-physical-contact-20260905-213018516Z`.
Installed DLL independently verified:
`211135C5706481D6EDCADEAC83334897AD03DEEA02B391C9EEF15AFCC0EE9FB8`.
Release build/core tests and Reach consistency check passed.
Result: `out/debug-openxr/20260905-213027986Z-controller-contact-result.json`.
Log SHA-256: `796070C3FD3C64DB7D04AD014285ADF25F68115D1F629FBCB3A334AF5D9DACF2`.
Steam / SteamVR null driver / Null Model Number, 21:30:33-21:33:00 UTC.
At the preserved cutoff 14:32:40.692: 2,782 sweeps, 93 native geometry samples,
36 weapon triangles, 178,048 wall rays, 8,334 approved render submissions;
zero prop hits, impulses, or melee. One wall block and one recovery were already
present at the first published status. Recovery stayed at one through 8,336
checks; missingPose stayed at six. The log does not capture that first recovery's
input/output coordinates, so its cause is unresolved. This is evidence of
normal contact-path admission, not a successful impact or a clean startup.
The inspected `out/debug-openxr/20260905-213216208Z-controller-contact-framing/`
still shows only partial weapon geometry near the bottom. No demo was recorded.
Both processes closed; null=false, forcedDriver empty, requireHmd=true and the
exact pre-test settings hash restored. No headset result or accepted pointer change.

## Recovery input capture (September 5, 21:35-21:43 UTC)

Candidate `eab25617a1fb95e03ae93a70c607bb27b05138d9` adds one immutable
first-recovery record per DLL lifetime. A render-side CAS reserves it; the
existing background status logger consumes the release-published record once.
No render logging, allocation, waiting or new lock. This is telemetry only.
Package: `out/candidates/eab2561-h3-physical-contact-20260905-213538542Z`.
Installed DLL SHA-256 independently verified:
`3979D40177D86789814B36007720B8E0D2A327D9B2A3F91874AFE5D7990E9891`.
Build/core tests and Reach consistency check passed.

The fixed-controller repeat passed normal contact admission, with 5,850 checks,
zero resets and zero missing poses at preserved cutoff 14:37:47.034. Thus the
previous startup reset is not deterministic across these launches/spawn points.
Result: `out/debug-openxr/20260905-213548533Z-controller-contact-result.json`.
Log SHA: `566B7E1E0118E1E915A57E1F266EC5EFAE55F21C1B1F83D695A9DCF71DDDBB5D`.

The first controlled-injection attempt crashed at the shell before a title
adapter startup was logged. Result:
`out/debug-openxr/20260905-213813969Z-visible-weapon-gap-result.json` (failed).
Log SHA: `7EA3B5A0C1007931D819A5778541C44F4905CF694B0CEEFE548C2756277FC45F`.
Original Windows dump remains at
`C:/Users/aj12a/AppData/Local/CrashDumps/MCC-Win64-Shipping.exe.20912.dmp`,
SHA `CEF909040936B27A41190E6429097AAAA9D8DD13A251E385FF874FEE02E83C2A`.
Read-only minidump parsing: exception execute-access violation at address zero,
exception RSP `0x9feb2ff5e8`, stack top `chrome_elf.dll+0x247cb`; additional raw
stack candidates at +0x110/+0x190 map to chrome_elf +0x24e95/+0x24f09.
These are stack contents, not a fully unwound stack or a proven crash cause.
No recovery capture execution is evidenced by that launch.

After terminal cleanup, the fresh controlled attempt passed exact visible gap
plus hand recovery. Result:
`out/debug-openxr/20260905-214110129Z-visible-weapon-gap-result.json`.
Log SHA: `24F8039A236D7BB10A7784D04E9D3CBB0FB17D22F99E1FC1F78B5E89830D1F9C`.
At 14:42:41.492 the first record has tracked=(-6.482758,-6.624856,12.511784),
final=(-6.350758,-6.624856,12.511784), scale=.33: .132/.33=.40 m,
matching the injected separation. Proposal/displayed serial=2, proof=3 (raw),
corrected=0, no target, consumed offset zero. This validates the capture against
a known event. The preserved final line at 14:43:27.952 has five resets and
8,002 checks, missingPose=5. The other four resets are unresolved: this first-only
record does not describe them. Do not call this a single-reset or fully stable run.

All runs were Steam / SteamVR null driver / Null Model Number, not headset
acceptance. No useful video was produced. Final cleanup independently verified
MCC/vrserver absent, null=false, forcedDriver empty, requireHmd=true, settings
SHA `175C79EDD6BBD58D1B7638BFFF7AAFF784625710BBEBE2A574BE76E4AC8BA89E`.
The accepted pointer is unchanged.

## Bounded recovery history: confirmed replay loop (September 5, 21:46 UTC)

Telemetry-only candidate `e34653d9dd61e1e7d3fa866d27421284cb35e764`
retains the first 16 immutable recovery records per DLL lifetime. Unique atomic
reservations and per-slot release/acquire publication prevent overwrite/read
races; logging remains on the existing cold status path. No collision or
recovery behavior was changed. Release/core tests and Reach consistency passed.
Package: `out/candidates/e34653d-h3-physical-contact-20260905-214614595Z`.
Installed DLL independently verified:
`95FFB0D5C8B17B3CDE4980A74EC6DEBDE29A664F908F28129288FD249CD41640`.

The exact visible gap + hand recovery run FAILED with six confirmed geometry
penetrations. Result:
`out/debug-openxr/20260905-214628670Z-visible-weapon-gap-result.json`.
Log SHA: `8E53687B5A620CBE9293355A00826B441A3972C9F3D39282D10017E95F1E789B`.
Steam / SteamVR null driver / Null Model Number, 21:46:34-21:48:14 UTC.
At preserved cutoff 14:48:01.969: 19 resets, 207 checks, 31 missing poses.
The first record is the expected .40 m injection. Records 1-15 all have target
E2740005, proof=1, corrected=1, finalGuard=1/1 and a displayed approval exactly
one proposal behind. Consumed reconstruction offset is zero. Their measured
root separation is .300065-.559766 m. Most intervals are 515-547 ms, immediately
after the 500 ms recovery cooldown (first two intervals are 578/750 ms).
Normalized event data:
`out/debug-openxr/20260905-214628670Z-recovery-events.json`.

This establishes a repeatable correction/recovery conflict in this fixture,
not a stale long-lived cached palette explanation: a fresh approved contact
correction repeatedly exceeds the hand limit. The cooldown then permits the
same target to constrain again. The exact replay itself moves the weapon via
compound support points and a sinusoidal placement; the next behavioral change
must distinguish that artificial placement from real controller motion. Source
inspection also confirms the render escape search can expand ten .025 m shells
(doubling each shell), so its allowed correction is not bounded by the .30 m
hand limit. No claim is made that a particular escape-search branch produced
these corrections; the records do not include that branch.

The instrumentation succeeded in exposing an existing conflict; the collision
fixture failed and must not be treated as accepted or as a successful demo.
No video recorded. Normal settings independently restored (null=false, empty
forcedDriver, requireHmd=true), MCC/vrserver absent, hash
`175C79EDD6BBD58D1B7638BFFF7AAFF784625710BBEBE2A574BE76E4AC8BA89E`.
Accepted-build pointer unchanged.

## Movement-gated recovery candidate (September 5, 21:56 UTC)

Candidate `0bf98d08f3f6920e35fceacc98cd4662492fc162` changes recovery rearming:
the existing 500 ms minimum remains, but elapsed time alone no longer releases
recovery. The uncorrected authored weapon root must translate at least .10 m or
turn at least 20 degrees (forward or up basis comparison). A valid weapon/tag
or runtime-generation change releases the hold. Invalid poses do not release
it. During the hold the contact worker remains reset and reconstruction does
not consume contact offsets; the gun follows the uncorrected pose. This is the
user-requested return-to-hand behavior, not proof of nonpenetration during the
recovery hold. Controller/headset feel, animation effects on the root comparison,
wall-contact release and same-prop retreat still require testing.

The reference uses the existing atomic pose publication with one root and the
runtime generation in its serial. Both publisher and rearm decision are in the
existing render path; the contact worker consumes the hold flag. Pure tests
cover supported world scales, stationary/subthreshold and above-threshold
translation/rotation, invalid input, roll and scaled basis vectors. Build/core
tests and Reach consistency passed. Package:
`out/candidates/0bf98d0-h3-physical-contact-20260905-215605159Z`.
Installed DLL independently verified:
`E9F1D3113F4166C6BB8879001C0A447C84241BDA98D44469EC5CAC493D34E4CE`.

Runtime test used the fixed-controller normal contact path, not the prop replay:
`run-h3-contact-validation.ps1 -Test controller-contact -TestHandRecovery`.
Result: `out/debug-openxr/20260905-215620874Z-controller-contact-result.json`.
Preserved log SHA: `ECA31443B4CD6D87464F224A711DB32B916B40F4B645BE250A9F42089981F880`.
Steam Construct Forge / SteamVR null driver / Null Model Number,
21:56:26-21:59:05 UTC. The one .40 m injection caused exactly one recovery.
From 14:57:48.113 through at least 14:58:00.233, `awaitingMotion=1`, resets=1,
checks=1 while the 500 ms cooldown was already inactive. Thus timeout alone
no longer rearms. Normal background W input held 600 ms, ending at 14:58:15.379,
then `awaitingMotion=0` by 14:58:16.394 and normal native sampling resumed.
At the preserved cutoff 14:58:46.692, resets remained one, checks=5,799,
missingPose=2, awaitingMotion=0. No contact-loop recurrence was observed in this
controlled hold/release test. It did not press against the previously failing
prop and does not supersede that failed gap result. The harness pass itself
checks admission plus recovery; the held-then-released sequence is established
by the separately inspected status lines and recorded normal input.

MCC/vrserver absent after cleanup; null=false, forcedDriver empty,
requireHmd=true, exact settings SHA
`175C79EDD6BBD58D1B7638BFFF7AAFF784625710BBEBE2A574BE76E4AC8BA89E` restored.
No headset acceptance, useful demo video or accepted-pointer advance.

## Visible fixed controller and wall approach (September 5, 22:02-22:11 UTC)

The old fixed controller position (.28,1.25,-.45) had never been measured
against the null headset's LOCAL origin. With installed `0bf98d0`, 23 F9 steps
(+.8 rad process-local head pitch trim) revealed weapon geometry above the
camera. Inspected image:
`out/debug-openxr/20260905-220409621Z-controller-pose-up/0000-220409901Z.jpg`.
Result: `out/debug-openxr/20260905-220222262Z-controller-contact-result.json`,
log SHA `8E57F6CF50AAABE2BCE33EF1DD7894F434341B2AF0B97B5B3DE200514FA2C262`.
This supports a fixture placement correction; no numerical null head-position
measurement is claimed.

Diagnostic-only candidate `a299b061aa2ec9526aa90209ce8051361e084355` sets the
shared aim/motion fixed position to (.18,-.18,-.65). Real controller input and
user configuration remain unchanged. Package:
`out/candidates/a299b06-h3-physical-contact-20260905-220638316Z`.
Installed DLL SHA independently verified:
`C3890C1AF27243EE7DEECC45C3A3E51B96B9B662D88945E7F473519897AB1977`.
Build/core tests and Reach consistency passed. A normal level-camera screenshot
now clearly shows the held assault rifle:
`out/debug-openxr/20260905-220831731Z-controller-front/0000-220832020Z.jpg`.
The framing change is visually verified; no actual head pose values were read.

Run: `out/debug-openxr/20260905-220649984Z-controller-contact-result.json`.
Preserved log SHA: `8DE2168C474546E8826BB568BF12DB0FEBFF314941DC5D8EF7792F58A8B80D23`.
Steam Construct Forge / SteamVR null driver / Null Model Number,
22:06:55-22:11:35 UTC, no synthetic prop replay or injected recovery.
A natural startup recovery records consumed offset=(0,-.000001,-.33) at scale
.33, with a matching 1 m downward final-root displacement. Its provenance is a
consumed wall offset, not the controlled injection; the cause of that offset
is not yet established.

Normal A600ms/D850ms, then D2000ms, established a wall-facing view. At that
point wallBlocks remained one. W1500ms then increased wallBlocks to nine and
resets to four. Inspected contact image:
`out/debug-openxr/20260905-221025988Z-controller-wall-forward/0000-221026253Z.jpg`.
Later S1100ms retreated. At preserved cutoff 15:11:16.426, wallBlocks=16,
hits=10, resets=7, checks=12,975, awaitingMotion=0, nativeSamples=93, and
stage=sweeping. Thus retreat did release the recovery hold and resume sampling.
A further S700ms attempt was rejected by the input tool because the harness
had already closed MCC; no input was sent. The admission test passed, but it
is not a nonpenetration test and does not prove clean sustained wall contact.
No video was recorded or represented as a successful functionality demo.

The new fixture supplies a visible ordinary controller-path wall reproduction.
Sustained pushing still drives large corrections and recoveries; this is not
claimed fixed by the earlier movement gate. The movement gate specifically
prevents time-only rearming when the uncorrected pose stays still.
MCC/vrserver absent after cleanup, null=false, forcedDriver empty,
requireHmd=true, exact settings SHA
`175C79EDD6BBD58D1B7638BFFF7AAFF784625710BBEBE2A574BE76E4AC8BA89E` restored.
No headset acceptance or accepted-pointer advance.

## Outward-retreat candidate (September 5, 22:18 UTC)

Candidate `b9b192b65e88a2e8ec9bb4d8bf66eee55fbedd94` replaces distance-only
translation rearming with a .10 m projection toward the recovery correction.
Moving farther into the obstruction or tangentially no longer releases the
hold. The existing 20-degree turn escape and weapon/generation changes remain.
If the correction vector is unusable, the prior distance escape remains so a
valid tracked pose is not stranded by an invalid output. The reference now
publishes two roots together (uncorrected and displaced), preserving the
correction direction without racing a later worker offset. Core regressions
cover inward/tangential/subthreshold/outward and diagonal movement across all
supported world scales, plus invalid-direction escape. Build/core tests and
Reach consistency passed.
Package: `out/candidates/b9b192b-h3-physical-contact-20260905-221803994Z`.
Installed DLL independently verified:
`419044BBBB035BA9EF6F30136CEEEBE4A720556FA49858174BFD4C5021A43C34`.

The live attempt passed controller contact admission but did not exercise the
new recovery rule: zero wall blocks, zero resets, and 27,275 leash checks at
preserved cutoff 15:22:13.385. Result:
`out/debug-openxr/20260905-221818553Z-controller-contact-result.json`.
Log SHA: `86CEB4E49C35A6E90568E4089ECDCC4DA3508DAFFB0C9E8C7A46F24CB4FA11A7`.
Steam Construct Forge / SteamVR null driver / Null Model Number,
22:18:24-22:22:32 UTC. The spawn faced an open railing beside the intended wall.
D600ms, then W2500ms and S2500ms did not reach the wall. The 26-second recording
frame shows forward movement toward the railing, so failed delivery of keyboard
input is not the explanation established by this attempt.

The 30-second file
`out/demos/20260905-222134-forge-wall-pressure-retreat-test/raw.mp4`
is an unsuccessful test recording, explicitly NOT a functionality demo.
Its adjacent `result.json` records that limitation. Frames at 18 and 26 seconds
were inspected; no frame-by-frame verification or success claim. A later
window capture coincided with harness shutdown and is black; it adds no wall
contact evidence. A valid wall-pressure/retreat test is still required before
claiming this candidate prevents the observed loop.

Normal settings independently restored: null=false, forcedDriver empty,
requireHmd=true, MCC/vrserver absent, SHA
`175C79EDD6BBD58D1B7638BFFF7AAFF784625710BBEBE2A574BE76E4AC8BA89E`.
No headset acceptance or accepted-pointer advance.

## Steered wall-pressure test (September 5, 22:27 UTC)

Installed candidate `13da6c4b377bc78591596828c74a87e66d59560c` adds bracket-key
15-degree camera turns only while the opt-in fixed-controller debug pose is
active. Normal headset input is unchanged. Build/core tests/Reach gate passed.
DLL SHA: `9682736EEBECD3F778E954660B8FD6D02680E39749D772B2FBFEFFECB76E4221`.
The live Forge view visibly responded to six right turns and two left turns.
W700ms and successive D movement reached a wall. At 15:31:49.538 the log
reported static-block, wallBlocks=3, hits=2, resets=1, checks=25194,
awaitingMotion=1. Additional D1500ms ended 15:31:53.369. Through 15:32:21.858,
the same recovery remained held with no additional reset despite inward pressure.
This supports the directional hold behavior, but does not establish retreat
release: A2200ms was attempted during shutdown and key delivery failed.

Result: `out/debug-openxr/20260905-222750311Z-controller-contact-result.json`.
Log SHA: `E8908D68B3968C733C0B22BBD67FE9CF6B7C9C38227EE256B49E4774D6BC4031`.
Steam / SteamVR null driver / Null Model Number. The admission harness failed
its final native-sampling check while recovery intentionally suspended sampling.
That failure is retained; this is not a passing complete contact test.
The 30-second `out/demos/20260905-223055-forge-steered-wall-retreat-test/raw.mp4`
ended before the later contact and retreat attempt and is not a functionality
demo. Normal SteamVR configuration was independently hash-verified after cleanup:
`175C79EDD6BBD58D1B7638BFFF7AAFF784625710BBEBE2A574BE76E4AC8BA89E`.
MCC and VR processes were absent. No headset acceptance or pointer advance.

## Synchronized wall approach and retreat (September 5, 22:41 UTC)

`tools/record-mcc-contact-sequence.ps1` now starts the bounded live recorder in
a background PowerShell job, waits for a nonempty recording file, verifies the
same MCC PID before movement, and timestamps approach/pressure/retreat. It lets
the recorder finish normally so window-state restoration runs. No game memory
writes or new runtime behavior. PowerShell parser and diff whitespace checks pass.

The first W/S sequence moved along the wall despite the diagnostic camera turn;
keyboard movement is not automatically aligned with that camera heading. It
added no wall blocks and is not a contact demo. The second D/A sequence did:
`out/demos/20260905-224128-forge-synchronized-side-wall-retreat/raw.mp4`.
Its adjacent sequence.json records D2500 at 22:41:30.965, D1500 at 22:41:37.214,
and A3500 retreat at 22:41:42.099 UTC. Recording completed successfully.
At 15:41:32.321-38.382, wallBlocks increased 39 to 400 and wallSetback stayed
0.098-0.109 m. At 15:41:40.404, one new leash recovery raised total resets from
one to two, awaitingMotion=1 and nativeSamples=0. At 15:41:42.424, immediately
after retreat began, awaitingMotion=0 and nativeSamples=93; no further resets
through 15:41:58.584. Thus this natural wall recovery released on actual retreat.
The earlier reset occurred before this recording and is not counted as its result.

Frames at 5, 10, 12 and 18 seconds show approach/contact and retreat with the
rifle visible. They are not an exact geometric nonpenetration proof. This is a
bounded wall-recovery demonstration, not a complete interaction showcase or
headset acceptance. No evidence here resolves the Floodgate rocks, sword blade
coverage, headset flicker, or performance report. Runtime remains 13da6c4.
