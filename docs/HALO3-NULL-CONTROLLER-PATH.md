# Halo 3 null-controller motion scaffold

September 5, 2026. Diagnostic support for the user's requested autonomous
weapon-interaction tests; no new engine binding and no headset acceptance.

The existing HALOMCCVR_H3_AIM_DEBUG_POSE=1 option supplied a stationary hand
to visible aim and zero controller velocity to contact. Synthetic contact
fixtures could move collision shapes without proving corresponding visible
controller motion. The scaffold adds addressed WM_COPYDATA pose commands to
the installed mod, restricted to this opt-in mode and active Halo 3 gameplay.
Other messages and normal headset launches retain their existing behavior.

tools/send-mcc-controller-pose.ps1 sends protocol 1 (36 bytes), identified by
0x48335053, to the sole MCC window. It supplies LOCAL-space metres, yaw/pitch/
roll degrees converted to a quaternion, and a 200-5000 ms duration. Its message
timeout is bounded; it reports a rejection and preserves a JSON command record
instead of retrying an uncertain or rejected request. It does not use key
injection, game-memory editing, an external server or on-disk game patches.

The mod validates payload size/version, finite position and quaternion, bounded
hand workspace, and peak speeds (5 m/s translation and 12 rad/s rotation).
It rejects a new command until the previous path has finished. Position follows
a quintic minimum-jerk interpolation; quaternion motion follows the shortest
arc with the same timing. Analytic linear/angular derivatives feed the normal
controller-motion sampler. A 0.30 m push over 1200 ms peaks at 0.46875 m/s;
the same displacement over 250 ms peaks at 2.25 m/s. Neither speed alone
produces melee: the existing contact/impact checks still have to pass.

The UI thread is the sole command writer. Render/simulation consumers use
bounded atomic snapshot reads and QPC time, without locks, allocation or file
I/O. Visible aim, raw right-controller pose and controller-motion getters use
the same analytic path. Changing the Halo 3 runtime generation returns to the
original diagnostic pose instead of carrying a command into another session.

Unit coverage checks endpoint rest, numerical versus analytic translation
velocity, quaternion/angular agreement, antipodal quaternion equivalence,
invalid inputs and peak-speed rejection. Build/runtime results are recorded
below only after observation. This scaffold does not itself prove wall/prop
collision, NPC shove, sword geometry, or either Campaign/Forge acceptance.

Example after the normal controller-contact harness reaches gameplay:

```powershell
& tools/send-mcc-controller-pose.ps1 -Z -.95 -DurationMilliseconds 1200 -Wait
& tools/send-mcc-controller-pose.ps1 -Z -.65 -DurationMilliseconds 1200 -Wait
& tools/send-mcc-controller-pose.ps1 -Z -.95 -DurationMilliseconds 250 -Wait
```

For floor targets, lower Y gradually and choose an appropriate pitch. Native
collision, visible alignment and target movement must be observed, not inferred
from successful command delivery. The separate fresh-region experiment remains
opt-in and its unresolved dynamic coverage is unchanged.

Initial source 14c75f0 compiled but packaging stopped at a core-test crash.
The executable returned C00000FD (STATUS_STACK_OVERFLOW). Moving the new
test cases into a noinline helper, outside the existing large main frame,
made the complete test executable pass without increasing its stack reserve.
No candidate from the failed packaging attempt was installed. The subsequent
candidate also adds cold cumulative command/moving-sample/peak-speed logging
to distinguish command receipt from observation by the collision motion reader.

## First live motion check, September 5, 18:16–18:21 PDT

Source `d7191516029b54d9e553c8d6d7a3c1a0715a6701`, package
`out/candidates/d719151-h3-physical-contact-20260906-011513701Z`, passed the
complete Release build/tests and installed with DLL SHA-256
`80339EAC9E94701E940BB9BAFB055A91D6AEFAAEF204149EC75F239CE01D2D05`.
Only the E: Steam edition is present; other configured edition paths were absent.

The normal controller-contact harness entered Forge Construct with SteamVR's
Null Model Number. Nine addressed pose commands were accepted. Collision's
motion reader observed 533 moving samples, a slow push peak of 0.4686 m/s,
fast push peak of 2.2500 m/s, and rotation peak of 1.7254 rad/s. Hand recovery
reported zero resets and zero missing poses. The fresh-region experiment was
off. No target contact occurred, so this is motion-scaffold validation only.

Result: `out/debug-openxr/20260906-011618587Z-controller-contact-result.json`.
Preserved runtime log SHA-256:
`692D0F3B4ED5999F09F3F45ACE5729E121035ACBE43E8F4B27B6F15FC5904B18`.
Video: `out/demos/20260906-012027-forge-commanded-hand-motion/raw.mp4`,
SHA-256 `36F11D004C2BB56C261F58DE24CFA74DA7A1A441B6B86A663C4A1FFFC5F35990`.
Eight sampled frames show the live weapon translating/rotating and returning
to rest. They do not establish frame-by-frame continuity or physical contact.

The harness exited normally. Independent checks found MCC and SteamVR stopped,
null disabled, forcedDriver empty and requireHmd true. SteamVR settings exactly
matched the pre-run hash
`175C79EDD6BBD58D1B7638BFFF7AAFF784625710BBEBE2A574BE76E4AC8BA89E`;
the installed DLL and configuration hashes were unchanged. This is not headset
acceptance and does not advance CURRENT-STATE.

## Live wall pressure and recovery, September 5, 18:22–18:29 PDT

Same installed d719151 candidate, normal contact path, fresh-region off,
Steam edition / SteamVR / Null Model Number. Result:
`out/debug-openxr/20260906-012224159Z-controller-contact-result.json`.
Runtime log SHA-256:
`721CE3C603CEC5A27FD856A0D9F7713CB1431185A6CA184779F01A86F4593BE6`.
Fourteen pose commands produced 878 observed moving samples, peak linear
speed 3.4989 m/s and peak angular speed 0.9086 rad/s, with no missing poses.

Construct spawned the player at (8.9613, -6.8211, 12.5159), recentered yaw
98.6 degrees. After a 350 ms D step, two side reaches to LOCAL X=1.3 m
(2200 ms and 600 ms, each followed by a 2200 ms return) produced 156 native
wall blocks, 49 hits and zero hand resets before the camera turn. The side
view loses the weapon during part of its travel and is not a clear showcase.

Six addressed 15-degree right turns faced the nearby wall. Forward pushes
from Z=-0.65 to -1.05 m (1500 ms and 300 ms), return motions and a 25-degree
yaw rotation exercised visible frontal pressure. A later shorter push to
Z=-0.80 m also crossed the recovery threshold because the resting pose was
already constrained by the wall. Three recovery records were observed,
all corrected=1 with target=FFFFFFFF (world), with tracked-to-final distances
of approximately 0.306, 0.323 and 0.301 m. Thus the 30 cm hand limit did
activate on real wall constraints. This does not establish that the snaps
look acceptable or that all rendered weapon geometry stays outside the wall.
Afterward, the weapon returned to its resting visible pose. No prop impulse
or melee occurred in this run; a later walk passed the nearby crate rather
than establishing a controlled contact test.

Both 40-second videos have eight-frame review sheets and explicit limitations:

- `out/demos/20260906-012507-forge-commanded-side-wall-contact/raw.mp4`,
  SHA-256 `1C56C7E606CB8F93895FC2512F04309EE11D0248C98B1D35B82F6D13177F803E`.
- `out/demos/20260906-012614-forge-commanded-frontal-wall-contact/raw.mp4`,
  SHA-256 `BEDA238FA6DBBAD3B992C1B09218845A3576D29D31AD02D85E41212BE1D682D1`.

The harness completed and restored the identical normal SteamVR settings hash
listed above; MCC and SteamVR were stopped. Neither the harness admission
result nor these diagnostic clips is full interaction or headset acceptance.

Next controlled contact work should align the hand, view and a selected live
prop before recording. Read-only object snapshots can supply candidate
positions, but TLS enumeration sometimes finds both live simulation/render
tables and must not silently select one. Explicit table addresses are only
valid while still enumerated. The existing probe rejected ambiguity and an
expired table as intended during these runs.

## Loose-weapon alignment exposes rejected contacts, 18:32–18:38 PDT

Source `36d511ca1b2a73c3a1963f85a0255bd1f6934200` adds a cold, opt-in
world-pose readout from the existing atomic proposed-palette publication. It
logs root, forward, identity, age and correction provenance every two seconds.
It does not alter collision or normal headset behavior. Release build/tests
passed; package `out/candidates/36d511c-h3-physical-contact-20260906-013211844Z`
installed DLL `45D61CABDAE3CA19B5E9E8123D4308FF0859AB2A9B25A6878660577C4495D984`.
All configured edition paths were checked; only E: Steam was present.

Normal controller-contact, Forge Construct, SteamVR Null Model Number, fresh
region off. Result: `out/debug-openxr/20260906-013222013Z-controller-contact-result.json`.
Log SHA-256 `8D4C8FE9A2283EECB01B9FE19EF464C3159DBA44F61FBC3CA07E3DEEC93AF585`.

The initial player position was (6.6960, -11.4452, 12.5159). The nearest loose
weapon E27A000B at (4.7569, -9.5416, 12.6185) disappeared as the player
approached, with reserve ammunition increasing 64 to 160: consistent with
normal same-weapon ammo pickup. No physical-contact hit accompanied it.

The second target E2900021 remained at (6.2375, -6.8090, 12.5385), visibly a
loose Brute weapon. Twenty F8 steps lowered the diagnostic view about 40 degrees.
Read-only TLS snapshots and world-pose telemetry guided ordinary movement and
timed hand commands. A low hand command near X=1, Y=-1.79, Z=-1.5 brought the
proposed weapon root to (6.2314, -6.8934, 12.5426). This was not a successful
nudge. The run ended with 4 hits, 2 authored-shape hits, 2 unreliable-normal
rejections, zero impulses and zero melee. At 18:37:45 the log specifically
identified E2900021 as the candidate with candidateNormal=0.

Five hand recovery events occurred: three world corrections and two target
render-guard failures for E2900021 (proof=3, finalGuard=1/0). Shape sampling
paused during recovery, and proposed weapon identity became FFFFFFFF. Code
inspection shows Halo3ResetPhysicalContact clears the active identity, so
this alone is not evidence that the renderer culled the weapon. The last two
target failures and the rejected authored contacts need a numerical geometry
replay before changing impulse-normal eligibility. In particular, the current
solver deliberately rejects a new pre-existing overlap without a reliable
swept plane; simply treating its fallback direction as an impulse normal would
remove an existing guard without proving the resulting physical response.

Diagnostic videos (neither demonstrates a successful shove):

- `out/demos/20260906-013611-forge-commanded-loose-weapon-contact/raw.mp4`,
  SHA-256 `2CA22728FAC2E0A707C860BFB65D06DCE4B66EE87481BC55C63B4BDF1D17FF8B`.
- `out/demos/20260906-013759-forge-loose-weapon-contact-normal-rejection/raw.mp4`,
  SHA-256 `8FABA9763FFED960DDB9FBE560658934E1137E2237B01E5885968D9EFFC6D3CA`.
  Six sampled frames show a stationary loose target and constrained/floating
  held weapon followed by the return command; contact continuity is unproven.

The sender also exposed a parameter-binding bug: float -1.6 became
-1.600000023841858 before PowerShell's double range check and was rejected.
X/Y/Z parameters now validate as doubles before the existing float wire-format
conversion. Isolated binding checks accept all six endpoints and reject
Z=-1.6001 without contacting MCC. The live test used -1.59 after the rejection.

The harness exited normally; MCC/SteamVR stopped and the exact original normal
SteamVR settings hash was restored. This remains diagnostic evidence, not
headset acceptance or successful prop interaction.

## Numerical replay identifies lost motion history, 18:45–18:52 PDT

Capture source `0c73680918b26a42c88933e65c17b6f3329cd018`, DLL
`06409C4B9BA061B2C6841BA2BE30AF510EDE0FEB07FD7BB0A671F7387CEF0060`,
package `out/candidates/0c73680-h3-physical-contact-20260906-014500058Z`.
Build/tests passed and all installed editions were checked (only E: Steam
present). The opt-in capture publishes at most two bounded selected-shape and
transform snapshots from simulation to the cold logger, with no I/O in the
contact callback. Normal headset behavior is unchanged by the capture.

Normal Forge Construct contact, Steam / SteamVR / Null Model Number, fresh
region off. Result: `out/debug-openxr/20260906-014522304Z-controller-contact-result.json`.
Log SHA-256 `6BA2E42421E62C298BD7EFC07807C52D5357ECEF6C3D4D65A289B5422038B33E`.
The live target was the upright sniper rifle E29C002D at
(4.291724, 6.488910, 11.641370). Hand placement and repeated gentle pushes
produced three native impulses, zero melee, and eighteen rejected normals.
This establishes some impulse delivery, not a polished or repeatable shove.
The 40-second repeat recording is
`out/demos/20260906-015004-forge-sniper-contact-replay/raw.mp4`, SHA-256
`81BE9D53A87C5F57A15E6250D89D9BEAA507BBC4A2719FF5F514C24FCB9062F1`.

`tools/extract-h3-contact-replay.py` extracts only complete bounded captures,
rejects missing vertices, emits JSON and a standalone CMake/C++ replay, and
restricts generated files to out/. It has no process access. Both captures
use a 20-vertex weapon convex and 44-vertex target convex (source=1), so they
came from the solid-overlap fallback, not a selected triangle-surface pair.
The preserved JSON is `out/debug-openxr/replay-0c73680/capture.json`, SHA-256
`ED170A832D468915F21A5E867F620B983CC90B0C7AE4D148E38658D43F1201BF`.

The standalone replay proved that BOTH prior poses were clear and both
intended poses overlapped. Sweeping the recorded pair returned reliable
normals, at fractions 0.597265601 and 0.695703149, approximately
(0.9075, 0.3930, 0.1481). The live fallback instead reported fraction=0 and
unreliable because it tested intended-to-intended. Thus these two rejections
were not actually pre-existing overlap: the fallback discarded motion history.
This does not retrospectively prove the same cause for the earlier Brute
weapon rejection, whose geometry was not captured.

The following candidate corrects only the two Halo 3 solid-overlap fallbacks.
It retains the existing current-overlap gate and selected convex child pair,
then sweeps that pair from the actual previous transform. Only a reliable
swept plane replaces the static fallback result. Truly pre-existing overlap
retains its unreliable normal; clear end poses do not gain admission from a
swept convex proxy. Exact captured fixtures in
`tests/fixtures/h3_solid_overlap_replay.h` cover both failures, genuine initial
overlap and clear-end/pass-through exclusions. Live acceptance of the fix is
still required; the capture run above predates the behavioral change.

After the capture run, MCC/SteamVR were stopped and the original normal
SteamVR settings hash was independently verified restored. Accepted pointer
unchanged.

Behavioral candidate `4ea27bfc3d1e2522a5f2d3ceee5bd80fef11a031` passed the
complete Release build/core tests (including both numerical captures and all
initial-overlap/clear-end controls) and the Reach consistency check. Package:
`out/candidates/4ea27bf-h3-physical-contact-20260906-015556881Z`. Installed DLL
SHA-256 `D277E687282D9301DB018B4FBEAF3436C05E8904630BE55517705D6010F2B646`,
independently verified. Launcher remains
`D489C5763E21FC339999DC734CED09A2068AAC6C035EA8B7BF4339B6810FA450`.
This is a candidate result; the prior diagnostic video predates this fix.

The installed fix passed the live synthetic weapon-scoop regression in Forge,
Steam / SteamVR / Null Model Number. It recorded 153 applied native impulses,
zero melee and zero unreliable-normal rejections; command=applied=153,
commandStatus=2. The fixture also reported validated=1. Result:
`out/debug-openxr/20260906-015611566Z-weapon-scoop-result.json`; log SHA-256
`47066D193653159106A90DF20D9B1FDC5AB4D6AA997A24AABE582BB987926A88`.
This confirms the broader synthetic impulse path still operates. It is not a
same-pose visible A/B of the sniper contact, nor Campaign or headset acceptance.
After normal harness completion, the installed DLL/configuration hashes were
unchanged, the original normal SteamVR hash matched, null was disabled,
forcedDriver empty, requireHmd true, and MCC/SteamVR were stopped.

## Free-space recovery latch, September 5, 19:00–19:05 PDT

Normal-controller run on installed 4ea27bf, Steam / SteamVR / Null Model Number,
fresh region off: `out/debug-openxr/20260906-015844877Z-controller-contact-result.json`.
Log SHA-256 `069599686AAA09F3E168D3CACDCFB1BE3DA17CFC69EFD3E74378F14DA70B8BCC`.
This run did not establish a normal prop shove. The nearby equipment disappeared
during ordinary approach and its icon appeared on the HUD, consistent with auto
pickup. No successful interaction video is claimed from this attempt.

It did record a distinct recovery problem. At 19:02:01, the stale final root
was (0.671679, -2.349108, 11.173552), while the tracked root had moved to
(0.226826, -2.443632, 11.211394), scale 0.33. The event had corrected=0,
finalGuard=0/1, target=FFFFFFFF and consumedOffset=(0,0,0); wallBlocks and
contact hits were zero. Recovery restored the tracked palette, but subsequent
logs remained awaitingMotion=1 and stage=visible-pose while the hand was lowered
in open space. Code unconditionally required retreat/rotation after every leash
reset, including uncorrected stale poses with no obstacle direction.

The next candidate distinguishes only this known uncorrected case. Displayed
correction, candidate correction, a required target guard, missing/nonfinite
offset provenance, or a nonzero consumed correction all retain retreat. Only
when all are absent may normal contact sampling resume after the existing
500 ms cooldown. The awaiting flag stays set during publication and cooldown;
the renderer clears it only after both the reference timestamp plus 500 ms
and the published cooldown expire. Collision/approval checks then run normally;
this does not grant an unqueried pose approval or enable the fresh-region
experiment. The event logger records retreatRequired for verification.

This is separate from the swept-normal fix. It does not resolve the cause of
stale approvals, and obstructed recovery behavior remains under headset review.

Candidate `9988fa3867186fd908dedf2795af1911e9f2440a` passed the full Release
build/core tests and was installed from
`out/candidates/9988fa3-h3-physical-contact-20260906-021013195Z`. Independently
verified DLL SHA-256:
`0205A2C43D091E6834F0032342BE951C793E4C3F9BBF1A1487F2B8A7D6677A14`.
The installer checked every configured edition; only E: Steam was present.

Forge Construct free-space regression, Steam / SteamVR OpenXR 2.17.8 /
Null Model Number: `out/debug-openxr/20260906-021258024Z-controller-contact-result.json`.
Log SHA-256 `7819F369A9C7A3F43E601FAF9D05CFE8D4709145EB02AD1F234D844F711E3BCB`.
The opt-in diagnostic injected one 40 cm final-palette separation; the production
30 cm leash caught it. Event index 0 records corrected=0, finalGuard=0/1,
zero consumed offset and retreatRequired=0. No controller-path or player-motion
commands were sent after entering Forge. By the first cold event report,
awaitingMotion was already zero and normal sweeps had resumed. Over the next
32 seconds the reset count stayed at one, leash checks rose from 13 to 5,819,
and sweeps rose from 12 to 1,948, with missingPose=0. This independently verifies
continued sampling after recovery; the harness's generic pass alone only
checks that a reset occurred. Cold log cadence does not measure exact 500 ms
rearm timing. There were no contact hits, impulses or melee in this stationary
free-space test; it is not an interaction demo or headset acceptance.

The harness completed normally. MCC and SteamVR were independently confirmed
closed; normal SteamVR settings restored exactly to SHA-256
`175C79EDD6BBD58D1B7638BFFF7AAFF784625710BBEBE2A574BE76E4AC8BA89E`, with null
disabled, forcedDriver empty and requireHmd true. Existing game configuration
remained `C570089F47A17AE8645310C02688CA1454E1A02C9239BC24C5CC316E4DA94946`.
The accepted pointer remains unchanged.
