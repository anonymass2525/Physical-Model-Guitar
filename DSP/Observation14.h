#pragma once
#include "PowerWaveStrings.h"
#include "Observation13.h"
#include <array>

namespace pmg {
// Reduced-order magnetic flux-linkage observer. Its two channels are a narrow
// local velocity pickup and a finite-aperture flux-gradient estimate. Transient
// response changes the interpolation between these physical observation paths,
// not the physical string or bridge states. Values are not yet calibrated volts.
class MagneticPickup14 {
public:
    void prepare(double sampleRate) noexcept;
    void reset() noexcept;
    double tick(const PowerWaveStrings& strings, double pickupPosition,
                double aperture, double character, double attackDetail) noexcept;
private:
    double sr=48000., smooth=0., attackFast=0., attackSlow=0.;
    double circuitLp=0., resonatorHp=0., lastOutput=0.;
    std::array<double,6> displacement{};
};

// Frequency-split pressure observation, not acoustic-radiation back-action.
// Body and direct string terms remain individually tunable and independently
// measured. Note that low-order radiated pressure is NOT a measured Pa/N FRF.
class NylonRadiation14 {
public:
    using Stereo=NylonRadiation13::Stereo;
    void prepare(double sampleRate) noexcept;
    void reset() noexcept;
    Stereo tick(const PowerWaveStrings& strings, double bodyLeft,
                double bodyRight, double bodyAmount, double upperRadiation) noexcept;
private:
    double sr=48000.;
    std::array<double,2> low{},mid{},upper{},cavity{};
    double stringLow=0.,cavityQ=0.,cavityP=0.;
};
}
