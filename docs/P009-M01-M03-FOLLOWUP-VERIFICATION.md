# P009 — second targeted M01/M03 follow-up verification

Exact product source: 8859e8be9a3aa12a9f5439780d369e273a820e79
CI: [45512723609](https://github.com/challanger2000/125A-Chopperator/actions/runs/45512723609), portable job 138864267703, Windows job 138864417915, both SUCCESS. Raw Windows job log inspected.

## Follow-up findings from independent audit 3a1773f4e6ff3b3e1b79194d8442040141cb14b4

- M01: same-pixel recall without host onSize acknowledgement: native NATIVE_P009_SAME_PIXEL_RECALL_NO_ACK_PASS. Both actual editors take exact logical zoom 1.0008 and subsequently reach 1921×1152 at 200% DPI; reverse recall returns 1920×1152. No dirty notification, no redundant host resize.
- M03: NATIVE_P009_DPI_DETACH_M03_PASS. Native test checks child HWND destroyed immediately on return from removed(), destroys parent HWND inside host resize callback, verifies rejection return and surviving peer.
- Original fractional DPI regression NATIVE_P009_FRACTIONAL_RECALL_M01_PASS and all previous native recall, text, peer, DPI, resource and lifecycle regressions PASS.
- M02: unchanged registry implementation, portable Release gate SUCCESS; previous independent GCC 14.2 confirmation remains applicable to unchanged portable sources.

## Full candidate gates

Windows 5/5 CTests; Steinberg validator 47/47; released Tester 57 PASS / 0 WARNING / 0 FAIL; identity negative controls PASS; independent Test Lab TECHNICAL_BASELINE_PASS and scoped REQUIRED_COVERAGE_COMPLETE. Paced 384 callbacks, mean 76.8388 µs, p99 133.5 µs, max 490.4 µs, budget 2666.67 µs, zero overruns. PCM oracle relative squared error 1.54519e-26, constant gain negative 0.17347. Intentional SINE oracle negative emits FAIL SINE carrier oracle; enclosing gate succeeds as expected. Archive five negative controls PASS, 17 files byte identity, extracted validator 47/47 and 20 lifecycle cycles PASS.

Module SHA-256 (Tester/Lab logs): 887400da3fd45451efee7bfed188d38ae409be7933ce32776fea1790533224a0
Inner candidate ZIP SHA-256 (archive gate): 907fe655872438de98620995b37a6d47fd75cacb93ee292e5a53b9c763b583c5

Outer GitHub artifact metadata:
- 125A-Chopperator-P009-QA-evidence: 14977440471, sha256:1b04d0dcff486506e649edf9d26baf68c83a1b36b1b41d936364ba66d5d45f70
- 125A-Chopperator-P009-Windows-x64-development: 14977420599, sha256:f9c68f25de96abf643ee795b9e1e38a3dbcb57a1b1ed334d9bcff4e8e93644c5
- 125A-Chopperator-P009-verified-candidate-archive: 14977116333, sha256:526bb76d1afba6d40651e7f0572113d312b140884bac040e125c2a9989293964

Evidence: connector-accessed job logs and artifact metadata, not independently downloaded bytes. Targeted correction tests PASS; independent technical release decision on exact new source still pending. Studio Pro practical acceptance, physical ASIO, listening and clean-machine acceptance NOT RUN. No product code, DSP or Glass-V2 change in this documentation commit. No new build requested.

