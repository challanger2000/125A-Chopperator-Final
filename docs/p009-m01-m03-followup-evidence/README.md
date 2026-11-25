# P009 · Nachweise zu den M01/M03-Folgefehlern

Prüf-SHA `8859e8be9a3aa12a9f5439780d369e273a820e79`, vorhandener Run `45512723609`. [Auditbericht](../P009-M01-M03-FOLLOWUP-CODEX-AUDIT.md)

Vom Repository-Stamm mit lokal vorhandenem Prüf-SHA, Git, Bash, Python 3 und einem C++20-Compiler ausführen. Alle generierten Dateien liegen im angegebenen Ausgabeverzeichnis.

```bash
set -o pipefail
evidence=docs/p009-m01-m03-followup-evidence
audit_work=$(mktemp -d)
bash "$evidence/reproduce.sh" "$audit_work" | tee "$audit_work/reproduce.log"
python3 "$evidence/run-metadata.py" | tee "$audit_work/run-metadata.log"
```

`reproduce.sh` übernimmt unveränderte Methoden aus exakt diesem Git-Commit und nutzt die Typen des vorigen Host-Modells. Es führt nur die neuen Assertions zu gleichem Pixelrechteck und sofortiger Child-Freigabe aus. Die Fensteroperationen sind modelliert. [Eigenlauf](logs/reproduce.log)

`run-metadata.py` liest den vorhandenen öffentlichen Run und die Jobseiten. Es prüft den exakten Commit, den nativen Editor-Schritt und die veröffentlichten äußeren Artefakt-Digests. [Metadatenabruf](logs/run-metadata.log)
