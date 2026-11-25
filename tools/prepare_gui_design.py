#!/usr/bin/env python3
"""Generate the static 125A Chopperator design proof using original approved assets."""
import base64
import hashlib
import io
import math
from pathlib import Path
import struct
import subprocess
import sys
import urllib.request
import xml.etree.ElementTree as ET

KNOB_COMMIT="c09cff5d958969eb025fe09255173e65103deae9"
BRAND_COMMIT="20c4d9d2a3b37e6a8d17e693ed76d9351203c288"
STRIPS=(
    ("L",128,100,"76f9f064a2a234bbf5675f6770416bc03b4adfd6"),
    ("L",192,150,"70f4f68b7462fbee4f52199d50d3af7b7b25c7af"),
    ("L",256,200,"1c25b2ff43ce237f136054624ea9ee7fe4602ec7"),
    ("L",384,300,"776a12411487ea5e2903c367529f662029171cf9"),
    ("S",64,100,"f273d308cc71616ad6c006293ad8e965afd0ed5c"),
    ("S",96,150,"72b60d2e088d0bb3966143ebde1bbc2230e1232d"),
    ("S",128,200,"f228c6187aef44ab328a91eede7329e15d9c8372"),
    ("S",192,300,"697a2e1ddd730227de91b6da9c0280088a510974"),
)
def acquire(url, target, expected):
    if target.exists():
        raw=target.read_bytes()
    else:
        with urllib.request.urlopen(url, timeout=60) as response:
            raw=response.read(6_000_001)
    actual=hashlib.sha1(b"blob "+str(len(raw)).encode()+b"\0"+raw).hexdigest()
    if actual!=expected: raise RuntimeError(f"Asset mismatch {target.name}")
    target.write_bytes(raw)
    return raw
def image_data(raw):
    return "data:image/png;base64,"+base64.b64encode(raw).decode()
def main():
    if len(sys.argv)!=2: raise SystemExit("Usage: prepare_gui_design.py OUTPUT_DIRECTORY")
    out=Path(sys.argv[1]);out.mkdir(parents=True,exist_ok=True)
    raw_url="https://raw.githubusercontent.com/challanger2000/"
    source={}
    for family,size,scale,sha in STRIPS:
        name=f"125A_MixEngine_Analog_{family}_{size}px_{scale}pct_128f.png"
        path=out/name
        raw=acquire(raw_url+"125A-Knob-Designer/"+KNOB_COMMIT+"/assets/mixengine/knobs/"+name,path,sha)
        if raw[:8]!=b"\x89PNG\r\n\x1a\n" or struct.unpack(">II",raw[16:24])!=(size,size*128):
            raise RuntimeError("Unexpected original strip geometry "+name)
        source[(family,scale)]=path
    # The private branding repository is not available to anonymous CI.
    # Only the designated public-facing product logo is vendored, no credentials or other files.
    logo=(Path(__file__).resolve().parent.parent/"resources"/"125A_Logo_Master_FINAL.svg").read_bytes()
    if hashlib.sha1(b"blob "+str(len(logo)).encode()+b"\0"+logo).hexdigest()!="bd398ffa3b728b1a847bb519fad8168222bccb48":
        raise RuntimeError("Vendored master logo integrity mismatch")
    (out/"125A_Logo_Master_FINAL.svg").write_bytes(logo)
    if ET.fromstring(logo).attrib.get("viewBox")!="0 0 1774 887": raise RuntimeError("logo geometry")
    svg=['<svg xmlns="http://www.w3.org/2000/svg" width="960" height="576" viewBox="0 0 960 576">',
         '<defs><linearGradient id="panel" x2="0" y2="1"><stop stop-color="#33363a"/><stop offset="1" stop-color="#1e2023"/></linearGradient></defs>',
         '<rect width="960" height="576" rx="8" fill="url(#panel)" stroke="#51545a"/>']
    def rect(x,y,w,h,fill,stroke="#494c50",r=6):
        svg.append(f'<rect x="{x}" y="{y}" width="{w}" height="{h}" rx="{r}" fill="{fill}" stroke="{stroke}"/>')
    def text(x,y,value,size=13,color="#d1d2d3",weight="normal"):
        svg.append(f'<text x="{x}" y="{y}" font-family="Arial,sans-serif" font-size="{size}" text-anchor="middle" fill="{color}" font-weight="{weight}">{value}</text>')
    def knob(cx,y,family,normalized,label,value):
        size=128 if family=="L" else 64
        frame=math.floor(normalized*127+.5)
        # Crop exact original pixels with no resize or redraw.
        command=["ffmpeg","-v","error","-nostdin","-i",str(source[(family,100)]),
                 "-vf",f"crop={size//2}:{size//2}:{size//4}:{frame*size+size//4}:exact=1","-frames:v","1","-f","image2pipe","-vcodec","png","-"]
        raw=subprocess.check_output(command)
        svg.append(f'<image x="{cx-size//2}" y="{y}" width="{size}" height="{size}" href="{image_data(raw)}"/>')
        text(cx,y-5,label,13,"#bfc1c4","bold")
        text(cx,y+size+15,value,16 if family=="L" else 13,"#f2f2f2","bold")
    for x in (10,950):
        for y in (10,566):
            svg.append(f'<circle cx="{x}" cy="{y}" r="3.5" fill="#696c70" stroke="#17181a"/><path d="M{x-2} {y+1} L{x+2} {y-1}" stroke="#242629"/>')
    logo_uri="data:image/svg+xml;base64,"+base64.b64encode(logo).decode()
    svg.append(f'<image x="24" y="10" width="84" height="42" href="{logo_uri}"/>')
    text(322,39,"CHOPPERATOR",27,"#e1e2e3","bold")
    rect(624,16,136,34,"#17191b")
    text(692,38,"ACTIVE OFF",14,"#d4d4d4","bold")
    rect(772,16,164,34,"#17191b")
    text(854,38,"ONE SHOT",14,"#f2a457","bold")
    rect(24,72,912,138,"#101315","#54585b")
    # Configured envelope, not playback position. Preview of the default 2-bar
    # 1/2 -> 1/16 -> 1/2 trajectory, linear, 50% width, 2/5% slew.
    for bar in range(3):
        x=44+bar*436
        svg.append(f'<path d="M{x} 105 V190" stroke="#4a4e53" stroke-width="1"/>')
    for y in (110,150,190):
        svg.append(f'<path d="M44 {y} H916" stroke="#272b2f"/>')
    text(66,91,"START",11,"#bfc1c4");text(480,91,"MID",11,"#bfc1c4");text(894,91,"END",11,"#bfc1c4")
    gain=1.0;phase=0.0;points=[]
    samples=48000*4;per_pixel=samples/872
    lows=[1.0]*872;highs=[0.0]*872
    for i in range(samples):
        x=i/samples
        rate=2+28*x if x<.5 else 16-28*(x-.5)
        target=1.0 if phase<.5 else 0.0
        period=2/rate
        seconds=max(.0005,period*(.02 if target>gain else .05))
        step=1/(48000*seconds)
        gain+=max(-step,min(step,target-gain))
        phase=(phase+rate/(48000*2))%1
        pixel=min(871,int(i/per_pixel));lows[pixel]=min(lows[pixel],gain);highs[pixel]=max(highs[pixel],gain)
    for pixel,(lo,hi) in enumerate(zip(lows,highs)):
        points.append(f'M{44+pixel} {190-80*lo:.2f}V{190-80*hi:.2f}')
    svg.append('<path d="'+" ".join(points)+'" stroke="#f59c49" stroke-width="1.2" stroke-linecap="round"/>')
    rect(24,224,912,176,"#24272a")
    for x,label,value,n in [(138,"START","1/2",1/6),(366,"MID","1/16",4/6),(594,"END","1/2",1/6),(822,"LENGTH","2 BARS",3/65)]:
        knob(x,250,"L",n,label,value)
    rect(24,414,912,104,"#24272a")
    for x,label,value,n in [(100,"MID POSITION","50%",.5),(252,"CURVE","0%",.5),(404,"WIDTH","50%",.5),(556,"ATTACK","2%",.02),(708,"RELEASE","5%",.05),(860,"MIX","100%",1)]:
        knob(x,436,"S",n,label,value)
    for i,label in enumerate(("RECT","SAW UP","SAW DOWN","TRIANGLE","SINE")):
        x=24+i*184
        rect(x,528,176,36,"#4b3422" if i==0 else "#151719","#d58745" if i==0 else "#494c50")
        text(x+88,550,label,14,"#f3ac65" if i==0 else "#c3c5c7","bold")
    svg.append("</svg>")
    (out/"Chopperator-static-draft.svg").write_text("\n".join(svg),encoding="utf-8")
    print("STATIC_DESIGN_GENERATED: all eight approved filmstrip geometries and master logo verified; visual review pending")
if __name__=="__main__": main()
