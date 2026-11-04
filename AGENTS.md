# Repository build notes
- This is a new 0.3 guitar engine. Do not reuse 0.2 golden audio or 0.2 tests as acceptance criteria.
- Verify all native tests and physical parameter differentiability before promoting.
- Do not introduce allocations, locks, disk/network I/O into the DSP audio callback.
- Body size morphs modal coefficients over 45 ms while retaining oscillator state; verify parameter sweeps for click-free automation.
- Preserve the unique 0.3 JUCE plugin code PmG3 and its bundle ID; do not overwrite 0.2 VST3.
- First validate on Windows x64 Release, CTest, pluginval strictness 5 (GUI enabled), then Studio One.
- Never label approximated waveguide contact/FEM body/magnetic load as empirical accuracy without recorded instrument calibration.
