"""Archive and verify the exact staged candidate, then extract to an isolated directory.
Not a clean Windows installation or DAW acceptance claim.
"""
import argparse
import hashlib
import json
from pathlib import Path
import zipfile

BUNDLE = "125A Chopperator.vst3"
REQUIRED = {
    "Contents/x86_64-win/125A Chopperator.vst3",
    "Contents/Resources/Steinberg-VST3-LICENSE.txt",
    "Contents/Resources/VSTGUI-LICENSE.txt",
    "Contents/Resources/chopper.uidesc",
    "Contents/Resources/moduleinfo.json",
    "Contents/Resources/logo.png",
    "Contents/Resources/logo_3x.png",
    *("Contents/Resources/knob_" + size + "_" + variant + ".png"
      for size in ("L", "S") for variant in ("0.5x", "0.75x", "200", "1.5x")),
}
def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()

def main():
    p = argparse.ArgumentParser(description=__doc__)
    for name in ("root", "out", "extract", "evidence", "sdk"):
        p.add_argument("--"+name, type=Path, required=True)
    a = p.parse_args()
    root = a.root.resolve(strict=True)
    bundle = root / BUNDLE
    if not bundle.is_dir() or set(root.iterdir()) != {bundle}:
        raise ValueError("expected exactly one staged VST3 bundle")
    files = {f.relative_to(root).as_posix(): sha(f)
             for f in bundle.rglob("*") if f.is_file()}
    if not {BUNDLE+"/"+r for r in REQUIRED} <= files.keys():
        raise ValueError("missing mandatory bundle resource or notice")
    # Pinned Steinberg SDK adds these two Explorer icon metadata files
    # at bundle root (SMTG_AddSMTGLibrary.cmake). Accept only the exact
    # upstream pair, not arbitrary root entries or modified icon metadata.
    template = a.sdk.resolve(strict=True) / "cmake/templates"
    root_expected = {
        BUNDLE+"/PlugIn.ico": sha(template / "VST_Logo_Steinberg.ico"),
        BUNDLE+"/desktop.ini": sha(template / "desktop.ini.in"),
    }
    expected = {BUNDLE+"/"+r for r in REQUIRED} | root_expected.keys()
    if files.keys() != expected:
        raise ValueError("bundle inventory mismatch: missing=" + repr(sorted(expected-files.keys()))
                         + "; unexpected=" + repr(sorted(files.keys()-expected)))
    for name, expected_hash in root_expected.items():
        if files.get(name) != expected_hash:
            raise ValueError("missing or altered SDK folder icon metadata: " + name)
    if a.extract.exists():
        raise ValueError("extraction directory must be fresh")
    a.out.mkdir(parents=True, exist_ok=True)
    a.evidence.mkdir(parents=True, exist_ok=True)
    target = a.out / "125A-Chopperator-P009-Windows-x64-candidate.zip"
    with zipfile.ZipFile(target, "x", compression=zipfile.ZIP_DEFLATED) as z:
        for name in sorted(files):
            z.write(root / name, name)
    with zipfile.ZipFile(target) as z:
        if len(z.namelist()) != len(files) or set(z.namelist()) != files.keys() or z.testzip():
            raise ValueError("archive inventory or CRC mismatch")
        for name in files:
            if hashlib.sha256(z.read(name)).hexdigest() != files[name]:
                raise ValueError("archive content mismatch: "+name)
        z.extractall(a.extract)
    extracted = {f.relative_to(a.extract).as_posix(): sha(f)
                 for f in a.extract.rglob("*") if f.is_file()}
    if extracted != files:
        raise ValueError("extracted bytes differ")
    report = {"archive": target.name, "sha256": sha(target),
              "file_count": len(files), "files": files,
              "archive_bytes_and_fresh_extraction": "PASS",
              "clean_windows_installation": "NOT RUN",
              "commercial_daw": "NOT RUN", "release_approved": False}
    (a.evidence / "CANDIDATE-ARCHIVE-INVENTORY.json").write_text(
        json.dumps(report, indent=2)+"\n", encoding="utf-8")
    print("P009_ARCHIVE_BYTE_IDENTITY_PASS", report["sha256"], len(files))

if __name__ == "__main__":
    main()
