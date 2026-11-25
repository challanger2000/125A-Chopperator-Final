# Requirement-to-evidence matrix (P007 verified baseline; P008 in execution)
This is a working gap register, not a FULL AUDIT or final approval. Verified baseline is exact commit 722f1edf65d5175f5066d97675944a2edaaae985, Windows run 45310491485. P008 code commit 5e2c5ad3bf8e9ffb33e89856bb1e3b983a616cfe changes readouts/version and adds a reset gate. No P008 result inferred from P007.

## Exact evidence
- Native run and staged bundle: [45310491485](https://github.com/challanger2000/125A-Chopperator/actions/runs/45310491485), artifact 14882837411. Downloaded ZIP SHA256 matched 49340a7d989aee8650665cf15bb006e7e7a3a7ccf4c20b45532fc8e9d3d2db94; CRC verified; 15 bundle entries including x86_64-win module, moduleinfo and all eleven GUI resources.
- QA evidence artifact 14882099065; downloaded ZIP SHA256 matched 0aed654d6537b7607320a38210b9c93138767450879be2ed8313043faa0c9bd7. Actual native-editor.bmp inspected at 960x576. Independent released 125A Tester v0.2.10 report 56 PASS / 0 WARNING / 0 FAIL. Five editor lifecycle cycles; I/O/event isolation; 100 independently guarded fresh-process rate/block/format profiles; offline/lifecycle stress; finite denormal input results; limited monitored callback API trace zero heap/blocking calls, NOT a complete custom allocation/lock proof.
- P007 native editor tested two attach cycles, thirteen parameter hit mappings, three segment interactions, all 66 length choices by left drag both directions, note and width drag, wheel step tests, 75% resize and 100% restore, genuine screenshot. Core tests and 47/47 validator passed, six programme PCM checks passed.

## Control chain inventory
Common implementation: resources/chopper.uidesc -> src/Editor.hpp/VSTGUI listener -> Controller::setParamNormalized and host IComponentHandler -> Processor::process/Lane -> HostEngine::set/apply -> Parameters::settings -> Core::tick/advance. State: component encode/decode, mailbox restore, controller component-state sync. These paths are identified in source; path existence is NOT proof of every end-to-end outcome.

| Control / ID | P007 GUI evidence | P007 DSP/host evidence | Missing or boundary check |
|---|---|---|---|
| START 100 | hit, drag, wheel | rate anchors, continuous phase trajectory | UI callback to independent audio oracle, all endpoints and automation |
| MID 101 | hit | rate anchors, per-sample trajectory | native individual drag, feedback, recall, end-to-end |
| END 102 | hit | rate anchors, trajectory | native individual drag, feedback, recall, end-to-end |
| LENGTH 103 | hit, 66 choices both directions, wheel | all 66 mapped, 64-bar exact core duration, native quarter-bar completion/recall | each UI action relayed to audio timing, non-4/4 full native |
| MID POSITION 104 | hit | moved midpoint core oracle, paced automation | independent native output for non-default midpoint, GUI drag and feedback |
| CURVE 105 | hit | nonlinear midpoint core oracle, paced automation | independent native nonlinear PCM trajectory, GUI drag and feedback |
| SHAPE 106 | hit, selected SINE | all five native analytic waveform oracles | each GUI shape relayed to audio; all combined width and envelope edge cases |
| WIDTH 107 | hit, continuous drag | native width zero in timing fixture, core endpoints and programme settings | UI-to-PCM full chain, intermediate width for all shapes |
| ATTACK 108 | hit | core measured slopes 100 ms at four sample rates, native minimum edge | native variable envelope timing and GUI-to-audio |
| RELEASE 109 | hit | core measured slopes 50 ms at four sample rates, native minimum edge | native variable envelope timing and GUI-to-audio |
| MIX 110 | hit | exact dry and native sample-offset ramp, programme dry | GUI drag, full-range callback/PCM, state feedback |
| ACTIVE 111 | hit, click, balanced host edits | native sample-offset PCM, core retrigger | direct UI callback relay and playback/seek use in Studio Pro |
| MODE 112 | hit, click, balanced host edits | core fractional phase carry loop | native noninteger-cycle loop and direct UI callback relay |
| host BYPASS 113 | not a custom GUI control by design | native settled dry and warm phase in host-engine oracle | DAW bypass semantics, state recall |
| preview | real screenshot with visible curve | source uses independent reference Core instance | compare displayed curve with its stated reference; no actual host-clock or live-playhead claim |
| resize | 720x432 and 960x576 actual HWND/report | N/A | 150%, scaling/DPI, mouse interaction after scaling, Studio Pro |

P008 adds all-ten-knob Ctrl+left native reset tests; pending until actual CI. No need to change existing product layout or DSP for missing QA evidence. Full GUI-to-audio automation/recall and per-control negative/edge coverage still NOT VERIFIED. The existing released tester automation stress proves accepted queues/finite output, not all semantic outcomes.

## Remaining release gates
- Full requirement-to-test chain, explicit negative oracle challenge, independent reviewer/audit (released tester is independent automated tooling, not human review).
- Programme subjective evaluation using level-matched A/B (including distorted performed powerchord if authorized fixture available). Existing two real isolated guitar note recordings and rendered pad, six independently checked PCM outputs, not human audition.
- Intended DAW Studio Pro: real project state, all controls, host activation/automation, editor reopen, continuous resize and listening. Only after technical candidate acceptance, no repeated interim requests to owner.
- Final version/package/install path QA and approval. No public FINAL/Gumroad claim.

## P008 verified delta
Run 45312830818 success: 56/0/0 independent tester, all-ten-knob Ctrl+left reset twice, native screenshot reviewed against owner timing diagram reference, existing DSP regressions pass. Actual timing preview pixel oracle on all seven relevant left-drag controls now added; pending run. No full audit or DAW acceptance yet.

P008 reactive oracle 45314075565 FAIL: START change retained exactly baseline pixels; direct parameter dependency refresh correction and repeat gate pending. New help and native corner-drag controls added; tooltip runtime verification, resize at intermediate sizes, scaled hit behavior and DAW test pending.

Runs 45314685057/45314850044 fail reactive preview after START drag despite compile/DSP/validator PASS. Dedicated timer + canonical draw-time refresh + controller notification and explicit repaint introduced. Dynamic pixel/zoom/help/recall remain NOT VERIFIED; forced repaint only diagnostic, not acceptance.

Run 45415026213: independent composed-pixel preview gate passes all seven left-drag settings, zero stable pixel mismatches; prior child DC method returned stale image. Editor gate fails first corner drag intermediate zoom. New fix/diagnostics pending; no Studio Pro acceptance.

45416468673 confirms responsive composed-pixel preview again. Grip reached requested 1115x669, then test's fake WM_MOUSEMOVE disagreed with real cursor after window resize. Real desktop pointer/button gesture fixture replaces synthetic grip movement; no reduced geometry gate. New Windows result pending.

## Verified P008 full workflow 45417400060
Exact commit b6e9fccce05cd5bcf82b590451bc84226cd4e8e3: 4/4 CTests, 47/47 validator, released tester 57/0/0, all-seven timing diagram reactive oracle, 2x actual corner gestures through three intermediate sizes, scaled hit mapping, saved nondefault zoom/help payload, fresh pre-attach recall, component-state independence and truncated state rollback PASS. Six programme checks PASS. Adds real visible tooltip toggle and fresh attached native recall geometry gates; not yet executed. All remaining full audit, DAW and listening gates still apply.

45424188334: native platform popup START visibility not proven; separate preview and actual corner drag pass. Replaced popup with in-canvas mouse-transparent VSTGUI overlay. Actual composed pixel visibility/two contextual text signatures/exit/off gates and attached recall test pending. No release approval.

## Verified P008 full workflow 45425299923
Exact 3b5877be6f0ee0dc35aed518ff25981cced33e5d: 4/4 CTests, 47/47 validator, independent tester 57/0/0, all-seven composed-pixel timing display, actual corner drag, visible in-canvas help on/off, fresh actual restored 1200x720 child and scaled hit mapping, UI state rollback, six programme PCM oracles PASS. New native GUI audio chain parity test for all 13 controls and fresh state recall added; execution pending. Studio Pro/listening/full audit remain open.

## P008 candidate 45427106965 (473bbbca236e3cb8d8a51e4c7dbcbf8b8136f8a4)
Complete workflow PASS; all thirteen actual native GUI-to-processor control paths, balanced host edits, no cross-parameter edits, expected gesture values, explicit automation PCM parity, meaningful change against unchanged baseline, and fresh controller/component recall PCM PASS. Existing independent DSP/GUI/validator/tester/programme gates PASS. Exact downloaded candidate ZIP SHA256 25a2ad07ef67b6c2376f803569e6392a81f2d2a4f06241c684a72a98b432c14a verified, ZIP CRC and complete bundle checked. Owner practical Studio Pro acceptance now next; no public FINAL release claim. Full audit and untested DPI/host-specific outcomes remain open.

Owner double-click entry requirement: SDK CTextEdit fields for all ten knob readouts; native valid entry, invalid input and Escape tests added, including normalized values and edit callbacks. Pending exact CI.

Updated native composed-pixel help test for centered unboxed text: baseline exact when off/leave, two different visible glyph patterns, centered pixel bounds and no solid panel. Pending CI.

45445714729 full workflow PASS, including native double-click numeric entry and centered unboxed help. New vector-only glass faceplate styling behind existing controls; run complete regression and inspect real native capture, not generated concept art. Pending.


## 2026-11-24 adversarial audit
See [P008 adversarial audit](P008-ADVERSARIAL-AUDIT.md). Exact Glass V2 build remains technically green; new local adversarial source probes detect malformed signed input and unrepresentable sample rate; DPI/live UI recall source findings and license notice package gap. PARTIAL / NOT VERIFIED, release NO-GO. No production change. Codex and full Lab evidence pending.
