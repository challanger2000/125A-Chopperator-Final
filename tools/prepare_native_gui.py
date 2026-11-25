#!/usr/bin/env python3
"""Build native GUI resources from the exact reviewed 125A source assets."""
from pathlib import Path
import sys
import re
import xml.etree.ElementTree as ET
from PIL import Image, ImageDraw, ImageChops
from prepare_gui_design import main as draft, STRIPS

def main():
    if len(sys.argv)!=2: raise SystemExit("Usage: prepare_native_gui.py OUTPUT")
    out=Path(sys.argv[1]).resolve()
    draft() # verifies source hashes, strip dimensions, and logo identity
    # Crop transparent padding only, preserve all nontransparent pixels,
    # retain each original resolution, tile to avoid tall GPU textures.
    for family,size,scale,sha in STRIPS:
        raw=out/f"125A_MixEngine_Analog_{family}_{size}px_{scale}pct_128f.png"
        strip=Image.open(raw).convert("RGBA")
        half=size//2; pad=size//4
        atlas=Image.new("RGBA",(half*16,half*8))
        for i in range(128):
            frame=strip.crop((0,i*size,size,(i+1)*size))
            crop=frame.crop((pad,pad,pad+half,pad+half))
            rebuilt=Image.new("RGBA",(size,size));rebuilt.paste(crop,(pad,pad))
            if ImageChops.difference(frame.getchannel("A"),rebuilt.getchannel("A")).getbbox():
                raise RuntimeError(f"Crop clips original visible pixels: {family} {scale} {i}")
            atlas.paste(crop,((i%16)*half,(i//16)*half))
        suffix={100:"0.5x",150:"0.75x",200:"200",300:"1.5x"}[scale]
        name=f"knob_{family}_{suffix}.png"
        atlas.save(out/name)
        check=Image.open(out/name).convert("RGBA")
        for i in range(128):
            x=(i%16)*half;y=(i//16)*half
            actual=check.crop((x,y,x+half,y+half))
            expected=strip.crop((pad,i*size+pad,pad+half,i*size+pad+half))
            if actual.tobytes()!=expected.tobytes(): raise RuntimeError("Atlas pixel mismatch")
    # The exact master contains only M/L/Z polygons; use even-odd fill.
    # Rasterize geometry at 3x logical display size and supersample 4x.
    root=ET.parse(out/"125A_Logo_Master_FINAL.svg").getroot()
    w,h=1008,504
    logo=Image.new("RGBA",(w,h))
    for path in root.findall(".//{http://www.w3.org/2000/svg}path"):
        tokens=re.findall(r"[MLZ]|-?\d+(?:\.\d+)?",path.attrib["d"])
        if re.search(r"[^MLZ0-9.,\s-]",path.attrib["d"]): raise RuntimeError("Unrecognized master path command")
        mask=Image.new("1",(w,h));points=[];i=0
        while i<len(tokens):
            cmd=tokens[i];i+=1
            if cmd in ("M","L"):
                points.append((float(tokens[i])*w/1774,float(tokens[i+1])*h/887));i+=2
            elif cmd=="Z":
                sub=Image.new("1",(w,h));ImageDraw.Draw(sub).polygon(points,fill=1)
                mask=ImageChops.logical_xor(mask,sub);points=[]
            else: raise RuntimeError("Unexpected master path token")
        if points: raise RuntimeError("Unclosed master polygon")
        logo.paste(path.attrib["fill"],(0,0,w,h),mask)
    logo.resize((252,126),Image.Resampling.LANCZOS).save(out/"logo_3x.png")
    logo.resize((84,42),Image.Resampling.LANCZOS).save(out/"logo.png")
    print("NATIVE_GUI_ASSETS_PASS all 1024 frames verified, official logo rasterized")
if __name__=="__main__": main()
