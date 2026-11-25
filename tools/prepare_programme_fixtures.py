#!/usr/bin/env python3
"""Acquire pinned CC0 source recordings for QA, not for the shipped VST3."""
import hashlib
import json
from pathlib import Path
import struct
import subprocess
import sys
import urllib.request
import math

SOURCES = (
    ("guitar_e2", "freepats/electric-guitar-FSBS-dist2",
     "21261b8bcb02d1cbf52dc02f1b1636df52d4a947",
     "samples/bridge/E2_s1_01.flac", "13db1aafb87c7c63b110ba422e8794113fa4340e", 1592468),
    ("guitar_b3", "freepats/electric-guitar-FSBS-dist2",
     "21261b8bcb02d1cbf52dc02f1b1636df52d4a947",
     "samples/bridge/B3_s5_01.flac", "6d133194926bc31956dee5d25f54a752f1a2e518", 821975),
    ("pad_c4", "freepats/sweep-pad",
     "ed3e294fcec8c3ea3ec84471f8c7a75bc09eba04",
     "samples/C4.flac", "ff29ecb88c9e4cb7eede50e3f587a71c1896f195", 235681),
)
def sha256(data):
    return hashlib.sha256(data).hexdigest()
def git_blob(data):
    return hashlib.sha1(b"blob " + str(len(data)).encode() + b"\0" + data).hexdigest()
def get_source(folder, spec):
    name, repo, commit, path, expected, size = spec
    url = f"https://raw.githubusercontent.com/{repo}/{commit}/{path}"
    flac = folder / f"{name}.flac"
    if flac.exists():
        raw = flac.read_bytes()
    else:
        with urllib.request.urlopen(url, timeout=60) as response:
            raw = response.read(size + 1)
    if len(raw) != size or git_blob(raw) != expected:
        raise RuntimeError(f"{name}: source integrity mismatch")
    flac.write_bytes(raw)
    return name, flac, {"source": url, "git_blob": expected, "source_sha256": sha256(raw),
                        "license": "CC0-1.0", "bytes": size}
def decode(folder, name, flac, metadata):
    destination = folder / f"{name}.f64le"
    # Fixed 48 kHz, stereo f64le, no peak/loudness normalization.
    # ffmpeg standard channel mapping and resampling are explicit fixture preparation.
    subprocess.run(["ffmpeg", "-v", "error", "-nostdin", "-y", "-i", str(flac),
                    "-ar", "48000", "-ac", "2", "-f", "f64le", str(destination)], check=True)
    raw = destination.read_bytes()
    if len(raw) < 48000 * 16 or len(raw) % 16:
        raise RuntimeError(f"{name}: invalid fixture frame count")
    peak = max(abs(x[0]) for x in struct.iter_unpack("<d", raw))
    if not math.isfinite(peak) or not 0.001 < peak <= 2:
        raise RuntimeError(f"{name}: invalid peak")
    if not all(math.isfinite(x[0]) for x in struct.iter_unpack("<d", raw)):
        raise RuntimeError(f"{name}: nonfinite PCM")
    metadata.update(decoded_sha256=sha256(raw), frames=len(raw)//16, peak=peak,
                    format="interleaved stereo f64le, 48000 Hz")
    return raw
def main():
    if len(sys.argv) != 2:
        raise SystemExit("Usage: prepare_programme_fixtures.py OUTPUT_DIRECTORY")
    out = Path(sys.argv[1])
    out.mkdir(parents=True, exist_ok=True)
    manifest = {"ffmpeg": subprocess.check_output(["ffmpeg", "-version"], text=True).splitlines()[0],
                "fixtures": {}}
    for spec in SOURCES:
        name, flac, metadata = get_source(out, spec)
        decode(out, name, flac, metadata)
        manifest["fixtures"][name] = metadata
    (out / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(manifest, indent=2))
if __name__ == "__main__":
    main()
