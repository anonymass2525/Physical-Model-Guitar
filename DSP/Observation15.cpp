#include "Observation15.h"
#include <algorithm>
#include <cmath>
namespace pmg {
namespace {
constexpr double pi=3.14159265358979323846;
inline double guard(double x,double lo,double hi,double backup=0.) noexcept {
    return std::clamp(std::isfinite(x)?x:backup,lo,hi);
}
inline double lowpassAlpha(double f,double sr) noexcept {return 1.-std::exp(-2.*pi*f/sr);}
}
void MeasuredMagnetic15::prepare(double sampleRate) noexcept {
    sr=guard(sampleRate,8000.,192000.,48000.);reset();
}
void MeasuredMagnetic15::reset() noexcept {
    displacement.fill(0.); history.fill(0.);dc=0.;head=0;
}
double MeasuredMagnetic15::tick(const PowerWaveStrings& strings,double pos,double aperture) noexcept {
    pos=guard(pos,.035,.46,.15);aperture=guard(aperture,0.,1.,.5);
    const double w[5]={.07,.24,.38,.24,.07};
    const double width=.003+.028*aperture;
    double emf=0.;
    for(int s=0;s<6;++s){
        double velocity=0.;
        for(int i=0;i<5;++i){
            const double x=guard(pos+(.5*(i-2))*width,.015,.48,pos);
            velocity+=w[i]*(strings.sampleTransverseVelocity(s,0,x)+
                            .12*strings.sampleTransverseVelocity(s,1,x));
        }
        // Field-gradient surrogate; no published three-dimensional flux map
        // is available for this pickup. Marked separately in documentation.
        const double gap=.0031;
        auto& d=displacement[size_t(s)];
        d=guard(d*.9992+velocity/sr,-.65*gap,.65*gap,0.);
        const double gradient=guard((gap*gap)/((gap+d)*(gap+d)),.2,8.,1.);
        emf+=(1.-.042*s)*velocity*gradient;
    }
    history[head]=guard(emf,-20.,20.);
    double filtered=0.;std::size_t index=head;
    for(int k=0;k<kMeasuredFIR15;++k){
        filtered+=kMeasuredCoil15[size_t(k)]*history[index];
        index=index==0?kMeasuredFIR15-1:index-1;
    }
    head=(head+1)%kMeasuredFIR15;
    dc+=lowpassAlpha(18.,sr)*(filtered-dc);
    return guard(filtered-dc,-8.,8.);
}
void MeasuredNylon15::prepare(double sampleRate) noexcept {
    sr=guard(sampleRate,8000.,192000.,48000.);reset();
}
void MeasuredNylon15::reset() noexcept {
    bass.fill(0.);treble.fill(0.);directHistory.fill(0.);
    directHP=0.;head=directHead=0;
}
MeasuredNylon15::Stereo MeasuredNylon15::tick(const PowerWaveStrings& strings,
        double bassVel,double trebleVel,double amount,double phase) noexcept {
    bassVel=guard(bassVel,-10.,10.);trebleVel=guard(trebleVel,-10.,10.);
    amount=guard(amount,0.,1.,.5);phase=guard(phase,0.,1.,.5);
    bass[head]=bassVel;treble[head]=trebleVel;
    double out[2]{};
    std::size_t index=head;
    for(int k=0;k<kMeasuredFIR15;++k){
        const double low=bass[index],hi=treble[index];
        out[0]+=kMeasuredNylon15[0][size_t(k)]*low+kMeasuredNylon15[1][size_t(k)]*hi;
        out[1]+=kMeasuredNylon15[2][size_t(k)]*low+kMeasuredNylon15[3][size_t(k)]*hi;
        index=index==0?kMeasuredFIR15-1:index-1;
    }
    head=(head+1)%kMeasuredFIR15;
    // String's direct acoustic path may interfere constructively or destructively
    // with soundboard pressure. Its mic-path phase is not measured here; this
    // parameter deliberately exposes a 0.12..0.95ms geometric delay hypothesis.
    double local=0.;
    for(int s=0;s<6;++s) local+=strings.sampleTransverseVelocity(s,0,.30)
                            +.20*strings.sampleTransverseVelocity(s,1,.30);
    directHP+=lowpassAlpha(600.,sr)*(local-directHP);
    directHistory[directHead]=guard(local-directHP,-10.,10.);
    const int samples=int(std::round((.00012+.00083*phase)*sr));
    const std::size_t delay=size_t(std::clamp(samples,0,126));
    const std::size_t read=(directHead+directHistory.size()-delay)%directHistory.size();
    const double direct=directHistory[read];
    directHead=(directHead+1)%directHistory.size();
    const double b=.28+.72*amount;
    // Output gain is *not* calibrated absolute Pa: model-to-SI conversion of
    // virtual modal bridge velocity is not yet instrumented on target guitar.
    return {guard(b*out[0]+.022*direct,-8.,8.),
            guard(b*out[1]+.021*direct,-8.,8.)};
}
}
