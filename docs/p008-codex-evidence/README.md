# P008 Codex-Auditnachweise

Diese Nachweise gehören zum [Auditbericht](../P008-CODEX-AUDIT.md) für Produktrevision `6ca70948c659898826fff8d9990a32fd29049339`. Die vier C++-Proben und der Python-Assetcheck sind aus dem Audit-Arbeitsverzeichnis übernommen. Die Buildskripte verwenden den jeweiligen Checkout und ein separates Arbeitsverzeichnis. Der Mutant wird beim Wiederholen erzeugt.

## Wiederholen unter Linux

Vorausgesetzt sind Git, GCC mit C++23, GNU Make, CMake ≥ 3.25, Python 3 mit `venv` und Zugriff auf GitHub und PyPI. Für Abweichungen im Toolpfad akzeptiert das Skript `CMAKE`, `CTEST` und `CXX`. Bei Wiederholung der Originalbefunde muss der Produktquellstand dem Auditstand entsprechen.

Vom Repository-Stamm aus:

```bash
bash docs/p008-codex-evidence/reproduce.sh
```

Das Skript benutzt standardmäßig ein mit `mktemp` erzeugtes Arbeitsverzeichnis. Ein anderes, für diese Reproduktion vorgesehenes Verzeichnis kann als einziges Argument übergeben werden. Dort werden SDK, Python-Umgebung, generierte Fixtures, ausführbare Proben und neue Logs abgelegt. Die archivierten Logs im Repository stammen aus dem ursprünglichen Audit. [Originalprotokolle](logs)

| Nachweis | Erwartung am Auditstand |
| --- | --- |
| `ctest.log` | 4/4 unter Linux einschließlich der erwarteten Release-Negativkontrolle. |
| `probe.log`, `clock-meter.log` | Exit 1 für die dokumentierten Processor-/Parser-Befunde, erfolgreiche Positivkontrollen im selben Lauf. |
| `negative.log`, `mutant.log` | Exit 1 bei bewusst falschem MIX-Orakel bzw. Testmutant mit deaktivierter Interpolation. |
| `extra.log` | Exit 0 für analytische Loop-PCM, Float32 und den validen Lane-Prefix. |
| `stress.log`, `stress-repeat.log` | Exit 1 für Bus- und Ratebefunde. Scheduling-Werte und Fristverletzungen können je nach VM und Systemlast variieren. |
| `asset-negative.log`, `programme-negative.log` | Je Exit 1 mit spezifischer Integritätsdiagnose; jeweils vor Erzeugung der betroffenen Ausgabedatei. |
| `programme-manifest.json` | Exakte Quellen- und PCM-Hashes der Audit-Fixtures. |

Die C++-Proben linken einen beim Build extrahierten Processor-Quellausschnitt gegen das gepinnte Steinberg SDK. Windows-Modul, native GUI, ASIO und DAW wurden in diesen Proben nicht ausgeführt.
