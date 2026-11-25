import sys,zipfile
from pathlib import Path
root=Path(sys.argv[1])
target=Path(sys.argv[2])
assert root.name=="125A Chopperator.vst3"
res="Contents/Resources/"
expected={"Contents/x86_64-linux/125A Chopperator.so",res+"moduleinfo.json",res+"chopper.uidesc",res+"logo.png",res+"logo_3x.png",res+"Steinberg-VST3-LICENSE.txt",res+"VSTGUI-LICENSE.txt"}
expected|={res+f"knob_{family}_{scale}.png" for family in ("L","S") for scale in ("0.5x","0.75x","200","1.5x")}
files={p.relative_to(root).as_posix():p for p in root.rglob("*") if p.is_file()}
if set(files)!=expected:raise SystemExit(f"Linux bundle inventory mismatch: {set(files)^expected}")
module=files["Contents/x86_64-linux/125A Chopperator.so"].read_bytes()
if module[:5]!=b"\x7fELF\x02" or module[18:20]!=b"\x3e\x00":raise SystemExit("wrong ELF architecture")
with zipfile.ZipFile(target,"x",compression=zipfile.ZIP_DEFLATED) as z:
    for name,p in sorted(files.items()):
        if p.is_symlink() or not p.stat().st_size:raise SystemExit(f"invalid file {p}")
        z.write(p,root.name+"/"+name)
with zipfile.ZipFile(target) as z:
    if z.testzip() or any(z.read(root.name+"/"+n)!=p.read_bytes() for n,p in files.items()):raise SystemExit("archive mismatch")
print("LINUX_CANDIDATE_ARCHIVE_PASS",len(files))
