#include "CoherentRadiation17.h"
#include <algorithm>
#include <cmath>
namespace pmg {
namespace {
constexpr double pi=3.14159265358979323846;
inline double bounded(double v,double lo,double hi) noexcept {
    return std::clamp(std::isfinite(v)?v:0.,lo,hi);
}
}
void CoherentRadiation17::prepare(double fs) noexcept {
    rate=bounded(fs,8000.,192000.);
    reset();
}
void CoherentRadiation17::reset() noexcept {
    bass.fill(0.);treble.fill(0.);
    for(auto& a:directHistory)a.fill(0.);
    for(auto& a:bodyHistory)a.fill(0.);
    head=delayHead=0;dc=0.;smoothedMix=0.;directLast=bodyLast={};
}
// Four-tap causal Lagrange fractional delay; no lookahead and no allocation.
double CoherentRadiation17::delayed(const std::array<double,historySize>& h,
                                  std::size_t pos,double samples) noexcept {
    const double d=bounded(samples,0.,double(historySize-5));
    const std::size_t n=std::size_t(d);
    const double mu=d-double(n);
    const double a0=-(mu-1.)*(mu-2.)*(mu-3.)/6.;
    const double a1=mu*(mu-2.)*(mu-3.)/2.;
    const double a2=-mu*(mu-1.)*(mu-3.)/2.;
    const double a3=mu*(mu-1.)*(mu-2.)/6.;
    const auto at=[&](std::size_t k){return h[(pos+historySize-n-k)%historySize];};
    return a0*at(0)+a1*at(1)+a2*at(2)+a3*at(3);
}
CoherentRadiation17::Frame CoherentRadiation17::tick(const PowerWaveStrings& strings,
         double bassVel,double trebleVel,LongRadiation16::Stereo late,
         double amount,double signedPhase01,double directMix01,double lateMix01) noexcept {
    // The source *proxies* are separate, but the acoustic radiation is not
    // identifiable without independently measured source-reference signals.
    bass[head]=bounded(bassVel,-10.,10.);
    treble[head]=bounded(trebleVel,-10.,10.);
    double body[2]{};
    std::size_t at=head;
    for(int i=0;i<kMeasuredFIR15;++i){
        const double x=bass[at],y=treble[at];
        body[0]+=kMeasuredNylon15[0][std::size_t(i)]*x+kMeasuredNylon15[1][std::size_t(i)]*y;
        body[1]+=kMeasuredNylon15[2][std::size_t(i)]*x+kMeasuredNylon15[3][std::size_t(i)]*y;
        at=at?at-1:kMeasuredFIR15-1;
    }
    head=(head+1)%kMeasuredFIR15;
    amount=bounded(amount,0.,1.);
    lateMix01=bounded(lateMix01,0.,1.);
    const double bodyGain=.28+.72*amount;
    body[0]=bounded(bodyGain*(body[0]+lateMix01*late.left),-10.,10.);
    body[1]=bounded(bodyGain*(body[1]+lateMix01*late.right),-10.,10.);
    double source=0.;
    for(int s=0;s<6;++s) {
        // Multiple positions reduce spurious single-point nodes. This is
        // a readout of the 1.2 physical waveguide, NOT a new wave force.
        source+=.5*strings.sampleTransverseVelocity(s,0,.22)
               +.35*strings.sampleTransverseVelocity(s,0,.38)
               +.15*strings.sampleTransverseVelocity(s,1,.30);
    }
    const double a=1.-std::exp(-2.*pi*330./rate);
    dc+=a*(bounded(source,-10.,10.)-dc);
    const double direct=bounded(source-dc,-10.,10.);
    // The direct/body acoustic relative delay is +/- 0.9ms, not an absolute
    // measured arrival time. Both paths are causal: delay ONLY the lagging one.
    const double delay= (bounded(signedPhase01,0.,1.)-.5)*.0018*rate;
    directHistory[0][delayHead]=direct;
    directHistory[1][delayHead]=direct*.92;
    bodyHistory[0][delayHead]=body[0];
    bodyHistory[1][delayHead]=body[1];
    const double directDelay=std::max(0.,delay);
    const double bodyDelay=std::max(0.,-delay);
    double d[2],b[2];
    for(int k=0;k<2;++k){
        d[k]=delayed(directHistory[std::size_t(k)],delayHead,directDelay);
        b[k]=delayed(bodyHistory[std::size_t(k)],delayHead,bodyDelay);
    }
    delayHead=(delayHead+1)%historySize;
    directLast={d[0],d[1]};bodyLast={b[0],b[1]};
    // 1.5 complex transfer (body) plus separate direct-string component.
    // The gain is a provisional perceptual scale, NOT SI Pa calibration.
    const double mix=bounded(directMix01,0.,1.);
    return {bounded(b[0]+.06*mix*d[0],-10.,10.),bounded(b[1]+.06*mix*d[1],-10.,10.)};
}
}
