#include "Observation14.h"
#include <algorithm>
#include <cmath>
namespace pmg {
namespace {
constexpr double pi=3.14159265358979323846;
inline double bounded(double x,double lo,double hi,double fallback) noexcept {
    return std::clamp(std::isfinite(x)?x:fallback,lo,hi);
}
inline double alpha(double hz,double sr) noexcept {return 1.-std::exp(-2.*pi*hz/sr);}
}
void MagneticPickup14::prepare(double rate) noexcept {
    sr=bounded(rate,8000.,192000.,48000.);reset();
}
void MagneticPickup14::reset() noexcept {
    smooth=attackFast=attackSlow=circuitLp=resonatorHp=lastOutput=0.;
    displacement.fill(0.);
}
double MagneticPickup14::tick(const PowerWaveStrings& strings,double loc,double aperture,
                              double character,double detail) noexcept {
    loc=bounded(loc,.035,.46,.15);aperture=bounded(aperture,0.,1.,.5);
    character=bounded(character,0.,1.,.5);detail=bounded(detail,0.,1.,.5);
    constexpr double w[7]={.035,.11,.225,.26,.225,.11,.035};
    double local=0.,flux=0.;
    const double halfSpan=.004+.023*aperture;
    const double gap=.0025+.0015*character;
    for(int s=0;s<6;++s) {
        const double vy=strings.sampleTransverseVelocity(s,0,loc);
        const double vx=strings.sampleTransverseVelocity(s,1,loc);
        local+=(1.-.042*s)*(vy+.09*vx);
        double pole=0.;
        for(int j=0;j<7;++j) {
            const double x=bounded(loc+(j-3)*halfSpan/3.,.012,.485,loc);
            pole+=w[j]*(strings.sampleTransverseVelocity(s,0,x)+.10*strings.sampleTransverseVelocity(s,1,x));
        }
        displacement[size_t(s)]=.9990*displacement[size_t(s)]+pole/sr;
        // Bounded field gradient - avoids the high-amplitude singularity of
        // a strict 1/(gap + displacement)^2 point-pole approximation.
        const double d=bounded(displacement[size_t(s)]/gap,-.65,.65,0.);
        const double gradient=1.-.65*d+.20*d*d;
        flux+=(1.-.042*s)*pole*gradient;
    }
    // Dynamic emphasis is based on the ratio of transient to sustained signal
    // energy. This modifies the observer, not the string excitation.
    const double magnitude=std::abs(local);
    attackFast+=alpha(1500.,sr)*(magnitude-attackFast);
    attackSlow+=alpha(35.,sr)*(magnitude-attackSlow);
    const double transient=bounded((attackFast-attackSlow)/(attackFast+attackSlow+1.e-8),0.,1.,0.);
    const double fluxBlend=bounded(.28+.40*character-.28*detail*transient,.10,.75,.35);
    const double emf=(1.-fluxBlend)*local+fluxBlend*flux;
    // Broadband parallel electrical load: a bounded high-frequency bypass
    // preserves legitimate high partials while a coil/cable pole still rolls
    // off the measured output. Not a complete measured RLC circuit.
    circuitLp+=alpha(4200.+2200.*character,sr)*(emf-circuitLp);
    const double feedthrough=.32+.46*detail*transient;
    const double raw=(1.-feedthrough)*circuitLp+feedthrough*emf;
    smooth+=alpha(19.,sr)*(raw-smooth);
    const double result=raw-smooth;
    lastOutput=result;
    return bounded(result,-8.,8.,0.);
}
void NylonRadiation14::prepare(double rate) noexcept {
    sr=bounded(rate,8000.,192000.,48000.);reset();
}
void NylonRadiation14::reset() noexcept {
    low.fill(0.);mid.fill(0.);upper.fill(0.);cavity.fill(0.);
    stringLow=cavityQ=cavityP=0.;
}
NylonRadiation14::Stereo NylonRadiation14::tick(const PowerWaveStrings& strings,
                   double modalL,double modalR,double amount,double upperDetail) noexcept {
    modalL=bounded(modalL,-5.,5.,0.);modalR=bounded(modalR,-5.,5.,0.);
    amount=bounded(amount,0.,1.,.5);upperDetail=bounded(upperDetail,0.,1.,.5);
    // Modal velocity split into physically interpretable radiation bands:
    // monopole/low cavity, mid soundboard and high flexural surface modes.
    // A wave-velocity readout alone is not the full soundboard velocity field.
    const double values[2]={modalL,modalR};
    double pressure[2]{};
    const double b= .28+.72*amount;
    const double aLo=alpha(360.,sr),aMid=alpha(2750.,sr),aHi=alpha(10300.,sr);
    const double omega=2.*pi*104./sr;
    const double nextP=.9977*cavityP+omega*cavityQ+.0022*(modalL+modalR);
    const double nextQ=.9977*cavityQ-omega*nextP;
    cavityP=bounded(nextP,-3.,3.,0.);
    cavityQ=bounded(nextQ,-3.,3.,0.);
    double string=0.;
    for(int s=0;s<6;++s)
        string+=strings.sampleTransverseVelocity(s,0,.29)
              +.20*strings.sampleTransverseVelocity(s,1,.29);
    stringLow+=alpha(650.,sr)*(string-stringLow);
    const double direct=string-stringLow;
    for(int c=0;c<2;++c) {
        const double m=values[c];
        low[size_t(c)]+=aLo*(m-low[size_t(c)]);
        mid[size_t(c)]+=aMid*(m-mid[size_t(c)]);
        upper[size_t(c)]+=aHi*(m-upper[size_t(c)]);
        const double bass=low[size_t(c)];
        const double mids=mid[size_t(c)]-bass;
        const double highs=upper[size_t(c)]-mid[size_t(c)];
        // High partial radiation gets a controllable but bounded directional
        // contribution, rather than hard low-passing all upper harmonics.
        const double highGain=.65+1.35*upperDetail;
        const double body=b*(.70*bass+1.00*mids+highGain*highs);
        const double air=.10*cavityP +(.013+.035*upperDetail)*direct*(c==0?1.:.91);
        pressure[c]=bounded(body+air,-8.,8.,0.);
    }
    return {pressure[0],pressure[1]};
}
}
