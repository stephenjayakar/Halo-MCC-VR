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
