#!/usr/bin/env python3
"""Build scoped Chopperator graphite controls at every original native scale."""
from pathlib import Path
import sys
import re
import math
import io
import base64
import hashlib
import xml.etree.ElementTree as ET
from PIL import Image, ImageDraw, ImageChops

def knob_base(size):
    n=size*4
    im=Image.new('RGBA',(n,n))
    p=im.load()
    for y in range(n):
        for x in range(n):
            dx=(x+.5-n/2)/(n/2);dy=(y+.5-n/2)/(n/2)
            r=math.hypot(dx,dy);a=math.atan2(dy,dx)
            if r>.91:
                rs=math.hypot(dx,dy-.035)
                alpha=round(95*max(0,1-(rs/.99)**12)) if rs<.99 else 0
                p[x,y]=(0,0,0,alpha);continue
            light=-.70*dy-.30*dx
            if r>.87: v=12+12*(light+1)
            elif r>.83: v=70+38*light
            elif r>.79: v=17+10*light
            elif r>.68:
                flute=(.5+.5*math.cos(56*a))**3
                v=25+22*flute+17*light
            elif r>.63: v=10+11*(light+1)
            elif r>.59: v=68+30*light
            else:
                grain=.7*math.sin(95*r)+.9*math.sin(271*r+1.7*math.sin(a*7))
                v=42+29*light+grain+4*(1-r/.59)
            v=max(0,min(255,round(v)))
            p[x,y]=(v,min(v+2,255),min(v+4,255),255)
    return im

def knob_frame(base,index,size):
    im=base.copy();n=im.width;c=n/2;radius=n/2
    angle=math.radians(135+270*index/127)
    def pos(r):return (c+radius*r*math.cos(angle),c+radius*r*math.sin(angle))
    d=ImageDraw.Draw(im)
    d.line([pos(.25),pos(.56)],fill=(8,9,10,255),width=max(2,round(n*.052)))
    d.line([pos(.25),pos(.56)],fill=(244,151,45,255),width=max(2,round(n*.031)))
    return im.resize((size,size),Image.Resampling.LANCZOS)

def marker_matches(frame,index):
    size=frame.width;dx=dy=0.
    for y in range(size):
        for x in range(size):
            r,g,b,a=frame.getpixel((x,y))
            if r>120 and r>g*1.3 and g>b*1.8 and a>180:
                dx+=x+.5-size/2;dy+=y+.5-size/2
    angle=math.radians(135+270*index/127)
    norm=math.hypot(dx,dy)
    return norm>0 and (dx*math.cos(angle)+dy*math.sin(angle))/norm>.97

def main():
    if len(sys.argv)!=2: raise SystemExit("Usage: prepare_native_gui.py OUTPUT")
    out=Path(sys.argv[1]).resolve();out.mkdir(parents=True,exist_ok=True)
    master=(Path(__file__).resolve().parent.parent/"resources"/"125A_Logo_Master_FINAL.svg").read_bytes()
    if hashlib.sha1(b"blob "+str(len(master)).encode()+b"\0"+master).hexdigest()!="bd398ffa3b728b1a847bb519fad8168222bccb48":
        raise RuntimeError("Vendored master logo integrity mismatch")
    if ET.fromstring(master).attrib.get("viewBox")!="0 0 1774 887": raise RuntimeError("logo geometry")
    (out/"125A_Logo_Master_FINAL.svg").write_bytes(master)
    samples=[]
    for family,logical in (("L",128),("S",64)):
        for suffix,ratio in (("0.5x",.5),("0.75x",.75),("200",1.),("1.5x",1.5)):
            size=round(logical*ratio)
            base=knob_base(size)
            frames=[knob_frame(base,i,size) for i in range(128)]
            if len({f.tobytes() for f in frames})!=128: raise RuntimeError("Duplicate knob frame")
            atlas=Image.new("RGBA",(size*16,size*8))
            for i,frame in enumerate(frames):
                atlas.paste(frame,((i%16)*size,(i//16)*size))
            target=out/f"knob_{family}_{suffix}.png";atlas.save(target)
            check=Image.open(target).convert("RGBA")
            if check.size!=(size*16,size*8): raise RuntimeError("Atlas dimensions")
            for i,expected in enumerate(frames):
                x=(i%16)*size;y=(i//16)*size
                actual=check.crop((x,y,x+size,y+size))
                if actual.tobytes()!=expected.tobytes() or not marker_matches(actual,i):
                    raise RuntimeError(f"Atlas frame mismatch {family} {suffix} {i}")
            if marker_matches(frames[0],127): raise RuntimeError("Negative marker test failed")
            if ratio==1.:
                samples.append((family,[frames[i] for i in (0,24,64,100,127)]))
    # Scoped style proof, not a mockup or a substitute for the native screenshot.
    svg=['<svg xmlns="http://www.w3.org/2000/svg" width="840" height="370" viewBox="0 0 840 370">',
         '<rect width="840" height="370" fill="#242629"/>',
         '<text x="24" y="30" fill="#ddd" font-family="Arial" font-size="16">Graphite control samples - original display excluded</text>']
    for row,(family,frames) in enumerate(samples):
        for i,frame in enumerate(frames):
            buf=io.BytesIO();frame.save(buf,format="PNG")
            uri=base64.b64encode(buf.getvalue()).decode("ascii")
            size=frame.width
            svg.append(f'<image x="{24+i*160+(128-size)//2}" y="{44+row*145}" width="{size}" height="{size}" href="data:image/png;base64,{uri}"/>')
    svg.extend(['<rect x="24" y="328" width="140" height="30" rx="6" fill="#151719"/>',
                '<rect x="168" y="328" width="140" height="30" rx="6" fill="#ffad50"/>',
                '<text x="94" y="348" text-anchor="middle" fill="#ddd" font-family="Arial">OFF</text>',
                '<text x="238" y="348" text-anchor="middle" fill="#161718" font-family="Arial">ON</text>','</svg>'])
    (out/"Chopperator-static-draft.svg").write_text("\n".join(svg),encoding="utf-8")
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
