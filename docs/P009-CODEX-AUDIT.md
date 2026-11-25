# Chopperator P009 · unabhängiger adversarialer Nachaudit

Der [abschließende Nachaudit](P009-FINAL-CODEX-AUDIT.md) dokumentiert den späteren Prüfstand `3bea058` und Windows-Run `45502305140`.

**PARTIAL / NOT VERIFIED · RELEASE NO-GO.** Die ursprünglichen Gegenbeispiele zu Clock, Komponenten-State, Busaktivierung, Parser und Extrem-Samplerate sind in eigenen Linux-Processor-Proben behoben. Neue Gegenproben zeigen eine reentrante Fehlerstelle bei Editor-Benachrichtigungen und Lücken in den Paket- und Testgates. Die Windows-Schrittstatus und veröffentlichten Artefakt-Digests sind unabhängig bestätigt. Der aktive Windows-Editor, die Original-ZIP-Bytes und die Ziel-DAW bleiben in diesem Audit **NOT RUN** bzw. **NOT VERIFIED**.

## Prüfstand

- 24. November 2026, `challanger2000/125A-Chopperator`, Windows x64 VST3 P009 / 0.9.0. Geprüfter Produktstand `aade95612ccf492819221b16ff896d11d138c907`. Remote `main` stand zu Beginn bei `5d387804304d428bd73c5a69b2af9c3a6ed4686d`. Der Unterschied zum Windows-Stand besteht aus Dokumentation. Vergleichsbasis `33d81e8a2a66c95999207819efeecfe9de313ede` (P008 mit archivierten Gegenproben). Kein genehmigtes FULL-/Demo-Paket oder Release-Tag vorgefunden. [P009-Handoff](P009-VERIFICATION-HANDOFF.md)
- Die vorgeschriebenen Engineering-Texte sind im selben Audit-Thread vollständig gelesen worden. Der aktuelle Engineering-Stand `135952779f855d1c920e56c7cb4f7f222c8832b3` wurde gegen den vollständig gelesenen `95f8f0592944bef187745443d355922aadf201ec` verglichen. Die einzige Inhaltsänderung sind 22 zusätzliche Guardrails-Zeilen. Beide neuen Abschnitte zu Clock/Streams/Bussen/UI und zum öffentlichen VSTGUI-Attach wurden vollständig gelesen. Der Audit-Completeness-Contract wurde für P009 erneut vollständig gelesen. Der frühere Standards-Lesestand und die fachlichen Ausschlüsse sind im P008-Bericht aufgeführt. [P008 Codex-Audit](P008-CODEX-AUDIT.md)
- Prüfumfang umfasst den P009-Diff zu vier Produktquellen, CMake, Windows-Workflow, zwei Testquellen und zwei neuen Python-Prüfwerkzeugen. Die übrigen produktiven Pfade und vorhandenen Assertions sind über unveränderte Quelldateien bzw. unveränderte Abschnitte aus dem P008-Inventar abgedeckt. Geprüft sind alle 14 Hostparameter, die 25 interaktiven Editoroberflächen als Inventar, die gemeinsamen Host-/PCM-/Recall-Ketten sowie die neuen GUI-/State-/Asset-Pfade. Vollständige Fremdquellen, proprietäre DAW, physisches ASIO und Compiler sind ausgeschlossen. MIDI, Sampler und Demo/Installer im Kandidatenumfang sind N/A.
- Eigene Ausführung auf Linux x86_64 (KVM), GCC 14.2, Release mit `NDEBUG`, Steinberg SDK `v3.8.1_build_84` bei `3cdf9ca5d1f5b1b21e0a86832aa4abe55607bd96`. Die Processor-Proben linken unveränderte Ausschnitte aus beiden Revisionen gegen die echten SDK-Bibliotheken. Für die Registry-Gegenprobe wird die unveränderte `EditorPreferences`-Definition kompiliert. Die vollständige P009-VST3-Übersetzungseinheit inklusive Editor hat den GCC-C++20-Syntaxcheck bestanden. Windows-Link, GUI, Factory-Laden und DAW sind andere Ausführungsgrenzen. Reproduktion und gemessene Ausgaben sind im Repository archiviert. [Auditnachweise](p009-codex-evidence/README.md)

## Windows-Provenienz

Die öffentliche GitHub-Seite zeigt für den exakten Quell-SHA den erfolgreichen Run **45487606108**, Job **138777034387**, mit 20 erfolgreichen Schritten. Darunter Build, CTest, staged Validator/Native, GUI/DPI, Ressourcen/Lifecycle, Programme, Tester, Test Lab und die Validierung des entpackten Kandidaten. Diese Schrittresultate sind direkt geprüft. [Windows-Run](https://github.com/challanger2000/125A-Chopperator/actions/runs/45487606108) · [Windows-Job](https://github.com/challanger2000/125A-Chopperator/actions/runs/45487606108/job/138777034387)

| GitHub-Artefakt | Direkt bestätigter veröffentlichter SHA-256 des äußeren Artefakts |
| --- | --- |
| 14963589373 · Kandidatenarchiv | `93940bf719053c2095117c87927394dfbfe743ca62c3f485e93b677c57d96776` |
| 14963229366 · QA-Nachweise | `ce1b985febb4067b980416b403fb652344907e996ee5663c7f80ca342571452a` |
| 14963873935 · Development Bundle | `436c798444834cfe3d5473a334e703ea1b51bde058e1d9894556974e39c03c7e` |

Die öffentliche Jobseite fordert für Logs Anmeldung. Der API-Abruf wurde mit 403 beantwortet, die Job-Einzellog- und Artefakt-URLs mit 404. Das gelieferte **innere** ZIP (`cffd9f0cc07e1b6dff649ce753f2bdc791a08fd08ee5cac1d4eb12a398f51fad`) und Modul (`417cab0996a0746e4ea5d3f6c1441a497f23201330b3e2c104ca2287f9585720`) konnten hier nicht selbst gehasht werden. 47/47 Validator, 57/0/0 Tester, P74 `TECHNICAL_BASELINE_PASS`, P65/P67 13/13 und 29/29, sechs Programm-PCM-Fälle und 0/384 Dauerüberschreitungen sind Handoff-Angaben innerhalb der öffentlich erfolgreichen Schritte. Rohprotokolle, tatsächliche ZIP-/Modulbytes und einzelne Ergebniswerte sind **NOT VERIFIED**. Die historischen Red-Runs zur Ressourcenvalidierung und zu SDK-Icon-Metadaten sind im Handoff dokumentiert, bleiben hier ohne Einzelprotokoll. [Abrufnachweis](p009-codex-evidence/logs/windows-metadata.log) · [Zugriffsgrenzen](p009-codex-evidence/logs/windows-access.log)

## Ursprüngliche Befunde und Testqualität

In der Matrix bedeutet **PASS** eine konkret ausgeführte Teilprüfung. Für native Fälle ist der öffentlich bestätigte Windows-Schritt getrennt von einer eigenen Ausführung benannt.

| P008 | Geprüfter P009-Pfad und Ergebnis | Grenze |
| --- | --- | --- |
| **C01 / Clock** | [Processor](../src/Vst3.cpp#L141) übernimmt gültige Teilfelder und prüft numerisch unbrauchbare Paare. **PASS** mit beiden alten PCM-Gegenproben: Tempo-only und Tempo 240 plus ungültiges Meter liefern je 0,25566666666667 statt P008 0,1705. Gültiges 3/4 neben NaN-Tempo liefert zum One-Shot-Ende 1 statt 0. | Native Assertions für positive und negative Pfade geprüft. Reale Host-Clock-Sitzung **NOT RUN**. |
| **C02 / Streams** | [Codec](../src/Vst3.cpp#L22) prüft Status und exakte Bytezahl. **PASS** für 120-Byte v1-Rundlauf und je 16 Lese-/Schreibfelder bei voller Bytezahl mit Fehler sowie bei kurzem Count trotz Erfolg. Zurückgewiesener Read behält den vollständigen vorherigen State. P008 akzeptiert die volle Bytezahl trotz Fehler (0/16 Ablehnungen je Richtung). | Neue native Tests injizieren volle Bytezahl mit Fehler an Processor-, Controller- und UI-Grenzen. Controller/UI nativ **NOT RUN** in diesem Audit. Externe teilweise geschriebene Streams können weiterhin einen Prefix enthalten. |
| **C03 / Busse** | [Render](../src/Vst3.cpp#L188) benutzt Aktivierungszustand für Buffer und Silenceflags. **PASS** bei 32/32 eigenen Kombinationen aus Float32/Float64, Mono/Stereo, in-place/separatem Buffer und allen Busaktivierungen; P008 nur 8/32. Nullpointer-Fallback und Reaktivierung ergänzend in Probe/Testpfaden. | Neue native Sentinels prüfen Stereo64 und Reaktivierung. Die eigene erweiterte Matrix läuft source-linked. |
| **C04 / Live-Recall** | [Präferenzen](../src/Editor.hpp#L25), [Recall](../src/Vst3.cpp#L292) und [Peer-Sync](../src/Editor.hpp#L544) schließen die frühere dauerhafte Kopie des Help-Werts. Native Tests verlangen tatsächliche Geometrie zweier Views, verweigerten Peer-Resize, erneutes Recall und Schließen einer View. | Win32 **NOT RUN** in diesem Audit. Reentrant-Detach scheitert in eigener Probe N01. Help-Raster unmittelbar nach Recall wird im neuen Live-UI-Test nicht gemessen. |
| **C05 / DPI** | [Scale-Pfad](../src/Editor.hpp#L470) verwendet den akzeptierten Benutzerzoom ohne alten privaten AspectRatio-Cache. Native Testfolge prüft 1→1,5→1, Ablehnung und Erholung samt HWND und START-Hit-Mapping. | Win32 **NOT RUN** in diesem Audit. Randfall mit beliebiger Fraktionsskalierung N02 und physischem Monitorwechsel bleiben offen. |
| **C06 / Dirty** | [Controller](../src/Vst3.cpp#L241) ruft über die UI-Präferenzen `setDirty(true)` auf. Native Handler misst zwei Hilfeereignisse und ein Zoomereignis; Recall und DPI sollen null zusätzliche melden. | Fehlender oder ablehnender Host-Handler und tatsächliches Host-Projektspeichern **NOT RUN**. |
| **C07 / Texteingabe** | [Parser](../src/ValueEntry.hpp#L11) **PASS** für Ablehnung von `+-25%` und `++25%` mit unverändertem Zielwert sowie Annahme von `-25%` und `+25%`. P008 akzeptiert `+-25%`. Neue native Regression prüft u. a. `+-25%` ohne Edit. | Native Focus/Readouts **NOT RUN** in diesem Audit. Neues Recall-Interaktionsrisiko N03. |
| **C08 / Extremrate** | [Core](../src/ChopperCore.hpp#L55) prüft Rechenbereich über alle LENGTH-Werte. `DBL_MAX` abgelehnt, exakt dieselbe laufende Phase und Gain-Folge über weitere 5000 Frames. **PASS**, P008 scheitert. Ungewöhnliche praktikable Raten im aktuellen CTest weiterhin grün. | Der native Adaptertest prüft danach Dry-PCM. Das unabhängige Core-Orakel prüft zusätzlich die nichttriviale aktive Phase. |
| **C09 / Clock-Lifetime und Queue** | Definierte Policy hält Clock pro Instanz über Reaktivierung vor, startet aber die Phase neu. **PASS** für PCM-Gleichheit nach Restart mit letztem Takt. Der Native-Test prüft Stop/Start, neue Instanz, gültiges Nachfolgeevent und Queue-Erholung nach NaN-Prefix. | Native Ergebnisdetails **NOT VERIFIED**. Policy-Verhalten im Ziel-DAW-Projekt **NOT RUN**. |
| **C10 / GUI-Grenzen** | [Readout](../src/Editor.hpp#L54) bewahrt offenen Texteingabepuffer. [Preview](../src/Editor.hpp#L212) verschiebt MID-Glyphen am Rand. [Attach](../src/Editor.hpp#L385) prüft Assets und propagiert Ablehnung. Native positive und vier isolierte Negativfälle verlangen Fehlerstatus, kein HWND und keinen Resize; Extrem-Preview prüft reale Pixel und unterschiedliche Dichte. | GUI-Pfade nativ **NOT RUN** in diesem Audit. Live-Recall mit offenem Editfeld, variierende DPI, gleichmäßig verkleinerte Atlanten und native Fenstererzeugungsfehler fehlen. |
| **C11 / Echtzeit** | [Native Gate](../tests/NativeVst3Tests.cpp#L246) verlangt null gemessene Dauerüberschreitungen, Stereo-Grenzen und zwei Energieratios. Eigene quellgelinkte Wiederholung liefert 1/384 Dauerüberschreitungen bei 192 kHz/32. **FAIL** für diese Profilbedingung, Ursache **INCONCLUSIVE**. | Scope für tatsächliche Windows-Ausgabe und ASIO getrennt. Das native Energiegate hat das Gegenbeispiel N06. |
| **C12 / Notices, Archiv** | [CMake](../CMakeLists.txt#L49) liefert beide Notices. [Workflow](../.github/workflows/windows.yml#L45) vergleicht staged Notice-Hashes vor den Tests. Eigene synthetische Gegenprobe weist fehlende Notice, verändertes SDK-Icon und Zusatzdatei im Bundle-Root ab. Der Byte-/CRC-Abgleich der Testextraktion stimmt. | Windows-Originalarchive und Clean Machine **NOT VERIFIED/NOT RUN**. Exakte Pfad-Allowlist bleibt offen (N04). |

Die vorhandenen 4/4 Linux-CTest-Prüfungen wurden erneut im Release-Modus ausgeführt. Drei funktionale Prozesse plus erwartete Negativkontrolle. Weitere eigene positive Kontrollen prüfen fünf Waveforms bei vier Sampleraten (maximaler Fehler 9,99e−16 für die verglichenen Innenbereiche), MIX-PCM über unterschiedliche Partitionen und laufende Restore/Save-Überlappung (20.000 Anfragen, 1.360 parallele Audio-Callbacks, null falsche State-Generation). Die GUI-Lab-Planprüfung bestätigt aus UIDESC, Plan und Hashes die 13 IDs, 20 Drag- und 9 Klickaktionen sowie ihre Ausgangskoordinaten. Unterdeckung für Text, Hilfe, Resize, BYPASS, PCM und Recall bleibt explizit. Das setzt die in P008 erfassten UI→Host→DSP-Anforderungen für die 13 sichtbaren Parameter nicht außer Kraft. [Linux-Logs](p009-codex-evidence/logs) · [Lab-Plangenerator](../tools/prepare_lab_control_plan.py)

### Getaktete Linux-Wiederholung

Zwei Sample-Raten und drei Blockgrößen, 384 Callbacks je Profil, sechs dichte Automations-Lanes, Stereo64. Gemessene Zeiten in µs. Bei 192 kHz/32 gab es drei Fertigstellungsüberschreitungen und eine Callback-Dauerüberschreitung. Das sind überlappende Metriken. CPU-Zeit und Scheduler-Preemption wurden nicht getrennt gemessen. Null gezählte C++-`new/new[]`-Aufrufe beweisen keine vollständige Heap-/OS-Abdeckung.

| Hz / Frames | Budget | p95 / p99 / Max | Callback-Dauer über Frist / Scheduler-Frist überschritten |
| --- | ---: | --- | --- |
| 48.000 / 32 | 666,67 | 23,04 / 27,83 / 36,38 | 0 / 1 |
| 48.000 / 128 | 2.666,67 | 40,64 / 108,66 / 518,19 | 0 / 0 |
| 48.000 / 512 | 10.666,67 | 218,74 / 877,52 / 1.696,58 | 0 / 0 |
| 192.000 / 32 | 166,67 | 28,92 / 104,71 / 203,51 | 1 / 3 |
| 192.000 / 128 | 666,67 | 43,27 / 81,51 / 110,57 | 0 / 0 |
| 192.000 / 512 | 2.666,67 | 253,00 / 639,66 / 1.081,13 | 0 / 0 |

## Neue Befunde

### N01 · Hoch · Reentrantes Abmelden invalidiert die Editor-Benachrichtigung

[EditorPreferences::notifyUI und remove](../src/Editor.hpp#L25) iterieren bzw. verändern denselben Vektor. [syncPreferences](../src/Editor.hpp#L544) ruft währenddessen `IPlugFrame::resizeView` synchron über das SDK auf. Wenn in dieser Rückrufkette eine View geschlossen wird, entfernt ihr [close](../src/Editor.hpp#L466) den Eintrag. Das trifft auch auf das Setzen eines UI-States zu.

**Reproduziert am unveränderten Registry-Code:** Zwei Callbacks ohne Abmeldung liefern Aufrufe 1,1. Der erste Callback meldet sich im negativen Szenario während der Benachrichtigung ab. Erwartet: Der zweite, weiterhin registrierte Callback wird einmal gerufen. Tatsächlich wirft die GCC-Ausführung `std::bad_function_call`. Das ist ein konkreter Fehler bei reentranter Nutzung des Registers, verbunden mit Iterator-/Callback-Lifetime-Invalidierung. Der auslösende Ablauf in einem realen Windows-Host bleibt **NOT RUN**. Der existierende Native-Test schließt erst nach der Rückkehr aus Recall/Resize.

**Korrektur:** Benachrichtigung über stabile Einträge mit eigener Lebensdauer und vor Aufruf geprüftem Registrierungszustand durchführen. Native Negativproben mit synchroner Abmeldung der aktuellen und einer späteren View während `resizeView`, UI-State-Recall und Host-Dirty-Benachrichtigungen ergänzen. Nach dem Rückruf sichere View-Lebensdauer bis zum Ende des Call-Stacks prüfen. [Gegenprobe](p009-codex-evidence/reentrant.cpp)

### N02 · Mittel · Fraktions-DPI kann beim kleinsten Zoom durch eigene Rundungsregeln scheitern

**Quellbefund.** Bei 75% Benutzerzoom und DPI-Meldung `1.28f` fordert der SDK-Pfad `CFrame::setZoom` → `VST3Editor::requestResize` die abgerundete Breite 921. [Editor::onSize](../src/Editor.hpp#L501) verlangt mindestens `lround(720*1.28f)` = 922. Ein Host, der das angeforderte Rechteck unverändert quittiert, erhält Ablehnung. Ein Host, der auf 922 korrigiert, trifft außerdem auf die SDK-Prüfung des unveränderten `sizeRequest`-Rechtecks. Der reguläre Test nutzt 1,25 als Benutzerzoom und ganzzahlige Pixel in der DPI-Strecke. Native Reproduktion **NOT RUN**.

**Korrektur:** Einheitliche Ganzzahlkonvention für Größenanfrage, Minimum/Maximum und Quittierung. Proportionsregression bei Fraktions-DPI und Benutzerzoom 0,75/1/1,5, erlaubter Korrektur durch Host und abgelehnter Größenanfrage. [Größenpfad](../src/Editor.hpp#L470)

### N03 · Mittel · Ein altes, offenes Texteingabefeld kann nach Komponenten-Recall weiter committen

**Quellbefund.** P009 schützt den physischen Editpuffer vor Automation. [Controller::setComponentState](../src/Vst3.cpp#L300) setzt neue Werte und ruft die Preview-Benachrichtigung auf. Es fehlt die Invalidation des offenen [ValueReadout](../src/Editor.hpp#L54) für den neuen State. Bei Fokusverlust kann dessen alter, gültiger Text über `looseFocus` dennoch eine Host-Edit-Geste auslösen. Das Automations-Orakel prüft Enter/Escape innerhalb derselben State-Generation. Das interagierende Recall-Szenario **NOT RUN**.

**Korrektur:** Erfolgreichen Komponenten-Recall von fortlaufendem Automationsfeedback unterscheiden. Alte Edit-Generation vor der Modellumschaltung invalidieren. Tatsächliches Win32-EDIT offen halten, anderen gültigen Komponenten-State laden, danach Enter, Escape und Fokuswechsel prüfen. Recall, Host-Edit-Zähler und reale PCM vergleichen. Fehlgeschlagener Recall muss eine laufende gültige Eingabe erhalten.

### N04 · Mittel · Archivprüfung lässt fehlendes Modulinfo und zusätzliche Dateien unter Contents zu

**Reproduziert am echten Prüftool mit synthetischem Staging.** Die 17-Dateien-Positivkontrolle erbringt CRC und Byteidentität. Falsches SDK-Icon, fehlende Notice und fremde Bundle-Root-Datei werden abgewiesen. Ohne `Contents/Resources/moduleinfo.json` erstellt das Tool ein 16-Dateien-Archiv. Zusatzdatei unter `Contents/Resources` erzeugt ein akzeptiertes 18-Dateien-Archiv. [verify_candidate_archive.py](../tools/verify_candidate_archive.py#L33) vergleicht eine Teilmenge der Sollpfade und erlaubt unter `Contents/` alle übrigen Dateien. Damit ist die Extraktion bytegetreu, die exakte Auslieferungsstruktur bleibt offen. Für das echte Windows-Archiv wurde keine falsche Struktur festgestellt; seine Bytes waren nicht abrufbar.

**Korrektur:** Sollmanifest einschließlich der Modulmetadaten und freigegebener Begleitdateien als exakte Gleichheit prüfen. Negativfälle fehlendes Modulinfo, unerwartete Contents-Datei und falscher Metadateninhalt ergänzen. [Archivgegenproben](p009-codex-evidence/logs/gates.log)

### N05 · Mittel · Die Tester-Report-Schranke prüft Trefferzahl statt vollständige Testidentität

**Reproduziert an den Regexen aus dem Workflow.** Ein leerer Bericht wird abgelehnt. 57 identische `[PASS] duplicate placeholder`-Zeilen erfüllen die Inhaltsprüfungen. Der Workflow setzt zusätzlich echten Prozess-Exitcode 0 und genau einen Report voraus. Die Probe sagt nichts über die interne Reportproduktion des signaturgepinnten Testers aus.

**Korrektur:** Erwartete Pflichtfall-IDs und eindeutige Resultate aus dem Report parsen und zweifelsfrei an den Modulhash binden. Leere, doppelte, fehlende und unvollständige Pflichtfälle mit isolierten Textfixtures ablehnen. [Workflow](../.github/workflows/windows.yml#L140)

### N06 · Niedrig · Neuer Echtzeit-Signalcheck erkennt konstante Dämpfung als ausreichend

**Reproduziertes Orakel-Gegenbeispiel.** `y=0,5*x` erfüllt begrenztes Stereo-PCM und liefert beide geforderten Energieratios 0,25. Somit kann die neue Echtzeitprüfung einen monotonen Gain-Fallback akzeptieren, solange er fristgerecht ist. Ein tatsächlicher konstanter Gain des Produkts wurde dabei nicht festgestellt. Stationäre Shape-Orakel prüfen andere Szenarien.

**Korrektur:** Zusätzlich einen unabhängigen zeitlichen Gain-/Phasenvergleich für ausgewählte Ausschnitte unter derselben dichten Automation verlangen und einen konstanten Gain im Testhost als Negativkontrolle prüfen. [Echtzeitgate](../tests/NativeVst3Tests.cpp#L266)

## Abschlussstatus

- **Quellreview** der P009-Korrekturen und neuen Assertions abgeschlossen. **Funktionale Vollabnahme bleibt offen.** N01 und N04/N05/N06 sind reproduziert. Native GUI-Auslösung von N01, Fraktions-DPI und reentrantem Projekt-Recall bleiben **NOT RUN**.
- **Windows Validator, Tester und Test Lab** haben einen öffentlich erfolgreichen Schritt auf dem exakten SHA. Einzelreports, Original-ZIP und veröffentlichte Modulhashes konnten unabhängig nicht mit heruntergeladenen Bytes abgeglichen werden. In diesem Audit neu ausgeführt wurden Linux-Quellproben und synthetische Archiv-/Plangates. Das ist der Umfang der eigenen PASS-Nachweise.
- **Zielabnahme:** Studio Pro mit realem Projekt-Save/Close/Reopen, Automation/Bypass und DPI sowie physisches ASIO, kompletter Realtime-Trace, musikalisches A/B und Clean-Machine-Installation sind **NOT RUN**. Für einen neuen Freigabeentscheid sind die offenen Befunde und Gates auf dem exakten Windows-Kandidaten zu schließen. Keine Freigabe als FULL/FINAL.
- Dieser Nachaudit ändert ausschließlich Bericht und Nachweise. Glass-V2-Design, Produktcode, Produkttests und Workflows sind unverändert.
