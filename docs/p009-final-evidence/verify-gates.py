"""Probe the candidate archive, Lab plan and tester report with isolated fixtures."""
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
                assert not destination.exists(), "rejected fixture emitted archive files"
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
        archive("extra-bundle-root", "bundle inventory mismatch")
        (bundle / "unexpected.txt").unlink()

        info.unlink()
        archive("missing-moduleinfo", "missing mandatory bundle resource")
        info.write_text("{}\n")
        stray = resources / "unexpected.txt"
        stray.write_text("ordinary stray file\n")
        archive("stray-resource", "bundle inventory mismatch")
        stray.unlink()

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


        # Reconstruct the original regex condition from the checked-in old
        # workflow to verify the fixture retains its original behavior.
        previous = subprocess.check_output(
            ["git", "-C", repo, "show", "aade956:.github/workflows/windows.yml"], text=True)
        count_regex = re.search(r"-Pattern '([^']+)'\).Count -lt 57", previous)[1]
        reject_regex = re.search(r"-Pattern '([^']+)' -Quiet\) \{ throw \"Tester reported", previous)[1]
        duplicate = "[PASS] duplicate placeholder"
        assert re.search(count_regex, duplicate, re.I) and not re.search(reject_regex, duplicate, re.I)
        print("Previous regex accepts 57 copies of a single PASS placeholder")

        sys.dont_write_bytecode = True
        sys.path.insert(0, str(repo / "tools"))
        from verify_tester_report import EXPECTED, validate
        plugin = str(bundle.resolve())
        header = ["Version: 0.2.10",
                  "Source revision: ec2db65b9b9dbce2b7a5d146cb54869c9ad82e6d",
                  "Plugin: " + plugin]
        footer = "RESULT: 57 PASS / 0 WARNING / 0 FAIL"
        rows = ["[PASS] " + name for name in sorted(EXPECTED)]
        assert len(rows) == 57
        report = work / "synthetic-report.txt"
        output_file = work / "report-identity.json"
        report.write_text("\n".join(header + rows + [footer]))
        command = [sys.executable, repo / "tools/verify_tester_report.py",
                   "--report", report, "--plugin", bundle, "--before", digest(module),
                   "--out", output_file]
        success = subprocess.run(command, capture_output=True, text=True)
        assert success.returncode == 0, success.stderr
        evidence = json.loads(output_file.read_text())
        assert len(evidence["verified_case_identities"]) == 57
        assert evidence["module_sha256"] == digest(module)
        assert evidence["report_sha256"] == digest(report)
        print("Tester gate accepts complete identity-only synthetic report and records both hashes")
        output_file.unlink()
        qualified = "[PASS] Processing setup matrix - 192000 Hz"
        for label, fixture, error in (
                ("repeated placeholders", header + [duplicate] * 57 + [footer], "result identities"),
                ("missing qualified case", header + [r for r in rows if r != qualified] + [footer],
                 "result identities"),
                ("wrong version", ["Version: 0.2.9"] + header[1:] + rows + [footer], "report identity"),
                ("wrong revision", [header[0], "Source revision: other", header[2]] + rows + [footer],
                 "report identity"),
                ("wrong bundle", header[:2] + ["Plugin: /other.vst3"] + rows + [footer],
                 "report identity"),
                ("warning", header + rows + ["[WARNING] unexpected", footer], "nonpassing report")):
            try:
                validate("\n".join(fixture), plugin)
            except ValueError as exc:
                assert error in str(exc), exc
                print("Tester gate rejects", label)
            else:
                raise AssertionError("accepted: " + label)
        module.write_bytes(b"modified synthetic module")
        rejection = subprocess.run(command, capture_output=True, text=True)
        assert rejection.returncode != 0 and "module changed across tester execution" in rejection.stderr
        assert not output_file.exists()
        print("Tester gate rejects module-byte change after capturing baseline SHA-256")


if __name__ == "__main__":
    main()
