"""Probe the actual P009 archive and Lab plan scripts with isolated fixtures."""
import argparse
import collections
import hashlib
import json
from pathlib import Path
import re
import shutil
import subprocess
import sys
import tempfile
import xml.etree.ElementTree as ET
import zipfile

BUNDLE = "125A Chopperator.vst3"
BASE = "Contents/Resources/"
MODULE = f"Contents/x86_64-win/{BUNDLE}"
EXPECTED_NOTICES = ["Steinberg-VST3-LICENSE.txt", "VSTGUI-LICENSE.txt"]


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("repo", type=Path)
    parser.add_argument("sdk", type=Path)
    args = parser.parse_args()
    repo, sdk = args.repo.resolve(), args.sdk.resolve()
    with tempfile.TemporaryDirectory(prefix="p009-gates-") as directory:
        work = Path(directory)
        root = work / "artifact"
        bundle = root / BUNDLE
        module = bundle / MODULE
        module.parent.mkdir(parents=True)
        module.write_bytes(b"synthetic-module-placeholder")
        resources = bundle / BASE
        resources.mkdir()
        shutil.copyfile(repo / "resources/chopper.uidesc", resources / "chopper.uidesc")
        for name in ("logo.png", "logo_3x.png", *(f"knob_{size}_{scale}.png"
                     for size in "LS" for scale in ("0.5x", "0.75x", "200", "1.5x"))):
            (resources / name).write_bytes(b"synthetic-image-placeholder")
        for name, source in zip(EXPECTED_NOTICES, ("LICENSE.txt", "vstgui4/LICENSE"), strict=True):
            shutil.copyfile(sdk / source, resources / name)
        for name, source in (("PlugIn.ico", "VST_Logo_Steinberg.ico"), ("desktop.ini", "desktop.ini.in")):
            shutil.copyfile(sdk / "cmake/templates" / source, bundle / name)
        info = resources / "moduleinfo.json"
        info.write_text("{}\n")

        def archive(label, error=None):
            destination = work / label
            result = subprocess.run(
                [sys.executable, repo / "tools/verify_candidate_archive.py",
                 "--root", root, "--out", destination, "--extract", destination / "extracted",
                 "--evidence", destination, "--sdk", sdk],
                text=True, capture_output=True,
            )
            if error:
                assert result.returncode != 0 and error in result.stderr, result
                print(label, "rejected with", error)
                return
            assert result.returncode == 0, result.stderr
            report = json.loads((destination / "CANDIDATE-ARCHIVE-INVENTORY.json").read_text())
            zpath = destination / report["archive"]
            with zipfile.ZipFile(zpath) as z:
                assert z.testzip() is None and len(z.namelist()) == report["file_count"]
                for name in z.namelist():
                    assert z.read(name) == (destination / "extracted" / name).read_bytes()
            assert digest(zpath) == report["sha256"]
            assert not report["release_approved"]
            print(label, "accepted with", report["file_count"], "files")
            return report

        positive = archive("17-file-control")
        assert positive["file_count"] == 17
        icon = bundle / "PlugIn.ico"
        icon.write_bytes(b"changed")
        archive("bad-icon", "missing or altered SDK folder icon metadata")
        shutil.copyfile(sdk / "cmake/templates/VST_Logo_Steinberg.ico", icon)
        notice = resources / EXPECTED_NOTICES[0]
        notice.unlink()
        archive("missing-notice", "missing mandatory bundle resource or notice")
        shutil.copyfile(sdk / "LICENSE.txt", notice)
        (bundle / "unexpected.txt").touch()
        archive("extra-bundle-root", "unexpected bundle layout")
        (bundle / "unexpected.txt").unlink()

        # Exact distributable layout is a separate contract from byte-preserving extraction.
        info.unlink()
        missing = archive("missing-moduleinfo")
        assert missing["file_count"] == 16
        info.write_text("{}\n")
        (resources / "unexpected.txt").write_text("ordinary stray file\n")
        extra = archive("stray-resource")
        assert extra["file_count"] == 18
        print("FAIL exact 17-file inventory: missing moduleinfo and stray Contents file accepted")

        gui_host = work / "dummy-host.exe"
        gui_host.write_bytes(b"synthetic-host-placeholder")
        subprocess.run([sys.executable, repo / "tools/prepare_lab_control_plan.py",
                        "--plugin", bundle, "--gui-host", gui_host, "--out", work / "plan"], check=True)
        plan = json.loads((work / "plan/approved-plan.json").read_text())
        matrix = json.loads((work / "plan/features.json").read_text())
        assert plan["product"]["sha256"] == matrix["module_sha256"] == digest(module)
        assert plan["hosts"]["gui"]["sha256"] == digest(gui_host)
        assert len(plan["cases"]) == len(matrix["features"]) == 13
        assert all(x["criticality"] == "required" and x["case_ids"] == [x["id"]] and x["waiver"] is None
                   for x in matrix["features"])
        uid = ET.parse(repo / "resources/chopper.uidesc").getroot()
        tags = {tag.attrib["name"]: int(tag.attrib["tag"]) for tag in uid.findall("control-tags/control-tag")}
        rectangles = {}
        for view in uid.find("template"):
            if view.attrib["class"] not in ("CAnimKnob", "CSegmentButton"):
                continue
            pid = tags[view.attrib["control-tag"]]
            rectangles[pid] = ([int(v) for v in view.attrib["origin"].split(",")],
                               [int(v) for v in view.attrib["size"].split(",")])
        all_ids = set()
        actions = collections.Counter()
        for case in plan["cases"]:
            initial, = case["contract"]["initial"]
            pid = initial["param_id"]
            all_ids.add(pid)
            before = initial["normalized"]
            (x, y), (w, h) = rectangles[pid]
            for step in case["contract"]["steps"]:
                assert step["param_id"] == pid and step["before_normalized"] == before
                assert x <= step["x"] < x+w and y <= step["y"] < y+h
                before = step["expected_normalized"]
                actions[step["action"]] += 1
            assert before == initial["normalized"]
        assert all_ids == set(range(100, 113))
        assert actions == {"drag": 20, "click": 9}
        print("PASS scoped Lab plan: 13 parameter IDs, 29 geometry-checked actions and both hashes")

        # Exercise the actual current workflow regexes against a crafted benign report.
        workflow = (repo / ".github/workflows/windows.yml").read_text()
        reject = re.search(r"-Pattern '([^']+)' -Quiet\) \{ throw \"Tester reported", workflow)[1]
        count = re.search(r"-Pattern '([^']+)'\).Count -lt 57", workflow)[1]
        tester_gate = lambda rows: not any(re.search(reject, row, re.I) for row in rows) and sum(
            bool(re.search(count, row, re.I)) for row in rows) >= 57
        assert not tester_gate([])
        assert tester_gate(["[PASS] duplicate placeholder"] * 57)
        print("FAIL test identity in report gate: 57 duplicates satisfy both workflow regex checks")

        # Algebraic counterexample for the current paced gate, no module substitution.
        samples = [1., -.5, .125, -.25]
        output = [.5*x for x in samples]
        inp = sum(x*x for x in samples)
        wet = sum(y*y for y in output)/inp
        changed = sum((x-y)**2 for x,y in zip(samples,output,strict=True))/inp
        assert wet==changed==.25 and wet>.001 and changed>.001
        print("FAIL paced energy gate discrimination: constant x0.5 passes both ratios at", wet)


if __name__ == "__main__":
    main()
