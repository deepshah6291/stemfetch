#!/usr/bin/env python3
"""
StemFetch backend.
Usage: python fetch_and_split.py <url> <out_dir> [model]

Requires:  pip install yt-dlp demucs
           ffmpeg on PATH
Prints one JSON object per line so the plugin can parse progress.
"""
import json
import os
import subprocess
import sys
from pathlib import Path


def emit(**kw):
    print(json.dumps(kw), flush=True)


def main():
    if len(sys.argv) < 3:
        emit(status="error", message="usage: fetch_and_split.py <url> <out_dir> [model]")
        return 2

    url, out_dir = sys.argv[1], Path(sys.argv[2])
    model = sys.argv[3] if len(sys.argv) > 3 else "htdemucs"
    out_dir.mkdir(parents=True, exist_ok=True)
    ff = Path(__file__).resolve().parent / "ffmpeg"   # bundled by the installer
    if ff.exists():
        os.environ["PATH"] = str(ff) + os.pathsep + os.environ.get("PATH", "")
    source = out_dir / "source.wav"

    # 1. Download audio only
    emit(status="downloading", progress=0.05)
    r = subprocess.run(
        [sys.executable, "-m", "yt_dlp", "--no-playlist", "-x",
         "--audio-format", "wav", "-o", str(out_dir / "source.%(ext)s"), url],
        capture_output=True, text=True)
    if r.returncode != 0 or not source.exists():
        emit(status="error", message="Download failed: " + r.stderr[-400:])
        return 1

    # 2. Split stems
    emit(status="splitting", progress=0.4)
    r = subprocess.run(
        [sys.executable, "-m", "demucs", "-n", model, "-o", str(out_dir / "stems"), str(source)],
        capture_output=True, text=True)
    if r.returncode != 0:
        emit(status="error", message="Stem split failed: " + r.stderr[-400:])
        return 1

    # 3. Report results
    stem_dir = out_dir / "stems" / model / "source"
    stems = {p.stem: str(p) for p in sorted(stem_dir.glob("*.wav"))}
    emit(status="done", progress=1.0, stems=stems)
    return 0


if __name__ == "__main__":
    sys.exit(main())
