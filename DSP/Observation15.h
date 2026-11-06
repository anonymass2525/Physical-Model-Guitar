#pragma once
#include "MeasuredObserver15Coeffs.h"
#include "PowerWaveStrings.h"
#include <array>
#include <cstddef>
namespace pmg {
// Measurement-informed magnetic pickup. Electrical FIR is fitted to a
// published *loaded / unloaded* complex transfer curve. The finite pole
// aperture and magnetic field gradient are still uncalibrated approximations.
class MeasuredMagnetic15 {
public:
    void prepare(double sampleRate) noexcept;
    void reset() noexcept;
    double tick(const PowerWaveStrings&,double position,double aperture) noexcept;
private:
    double sr=48000.,dc=0.;
    std::array<double,6> displacement{};
    std::array<double,kMeasuredFIR15> history{};
    std::size_t head=0;
};
// 2x2 complex microphone/bridge transfer (near-field only). Four real causal
// FIRs capture measured magnitude and phase. The relative direct-string path
// is separately delayed: its real physical air-path transfer remains unknown.
class MeasuredNylon15 {
public:
    struct Stereo {double left=0.,right=0.;};
    void prepare(double sampleRate) noexcept;
    void reset() noexcept;
    Stereo tick(const PowerWaveStrings&,double bridgeBass,double bridgeTreble,
                double bodyAmount,double stringPhase) noexcept;
private:
    double sr=48000.,directHP=0.;
    std::array<double,kMeasuredFIR15> bass{},treble{};
    std::array<double,128> directHistory{};
    std::size_t head=0, directHead=0;
};
}
