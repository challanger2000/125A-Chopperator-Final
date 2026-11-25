# V1.0 — distribution clarification (2026-11-25)
- Owner confirms freeware, not Gumroad. Documentation only; unchanged binary and ZIP.

# V1.0 — first FULL distribution candidate (2026-11-25)
- Transfer audited P009 baseline to Final repository; exposed VST3 version 1.0.
- Preserve DSP, GUI, parameters, states and tests.
- Add bilingual PDF manuals and customer ZIP gate. See CURRENT.md for pending package acceptance.

# Changelog
## P001 / 0.1.0
- Standalone phase-integrating audio gain modulation and functional contract.
- START/MID/END, LENGTH, midpoint, curve, width, five shapes, attack/release, MIX, activation, one-shot/loop.
- Deterministic tests added; no VST3 adapter or GUI yet.

## P002 / 0.2.0
- Stable normalized parameter IDs 100–113, shared discrete mapping and defaults for future host/controller paths.
- Extended local Release tests for all note divisions and lengths, envelope timing, 64 bars and unusual sample rates.

## P003 / 0.3.0
- Host-engine state mailbox, pending restore precedence and warm bypass.
- Initial native Windows VST3 adapter: sample-accurate parameter queues, stereo/mono and 32/64-bit processing, versioned state, host tempo/meter; generic host controls only.
- Add one Windows build/validator workflow; no release acceptance yet.

## P004 / 0.4.0
- Real packaged-VST3 native timing/automation/recall test harness (Windows CI gate).
- Reset reused automation lane on initialization; preserve established DSP and parameter IDs.

## P005 / 0.5.0
- Add actual-module coherent AM spectrum, folded rectangular sideband, all five shapes, and paced native dense automation measurements.
- No DSP algorithm change. Verify spectral oracle with a deliberately wrong expected carrier.
- P005 CI correction: consume intentional negative-control exit only after verifying both exit code and diagnostic; preserve failed-run evidence. No plugin source change.

## P006 / 0.6.0
- P005 exact Windows repeat succeeded including spectral negative control.
- Native PCM checks using pinned real distorted guitar recordings and rendered pad; evidence WAVs separate from plugin.
- Independent official released 125A Plugin Tester v0.2.10 pinned executable and report gate. No DSP changes.

## P007 / 0.7.0
- P006 complete Windows QA success and static design visual review.
- Native VSTGUI editor with existing parameter bindings, official logo and all four original knob resolutions. Remove transparent frame padding and tile without altering retained pixels. Core-derived preview at reference tempo/meter, no fake playback indicator. No DSP change.
- P007 resource-copy correction for Windows package names with spaces, native editor host interaction gate.
- P007 SDK audit: correct normalized discrete wheel increments and resource filename scale suffixes; verify all four actual loaded bitmap resolutions on attach; max editor scale and native wheel checks.
- Primary linear left-drag independent of DAW knob mode; explicit 66-length bidirectional native drag oracle. Mouse wheel optional.
- P007 resize gate found a stale reported rectangle; forward accepted aspect resize through SDK base and check native child geometry.

## P008 / 0.8.0
- P007 full Windows QA success, exact native screenshot inspected.
- GUI-only compact percent readouts, preserve full parameter precision.
- Native Ctrl+left reset oracle for all ten knobs, including host edit notifications.
- Owner display correction: filled rhythmic impulse preview with shape contour; screenshot at agreed defaults rather than arbitrary QA state.
- Exact owner SVG display reference: equal-height orange timing rectangles, variable width and spacing, labels below, dashed vertical grid. Supersedes filled envelope interpretation. No DSP changes.
- P008 exact native GUI and reset checks passed; added rendered pixel-level dynamic timing display oracle for seven relevant controls, not a static screenshot test.
- Fix stale timing display detected by native pixel oracle; parameter-dependent refresh.
- Add switchable contextual ? help and actual bottom-right proportional resize grip.
- Persist per-instance GUI zoom and ? help in separate VST3 controller state, independent of audio presets.
- Correct live preview refresh via explicit controller notifications, dedicated GUI timer and coalesced invalidation; native pixel diagnostic preserves independent acceptance oracle.
- Separate fresh native preview and editor gates; add opt-in model/render diagnostics and independent composed-desktop pixel capture to isolate stale display/capture.
- Verify live preview in actual composed pixels; eliminate recursive host resize request during accepted zoom and log actual grip negotiation.
- Fix native corner QA to use real desktop mouse movement and button state; validate uninterrupted resize at multiple intermediate sizes.
- Confirm full P008 green Windows workflow; extend native coverage for actual tooltip visibility and fresh attached restored editor geometry.
- Replace unverified platform popup tooltips with scaled in-canvas contextual help; test visible composed pixels for two controls and disabled state.
- Verify all P008 GUI requirements in native Windows; extend QA with actual GUI-to-host-to-processor PCM and fresh state recall across all thirteen controls.
- P008 verified all thirteen native GUI-to-audio paths and exact fresh state PCM recall; supply unchanged, hash-verified Windows x64 VST3 candidate for owner practical acceptance.
- Add double-click value entry for all ten knob readouts using SDK-native text edits; strict note/bar/percent parsing and cancellation.
- Simplify contextual help to a centered muted text line without panel or border.
- Add restrained glass-like faceplate lighting, recessed panel shading and fine display rim, preserving original controls and pulse geometry.
- Increase glass faceplate reflection contrast and bevel lighting after owner visual feedback.


## P009 / 0.9.0 development candidate — NOT RELEASED
- Targeted audit corrections: independent tempo/meter validity, state stream return codes and counts, inactive bus access, representable clock/setup, strict numeric input, live UI preferences and DPI scaling, retained text edit, high-DPI logo and pinned dependency notices.
- Actual loaded VST3 counterexamples and standalone core rollback tests added; existing evidence and tests preserved. Native UI interactions, exact Windows results and final package acceptance still unverified at commit time.

- P009 exact Windows run 45487606108 SUCCESS, including independent scoped control matrix, resource rejection, exact candidate archive and extracted-module tests. Independent re-audit and owner DAW acceptance pending; no release promotion.
