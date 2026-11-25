"""Create a scoped 13-control independent Test Lab P65/P67 plan.

Expected values come from PRODUCT-CONTRACT.md, Parameters.hpp and the
owner-approved 960x576 GUI geometry; never read the plugin under test to
derive the oracle. The Lab drives actual native gestures in a distinct host.
This is declared GUI parameter coverage only, not full product approval.
"""
import argparse
import hashlib
import json
from pathlib import Path

def digest(p):
    return hashlib.sha256(p.read_bytes()).hexdigest()

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--plugin", type=Path, required=True)
    parser.add_argument("--gui-host", type=Path, required=True)
    parser.add_argument("--out", type=Path, required=True)
    args = parser.parse_args()
    bundle = args.plugin.resolve(strict=True)
    module = (bundle / "Contents/x86_64-win/125A Chopperator.vst3").resolve(strict=True)
    gui = args.gui_host.resolve(strict=True)
    sha = digest(module)
    product = {"binary": str(module), "sha256": sha}
    cases, features = [], []

    def case(name, title, pid, start, steps):
        cases.append({"id": name, "kind": "gui_workflow", "contract": {
            "schema": "125a-gui-workflow-v1", "product": product, "gui_host": str(gui),
            "initial": [{"param_id": pid, "normalized": start}],
            "steps": steps}})
        features.append({"id": name, "title": title, "criticality": "required",
                         "case_ids": [name], "waiver": None})

    def action(name, pid, x, y, before, after, dy=0):
        result = {"id": name, "action": "drag" if dy else "click",
                  "x": x, "y": y, "param_id": pid,
                  "before_normalized": before, "expected_normalized": after}
        if dy:
            result.update(end_x=x, end_y=y-dy)
        return result

    # 150 logical pixels full span for note lists and continuous controls;
    # LENGTH has 65 intervals at 3 logical pixels per step. Each direction
    # starts on the same knob center, avoiding off-control click ambiguity.
    knobs = [
        ("start", "START note division", 100, 138, 314, 1/6, 2/6, 25),
        ("mid", "MID note division", 101, 366, 314, 3/6, 4/6, 25),
        ("end", "END note division", 102, 594, 314, 2/6, 3/6, 25),
        ("length", "LENGTH increment", 103, 822, 314, 0, 1/65, 3),
        ("position", "MID POSITION", 104, 100, 468, .5, .7, 30),
        ("curve", "CURVE", 105, 252, 468, .5, .7, 30),
        ("width", "WIDTH", 107, 404, 468, .5, .3, -30),
        ("attack", "ATTACK", 108, 556, 468, .02, .22, 30),
        ("release", "RELEASE", 109, 708, 468, .05, .25, 30),
        ("mix", "MIX", 110, 860, 468, .8, .6, -30),
    ]
    for name, title, pid, x, y, before, after, dy in knobs:
        case(name, title + " native bidirectional drag", pid, before, [
            action("forward", pid, x, y, before, after, dy),
            action("reverse", pid, x, y, after, before, -dy)])
    # Five equal segments in [24,936], centers derived independently
    # from the approved UIDESC geometry and ordered shape names.
    case("shape", "All five SHAPE choices", 106, 0, [
        action("saw_up", 106, 298, 546, 0, .25),
        action("saw_down", 106, 480, 546, .25, .5),
        action("triangle", 106, 662, 546, .5, .75),
        action("sine", 106, 845, 546, .75, 1),
        action("rectangle", 106, 115, 546, 1, 0)])
    case("active", "ACTIVE ON and OFF", 111, 0, [
        action("on", 111, 650, 34, 0, 1),
        action("off", 111, 573, 34, 1, 0)])
    case("mode", "ONE SHOT and LOOP", 112, 0, [
        action("loop", 112, 920, 34, 0, 1),
        action("one_shot", 112, 760, 34, 1, 0)])
    plan = {"schema": "125a-product-test-plan-v1",
            "product": {"id": "chopperator_gui_controls", "module": str(module), "sha256": sha},
            "hosts": {"gui": {"path": str(gui), "sha256": digest(gui)}}, "cases": cases}
    matrix = {"schema": "125a-product-feature-matrix-v1",
              "product_id": "chopperator_gui_controls", "module_sha256": sha,
              "features": features}
    assert len(cases) == len(features) == 13
    args.out.mkdir(parents=True, exist_ok=True)
    for name, value in [("approved-plan.json", plan), ("features.json", matrix)]:
        (args.out / name).write_text(json.dumps(value, indent=2) + "\n", encoding="utf-8")

if __name__ == "__main__":
    main()
