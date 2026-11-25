# 125A Chopperator V1.0 — authoritative current baseline

**Windows x64 VST3 freeware, unrestricted FULL edition. Technical/package gates PASS.** Owner designated V1.0 and explicitly confirmed freeware on 2026-11-25. No Gumroad distribution planned; publication channel not specified. Owner confirmed the rest is okay on 2026-11-25; detailed independent DAW/clean-machine evidence remains unconfirmed.

## Source and QA

- Authoritative repository: `challanger2000/125A-Chopperator-Final`, stable reference `main`. For further development, use an intentional version branch.
- Exact V1.0 build source: `253dbea2ec140e2fc8d7994242afb36e89f42f00`.
- Original technical GO: [Codex audit](docs/P009-M01-M03-FOLLOWUP-CODEX-AUDIT.md), audited development source `8859e8be9a3aa12a9f5439780d369e273a820e79`.
- Complete original snapshot: `e3e60a584a649e0f32645a495c474943a62a6eb8` in [development repository](https://github.com/challanger2000/125A-Chopperator). All 112 original blobs and modes matched before version/package/documentation-only edits. DSP, parameter IDs, state, GUI and original tests were preserved. Historical reproduction scripts referring to original SHAs belong to the original Git checkout.
- [Windows and portable CI 45625738732](https://github.com/challanger2000/125A-Chopperator-Final/actions/runs/45625738732): SUCCESS. CTest 5/5; validator 47/47; released Tester 57/0/0; native editor, Test Lab and archive checks PASS. Raw Windows logs inspected.
- CMake version 1.0.0; exposed VST3 version 1.0. No Demo baseline.

## Freeware distribution package

- [`125A-Chopperator-V1.0-Windows-x64.zip`](https://github.com/challanger2000/125A-Chopperator-Final/actions/runs/45625738732/artifacts/15026734717) — inner ZIP inside GitHub's outer artifact.
- Inner ZIP SHA-256: `b1d5072c850ddeed174ece4952748bb41b1286f47de5dc8fdb6abdfe56fff3a5`.
- Outer artifact SHA-256: `45b16f7b62bca804fc92c42d44e56f17fba6a9a218ff2955b24d459897f1a000`.
- VST3 module SHA-256: `788264be75f8eaae491b1473a5321983f7acccf53c249f12263fd7889a805922`.
- 19 entries: complete 17-file VST3 bundle including notices, DE and EN PDF manuals (5 pages each).
- PDF automated text/geometry/render checks PASS. Separate visual inspection NOT RUN. Artifact bytes were not independently downloaded; hashes are from CI and GitHub metadata.

Machine-readable provenance: [release-manifest.json](release-manifest.json). Historical development evidence: [AKTUELLE-VERSION.md](AKTUELLE-VERSION.md). Do not select older P009 artifacts in place of the V1.0 package.

## Remaining acceptance

Studio Pro practical acceptance details, physical ASIO and clean-machine acceptance are not independently verified. Owner's general acceptance recorded ("ansonsten alles okay"); no specific DAW test protocol or independent verification of delivered ZIP bytes supplied. Gumroad upload is NOT APPLICABLE; this is freeware. Do not infer publication or full practical QA from technical success. No new build needed for documentation changes.
