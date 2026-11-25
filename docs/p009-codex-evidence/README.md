# P009 Codex-Auditnachweise

Die acht Protokolle enthalten die Beobachtungen aus dem unabhängigen P009-Nachaudit. [Auditbericht](../P009-CODEX-AUDIT.md)

## Reproduktion

Die Befehle laufen ab Repository-Stamm unter Linux mit Git, GCC 14.2 oder einem vergleichbaren C++23-Compiler, Python 3 und CMake ab 3.25. CMake muss mit GNU Make konfiguriert werden. Die Git-Objekte für P008 `33d81e8` und P009 `aade956` müssen lokal verfügbar sein.

```bash
evidence=docs/p009-codex-evidence
audit_work=$(mktemp -d)
git clone --branch v3.8.1_build_84 --depth 1 --recurse-submodules --shallow-submodules \
  https://github.com/steinbergmedia/vst3sdk.git "$audit_work/sdk"
cmake -S "$audit_work/sdk" -B "$audit_work/sdk-build" -DCMAKE_BUILD_TYPE=Release \
  -DSMTG_ENABLE_VSTGUI_SUPPORT=OFF -DSMTG_ENABLE_VST3_PLUGIN_EXAMPLES=OFF \
  -DSMTG_ENABLE_VST3_HOSTING_EXAMPLES=OFF
cmake --build "$audit_work/sdk-build" --parallel 4 --target sdk sdk_hosting
bash "$evidence/re-run.sh" "$audit_work/sdk" "$audit_work/sdk-build" "$audit_work/results"
python3 "$evidence/verify-gates.py" . "$audit_work/sdk"
python3 "$evidence/windows-metadata.py"
cmake -S . -B "$audit_work/ctest" -DCMAKE_BUILD_TYPE=Release -DCHOPPER_BUILD_VST3=OFF
cmake --build "$audit_work/ctest" --parallel 4
ctest --test-dir "$audit_work/ctest" -V
```

`re-run.sh` extrahiert beide gepinnten Produktquellstände aus Git und vergleicht ihre Processor-Ausgaben. Die P008-Gegenprobe und die reentrante Benachrichtigungsprobe müssen mit den dokumentierten Diagnosen fehlschlagen. Die Lastprobe protokolliert messabhängige Fristverletzungen. `verify-gates.py` betreibt harmlose synthetische Staging-Fixtures. Die Ausgabe mit `FAIL` verzeichnet die erwarteten Gegenbeispiele der aktuellen Gates. Die öffentlichen Windows-Metadaten können unabhängig erneut abgefragt werden. Das Skript bezieht keine Archivbytes oder Job-Einzelprotokolle.
