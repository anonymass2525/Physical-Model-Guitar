#pragma once
#include "MeasuredBridge.h"
#include <array>
#include <cstdint>

namespace pmg {
// v0.9 research-grade strictly passive *boundary subnetwork*.
// Two physical locations: bass-bridge and treble-bridge (not two transverse axes).
// Six normalized velocity-wave inputs from the strings are coupled through
// a reciprocal, positive-real modal bridge mobility matrix. Each string has
// its own positive real port impedance. No heap operations or I/O in tick().
// The exact energy identity covers these six *boundary wave ports* plus the
// bridge modes; legacy displacement-delay-line energies remain approximate.
class TwoPortBridge {
public:
    static constexpr unsigned nModes = MeasuredBridge::numberOfModes;
    static constexpr int nStrings = 6;
    using Params = std::array<MeasuredBridge::ModeParams,nModes>;
    struct Frame {
        std::array<double,nStrings> outgoing{};
        std::array<double,nStrings> stringEndpointVelocity{};
        std::array<double,2> bridgeVelocity{};
        std::array<double,2> bridgeForce{};
        double radiatedL=0.,radiatedR=0.;
        double incomingEnergy=0., outgoingEnergy=0.; // Joule-equivalent *per sample*
    };
    void prepare(double sampleRate, const Params& params) noexcept;
    void reset() noexcept;
    Frame tick(const std::array<double,nStrings>& incoming,
               const std::array<double,nStrings>& portImpedance) noexcept;
    double storedEnergy() const noexcept;
    double dissipatedEnergy() const noexcept {return dissipated;}
    double incomingEnergy() const noexcept {return incomingWork;}
    double outgoingEnergy() const noexcept {return outgoingWork;}
    double couplingWork() const noexcept {return workToBridge;}
    double energyResidual() const noexcept {
        return storedEnergy()+dissipated-(incomingWork-outgoingWork);
    }
    bool sane() const noexcept;
    static std::array<double,nStrings> normalizedImpedance(int model) noexcept;
private:
    struct Mode {
        double p=0.,v=0.,omega=0.,gamma=0.,mass=1.,bx=1.,by=0.,left=0.,right=0.;
    };
    std::array<Mode,nModes> modes{};
    double h=1./48000.,incomingWork=0.,outgoingWork=0.,workToBridge=0.,dissipated=0.;
};
} // namespace pmg
