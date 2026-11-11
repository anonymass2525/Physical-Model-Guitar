# PMG 1.7.1 — Unified Diagnostic (research A/B)

This is a **new VST3 identity**, not a replacement for PMG 1.7. The baseline 1.7 code remains in `research/coherent-direct-body-v17`. No measured improvements are claimed yet.

## Why this build exists

Studio One listening review of v1.7:
- Hybrid Guitar: widening/doubled/reverb-like character during sustain and after note-off; mono summation **did not** remove it.
- Electric with `1.2 UNIFIED`: reported silent; `POWER-WAVE 1.0` alone is audible.
- Classical and Steel: audible.
- Preference: Power-Wave 1.0 currently preferred over Unified.

## Test procedure in Studio One

1. Install 1.7.1 alongside 1.7. For comparisons use an identical MIDI phrase, gain and sample rate, **no DAW inserts or sends**.
2. Select Hybrid Guitar and enable **1.2 UNIFIED**. First play with **1.7.1 UNIFIED DIAGNOSTICS OFF**. This is the unaltered v1.7 path.
3. Enable **1.7.1 UNIFIED DIAGNOSTICS**. The output is now a **raw 1.2 observer probe**; optional 1.3–1.7 custom observers are bypassed **for audible output only**, while their states continue to run.
4. Turn **STRING / PICKUP DIRECT** OFF, leaving **PHYSICAL BRIDGE COUPLING** and **BRIDGE / BODY RADIATION** ON. Listen to bridge/body radiation only.
5. Turn **BRIDGE / BODY RADIATION** OFF and **STRING / PICKUP DIRECT** ON. Listen to string-only radiated readout (Electric: magnetic pickup readout).
6. Turn **PHYSICAL BRIDGE COUPLING** OFF. This replaces the physical two-port bridge scattering with a rigid passive string reflection. Re-trigger notes after topology changes. The former bridge modes are **absorbed** into the energy loss ledger; they cannot reappear later as a frozen reverb tail.
7. Repeat with Electric; compare with `1.2 UNIFIED` OFF and `POWER-WAVE 1.0` ON. If the electric path is silent on your machine, export a WAV with 1.2 enabled and report its RMS/peak.

**Meaning of switches:** direct and body are *readout/output isolation* (do not change the waveguide physics); bridge coupling is an *actual physical boundary A/B*. The bridge and body are not independently modeled radiation engines in raw 1.2: body output is a readout of the bridge's modal radiation. For true finer decomposition, a future version must instrument its force/velocity transfer and radiation operators.

**Key safety:** all diagnostic switches default to their bypass settings (master OFF, three components ON). Existing model parameters and presets are not reset. Diagnostic master off must be bit-identical to 1.7. No plugin callback allocation/lock/file I/O; all outputs remain safety-limited. The optional diagnostic may click on live bridge topology changes; stop/re-trigger MIDI notes for controlled comparisons.

## Validation

`cmake -S . -B build -DPMG_BUILD_PLUGIN=OFF -DBUILD_TESTING=ON`
`cmake --build build --parallel`
`ctest --test-dir build --output-on-failure`

Specific tests in `Tests/Unified171DiagnosticTests.cpp` check:
- OFF baseline bit-exact with all three diagnostics flipped (no effect).
- String-only/body-only and both-muted output.
- Rigid reflection and residual energy.
- Bridge topology switching with absorbed energy accounted.
- Nonzero Electric unified output (only software-level sanity; DAW troubleshooting still required).

Windows VST3 installer: `.github/workflows/windows-v171.yml`. No claim of successful Studio One/pluginval inspection until performed separately.
