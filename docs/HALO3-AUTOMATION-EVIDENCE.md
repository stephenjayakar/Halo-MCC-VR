# Halo 3 autonomous test control

## 2026-09-05 retail native command queue

`tools/mcc-engine-control.py` is read-only by default. With `--script`, it
submits one ASCII HaloScript expression to the game's existing command queue.
This is separate from the injected VR DLL and does not change game files.
Command submission requires the current process's log to identify a null-driver
physical-contact diagnostic session. Do not change titles during submission.

The research leads were the primary implementations in
[AlphaRing CGameEngine.h](https://github.com/WinterSquire/AlphaRing/blob/master/src/mcc/CGameEngine.h)
and [HaloCheckpointManager](https://github.com/Burnt-o/HaloCheckpointManager).
Neither a copied address nor an assumed vtable slot is used for admission.

Pinned retail `halo3.dll` SHA-256:
`B209D8454B12DC77E54CCD2C9924EC8D44B8619D21CF98E36FFAF601E67EFB63`.
Offline inspection of its exported `CreateGameEngine` factory (RVA `0xC2EC`)
shows the `0x460` allocation, vtable publication (`0x761B40`), and publication
of that same object to the module singleton (`0xA3CA90`). The uniquely matched
command producer is RVA `0xBF68`, referenced by vtable slot 9. Its instructions
obtain a queue entry, duplicate the supplied string, and publish the entry.
The helper derives those references, requires unique matches, and compares
the live factory, command bytes, and vtable with the pinned image.

On 2026-09-05, installed candidate `abbb017241ad642daabd52afc9343709df122f66`
was tested in Steam MCC, Halo 3 Forge, SteamVR OpenXR 2.17.8 with the null
headset. PID 22744's live singleton was `0x234F94B38E0`. The commands
`(cinematic_show_letterbox_immediate true)` and then
`(cinematic_show_letterbox_immediate false)` were submitted with the `HS: `
prefix. Window captures confirmed black letterbox bars appeared and then
cleared. This verifies these commands executed in retail, beyond merely
returning from the producer. The tool deliberately reports
`script_executed: false`: it cannot infer arbitrary command success from queue
submission alone.

`tools/send-mcc-keys.ps1 -Background` separately posts addressed key messages
to MCC's main window. It successfully traversed Creative, Forge, Halo 3, map
selection and Start without relying on foreground focus. It remains menu
navigation; it is not a direct scenario loader.

Direct arbitrary retail scenario loading is **not established**. The official
[H3EK quickstart](https://learn.microsoft.com/en-us/halo-master-chief-collection/h3/quickstart/process/step8)
documents `game_start` via `init.txt` for the editing-kit executable. This is
not evidence that retail MCC accepts that startup mechanism. Do not apply
editing-kit startup files to the retail game based on that documentation.

### Campaign object-script failure and isolation

The second Crow's Nest null-driver run reached gameplay, but the player-unit
getter and both validated TLS object-table copies reported local unit
`0xE3DE016F` with slot `-1` and all four weapon handles `FFFFFFFF`. The contact
worker correctly stayed at its held-weapon gate. The HUD silhouette alone was
not evidence of a held weapon. `tools/probe-h3-objects.py` exposes these read-only
snapshots, requires a pinned image, validates the native table header, and
requires an explicit table selection when more than one game-state copy exists.

An external `(object_set_velocity (list_get ...) ...)` experiment subsequently
crashed. The native exception dump in
`out/test-runs/20260905-campaign-hang/crash_report/minidump.dmp` records a null
read at `halo3+0x9B710`, with the interrupted stack inside script evaluation.
Script descriptors on that stack resolve to `list_get` (`0x7EED00`) and
`object_set_velocity` (`0x7EDBA0`). The faulting native routine accesses a
thread-context allocation table. No character movement was established.
The later apparent hang was the game writing its large crash dump.

The external helper now admits only the observed letterbox boolean toggle.
Object/list expressions remain disabled while simulation-context control is
investigated. An enqueued command still must not be reported as executed.
Campaign saves were copied with verified hashes before mission selection to
`out/test-runs/20260905-campaign-save-backup`; that backup preserves the user's
previous Floodgate progress for restoration after testing.

### Native biped motor probe provenance

The official H3EK string `biped_accelerate` has its named dispatch record at
`0x18E0D38`, whose callback `0xB20460` passes the vector at argument `+0xC` to
`0xB21690`. In the pinned retail image the corresponding named dispatch record
is `0x8B4E98`; callback `0x3DF6C0` has the same argument adjustment and calls
`0x3DF244`. Both implementations mark the character physics acceleration state
at object `+0x4DC` and enter the native object acceleration path. The runtime
probe matches a unique instruction signature at `0x3DF244`; it does not bind by
the recorded address. Its signature was checked against the pinned file.

The diagnostic's living-target and damage readbacks follow official H3EK
`unit_get_health`: script wrapper `0x7B92A0` -> `0xAE4F40` -> `0xAD7540`, which
returns zero for object damage flag bit 2 at `+0x110`, otherwise reads health at
`+0xF4`. The shield sibling at `0xAD75C0` reads `+0xF8`. Pinned retail wrappers
`0x1E3364` and `0x1E33C0` independently confirm these fields and the flag test.

`HALOMCCVR_H3_CONTACT_DEBUG_NPC_SHOVE=1` enables an isolated simulation-callback
probe: nearest live root biped excluding the player, three seconds of baseline,
at most twelve horizontal 0.15 m/s-vector pulses, then three seconds of
observation. Pulses stop above 0.6 m/s horizontal native speed. It monitors
health/shield loss and disables only the probe if either falls or a native call
faults. This is an experiment to establish motor behavior; no production NPC
weapon shove is enabled by this scaffolding.

Candidate `995fca7`'s sparse-pulse experiment completed without a fault. Target
`E466008C` had character mode 1 (ground), health 1, shield 0; twelve native calls
each changed projected velocity from approximately zero to 0.06839 m/s. Baseline
drift was 0.00074 m and total measured pulse displacement was 0.01143 m, below
the diagnostic's 0.02 m movement requirement. Health and shield loss remained
zero. The sparse-pulse behavior is disabled before changing the cadence.

Its real recording is
`out/debug-openxr/20260905-090938315Z-995fca7-campaign-npc-motor/recording.mp4`,
92 frames over 24.913963 seconds, SHA-256
`A4B74EA2F256FE9623B6A0A2EC2FF26C7F5FC1EA1F2591704639DA250B3A2CCC`.
The native velocity response is useful evidence, but this is not a useful
weapon shove or a headset acceptance result.

## Local runtime evidence and limits

The null-driver harness now keeps the stationary compositor awake during its
owned diagnostic session, then restores the exact original SteamVR settings.
Both completed runs restored settings hash
`82A1433FF3239C03ABB3FE0A310791C7ACE945188FF37ACF11908D09739A965C`.

- Wall run: `out/debug-openxr/20260905-075714563Z-wall-result.json` and
  corresponding log; XR visible with `shouldRender=1`, submitted layers, and
  stereo rendering. Log SHA-256
  `CAD800F2699B2C6C06AB38EAD4A1FF7F345E42A36530A518F171885299815D86`.
  Real window video:
  `out/debug-openxr/20260905-080320876Z-abbb017-forge-wall-null/recording.mp4`,
  107 frames, 29.897866 seconds, SHA-256
  `514CA545A186B6CCB21839BF6A76172CC12A845DBAA4FB66D9A888E9C539565F`.
- Nudge run: `out/debug-openxr/20260905-080827772Z-visible-weapon-nudge-result.json`
  and corresponding log, SHA-256
  `0B752EF928D65DB5776D5874BEED2E4C0F35210803A41A42CBDF3C9D7F6FE768`.
  A sample records 5,914 object impulses and zero melees. Real window video:
  `out/debug-openxr/20260905-081759385Z-abbb017-visible-nudge-null/recording.mp4`,
  106 frames, 29.858681 seconds, SHA-256
  `B616DC20449B71135C6E9F578E158E48B023BC6EE4FEC17513968DF6E6EE55FE`.
  The captured view shows hands but the weapon is out of view. This run passes
  native diagnostic counters, not visual presentation acceptance. The initial
  menu process crashed before title activity; its log and minidump are preserved
  under `out/test-runs/20260905-startup-crash`. A restarted process completed
  this run. The native command helper was first used after that restart.

Videos use actual window captures and measured frame timestamps. Neither these
videos nor null-driver passes provide headset acceptance or demonstrate NPC
shoving. `docs/CURRENT-STATE.md` remains unchanged.

### Sustained native motor result (2026-09-05)

Candidate e6dace8 changed only the opt-in probe cadence: unchanged 0.15 m/s
input, up to one call per simulation update (10 ms minimum), for three seconds.
Steam / SteamVR 2.17.8 null driver / Crow's Nest: 179 calls, 0.00043 m baseline
drift, 0.17294 m displacement, health 1 and shield 0 unchanged, no fault.
The run passed its motor-only criterion; this is not weapon-contact or headset
acceptance. Log SHA-256 A1E72BF753C0A61FE084745743D298B6425114E51CD2DA0EA9C732A3CC002DBC.
Local recording: out/debug-openxr/20260905-092104845Z-e6dace8-campaign-npc-motor/recording.mp4
(93 captured frames, 25.009272 seconds), SHA-256
A4945F58E5B8CA6D399420ADCB3E5C5ADC2F21A31139732DBAEBD4268A34DB57.

The next candidate connects this proven motor entry to exact slow biped contact.
It excludes the melee catch zone, unreliable normals, stationary/separating hands,
melee-speed swings, attached/dead objects, and stale commands. Motor input is
horizontal and bounded by the tested 0.15 m/s input and 0.60 m/s target speed.
Native calls run after objects_update, with an independent fault latch; damage
and camera ownership are untouched. This integration remains untested.
