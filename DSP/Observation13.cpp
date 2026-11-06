#include "Observation13.h"
#include <algorithm>
#include <cmath>
namespace pmg {
namespace {
constexpr double pi=3.14159265358979323846;
inline double bounded(double x,double low,double high,double fallback) noexcept {
    return std::clamp(std::isfinite(x)?x:fallback,low,high);
}
inline double alpha(double f,double sr) noexcept {
    return 1.-std::exp(-2.*pi*f/sr);
}
}
void MagneticPickup13::prepare(double sampleRate) noexcept {
    sr=bounded(sampleRate,8000.,192000.,48000.);reset();
}
void MagneticPickup13::reset() noexcept {
    highpassMemory=circuitIc1=circuitIc2=0.;cachedCharacter=-1.;displacement.fill(0.);
}
double MagneticPickup13::tick(const PowerWaveStrings& strings,double location,
                              double aperture,double character) noexcept {
    location=bounded(location,.035,.46,.15);
    aperture=bounded(aperture,0.,1.,.5);
    character=bounded(character,0.,1.,.5);
    // Five-point symmetric finite pole-width convolution. In a true pickup,
    // each string contributes to flux linkage across a spatial weighting field.
    // The normalized Gaussian-like mask suppresses high partials without
    // introducing delay or artificial post-EQ combs.
    constexpr double kernel[5]={.07,.24,.38,.24,.07};
    const double halfSpan=.003+.030*aperture;
    const double hp=alpha(22.,sr);
    const double integratorLeak=std::exp(-2.*pi*7./sr);
    const double gap=.0023+.0020*character; // unmeasured effective gap in metres
    double electromotive=0.;
    for(int s=0;s<6;++s){
        double vertical=0., horizontal=0.;
        for(int j=0;j<5;++j){
            const double x=bounded(location+(double(j)-2.)*.5*halfSpan,.012,.485,location);
            vertical+=kernel[j]*strings.sampleTransverseVelocity(s,0,x);
            horizontal+=kernel[j]*strings.sampleTransverseVelocity(s,1,x);
        }
        // Small-displacement Faraday approximation: V ~ -dPhi/dy * dy/dt.
        // The field gradient is regularized so it remains finite for strong
        // attacks; horizontal polarization has weaker magnetic sensitivity.
        auto& dy=displacement[size_t(s)];
        dy=integratorLeak*dy+vertical/sr;
        dy=bounded(dy,-.65*gap,.65*gap,0.);
        const double gradient=std::pow(gap/(gap+dy),2.);
        const double stringGain=1.0-.045*s;
        electromotive+=stringGain*gradient*(vertical+.12*horizontal);
    }
    // Coil/cable/load approximation: bilinear 2-pole RLC-like resonator.
    // Stable state-variable filter. All coefficients are sample-rate aware,
    // with Q bounded independently of the string/bridge passivity ledger.
    if(character!=cachedCharacter){
        const double fc=std::min(.38*sr,2900.+1400.*character);
        const double q=.66+.40*character;
        cachedG=std::tan(pi*fc/sr);
        cachedK=1./q;
        cachedCharacter=character;
    }
    const double g=cachedG;
    const double k=cachedK;
    const double v3=electromotive-circuitIc2;
    const double v1=(circuitIc1+g*v3)/(1.+g*(g+k));
    const double v2=circuitIc2+g*v1;
    circuitIc1=2.*v1-circuitIc1;
    circuitIc2=2.*v2-circuitIc2;
    const double circuitOut=.80*v2+.20*electromotive;
    highpassMemory+=hp*(circuitOut-highpassMemory);
    return bounded(circuitOut-highpassMemory,-8.,8.,0.);
}
void NylonRadiation13::prepare(double sampleRate) noexcept {
    sr=bounded(sampleRate,8000.,192000.,48000.);reset();
}
void NylonRadiation13::reset() noexcept {
    modalLowL=modalLowR=pressureLowL=pressureLowR=0.;
    stringLow=cavityQ=cavityP=0.;
}
NylonRadiation13::Stereo NylonRadiation13::tick(const PowerWaveStrings& strings,
                   double modalL,double modalR,double bodyAmount) noexcept {
    modalL=bounded(modalL,-5.,5.,0.);
    modalR=bounded(modalR,-5.,5.,0.);
    bodyAmount=bounded(bodyAmount,0.,1.,.5);
    // Radiated pressure in the far field scales with the time derivative of
    // vibrating surface volume velocity. A leaky differentiated modal velocity
    // approximates this without differentiating an ideal impulse (unbounded HF).
    const double hp=alpha(560.,sr);
    modalLowL+=hp*(modalL-modalLowL);
    modalLowR+=hp*(modalR-modalLowR);
    const double acceleratedL=modalL-modalLowL;
    const double acceleratedR=modalR-modalLowR;
    // Stable low-frequency cavity resonance, driven by collective soundboard
    // motion. This is an unmeasured effective air response, NOT calibrated
    // Helmholtz frequency nor a guaranteed physically passive air-coupled mode.
    const double omega=2.*pi*105./sr;
    const double force=.0025*(modalL+modalR);
    const double nextP=.9978*cavityP+omega*cavityQ+force;
    const double nextQ=.9978*cavityQ-omega*nextP;
    cavityP=bounded(nextP,-3.,3.,0.);
    cavityQ=bounded(nextQ,-3.,3.,0.);
    // Read-only direct string leakage: velocity of string section near .31L.
    double direct=0.;
    for(int s=0;s<6;++s) {
        direct+=(strings.sampleTransverseVelocity(s,0,.31)
                +.25*strings.sampleTransverseVelocity(s,1,.31));
    }
    const double directHp=alpha(390.,sr);
    stringLow+=directHp*(direct-stringLow);
    const double stringAir=direct-stringLow;
    // A modal velocity component, its acceleration-related radiation term,
    // weak near-field strings, and a common cavity component are kept separate.
    // Position-to-pressure calibration is intentionally tentative.
    const double body=.28+.72*bodyAmount;
    const double rawL=body*(.64*modalL+.36*acceleratedL)+.026*stringAir+.12*cavityP;
    const double rawR=body*(.64*modalR+.36*acceleratedR)+.023*stringAir+.12*cavityP;
    const double lp=alpha(6000.,sr);
    pressureLowL+=lp*(rawL-pressureLowL);
    pressureLowR+=lp*(rawR-pressureLowR);
    return {pressureLowL,pressureLowR};
}
}
