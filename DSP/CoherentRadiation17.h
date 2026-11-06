#pragma once
#include "PowerWaveStrings.h"
#include "LongRadiation16.h"
#include "MeasuredObserver15Coeffs.h"
#include <array>
#include <cstddef>

namespace pmg {
// Experimental, explicitly non-loading 2-source/2-microphone observer.
// The two sources are derived independently from string velocity and bridge
// velocity.  Neither sources nor the optional modal tail are fitted to the
// SAME-guitar independent reference capture yet. No blind-source-separation
// claim is made. Coefficients remain research estimates.
class CoherentRadiation17 {
public:
    struct Frame { double left=0., right=0.; };
    void prepare(double sampleRate) noexcept;
    void reset() noexcept;
    Frame tick(const PowerWaveStrings& strings,double bassVel,double trebleVel,
               LongRadiation16::Stereo late, double bodyAmount,
               double signedPhase01,double directMix01,double lateMix01) noexcept;
    Frame lastDirect() const noexcept {return directLast;}
    Frame lastBody() const noexcept {return bodyLast;}
private:
    static constexpr std::size_t historySize=512;
    std::array<double,kMeasuredFIR15> bass{},treble{};
    std::array<std::array<double,historySize>,2> directHistory{},bodyHistory{};
    std::size_t head=0,delayHead=0;
    double rate=48000.,dc=0.,smoothedMix=0.;
    Frame directLast{},bodyLast{};
    static double delayed(const std::array<double,historySize>& h,std::size_t pos,double samples) noexcept;
};
}
