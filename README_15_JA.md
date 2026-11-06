# Physical Model Guitar 1.5 — Measurement-Informed Observation (Research)

## Implementation

- **Electric**: measured *loaded/unloaded* complex transfer from DiMarzio Air Norton pickup VNA recordings (strayr pickupAnalyseR-data, CC0): real causal 96-tap FIR fitted to magnitude and relative phase. The published measurement protocol uses a 200 kΩ / 470 pF load. 102 held-out frequency bins: relative complex error 1.10%, median absolute phase residual 0.21 degrees. **This is electrical-loading transfer, not 3D magnetic-field measurements or string-to-output absolute volts**. The spatial field-gradient kernel remains a theoretical approximation.
- **Nylon**: calibrated two-input/two-output *near-field* pressure/bridge velocity transfer from Robert Mores' original force, bridge accelerometer, and microphone impulse recordings (Zenodo 4604577, Guitar #1). 96 taps for each of four complex transfer matrix paths. Near-field sound is available at only a few microphone positions. **This is not far-field spherical directivity**. The fitted held-out-bin complex residual is currently large (about 54–80%). Candidate is experimental only, OFF by default.
- **Phase-aware direct string component**: an independent 0.12–0.95 ms air-path delay hypothesis. The body's FIR contains measured relative phase; the **relative phase between the synthetic direct string field and measured pressure is not calibrated**. Toggling the delay changes the interference in the reproduced audio without injecting energy into the strings.
- Stable fixed-size arrays, real-time allocation-free FIR computations. 1.4 source/effects unchanged when new switches are OFF. All current measured filters observe the same unified waveguide; **1.2 Unified mode must be ON** to hear the changes.

## Sources

1. https://github.com/strayr/pickupAnalyseR-data — CC0 network analyzer captures. Input paths: `Guitar/DiMarzio/AirNorton/AirNorton-LD.csv`, `AirNorton-UL.csv`. Phase and magnitude, not full magnetic flux field.
2. https://zenodo.org/records/4604577 — Robert Mores, 2021; raw `qualified_selected_impulses.mat`. The authors request attribution. Microphone data are near-field with room-related uncertainty.
3. https://courses.physics.illinois.edu/phys406/sp2017/406emi_guitar_pickup_results.html — independent complex V/I, impedance and coil loading measurement methods.
4. https://dafx.de/paper-archive/details/KZ0Ws85vmVK5-Tceo9RZ1w — measured guitar bridge and body/air modal characterization, DAFx 2026; **not claimed to be the algorithm implemented here**.

## Acoustic validation and decisions

- No real source samples, 3D magnetic maps, or measured microphone captures shipped with plugin source or installer. Only derived numerical FIR coefficients from permitted published measurements are included.
- Hold-out bins come **from the same pickup or guitar** as fitted; they test transfer approximation, not instrument generalization. Audio-note testing uses separate, unmatched guitars/capture conditions and does not establish perceptual superiority.
- No auto-activation for either new switch. Repeated controlled takes, Hall-probe xyz field maps, measured source directivity using multi-angle calibrated microphones, and level-matched blind AB are needed before replacing older settings.
- The 1.5 magnetic electric filter captures a *relative electrical* load curve only. The direct nylon sound-path delay is user-adjustable and still unmeasured.
- The 1.5 model uses an experimental inverse map derived from Guitar #1, not a best-fit model for all 65 guitars. The 96-tap nylon transfer has large relative complex error (54%-80%). Do **not** mistake this for complete physical accuracy.

## Studio One

Start with `1.2 UNIFIED` ON. For Electric, use `1.5 MEASURED COIL LOAD A/B`. For Classical Nylon use `1.5 MEASURED NYLON PHASE A/B`. The `1.5 AIR DELAY` control cycles 0, 0.5, 1.0 (direct-path relative time ~0.12/0.535/0.95 ms). Note: the new switches default to OFF, preserving previous audio exactly.
