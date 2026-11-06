#include "LongRadiation16.h"
#include <algorithm>
#include <cmath>
#include <limits>
namespace pmg {
namespace {
constexpr double pi=3.14159265358979323846;
inline double finiteClamp(double a,double lo,double hi) noexcept {
    return std::clamp(std::isfinite(a)?a:0.,lo,hi);
}
}
void LongRadiation16::prepare(double sampleRate) noexcept {
    const double sr=finiteClamp(sampleRate,8000.,192000.);
    const double fs=(sr>0.)?sr:48000.;
    delaySamples=std::clamp(int(std::lround(double(earlySamples48k)*fs/48000.)),1,255);
    for(int j=0;j<kLongModeCount16;++j){
        const auto& m=kRadiationModes16[size_t(j)];
        const double r=std::exp(-1.0/(fs*finiteClamp(m.tau,.025,1.0)));
        const double f=finiteClamp(m.hz,40.,std::min(8000.,.44*fs));
        const double theta=2.*pi*f/fs;
        rotRe[size_t(j)]=r*std::cos(theta);
        rotIm[size_t(j)]=r*std::sin(theta);
    }
    reset();
}
void LongRadiation16::reset() noexcept {
    for(auto& s:states)for(auto& v:s)v=State{};
    for(auto& d:delayed)d.fill(0.);
    cursor=0;
}
LongRadiation16::Stereo LongRadiation16::tick(double bass,double treble) noexcept {
    const double a=finiteClamp(bass,-12.,12.),b=finiteClamp(treble,-12.,12.);
    delayed[0][size_t(cursor)]=a;
    delayed[1][size_t(cursor)]=b;
    const int read=(cursor+256-delaySamples)%256;
    const double v[2]{delayed[0][size_t(read)],delayed[1][size_t(read)]};
    cursor=(cursor+1)%256;
    double output[2]{};
    for(int j=0;j<kLongModeCount16;++j){
        const double cr=rotRe[size_t(j)],ci=rotIm[size_t(j)];
        for(int k=0;k<4;++k){
            auto& state=states[size_t(j)][size_t(k)];
            const double& in=v[k%2];
            const auto& coeff=kRadiationModes16[size_t(j)].residues;
            const double re=cr*state.re-ci*state.im+coeff[size_t(k*2)]*in;
            const double im=ci*state.re+cr*state.im+coeff[size_t(k*2+1)]*in;
            state={re,im};
            output[k/2]+=re;
        }
    }
    return {finiteClamp(output[0],-12.,12.),finiteClamp(output[1],-12.,12.)};
}
double LongRadiation16::maximumPoleMagnitude() const noexcept {
    double maxMag=0.;
    for(int j=0;j<kLongModeCount16;++j)
        maxMag=std::max(maxMag,std::hypot(rotRe[size_t(j)],rotIm[size_t(j)]));
    return maxMag;
}
}
