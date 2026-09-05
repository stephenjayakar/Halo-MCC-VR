"""Encode a recorded MCC window sequence, preserving its measured timing.

Requires imageio-ffmpeg (or --ffmpeg). Input comes from
capture-mcc-window-sequence.ps1. This encodes captured frames, not a simulation.
"""
import argparse
import datetime as dt
import hashlib
import json
import re
from pathlib import Path
import subprocess


def timestamp(value):
    # PowerShell emits seven fractional digits; Python 3.10 accepts six.
    value = re.sub(r"(\.\d{6})\d+", r"\1", value)
    return dt.datetime.fromisoformat(value.replace("Z", "+00:00"))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("sequence", type=Path)
    parser.add_argument("--ffmpeg", type=Path)
    args = parser.parse_args()
    root = args.sequence.resolve(strict=True)
    manifest = json.loads((root / "manifest.json").read_text(encoding="utf-8-sig"))
    frames = manifest["frames"]
    if len(frames) < 2:
        raise ValueError("At least two captured frames are required")
    concat = []
    total = 0.0
    for index, frame in enumerate(frames):
        name = frame["file"]
        path = (root / name).resolve(strict=True)
        if path.parent != root or "'" in name or "\n" in name:
            raise ValueError("Invalid frame path")
        start = timestamp(frame["utc"])
        end_text = (frames[index + 1]["utc"] if index + 1 < len(frames)
                    else manifest["completed_utc"])
        end = timestamp(end_text)
        duration = (end - start).total_seconds()
        if not 0 < duration <= 60:
            raise ValueError("Non-monotonic or implausible capture timestamps")
        total += duration
        concat.extend([f"file '{name}'", f"duration {duration:.6f}"])
    concat.append(f"file '{frames[-1]['file']}'")
    listing = root / "frames.ffconcat"
    listing.write_text("ffconcat version 1.0\n" + "\n".join(concat) + "\n", encoding="utf-8")
    if args.ffmpeg:
        ffmpeg = str(args.ffmpeg.resolve(strict=True))
    else:
        import imageio_ffmpeg
        ffmpeg = imageio_ffmpeg.get_ffmpeg_exe()
    video = root / "recording.mp4"
    subprocess.run([ffmpeg, "-hide_banner", "-loglevel", "error", "-n",
                    "-f", "concat", "-safe", "1", "-i", str(listing),
                    "-vf", "pad=ceil(iw/2)*2:ceil(ih/2)*2", "-fps_mode", "vfr",
                    "-c:v", "libx264", "-preset", "fast", "-crf", "20",
                    "-pix_fmt", "yuv420p", "-movflags", "+faststart", str(video)], check=True)
    result = {"video": str(video), "frames_captured": len(frames),
              "measured_duration_seconds": total,
              "sha256": hashlib.sha256(video.read_bytes()).hexdigest(),
              "source": "MCC window capture; measured timestamps; no generated imagery",
              "headset_acceptance": False}
    (root / "recording.json").write_text(json.dumps(result, indent=2) + "\n")
    print(json.dumps(result, indent=2))


if __name__ == "__main__":
    main()
