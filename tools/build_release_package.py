#!/usr/bin/env python3
"""Create bilingual manuals and the customer ZIP from the exact validated VST3."""
import argparse, hashlib, json, math, re, zipfile
from pathlib import Path
from PIL import Image
from reportlab.pdfgen import canvas
from reportlab.lib.pagesizes import A4
from reportlab.lib.colors import HexColor
from reportlab.lib.styles import ParagraphStyle
from reportlab.platypus import Paragraph
import fitz

VERSION = "1.0"
BUNDLE = "125A Chopperator.vst3"
ZIP = "125A-Chopperator-V1.0-Windows-x64.zip"
INK = HexColor("#1e262d")
MUTED = HexColor("#515e68")
AMBER = HexColor("#b45e24")
PALE = HexColor("#e8e9e8")
W,H = A4
M = 46
CW = W-2*M

LANG = {
"DE": {
"title":"Bedienungsanleitung",
"intro":"Tempo-synchroner Audio-Effekt für rhythmisches Chopping, pulsierende Flächen und sich beschleunigende oder verlangsamende Lautstärkebewegungen.",
"install":"Installation und Schnellstart",
"install_body":"Entpacke das ZIP vollständig. Kopiere den gesamten Ordner <b>125A Chopperator.vst3</b> (nicht nur die darin enthaltene Binärdatei) nach <b>C:/Program Files/Common Files/VST3/</b>. Administratorrechte können erforderlich sein. Entferne gegebenenfalls ältere Chopperator-Kopien aus anderen VST3-Suchpfaden. Starte die 64-Bit-DAW neu oder führe einen VST3-Rescan aus. Lade 125A Chopperator als Audio-Insert-Effekt, nicht als Instrument.",
"quick":"Spiele ein gehaltenes Signal ab. Stelle START und END auf 1/2, MID auf 1/16, LENGTH auf 2 BARS und MIX auf 100 %. Wähle RECT und ONE SHOT, dann schalte ACTIVE von OFF auf ON. Der Effekt wird zur Mitte schneller und zum Ende langsamer. Für einen erneuten Durchlauf erst OFF, dann ON wählen.",
"system":"Windows x64 | VST3-Audioeffekt | Mono und Stereo | kein MIDI erforderlich",
"caption":"Originale Plugin-Oberfläche, aufgenommen im nativen Windows-VST3-Test.",
"controls":"Regler und Funktionen",
"timing":"Timing und Verlauf",
"timing_intro":"START, MID und END geben die rhythmische Unterteilung an. Je größer der Nenner, desto schneller die Impulse. Bei 120 BPM bedeutet 1/4 zwei Impulse pro Sekunde, 1/16 acht Impulse pro Sekunde. Die Übergänge sind kontinuierlich und an das Hosttempo gekoppelt.",
"rows":[
("START","1/1 bis 1/64 (1/2)","Rhythmische Unterteilung am Anfang des Durchlaufs."),
("MID","1/1 bis 1/64 (1/16)","Rhythmische Unterteilung am einstellbaren MID-Punkt."),
("END","1/1 bis 1/64 (1/2)","Rhythmische Unterteilung am Ende des Durchlaufs."),
("LENGTH","1/4, 1/2, 1–64 Takte (2)","Gesamtdauer in Takten, entsprechend Taktart und Tempo der DAW. Bruchteile sind Taktbruchteile, keine Notenwerte."),
("MID POSITION","1–99 % (50 %)","Zeitliche Lage des MID-Punkts innerhalb der Gesamtdauer."),
("CURVE","-100 bis +100 % (0 %)","Verlauf der Geschwindigkeitsänderung zwischen START, MID und END. 0 % linear; negativ früh, positiv spät innerhalb jedes Abschnitts."),
("WIDTH","0–100 % (50 %)","Aktiver Anteil einer Impulsperiode. 50 % = halbe Periode geöffnet; 0 % = Zielpegel geschlossen."),
("ATTACK","0–100 % (2 %)","Anstieg der Verstärkung als Anteil der aktuellen Impulsperiode. Höhere Werte glätten den Einsatz."),
("RELEASE","0–100 % (5 %)","Abfall der Verstärkung als Anteil der aktuellen Impulsperiode. Höhere Werte glätten das Ausklingen."),
("MIX","0–100 % (100 %)","Dry/Wet-Verhältnis. 0 % = unverändertes Eingangssignal; 100 % = vollständige Modulation."),
],
"buttons":"Schalter und Bedienung",
"button_rows":[
("ACTIVE","OFF / ON (OFF)","OFF: unverarbeitetes Signal. OFF → ON startet die Sequenz von vorn. ON allein löst keinen weiteren Start aus."),
("MODE","ONE SHOT / LOOP (ONE SHOT)","ONE SHOT: einmaliger Durchlauf, danach Rückkehr zu Dry. LOOP: wiederholt den Geschwindigkeitsverlauf ohne Zurücksetzen der Impulsphase."),
("SHAPE","RECT / SAW UP / SAW DOWN / TRIANGLE / SINE (RECT)","Form jedes einzelnen Impulses, nicht des gesamten Geschwindigkeitsverlaufs."),
("HOST BYPASS","DAW-Bypass","Unabhängig von ACTIVE; blendet zum unveränderten Signal. Der interne Verlauf kann weiterlaufen."),
],
"edit":"Bedienung: Regler vertikal ziehen oder Mausrad verwenden. Strg + Linksklick setzt den Regler auf seinen Standardwert. Doppelklick auf die Wertanzeige erlaubt Direkteingabe (z. B. 1/16, 2 bars, 50 %, -25 %). Enter bestätigt, Esc bricht ab. Ungültige Eingaben werden verworfen.",
"view":"Die Vorschau zeigt den eingestellten Impulsverlauf und ist kein Live-Abspielcursor. Das Fragezeichen schaltet kontextbezogene Hilfe ein. Ziehe die rechte untere Ecke für stufenlose Größenänderung von 75 bis 150 %, sofern der Host die Größe akzeptiert. Parameter und GUI-Einstellungen werden im DAW-Projekt gespeichert.",
"shapes":"Impulsformen",
"shapetext":["Rechteck: konstanter offener Pegel.","Sägezahn aufwärts: steigender Pegel.","Sägezahn abwärts: fallender Pegel.","Dreieck: Anstieg und Abfall.","Sinus: weich gerundeter Impuls."],
"curve_title":"MID POSITION und CURVE",
"position_explain":"MID POSITION verschiebt den Zeitpunkt, an dem die MID-Unterteilung erreicht wird. Das verändert die Länge der beiden Abschnitte. Beispiel: bei zwei Takten und 25 % liegt MID nach einem halben Takt; bei 75 % nach anderthalb Takten.",
"curve_explain":"CURVE ändert die Form beider Übergänge, ohne den MID-Punkt zu verschieben. Bei -100 % erfolgt ein Großteil der Änderung früh im jeweiligen Abschnitt; bei +100 % spät. Bei gleichen START-, MID- und END-Werten hat die Kurve keinen Einfluss auf die Geschwindigkeit.",
"graphcap":"Schematische Geschwindigkeit (nicht die Lautstärke), bei START = END = 1/2, MID = 1/16.",
"axis":"Zeit im Durchlauf",
"usage":"Praxisbeispiele",
"examples":[
("Industrial / Metal","Nach der Amp-Simulation auf gehaltene Powerchords. 1/2 → 1/32 → 1/2, 2 Takte, RECT, WIDTH 40 %, ATTACK 2 %, RELEASE 5 %. ONE SHOT per ACTIVE automatisieren."),
("Pads / EBM","1/8 → 1/16 → 1/8, 4 Takte, TRIANGLE oder SINE, WIDTH 75 %, MIX 60 %. LOOP erzeugt eine wiederkehrende Bewegung."),
("Beschleunigender Übergang","1/4 → 1/16 → 1/64, LENGTH 1 Takt, MID POSITION 50 %, CURVE +50 %, RECT. ACTIVE am gewünschten Startpunkt automatisieren.")
],
"notes":"Wichtig: Bei ACTIVE OFF oder nach beendetem ONE SHOT kehrt das Signal mit kurzer Glättung zu Dry zurück. ATTACK und RELEASE sind prozentuale Slew-Zeiten bezogen auf die Impulsperiode (mindestens 0,5 ms), keine festen Millisekunden. MIX und Bypass werden ebenfalls kurz geglättet. Bei hohen Raten kann dies extreme Impulse abrunden. Chopperator verändert die Lautstärke, nicht Tonhöhe oder Sampleposition. Die Sequenz läuft in lokaler musikalischer Zeit ab dem ACTIVE-Start, nicht automatisch ab Songposition oder Taktanfang.",
"trouble":"Kein hörbarer Effekt? Prüfe ACTIVE, MIX, DAW-Bypass und ob ONE SHOT bereits beendet ist. Wähle LOOP oder schalte OFF → ON. Bei WIDTH 100 % und RECT bleibt das Zielsignal dauerhaft offen. Falls der Effekt nicht erscheint, prüfe den VST3-Ordner, das vollständige Bundle und den Plugin-Rescan.",
"range":"Bereich (Standard)",
"shape_names":["RECT","SAW UP","SAW DOWN","TRIANGLE","SINE"],
},
"EN": {
"title":"User Manual",
"intro":"Tempo-synchronized audio effect for rhythmic chopping, pulsing textures and accelerating or decelerating volume movements.",
"install":"Installation and quick start",
"install_body":"Extract the entire ZIP. Copy the complete <b>125A Chopperator.vst3</b> folder (not just the binary inside it) to <b>C:/Program Files/Common Files/VST3/</b>. Administrator privileges may be required. Remove older Chopperator copies from other VST3 search paths if necessary. Restart your 64-bit DAW or rescan VST3 plug-ins. Load 125A Chopperator as an audio insert effect, not as an instrument.",
"quick":"Play a sustained signal. Set START and END to 1/2, MID to 1/16, LENGTH to 2 BARS and MIX to 100%. Choose RECT and ONE SHOT, then switch ACTIVE from OFF to ON. The effect speeds up towards MID and slows down towards END. To trigger another run, select OFF and then ON.",
"system":"Windows x64 | VST3 audio effect | Mono and stereo | No MIDI required",
"caption":"Original plug-in interface captured in the native Windows VST3 test.",
"controls":"Controls and functions",
"timing":"Timing and rate trajectory",
"timing_intro":"START, MID and END set rhythmic note divisions. A higher denominator means faster pulses. At 120 BPM, 1/4 means two pulses per second and 1/16 means eight pulses per second. Transitions are continuous and synchronized to host tempo.",
"rows":[
("START","1/1 to 1/64 (1/2)","Rhythmic division at the beginning of the run."),
("MID","1/1 to 1/64 (1/16)","Rhythmic division at the adjustable MID point."),
("END","1/1 to 1/64 (1/2)","Rhythmic division at the end of the run."),
("LENGTH","1/4, 1/2, 1–64 bars (2)","Total duration in bars, following the DAW time signature and tempo. Fractions are fractions of a bar, not note values."),
("MID POSITION","1–99% (50%)","Time location of the MID point within the total duration."),
("CURVE","-100 to +100% (0%)","Rate transition between START, MID and END. 0% is linear; negative changes early, positive late in each segment."),
("WIDTH","0–100% (50%)","Active portion of each pulse period. 50% = half the period open; 0% = closed target gain."),
("ATTACK","0–100% (2%)","Gain rise as a fraction of the current pulse period. Higher values smooth the onset."),
("RELEASE","0–100% (5%)","Gain fall as a fraction of the current pulse period. Higher values smooth the decay."),
("MIX","0–100% (100%)","Dry/wet balance. 0% = unchanged input; 100% = full modulation."),
],
"buttons":"Switches and operation",
"button_rows":[
("ACTIVE","OFF / ON (OFF)","OFF: unprocessed signal. OFF → ON starts the sequence from the beginning. Holding ON does not retrigger."),
("MODE","ONE SHOT / LOOP (ONE SHOT)","ONE SHOT: one run, then returns to dry. LOOP: repeats the rate trajectory without resetting pulse phase."),
("SHAPE","RECT / SAW UP / SAW DOWN / TRIANGLE / SINE (RECT)","Shape of each individual pulse, not the overall rate trajectory."),
("HOST BYPASS","DAW bypass","Independent of ACTIVE; fades to the unprocessed signal. The internal trajectory can continue."),
],
"edit":"Operation: drag knobs vertically or use the mouse wheel. Ctrl + left-click restores a knob's default value. Double-click the value display to type a value (e.g. 1/16, 2 bars, 50%, -25%). Enter confirms; Esc cancels. Invalid entries are discarded.",
"view":"The preview displays the configured pulse pattern, not a live playback cursor. The question mark toggles contextual help. Drag the bottom-right corner for continuous resizing from 75 to 150%, subject to host acceptance. Parameters and GUI preferences are stored in the DAW project.",
"shapes":"Pulse shapes",
"shapetext":["Rectangle: constant open gain.","Rising saw: increasing gain.","Falling saw: decreasing gain.","Triangle: rise and fall.","Sine: smoothly rounded pulse."],
"curve_title":"MID POSITION and CURVE",
"position_explain":"MID POSITION moves the time at which the MID division is reached, changing the duration of the two segments. For a two-bar run, 25% puts MID half a bar after the start; 75% puts it one and a half bars after the start.",
"curve_explain":"CURVE changes the shape of both transitions without moving the MID point. At -100%, much of the change occurs early in each segment; at +100%, it occurs late. When START, MID and END are identical, CURVE has no effect on rate.",
"graphcap":"Schematic rate (not volume), with START = END = 1/2 and MID = 1/16.",
"axis":"Time within the run",
"usage":"Production examples",
"examples":[
("Industrial / Metal","After the amp simulation on sustained power chords. 1/2 → 1/32 → 1/2, 2 bars, RECT, WIDTH 40%, ATTACK 2%, RELEASE 5%. Trigger ONE SHOT via ACTIVE automation."),
("Pads / EBM","1/8 → 1/16 → 1/8, 4 bars, TRIANGLE or SINE, WIDTH 75%, MIX 60%. LOOP creates recurring movement."),
("Accelerating transition","1/4 → 1/16 → 1/64, LENGTH 1 bar, MID POSITION 50%, CURVE +50%, RECT. Automate ACTIVE at the desired starting point.")
],
"notes":"Important: With ACTIVE OFF or after a completed ONE SHOT, the signal returns to dry with brief smoothing. ATTACK and RELEASE are slew times expressed as percentages of the pulse period (minimum 0.5 ms), not fixed milliseconds. MIX and bypass are also briefly smoothed. At high rates, this can round extreme pulses. Chopperator changes volume, not pitch or sample position. The sequence runs in local musical time from the ACTIVE trigger, not automatically from song position or the start of a bar.",
"trouble":"No audible effect? Check ACTIVE, MIX, DAW bypass and whether ONE SHOT has already completed. Choose LOOP or switch OFF → ON. WIDTH 100% with RECT keeps the target signal continuously open. If the effect is missing, check the VST3 directory, complete bundle and plug-in rescan.",
"range":"Range (default)",
"shape_names":["RECT","SAW UP","SAW DOWN","TRIANGLE","SINE"],
}}
def sha(data):
    return hashlib.sha256(data).hexdigest()

def safe(text):
    return text.replace("→", " > ").replace("–", "-").replace("×"," x ")

def paragraph(c, text, x, y, width=CW, size=10.6, leading=14, color=INK, bold=False):
    style=ParagraphStyle("body",fontName="Helvetica-Bold" if bold else "Helvetica",
        fontSize=size,leading=leading,textColor=color,spaceBefore=0,spaceAfter=0)
    p=Paragraph(safe(text),style)
    _,height=p.wrap(width,H)
    if y-height < 51: raise ValueError("Manual text overflow: "+text[:70])
    p.drawOn(c,x,y-height)
    return y-height

def header(c, title, page, lang):
    c.setFillColor(INK);c.rect(0,H-22,W,22,fill=1,stroke=0)
    c.setFillColor(AMBER);c.rect(M,H-47,33,3,fill=1,stroke=0)
    c.setFont("Helvetica-Bold",10);c.setFillColor(INK)
    c.drawString(M,H-65,"125A SYSTEMS   /   CHOPPERATOR")
    c.setFont("Helvetica",9);c.setFillColor(MUTED);c.drawRightString(W-M,H-65,"V1.0  |  "+lang)
    c.setFont("Helvetica-Bold",24);c.setFillColor(INK);c.drawString(M,H-103,title)
    c.setStrokeColor(PALE);c.line(M,43,W-M,43)
    c.setFont("Helvetica",9);c.setFillColor(MUTED);c.drawString(M,28,"125A Chopperator V1.0")
    c.drawRightString(W-M,28,f"{page} / 5")
    return H-126

def sub(c,txt,y):
    return paragraph(c,txt,M,y,size=13,leading=17,color=AMBER,bold=True)-7

def rows(c, data, y, d, left=139):
    c.setFillColor(INK);c.rect(M,y-24,CW,24,fill=1,stroke=0)
    c.setFillColor(HexColor("#ffffff"));c.setFont("Helvetica-Bold",9)
    c.drawString(M+9,y-16,"CONTROL");c.drawString(M+left+8,y-16,d["range"])
    y-=28
    for name,range_,desc in data:
        start=y
        y1=paragraph(c,name,M+9,y-5,left-17,size=10,leading=12,bold=True)
        y2=paragraph(c,range_,M+left+8,y-5,CW-left-16,size=9.4,leading=12,color=AMBER,bold=True)
        y2=paragraph(c,desc,M+left+8,y2-3,CW-left-16,size=10.2,leading=13)
        y=min(y1,y2)-8
        c.setStrokeColor(PALE);c.line(M,y,W-M,y)
        if start-y<24:raise ValueError("Invalid table geometry")
    return y

def graph(c,x,y,w,h,p,cv,label):
    c.setFillColor(INK);c.setFont("Helvetica-Bold",9);c.drawString(x,y+h+9,label)
    c.setStrokeColor(PALE);c.line(x,y,x+w,y);c.line(x,y,x,y+h)
    c.setDash(2,3);c.setStrokeColor(MUTED);c.line(x+p*w,y,x+p*w,y+h);c.setDash()
    path=c.beginPath()
    for i in range(121):
        u=i/120;k=2**(2*cv)
        r=2+14*(u/p)**k if u<=p else 16-14*((u-p)/(1-p))**k
        xx=x+u*w;yy=y+5+(h-10)*(r-2)/14
        if i==0:path.moveTo(xx,yy)
        else:path.lineTo(xx,yy)
    c.setStrokeColor(AMBER);c.setLineWidth(1.7);c.drawPath(path)
    c.setFillColor(MUTED);c.setFont("Helvetica",7.5)
    c.drawString(x,y-11,"START");c.drawCentredString(x+p*w,y-11,"MID");c.drawRightString(x+w,y-11,"END")
    c.setLineWidth(1)

def shape(c,x,y,w,h,i,label,description):
    c.setFont("Helvetica-Bold",9);c.setFillColor(INK);c.drawString(x,y+h+8,label)
    c.setStrokeColor(PALE);c.line(x,y,x+w,y)
    path=c.beginPath()
    for j in range(101):
        p=j/100
        if p>=.75:g=0
        else:
            u=p/.75
            g=[1,u,1-u,1-abs(2*u-1),.5-.5*math.cos(2*math.pi*u)][i]
        xx=x+p*w;yy=y+g*h
        if j==0:path.moveTo(xx,yy)
        else:path.lineTo(xx,yy)
    c.setStrokeColor(AMBER);c.setLineWidth(1.5);c.drawPath(path);c.setLineWidth(1)
    paragraph(c,description,x,y-7,w,size=8.6,leading=11)

def manual(path,lang,screenshot):
    d=LANG[lang]
    c=canvas.Canvas(str(path),pagesize=A4,pageCompression=1)
    c.setTitle("125A Chopperator V1.0 - "+d["title"]);c.setAuthor("125A Systems")
    y=header(c,d["title"],1,lang)
    y=paragraph(c,d["intro"],M,y,size=12,leading=17)-12
    y=paragraph(c,d["system"],M,y,size=9,leading=12,color=AMBER,bold=True)-14
    img_h=CW*576/960
    c.drawImage(str(screenshot),M,y-img_h,width=CW,height=img_h)
    y=paragraph(c,d["caption"],M,y-img_h-6,size=8.5,leading=11,color=MUTED)-13
    y=sub(c,d["install"],y)
    y=paragraph(c,d["install_body"],M,y,size=10,leading=13)-10
    paragraph(c,d["quick"],M,y,size=10,leading=13)
    c.showPage()
    y=header(c,d["controls"],2,lang)
    y=sub(c,d["timing"],y)
    y=paragraph(c,d["timing_intro"],M,y,size=10.5,leading=14)-12
    rows(c,d["rows"],y,d)
    c.showPage()
    y=header(c,d["buttons"],3,lang)
    y=rows(c,d["button_rows"],y,d)-14
    y=paragraph(c,d["edit"],M,y,size=10.3,leading=14)-11
    y=paragraph(c,d["view"],M,y,size=10.3,leading=14)-16
    y=sub(c,d["shapes"],y)
    cell=(CW-4*12)/5
    for i in range(5):
        shape(c,M+i*(cell+12),y-52,cell,37,i,d["shape_names"][i],d["shapetext"][i])
    c.showPage()
    y=header(c,d["curve_title"],4,lang)
    y=sub(c,"MID POSITION",y)
    y=paragraph(c,d["position_explain"],M,y)-18
    cell=(CW-2*18)/3
    for i,p in enumerate((.25,.5,.75)):
        graph(c,M+i*(cell+18),y-90,cell,75,p,0,f"{int(p*100)} %")
    y-=125
    y=sub(c,"CURVE",y)
    y=paragraph(c,d["curve_explain"],M,y)-18
    for i,k in enumerate((-1,0,1)):
        graph(c,M+i*(cell+18),y-90,cell,75,.5,k,f"{int(k*100):+d} %")
    y-=125
    y=paragraph(c,d["graphcap"],M,y,size=9.8,leading=13,color=MUTED)-8
    paragraph(c,d["axis"]+"  >",M,y,size=9.5,leading=13,color=MUTED)
    c.showPage()
    y=header(c,d["usage"],5,lang)
    for title,body in d["examples"]:
        y=sub(c,title,y)
        y=paragraph(c,body,M,y)-16
    y=sub(c,"Hinweise" if lang=="DE" else "Notes",y)
    y=paragraph(c,d["notes"],M,y)-15
    y=sub(c,"Fehlerbehebung" if lang=="DE" else "Troubleshooting",y)
    paragraph(c,d["trouble"],M,y)
    c.showPage();c.save()

def verify_pdf(path, renders):
    with fitz.open(path) as doc:
        if len(doc)!=5:raise ValueError("Unexpected manual page count")
        alltext=""
        for i,page in enumerate(doc):
            text=page.get_text()
            if len(text)<80 or "\ufffd" in text:raise ValueError("Missing/broken manual text")
            alltext+=text
            for word in page.get_text("words"):
                if word[0]<5 or word[1]<5 or word[2]>page.rect.width-5 or word[3]>page.rect.height-5:
                    raise ValueError("Manual text outside page")
            page.get_pixmap(matrix=fitz.Matrix(1.5,1.5),alpha=False).save(str(renders/(path.stem+f"-{i+1}.png")))
        for name in ("START","MID","END","LENGTH","MID POSITION","CURVE","WIDTH","ATTACK","RELEASE","MIX","ACTIVE","ONE SHOT","LOOP","SHAPE"):
            if name not in alltext:raise ValueError("Missing manual control: "+name)

def main():
    p=argparse.ArgumentParser()
    p.add_argument("--candidate",type=Path,required=True)
    p.add_argument("--screenshot",type=Path,required=True)
    p.add_argument("--out",type=Path,required=True)
    p.add_argument("--evidence",type=Path,required=True)
    args=p.parse_args()
    args.out.mkdir(parents=True,exist_ok=True);args.evidence.mkdir(parents=True,exist_ok=True)
    renders=args.evidence/"manual-renders";renders.mkdir(exist_ok=True)
    with Image.open(args.screenshot) as im:
        if im.size!=(960,576):raise ValueError("Expected native 960 x 576 screenshot")
        png=args.evidence/"manual-native-editor.png";im.convert("RGB").save(png)
    pdfs=[]
    for lang,label in (("DE","Bedienungsanleitung"),("EN","User-Manual")):
        path=args.evidence/f"125A-Chopperator-V1.0-{label}-{lang}.pdf"
        manual(path,lang,png);verify_pdf(path,renders);pdfs.append(path)
    with zipfile.ZipFile(args.candidate) as z:
        names=z.namelist()
        if len(names)!=17 or len(set(names))!=17 or z.testzip() or any(not n.startswith(BUNDLE+"/") for n in names):
            raise ValueError("Unexpected verified candidate archive")
        original={n:z.read(n) for n in names}
    info=original[BUNDLE+"/Contents/Resources/moduleinfo.json"].decode("utf-8-sig")
    if not re.search(r'"Version"\s*:\s*"1\.0"',info):
        raise ValueError("V1.0 metadata missing")
    files=original|{p.name:p.read_bytes() for p in pdfs}
    target=args.out/ZIP
    with zipfile.ZipFile(target,"x",compression=zipfile.ZIP_DEFLATED) as z:
        for name,data in sorted(files.items()):z.writestr(name,data)
    with zipfile.ZipFile(target) as z:
        if set(z.namelist())!=set(files) or len(z.namelist())!=19 or z.testzip():
            raise ValueError("Customer ZIP inventory/CRC mismatch")
        if any(z.read(n)!=data for n,data in files.items()):raise ValueError("Customer ZIP byte mismatch")
    manifest={"version":"1.0","package":ZIP,"package_sha256":sha(target.read_bytes()),
        "module_sha256":sha(original[BUNDLE+"/Contents/x86_64-win/125A Chopperator.vst3"]),
        "files":{n:sha(data) for n,data in files.items()},"pdf_pages_each":5,
        "customer_zip_byte_identity":"PASS","manual_visual_review":"PENDING",
        "studio_pro_owner_acceptance":"NOT INDEPENDENTLY VERIFIED"}
    (args.evidence/"V1.0-PACKAGE-MANIFEST.json").write_text(json.dumps(manifest,indent=2)+"\n",encoding="utf-8")
    print("V1.0_CUSTOMER_ZIP_PASS",manifest["package_sha256"],len(files))
if __name__=="__main__":main()
