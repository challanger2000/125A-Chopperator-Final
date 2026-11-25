# P009 · Nachweise für M01–M03

Die Skripte beziehen sich auf `a276f0228acf58230ee5ca5c48267f062b5c79c1` und den vorhandenen Actions-Run `45506553532`. [Auditbericht](../P009-M01-M03-CODEX-AUDIT.md)

Vom Repository-Stamm mit Git, GCC 14.2, CMake ab 3.25, Unix Makefiles, ripgrep und Python 3 ausführen. Optional kann für den Syntaxcheck der SDK-Checkout `v3.8.1_build_84` mit seinen Submodulen als zweites Argument angegeben werden. Das Skript braucht das Git-Objekt des Prüf-SHA und schreibt seine Quellen und Binärdateien in das angegebene Ausgabeverzeichnis.

```bash
set -o pipefail
evidence=docs/p009-m01-m03-evidence
audit_work=$(mktemp -d)
bash "$evidence/reproduce.sh" "$audit_work" 2>&1 | tee "$audit_work/reproduce.log"
python3 "$evidence/run-metadata.py" | tee "$audit_work/run-metadata.log"
```

- `extract.py` übernimmt sechs unveränderte Methoden der produktiven Editor-Klasse. `host-contract.cpp` simuliert Resize, `onSize` und die Frame-Freigabe. Der Prozess liefert Exit 0, wenn Positivfälle und beide dokumentierten Gegenbeispiele beobachtet wurden. Das Modell führt keine Windows-Fensteroperationen aus.
- `reproduce.sh` baut außerdem den unveränderten CMake-Release-Prüfstand und führt CTest aus. Die letzte absichtliche Negativkontrolle muss unter CTest den erwarteten Fehlerstatus liefern. [Linux-Protokoll](logs/reproduce.log)
- `run-metadata.py` bestätigt den Prüf-SHA, die öffentlichen Job- und Schrittstatus sowie die äußeren Artefakt-Digests. Es verwendet ausschließlich Lesezugriffe. [Abrufprotokoll](logs/run-metadata.log)
