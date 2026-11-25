# P005 actual VST3 quality measurement plan
P004 established native functional baseline. Do not alter musical sound or add oversampling without measurement. 
- Native all five waveforms at 2 Hz with full width: independent analytic shape sampled in second cycle, excluding 100 samples at wrap, max error 1e-7.
- Sine AM at 8 Hz: 1000 Hz 0.5-peak input at 48 kHz, one-second coherent window following one-second warm-up. Expected carrier 0.25 and sidebands 0.125 each; residual RMS vs analytic waveform <1e-6. Negative control changes expected carrier to 0.5 and must fail specifically at carrier oracle. This tests assertion sensitivity, not a mutated binary.
- Rectangle at 64 Hz (240 BPM, 1/64), 50% width and minimal 0.5 ms slew: 23 kHz 0.5-peak carrier, measure folded 17th upper sideband at 23912 Hz. For ideal hard pulse the normalized complex coefficient magnitude is 1/(17*pi); continuous linear slew multiplies by abs(sinc(17*pi*0.0005*64)). Require below hard and within absolute 0.002 of predicted coefficient. This isolated component does NOT measure all alias energy or determine audibility. Multiplication shifts spectra, so even perfectly sinusoidal modulation can fold near Nyquist; zero alias is not promised. No anti-alias design decision before broader real-programme and reference measurements.
- Paced actual module, 48 kHz/128 sample blocks, 384 callbacks, dense mid-position/curve/width ramps, Windows cloud runner. Record mean/p95/p99/max callback duration and count durations exceeding 2666.67 us. Sleep outside process; not physical ASIO; no claim about scheduler XRUNs, allocations or all workloads. Timer overhead not subtracted. Record machine metadata if available. No hard deadline certification from this short probe.
- All earlier native, SDK and core regressions remain required.
- Real guitar/pad fixtures, independent 125A test tool, native full parameter matrix, GUI and Studio Pro NOT RUN. No commercial approval.

## Executed results — Windows run 45300605488
Source b08522c6b067146613b74c4ab2aa602fef765439; job 138131883189. Positive build and 4/4 CTests, Steinberg 47/47 PASS. Actual loaded staged module:
- All five shape oracles PASS.
- Sine AM: carrier 0.25, lower/upper 0.125 each, analytic RMS residual 7.32462e-14. PASS.
- Rect 64 Hz, 23 kHz carrier: folded 23912 Hz sideband -39.2755 dBc (hard reference -34.552 dBc; continuous-slew reference -39.2902 dBc). PASS for declared isolated-component oracle, not full alias-power certification. Approximately 4.72 dB less folded component than hard-edge reference. No claim of alias-free processing. No oversampling added.
- Paced 384 callbacks: mean 76.0271 us, p95 95.5 us, p99 134.6 us, max 518.5 us, deadline 2666.67 us, 0 measured duration overruns. Windows cloud runner, no physical hardware, no inferred ASIO PASS. CPU model not recorded.
- Prior native timing and state oracles PASS; 14909 open/7964 closed samples.
- Negative check emitted expected FAIL SINE carrier oracle and exit 1. Workflow erroneously propagated expected exit (GitHub PowerShell wrapper uses LASTEXITCODE); actual job FAIL and no artifact. Fix explicitly exits 0 only after matching both expected diagnostic and code. No DSP change. Rerun required.
## Remaining programme material
One concrete legal guitar fixture candidate: FreePats Electric Guitar FSBS dist2, version 2022-09-11, CC0, https://github.com/freepats/electric-guitar-FSBS-dist2/tree/21261b8bcb02d1cbf52dc02f1b1636df52d4a947 ; recorded and amp-processed Fender. Exact source file samples/bridge/E2_s1_01.flac, Git blob 13db1aafb87c7c63b110ba422e8794113fa4340e; A2_s2_01.flac blob 099f4ba10c56241c7e54d41524adf76fc74af30a. These are single-note recordings, not a real performed power chord. Freesound real power chord and pad sources require original-file download access. Candidates only: bytes not acquired or tested, NOT RUN.

## Corrected workflow run 45300890050
SUCCESS on 32711507f537bbe462548f28598fd823eb4ba1bd. Repeat of all positive oracles, spectral negative gate PASS, 384 callbacks mean 75.5229 us, p95 93.4 us, p99 131.7 us, max 562.5 us, 0 overruns. Artifact 14878567914, ZIP SHA256 f9659185ea31233343387f9f6a91323fd175f07d37a68f2e82087bbd7919365a.
