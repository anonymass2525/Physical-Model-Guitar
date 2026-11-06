# Physical Model Guitar 1.6 Research — Early/Late Radiation

## DSP

Nylon only: measured nearfield 2×2 96-tap complex-frequency fitted response
(from 1.5) plus independent 16-mode causal late-response IIR. Each mode has
measured-conditional or provisional f, time constant and complex residues for
all four paths (two bridge velocity ports × two microphone positions). Real
residues + quadrature state capture magnitude, time decay and relative phase;
no absolute Pa or full-sphere directivity is claimed.

The explicit 2-ms delayed late modes must not precede the 96-tap early
response; their decay is bounded, state remains finite, and neither observer
feeds force into the waveguide. Direct string radiation is still from 1.5
with a manually adjustable unmeasured path delay. Interference is a real
signed time-domain sum of direct+early body+late body, not an envelope blend.

1.6 candidate is OFF by default. Turn ON 1.2 UNIFIED, use Classical Nylon,
then 1.6 LONG NYLON MODES A/B. 1.6 TAIL cycles 0/0.5/1.0. 1.5 AIR DELAY
shifts direct string relative to body. 1.5 baseline preserved when 1.6 OFF.
Electric / Steel models never use the 1.6 observer.

## Measurement validity

The Mores 2021 measurements (https://zenodo.org/records/4604577) are two
bridge response positions and sparse nearfield microphones, not 3D radiation
or room-free high resolution. `tools/FitLongRadiation16.py` retrieves the
source, verifies the MD5, uses the v1.5 early response and fitted bridge modes,
then estimates 16 phase-aware modal residues with 0.15–0.35 s gated data.
Residuals from the 0.15–0.35 s tail may include acoustic room reflection,
and the 5-fold interleaved frequency-bin validation is correlated: the fit
must NOT be described as an independent physical identification. A more
robust late model requires anechoic multi-mic transfer functions and
repeated controlled hammer impulses with cross-validation by impulse/take.

The generator only publishes fitted coefficient header and metrics; not the
original recorded source. The fallback header is marked unmeasured.

## Quality gates

- 17 native CTest tests including impulse early/late causality, modal pole
  bounds, mechanical-energy observer noninterference, 1.5 OFF bit-identity,
  and 44.1/48/96 kHz.
- Experimental 12-note nylon benchmark in low/high register with
  independently recorded OGG excerpts (do not redistribute references).
- RMS-matched blind listening with separate code/answer key.
- Distinct Windows VST3 identity and versioned Inno Setup EXE installer.

No uncompensated claim of superior audio: both frequency/phase fit and
listening must improve before changing the default.
