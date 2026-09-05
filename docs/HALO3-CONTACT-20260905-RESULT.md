# Halo 3 physical contact candidate — 2026-09-05

Installed runtime: `1b2a43c6949a4bf2badc60da521a47928187976f`.
Package: `out/candidates/1b2a43c-h3-physical-contact-20260905-095735594Z`.
DLL SHA-256: `E60B3D807880B2DF8F87D691F28901D9CD61F466E5F97DCE4C5A87EDEBA88473`.
Release build and core tests passed during packaging. This is a tested null-driver
candidate, **not a headset-accepted build**. CURRENT-STATE.md is unchanged.

The melee policy now honors the configured 1.50 m/s threshold for NPCs as well
as props; the old NPC override was 0.10 m/s. Slow exact contact can drive the
uniquely bound native biped motor horizontally, independently of damage. Only
living, grounded root bipeds are admitted. An NPC-shove fault disables that
feature alone. Animated contact now reads the complete bounded native node bank
instead of trying to fit a 55-node NPC into a 16-node weapon buffer.

## Runtime evidence

All sessions below used Steam MCC, Halo 3, SteamVR OpenXR 2.17.8 and the null
headset. Campaign was Sierra 117; Forge was Construct. Paths are beneath
`out/debug-openxr/`. Detailed native binding evidence, failed candidates, and
their reversions are in HALO3-AUTOMATION-EVIDENCE.md.

| Check | Result | Result JSON |
| --- | --- | --- |
| Campaign slow NPC contact | Native motor applied; zero melee; 86 snapshots retained health 1/shield 0; measurable displacement | `20260905-095758364Z-npc-shove-result.json` |
| Campaign exact slow NPC replay, 0.25 m/s | Motor applied; zero melee; follow-up health/shield unchanged | `20260905-100341144Z-npc-shove-result.json` |
| Campaign exact fast NPC replay, 2.25 m/s | 14 native melee applications; zero rejection/fault/no-damage results | `20260905-100858311Z-npc-melee-result.json` |
| Forge walls | Structure and fixed-object wall constraints validated | `20260905-101326640Z-wall-result.json` |
| Forge loose weapon | Authored geometry, dynamic constraint and native impulses observed; zero melee | `20260905-102319222Z-visible-weapon-nudge-result.json` |
| Campaign walls | Structure and fixed-object wall constraints validated | `20260905-102616203Z-wall-result.json` |
| Campaign loose weapon, Sierra 117 | **Not exercised**: no eligible target at the checkpoint; timed out, not a pass | `20260905-103000713Z-visible-weapon-nudge-result.json` |
| Campaign loose weapon, Floodgate | Passed: native impulses and body constraints, zero melee, zero recorded render separation failures | `20260905-104944313Z-visible-weapon-nudge-result.json` |

The NPC motor-only experiment separately measured 0.17294 m movement against
0.00043 m baseline drift without health/shield loss. Its result is supporting
native-function evidence, not ordinary weapon-contact acceptance.

## Local recordings

These are actual MCC window captures encoded with measured timestamps. Capture
throughput was approximately 3–4 fps; requesting 12 fps did not achieve 12 fps.
Each directory contains the MP4, original frames, timestamps, and SHA-256 JSON.

| Recording directory under out/debug-openxr | Duration | What it supports |
| --- | --- | --- |
| `20260905-102148010Z-1b2a43c-forge-wall-regression` | 29.99 s | Forge wall diagnostic; weapon visible |
| `20260905-102836137Z-1b2a43c-campaign-wall` | 29.99 s | Campaign wall diagnostic; weapon visible |
| `20260905-102445378Z-1b2a43c-forge-nudge-regression` | 29.82 s | Forge nudge runtime; replayed gun poorly framed/out of view |
| `20260905-100026603Z-1b2a43c-campaign-npc-shove` | 29.99 s | Slow NPC contact; fixed null view poorly frames the gun |
| `20260905-100548691Z-1b2a43c-campaign-npc-exact` | 34.79 s | Exact slow NPC replay; lower-body framing |
| `20260905-101134526Z-1b2a43c-campaign-npc-melee` | 17.84 s | Fast NPC melee diagnostic |
| `20260905-105412484Z-1b2a43c-floodgate-nudge` | 29.96 s | Campaign loose-weapon nudge diagnostic |

Open `recording.mp4` inside each directory. The recordings do not establish
polished headset visuals. Exact NPC replay recorded render separation fallback
holds; Forge nudge also recorded a few unproved target fallbacks. No claim of
universal zero clipping or Alyx-equivalent feel is warranted from these tests.
Real controller tracking, haptic feel, and visual collision acceptance remain
open. Creature/giant shoving is not implemented.

## Programmatic MCC control

`tools/resume-halo3-programmatically.ps1` resumes the existing Halo 3 Campaign.
`tools/start-halo3-forge-programmatically.ps1` starts the selected Halo 3 Forge
map. Both use captured-frame OCR before each menu transition and addressed
keyboard messages; unknown pages receive no input. Both reached gameplay in
these tests. The Campaign matcher tolerates the observed ODST-to-OOST OCR error.
`tools/approach-halo3-npc.py` uses read-only object snapshots and calibrated
ordinary movement inputs to approach a live NPC. It requires an explicitly
observed object-table address and stops on a blocked path or changed player.

The native engine queue executed the small tested letterbox commands, but an
object/list HaloScript experiment crashed in native script allocation. The
tool is restricted to the proven letterbox commands. Arbitrary direct scenario
loading remains unproven; do not describe menu automation as a direct-load API.

## Restored machine state

MCC is closed. SteamVR is stopped with the original real-headset settings
restored byte-for-byte: SHA-256
`82A1433FF3239C03ABB3FE0A310791C7ACE945188FF37ACF11908D09739A965C`.
The installed config remains unchanged, physical contact enabled, threshold
1.50 m/s; config SHA-256
`C570089F47A17AE8645310C02688CA1454E1A02C9239BC24C5CC316E4DA94946`.

The original Floodgate Campaign deepSave and header were restored from the
pre-test backup, with their original hashes verified. Test save copies and the
restoration record are in `out/test-runs/20260905-campaign-save-after-testing/`.
Restoring only deepSave/header initially left Resume absent. Restoring the
matching pre-test AceSettings/data snapshot made Resume return and successfully
loaded Floodgate. After that test, the original save/profile pair was restored
together and hash-verified; see final-restored-save-profile.json in that folder.
The prior test profile is retained as AceSettings-before-restore.dat. The UI
initially showed differing tag/completion displays. Read-only decoding of the
two ACC files (zlib JSON at byte 44) found exactly one differing JSON value:
GameData[halo3]/DifficultySelectedHash was 2 before testing and 1 during testing
(Heroic versus Normal). All completion records and customization values are
identical. The comparison is preserved as profile-comparison.json in that
folder. No profile progress was removed from the saved data. Both original
binary snapshots remain available; neither was reconstructed or re-encoded.
Steam is the only present MCC edition; no Store installation was found.

The next headset session can be preserved with
`tools/run-h3-contact-headset-session.ps1`; its analyzer now includes native NPC
shove counters alongside wall, melee, visibility, aiming, and pickup evidence.
Its VerifyOnly check passed against the installed candidate after restoration.
