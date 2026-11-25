# P009 verification and independent re-audit handoff

## Exact verified candidate

- Source: `aade95612ccf492819221b16ff896d11d138c907` (main, P009 / 0.9.0).
- [Windows run 45487606108](https://github.com/challanger2000/125A-Chopperator/actions/runs/45487606108), job 138777034387: SUCCESS, 2026-11-24.
- Tested module SHA256: `417cab0996a0746e4ea5d3f6c1441a497f23201330b3e2c104ca2287f9585720` (reported by both independent Lab pipelines).
- Candidate ZIP SHA256: `cffd9f0cc07e1b6dff649ce753f2bdc791a08fd08ee5cac1d4eb12a398f51fad` (17 files, byte-identical extraction on Windows runner).
- [Candidate archive artifact 14963589373](https://github.com/challanger2000/125A-Chopperator/actions/runs/45487606108/artifacts/14963589373), outer GitHub artifact SHA256 `93940bf719053c2095117c87927394dfbfe743ca62c3f485e93b677c57d96776`.
- [QA evidence artifact 14963229366](https://github.com/challanger2000/125A-Chopperator/actions/runs/45487606108/artifacts/14963229366), outer artifact SHA256 `ce1b985febb4067b980416b403fb652344907e996ee5663c7f80ca342571452a`.
- Development bundle artifact 14963873935, outer artifact SHA256 `436c798444834cfe3d5473a334e703ea1b51bde058e1d9894556974e39c03c7e`.
- GitHub logs and artifact metadata inspected; archive bytes not independently downloaded in this chat. CI did verify the archive and extracted bundle. No clean-machine or physical ASIO claim.

## Reconciliation of audit findings

| Finding | Exact Windows evidence | Remaining limitation |
|---|---|---|
| C01 | Independent tempo/meter native PCM counterexamples PASS | Independent re-audit |
| C02 | Full-count stream error injection and rollback PASS for processor/controller/UI | Independent re-audit |
| C03 | Inactive nonnull bus sentinel and reactivation PASS | Full bus/sample-format cross-product not claimed |
| C04 | Two simultaneous attached views, live recall, resize rejection/recovery PASS | Commercial host behavior not tested |
| C05 | Native DPI interface 1→1.5→1, accepted/rejected changes, HWND dimensions and hit mapping PASS | Physical monitor transitions not tested |
| C06 | User zoom/help dirty counts and no recall/DPI dirty echo PASS | Optional/failing handler matrix not fully established |
| C07 | Standalone and native malformed-sign rejection PASS | Independent re-audit |
| C08 | Extreme rate rejected, previous setup/PCM preserved PASS | Independent re-audit |
| C09 | Native per-instance retained clock, processing pause, reactivation, fresh instance clock PASS | Defined retention policy subject to reviewer |
| C10 | Native text commit/cancel during automation, all ten readouts, all seven preview controls, extreme labels/density, 20 attach/detach, four damaged/missing resource probes PASS | Full focus, DPI and GUI edge matrix not independently established |
| C11 | Strict paced 384 callbacks, actual modulation energy and no overruns PASS | Original shared Linux observations not erased; physical ASIO and comprehensive RT allocation/CPU attribution not verified |
| C12 | SDK notices verified against pinned bytes; exact archive inventory/CRC/SHA and extraction PASS; extracted validator and 20 lifecycle cycles PASS | Clean machine install and rights review not established |

Original P008 reports and historical failures preserved, including the SDK attach false-success regression. Malformed automation queue valid-prefix behavior now also has native evidence. Four CTests, 47/47 Validator, released Tester 57/0/0 including 100 fresh process profiles; all five shapes, native spectral negative control, native GUI-to-PCM 13/13, 66 LENGTH choices, ten resets, help, grip, recall, six programme PCM cases passed. Pinned Test Lab P74 technical baseline PASS; P65/P67 scoped GUI control contract 13/13 cases, 29/29 actions PASS. The latter is not full product feature coverage. Latest paced mean 106.763 us, p99 171.3 us, max 250.6 us, deadline 2666.67 us, 0/384 overruns; wet energy ratio .222227 and change energy ratio .624714.

## Handoff to independent Codex reviewer

Review the exact above candidate source, not a floating branch, with the original P008 source and both audit reports as references. Read all mandatory current 125A-Engineering documents in full. Independently rerun the archived Codex counterexamples (adapt to test the corrected API without changing product code or treating obsolete expected failures as passes); use original failing revision or isolated mutants as negative controls. Verify the exact Windows evidence and report hashes, inspect all new code including state codec, native GUI, clock, bus, packaging and test gates; look for new regressions. Distinguish direct executed evidence, inspected CI evidence, source findings, and NOT RUN. Record each finding, severity, reproducible test, evidence and coverage gap. If a critical gate remains unverified, do not issue FULL AUDIT PASS. Preserve product sources and approved Glass V2; archive audit sources and logs in docs/P009-CODEX-AUDIT.md and docs/p009-codex-evidence, then commit documentation/evidence only on main.

**Current decision: Windows candidate verified for the executed gates; independent re-audit pending, no FULL AUDIT PASS, release NO-GO.** Studio Pro final owner acceptance, physical ASIO, actual listening and clean-machine installation remain NOT RUN. Do not repeat CI merely because documentation changes. 
