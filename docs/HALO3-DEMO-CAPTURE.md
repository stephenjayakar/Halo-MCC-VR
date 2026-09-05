# Visible contact demonstrations

The earlier September 5 diagnostic videos did not clearly show the requested
interactions. Their pass counters are runtime evidence, not demonstration
footage. User feedback explicitly rejected that presentation.

The next diagnostic candidate adds HALOMCCVR_H3_CONTACT_DEMO_CAMERA=1. It is
inactive without the existing explicit contact debug rig. It temporarily
frames the selected replay target for the native camera copy, restoring the
source camera immediately afterward. It uses the already evidenced source
position/forward/up fields; simulation and controller publications retain their
ordinary frame. There are no new engine bindings or normal-play changes.

tools/record-mcc-demo.ps1 captures the actual MCC window to out/demos at a
requested 30 fps with FFmpeg, temporarily exposing the MCC window above other
windows and restoring its prior topmost state. Inspect the actual footage
before calling it a demo: both the weapon and contact target must be visible,
motion and contact must be clear, and any scripted input must be labeled.

The user authorized removing obsolete captures and bulky temporary recordings.
Automatic approval review rejected the attempted cleanup with only "blocked by
policy". No deletion was performed by the agent. The user was given a scoped
PowerShell command that preserves source, builds, logs, saves, and out/demos.
