"""Exercise the actual candidate archive gate and SDK metadata validator on
isolated copies of the native staged candidate. Never modify the original."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import stat

def inventory(root):
    return {p.relative_to(root).as_posix(): hashlib.sha256(p.read_bytes()).hexdigest()
            for p in root.rglob("*") if p.is_file()}

def main():
    p=argparse.ArgumentParser(description=__doc__)
    for name in ("root","sdk","infotool","evidence"):
        p.add_argument("--"+name,type=Path,required=True)
    a=p.parse_args()
    root=a.root.resolve(strict=True)
    sdk=a.sdk.resolve(strict=True)
    info=a.infotool.resolve(strict=True)
    a.evidence.mkdir(parents=True,exist_ok=True)
    before=inventory(root)
    results=[]
    gate=Path(__file__).with_name("verify_candidate_archive.py").resolve()
    with tempfile.TemporaryDirectory() as tmp:
        base=Path(tmp)
        for name,fragment in (("missing_moduleinfo","missing mandatory bundle resource"),
                              ("extra_contents","bundle inventory mismatch"),
                              ("missing_notice","missing mandatory bundle resource"),
                              ("bad_icon","missing or altered SDK folder icon metadata")):
            folder=base/name
            copy=folder/"stage"
            shutil.copytree(root,copy,copy_function=shutil.copyfile)
            bundle=copy/"125A Chopperator.vst3"
            res=bundle/"Contents/Resources"
            if name=="missing_moduleinfo": (res/"moduleinfo.json").unlink()
            elif name=="extra_contents": (res/"unapproved.txt").write_text("fixture")
            elif name=="missing_notice": (res/"VSTGUI-LICENSE.txt").unlink()
            else:
                # Windows SDK marks folder metadata as SYSTEM. Use fresh
                # byte-only copies above, and clear protection only on the
                # isolated fixture, not the actual distributable.
                icon=bundle/"PlugIn.ico"
                if sys.platform=="win32":
                    subprocess.run(["attrib","-s","-h","-r",str(icon)],
                                   check=True,capture_output=True,timeout=10)
                icon.chmod(stat.S_IREAD|stat.S_IWRITE)
                icon.write_bytes(b"wrong icon")
                if icon.read_bytes()!=b"wrong icon":
                    raise RuntimeError("icon mutation not established")
            proc=subprocess.run([sys.executable,str(gate),"--root",str(copy),"--sdk",str(sdk),
                "--out",str(folder/"out"),"--extract",str(folder/"extract"),
                "--evidence",str(folder/"evidence")],capture_output=True,text=True,timeout=30)
            log=proc.stdout+"\n"+proc.stderr
            (a.evidence/(name+".log")).write_text(log,encoding="utf-8")
            if proc.returncode==0 or fragment not in log or (folder/"out").exists():
                raise RuntimeError("archive negative not rejected: "+name)
            results.append(name)
        # A structurally valid JSON with false class identity must fail
        # the official SDK comparison with the actual factory.
        copy=base/"metadata"/"stage"
        shutil.copytree(root,copy,copy_function=shutil.copyfile)
        bundle=copy/"125A Chopperator.vst3"
        res=bundle/"Contents/Resources/moduleinfo.json"
        # SDK JSON accepts trailing commas; change exactly the known class
        # name in the generated text without reserializing other fields.
        text=res.read_text(encoding="utf-8-sig")
        old='"Name": "125A Chopperator"'
        if old not in text: raise RuntimeError("metadata fixture anchor absent")
        offset=text.rfind(old)
        res.write_text(text[:offset]+text[offset:].replace(old,'"Name": "Invalid Chopperator"',1),encoding="utf-8")
        proc=subprocess.run([str(info),"-validate","-path",str(bundle)],capture_output=True,text=True,timeout=30)
        log=proc.stdout+"\n"+proc.stderr
        (a.evidence/"wrong_moduleinfo.log").write_text(log,encoding="utf-8")
        if proc.returncode==0 or "different" not in log.lower():
            raise RuntimeError("semantic metadata negative not rejected")
        results.append("wrong_moduleinfo")
    if inventory(root)!=before:raise RuntimeError("positive candidate changed")
    (a.evidence/"ARCHIVE-NEGATIVES.json").write_text(json.dumps({"rejected":results,
        "positive_bundle_unchanged":True},indent=2)+"\n",encoding="utf-8")
    print("P009_ARCHIVE_NEGATIVE_CONTROLS_PASS",len(results))
if __name__=="__main__":main()
