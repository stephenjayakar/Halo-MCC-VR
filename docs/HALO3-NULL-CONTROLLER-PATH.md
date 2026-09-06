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
