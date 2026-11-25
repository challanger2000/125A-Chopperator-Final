# P009 · Nachweise des abschließenden Nachaudits

Prüfstand `3bea0580c10fb30943c405b230bdf3268519c6e2`. Dateien in `logs/` enthalten die eigenen Rohbeobachtungen und die öffentlich abrufbaren Metadaten des vorhandenen Windows-Jobs. [Auditbericht](../P009-FINAL-CODEX-AUDIT.md)

## Reproduktion unter Linux

Voraussetzungen sind Git, GCC 14.2, Python 3, GNU Make und CMake ab 3.25. Die Git-Objekte `aade956` und `3bea058` müssen lokal vorhanden sein. Die Befehle laufen vom Repository-Stamm und halten Buildausgaben in einem separaten temporären Verzeichnis.

```bash
set -o pipefail
evidence=docs/p009-final-evidence
audit_work=$(mktemp -d)
git clone --branch v3.8.1_build_84 --depth 1 --recurse-submodules --shallow-submodules \
  https://github.com/steinbergmedia/vst3sdk.git "$audit_work/sdk"
cmake -S "$audit_work/sdk" -B "$audit_work/sdk-build" -DCMAKE_BUILD_TYPE=Release \
  -DSMTG_ENABLE_VSTGUI_SUPPORT=OFF -DSMTG_ENABLE_VST3_PLUGIN_EXAMPLES=OFF \
  -DSMTG_ENABLE_VST3_HOSTING_EXAMPLES=OFF
cmake --build "$audit_work/sdk-build" --parallel 4 --target sdk sdk_hosting
bash "$evidence/reproduce.sh" "$audit_work/sdk" "$audit_work/sdk-build" "$audit_work/results" \
  2>&1 | tee "$audit_work/reproduction.log"
bash "$evidence/core-check.sh" "$audit_work/core" 2>&1 | tee "$audit_work/core.log"
git worktree add --detach "$audit_work/candidate" 3bea0580c10fb30943c405b230bdf3268519c6e2
python3 "$evidence/verify-gates.py" "$audit_work/candidate" "$audit_work/sdk" |
  tee "$audit_work/gates.log"
git worktree remove "$audit_work/candidate"
python3 "$evidence/windows-metadata.py" | tee "$audit_work/windows.log"
```

- `reproduce.sh` extrahiert Produktquellen direkt aus Git. Processor-Orakel und Stressprobe nutzen die bestehenden Nachweisquellen. `recall.cpp` verbindet die Controller-Implementierung aus dem Prüf-SHA mit einem kleinen Editor-Testdouble. `registry.cpp` reproduziert den erwarteten Altfehler und prüft die aktuelle Registry. `dpi.cpp` reproduziert M01 mit den originalen Formeln und einer direkten Größenquittierung. Der gemessene Stress-Exitcode steht im Log. Variierende Fristverletzungen auf Linux sind Messdaten. [Ausgabe](logs/reproduction.log) · [Erster Lastlauf](logs/stress-first.log) · [Zweiter Lastlauf](logs/stress-second.log)
- `core-check.sh` baut mit projektseitigen Standardeinstellungen und verlangt beim Prüf-SHA den beobachteten GCC-Fehler. Anschließend führt es alle Assertions mit einer einzelnen herabgestuften GCC-Warnung aus. CTest erwartet für `release_negative_control` einen Fehlercode. [Ausgabe](logs/core.log)
- `verify-gates.py` benutzt die echten Python-Gates auf isolierten synthetischen Dateien. Es überprüft strukturelle Archivfehler, Bytegleichheit des Positivfalls, die UIDESC-Rechtecke des Lab-Plans, 57 Testeridentitäten und den Modulhash-Vergleich. Der Windows-Factory-Vergleich benötigt das echte Modul und SDK-`moduleinfotool.exe`. [Ausgabe](logs/gates.log)
- `windows-metadata.py` liest vorhandene Run- und Jobseiten. Es prüft den Prüf-SHA, die Schrittabschlüsse und veröffentlichte Artefakt-Digests. Der Abruf löst keine GitHub Actions aus. [Ausgabe](logs/windows.log)
