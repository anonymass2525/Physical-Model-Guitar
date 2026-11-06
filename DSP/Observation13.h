#pragma once
#include "PowerWaveStrings.h"
#include <array>

namespace pmg {
// Read-only, research-grade electromechanical/acoustic transducers operating
// on the SAME underlying unified power-wave states. These approximations do
// not identify measured flux linkage, air radiation impedance, or string-to-air
// power loss. No samples, allocations, forces or feedback into the string.
class MagneticPickup13 {
public:
    void prepare(double sampleRate) noexcept;
    void reset() noexcept;
    // pickupFraction: position along speaking length; aperture: 0..1 width;
    // character controls effective magnetic gap and resonant electrical load.
    double tick(const PowerWaveStrings&, double pickupFraction,
                double aperture, double character) noexcept;
private:
    double sr=48000., highpassMemory=0., circuitIc1=0., circuitIc2=0.;
    double cachedCharacter=-1., cachedG=0., cachedK=1.;
    std::array<double,6> displacement{};
};

class NylonRadiation13 {
public:
    struct Stereo { double left=0., right=0.; };
    void prepare(double sampleRate) noexcept;
    void reset() noexcept;
    Stereo tick(const PowerWaveStrings&, double modalL, double modalR,
                double bodyAmount) noexcept;
private:
    double sr=48000.;
    double modalLowL=0., modalLowR=0., pressureLowL=0., pressureLowR=0.;
    double stringLow=0., cavityQ=0., cavityP=0.;
};
}
