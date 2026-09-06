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
