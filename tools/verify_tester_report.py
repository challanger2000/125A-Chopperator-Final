"""Verify pinned v0.2.10 Chopperator QA report identity and module provenance.
The mandatory labels are from the released tester's actual P009 report.
Repeated labels with distinct rates/bus directions have explicit identities.
"""
import argparse
import hashlib
import json
from pathlib import Path
import re
import tempfile

EXPECTED = frozenset([
    "Plugin path",
    "Module load",
    "Factory classes",
    "Component creation",
    "Component initialize",
    "Bus metadata - Audio Input #0",
    "Bus metadata - Audio Output #0",
    "Controller initialize",
    "Component/controller connection",
    "IComponentHandler",
    "Parameter metadata",
    "Parameter string conversion",
    "Parameter conversion stress",
    "Component state roundtrip",
    "Controller component-state sync",
    "Controller state roundtrip",
    "IAudioProcessor",
    "32-bit sample processing",
    "64-bit sample processing",
    "Processing setup matrix - 44100 Hz",
    "Processing setup matrix - 48000 Hz",
    "Processing setup matrix - 88200 Hz",
    "Processing setup matrix - 96000 Hz",
    "Processing setup matrix - 192000 Hz",
    "Processing setup summary",
    "Processing setup restore",
    "Default bus activation",
    "Audio torture lifecycle (32-bit)",
    "Audio Silence process (32-bit)",
    "Audio Impulse process (32-bit)",
    "Audio DC process (32-bit)",
    "Audio Denormal process (32-bit)",
    "Callback API tracing (32-bit)",
    "Paced realtime (32-bit)",
    "Sustained processing (32-bit)",
    "Parameter automation stress (32-bit)",
    "Bypass processing stress (32-bit)",
    "Audio torture lifecycle (64-bit)",
    "Audio Silence process (64-bit)",
    "Audio Impulse process (64-bit)",
    "Audio DC process (64-bit)",
    "Audio Denormal process (64-bit)",
    "Callback API tracing (64-bit)",
    "Paced realtime (64-bit)",
    "Sustained processing (64-bit)",
    "Actual processing matrix (32-bit)",
    "Actual processing matrix (64-bit)",
    "Lifecycle stress (32-bit)",
    "Offline processing (32-bit)",
    "Post-stress state restore",
    "Supported audio processor class",
    "Fresh-process audio matrix",
    "Reload/instantiation stress",
    "Fresh-instance state verification",
    "Editor lifecycle / GUI runtime",
    "I/O, sidechain and event isolation",
    "Steinberg VST3 validator"
])
def sha(p):
    return hashlib.sha256(p.read_bytes()).hexdigest()
def identity(line):
    s=line.removeprefix("[PASS] ")
    if s.startswith("Bus metadata - "):
        return s.split(",")[0]
    if s.startswith("Processing setup matrix - "):
        return s.split(":")[0]
    return s.split(" - ",1)[0]
def validate(text, plugin):
    lines=text.splitlines()
    for line in ("Version: 0.2.10",
                 "Source revision: ec2db65b9b9dbce2b7a5d146cb54869c9ad82e6d",
                 "Plugin: "+plugin,
                 "RESULT: 57 PASS / 0 WARNING / 0 FAIL"):
        if lines.count(line)!=1:
            raise ValueError("missing or duplicate report identity: "+line)
    if any(re.search(r"\[(FAIL|WARNING|SKIP|NOT RUN|NOT VERIFIED)\]", l) for l in lines):
        raise ValueError("nonpassing report")
    found=[identity(l) for l in lines if l.startswith("[PASS] ")]
    if len(found)!=57 or len(set(found))!=57 or set(found)!=EXPECTED:
        raise ValueError("wrong mandatory result identities: missing="+repr(sorted(EXPECTED-set(found)))+
                         "; extra="+repr(sorted(set(found)-EXPECTED)))
def selftest():
    plugin="C:/fixture/125A Chopperator.vst3"
    header="Version: 0.2.10\nSource revision: ec2db65b9b9dbce2b7a5d146cb54869c9ad82e6d\nPlugin: "+plugin+"\n"
    rows=["[PASS] "+x+" - fixture" for x in sorted(EXPECTED)]
    # qualified identities have the same delimiters as actual report
    rows=[r.replace("channels=fixture","channels=2") for r in rows]
    rows=[r.replace(" - fixture",", channels=2") if r.startswith("[PASS] Bus metadata - ") else
          r.replace(" - fixture",": 10/10 block sizes accepted") if r.startswith("[PASS] Processing setup matrix - ") else r for r in rows]
    footer="\nRESULT: 57 PASS / 0 WARNING / 0 FAIL\n"
    good=header+"\n".join(rows)+footer
    validate(good,plugin)
    for bad in ("",header+("\n[PASS] duplicate placeholder"*57)+footer,
                header+"\n".join(rows[:-1])+footer,
                header+"\n".join(rows[:-1]+[rows[0]])+footer,
                good.replace(plugin,"C:/wrong.vst3"),good+"[WARNING] unexpected\n"):
        try: validate(bad,plugin)
        except ValueError: continue
        raise AssertionError("invalid report accepted")
    print("P009_REPORT_IDENTITY_NEGATIVES_PASS")
def main():
    p=argparse.ArgumentParser()
    p.add_argument("--selftest",action="store_true")
    p.add_argument("--report",type=Path)
    p.add_argument("--plugin",type=Path)
    p.add_argument("--before")
    p.add_argument("--out",type=Path)
    a=p.parse_args()
    if a.selftest: selftest();return
    bundle=a.plugin.resolve(strict=True)
    binary=bundle/"Contents/x86_64-win/125A Chopperator.vst3"
    current=sha(binary)
    if current!=a.before.lower():raise ValueError("module changed across tester execution")
    validate(a.report.read_text(encoding="utf-8-sig"),str(bundle))
    a.out.write_text(json.dumps({"module_sha256":current,"report_sha256":sha(a.report),
        "verified_case_identities":sorted(EXPECTED),"status":"PASS"},indent=2)+"\n",encoding="utf-8")
    print("P009_REPORT_IDENTITY_PASS",current)
if __name__=="__main__":main()
