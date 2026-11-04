# Repository build notes
- This is a new 0.4 guitar engine. Do not reuse 0.2 golden audio or 0.2 tests as acceptance criteria.
- Verify all native tests and physical parameter differentiability before promoting.
- Do not introduce allocations, locks, disk/network I/O into the DSP audio callback.
- Body size morphs modal coefficients over 45 ms while retaining oscillator state; verify parameter sweeps for click-free automation.
- Preserve the unique 0.4 JUCE plugin code PmG4 and its bundle ID; never overwrite the 0.2 or 0.3 VST3.
- First validate on Windows x64 Release, CTest, pluginval strictness 5 (GUI enabled), then Studio One.
- Never label approximated waveguide contact/FEM body/magnetic load as empirical accuracy without recorded instrument calibration.
