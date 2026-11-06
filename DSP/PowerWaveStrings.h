#pragma once
#include "TwoPortBridge.h"
#include <array>
#include <cstdint>

namespace pmg {
// v1.0 RESEARCH: physically dimensioned, 6-string x 2-polarization travelling
// transverse *velocity* waves. Ring represents a rigid-nut round trip; an
// orthogonal one-pole allpass supplies fractional phase delay without losing
// energy. Bridge scattering works in SI F(N) / v(m/s) / Z(Ns/m).
// The exact ledger covers this new subsystem, not the legacy 0.9 dry model.
class PowerWaveStrings {
public:
    static constexpr int strings=6, planes=2, maxDelay=8192;
    struct Frame {
        double left=0.,right=0.,stringVelocity=0.,bridgeVelocity=0.;
    };
    void prepare(double sr, int model) noexcept;
    bool strike(int stringIndex,int midiNote,double velocity,double pluck) noexcept;
    void release(int stringIndex) noexcept;
    void releaseAll() noexcept;
    void setDecay(double seconds) noexcept;
    Frame tick() noexcept;
    double storedStringEnergy() const noexcept;
    double storedBridgeEnergy() const noexcept;
    double propagationLoss() const noexcept {return lostToPropagation;}
    double dampingAtReattack() const noexcept {return mutedEnergy;}
    double injectedEnergy() const noexcept {return externalEnergy;}
    double bridgeDissipation() const noexcept;
    double energyResidual() const noexcept;
    double largestAbsResidual() const noexcept {return maxResidual;}
    bool sane() const noexcept;
    double stringImpedance(int s) const noexcept;
    double stringLength(int s) const noexcept;
private:
    struct Wave {
        std::array<double,maxDelay> ring{};
        int cursor=0, integerDelay=400;
        double z=0.2, allpassA=0., allpassB=1., apState=0., queueEnergy=0.;
        double gain=0.998, nominalHz=110., stringLength=.65;
        bool releasing=false;
        double read() const noexcept {return ring[std::size_t(cursor)];}
        void clear() noexcept;
    };
    std::array<std::array<Wave,planes>,strings> waves{};
    std::array<TwoPortBridge,planes> bridges{};
    double dt=1./48000.,rate=48000.,decaySec=4.,externalEnergy=0.,
           lostToPropagation=0.,mutedEnergy=0.,maxResidual=0.;
    int guitarModel=2;
};
}
