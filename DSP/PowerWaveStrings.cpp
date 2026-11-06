#include "PowerWaveStrings.h"
#include <algorithm>
#include <cmath>
#include <limits>
namespace pmg {
namespace {
constexpr double pi=3.1415926535897932384626433832795;
constexpr std::array<int,6> openMidi{{40,45,50,55,59,64}};
constexpr std::array<double,6> fOpen{{82.406889,110.0,146.832384,195.997718,246.941651,329.627557}};
constexpr std::array<double,6> tensionElectric{{78.,72.,70.,68.,69.,71.}};
constexpr std::array<double,6> tensionSteel{{80.,76.,75.,70.,68.,68.}};
constexpr std::array<double,6> tensionNylon{{60.,52.,49.,46.,43.,43.}};
const std::array<double,6>& tensions(int m) noexcept {
    return m==3?tensionNylon:m==2?tensionSteel:tensionElectric;
}
double clip(double v,double lo,double hi) noexcept {return std::clamp(std::isfinite(v)?v:lo,lo,hi);}
}
void PowerWaveStrings::Wave::clear() noexcept {
    ring.fill(0.);cursor=0;apState=queueEnergy=0.;releasing=false;
}
void PowerWaveStrings::prepare(double sr,int model) noexcept {
    rate=clip(sr,8000.,192000.);dt=1./rate;
    guitarModel=std::clamp(model,0,3);
    externalEnergy=lostToPropagation=mutedEnergy=maxResidual=0.;
    auto modes=MeasuredBridge::fallbackGuitar(guitarModel);
    // Explicit physical mass ASSUMPTIONS, not yet measured modal masses.
    // Compared with v0.9's normalized 20-200 unit inertias these approximate
    // effective kg per soundboard mode and permit SI wave coupling.
    for(unsigned i=0;i<modes.size();++i) modes[i].effectiveMass=.032+double(i)*.009;
    for(auto& bridge:bridges)bridge.prepare(rate,modes);
    for(int s=0;s<strings;++s) {
        const double tension=tensions(guitarModel)[size_t(s)];
        const double z=tension/(2.*fOpen[size_t(s)]*.65); // sqrt(T*mu) with mu=T/(2 f_open L)^2
        for(int p=0;p<planes;++p) {
            auto& w=waves[size_t(s)][size_t(p)];w.clear();
            w.z=z;w.nominalHz=fOpen[size_t(s)];w.stringLength=.65;
            w.integerDelay=std::clamp(int(std::floor(rate/w.nominalHz)),16,maxDelay-1);
            const double fractional=clip(rate/w.nominalHz-w.integerDelay,1e-8,1.);
            w.allpassA=(1.-fractional)/(1.+fractional);
            w.allpassB=std::sqrt(std::max(0.,1.-w.allpassA*w.allpassA));
            w.gain=.997;
        }
    }
    setDecay(decaySec);
}
void PowerWaveStrings::setDecay(double seconds) noexcept {
    decaySec=clip(seconds,.5,20.);
    for(auto& both:waves)for(auto& w:both)
        w.gain=std::exp(-6.907755278982137/(decaySec*w.nominalHz));
}
bool PowerWaveStrings::strike(int s,int midiNote,double velocity,double pluck) noexcept {
    if(s<0||s>=strings || midiNote<openMidi[size_t(s)] || midiNote>openMidi[size_t(s)]+24)return false;
    const double f=440.*std::pow(2.,double(midiNote-69)/12.);
    const double pick=clip(pluck,.06,.46);
    const double strength=.005+.048*clip(velocity,0.,1.);
    const double delay=rate/f;
    const int n=std::clamp(int(std::floor(delay)),16,maxDelay-1);
    const double frac=clip(delay-n,1e-8,1.);
    // Onset/reattack inputs are counted as external work; old wave energy
    // is counted explicitly as energy lost to stopping the previous string.
    for(int p=0;p<planes;++p) {
        auto& w=waves[size_t(s)][size_t(p)];
        mutedEnergy+=w.queueEnergy+dt*w.z*w.apState*w.apState;
        w.clear();w.nominalHz=f;
        w.stringLength=.65*fOpen[size_t(s)]/f;
        w.integerDelay=n;
        w.allpassA=(1.-frac)/(1.+frac);
        w.allpassB=std::sqrt(std::max(0.,1.-w.allpassA*w.allpassA));
        w.gain=std::exp(-6.907755278982137/(decaySec*f))*(p==0?1.:.9999);
        const double amplitude=strength*(p==0?.84:.28);
        // Piecewise-constant velocity-wave profile is the travelling-wave
        // representation of an ideal triangular initial displacement. A small
        // raised-cosine shoulder suppresses unrealistic infinite-bandwidth
        // corners; no samples or stochastic buzz are added.
        const double hi=1./pick,lo=-1./(1.-pick);
        double sum=0.;
        for(int k=0;k<n;++k){
            const double x=(k+.5)/double(n);
            const double t=clip((x-pick)/.018,-1.,1.);
            const double raised=.5+.5*std::sin(t*pi*.5);
            const double wav=amplitude*(hi*(1.-raised)+lo*raised)*.15;
            w.ring[size_t(k)]=wav;
            sum+=wav;
        }
        // Remove DC so the rigidly terminated waveguide does not retain a
        // constant velocity offset across the whole round trip.
        const double mean=sum/double(n);
        w.queueEnergy=0.;
        for(int k=0;k<n;++k){
            w.ring[size_t(k)]-=mean;
            const double x=w.ring[size_t(k)];
            w.queueEnergy+=dt*w.z*x*x;
        }
        externalEnergy+=w.queueEnergy;
    }
    return true;
}
void PowerWaveStrings::release(int s) noexcept {
    if(s>=0&&s<strings)for(auto& w:waves[size_t(s)])w.releasing=true;
}
void PowerWaveStrings::releaseAll() noexcept {
    for(int i=0;i<strings;++i)release(i);
}
PowerWaveStrings::Frame PowerWaveStrings::tick() noexcept {
    Frame result{};
    std::array<std::array<double,strings>,planes> incident{};
    std::array<double,strings> z{};
    for(int s=0;s<strings;++s){
        z[size_t(s)]=waves[size_t(s)][0].z;
        for(int p=0;p<planes;++p)
            incident[size_t(p)][size_t(s)]=waves[size_t(s)][size_t(p)].read();
    }
    std::array<TwoPortBridge::Frame,planes> frames{};
    for(int p=0;p<planes;++p)frames[size_t(p)]=bridges[size_t(p)].tick(incident[size_t(p)],z);
    for(int s=0;s<strings;++s){
        for(int p=0;p<planes;++p){
            auto& w=waves[size_t(s)][size_t(p)];
            const double original=w.read();
            const double reflected=frames[size_t(p)].outgoing[size_t(s)];
            const double gain=w.gain*(w.releasing?.99985:1.);
            const double intoNut=-gain*reflected;
            const double previousAp=w.apState;
            const double afterAp=w.allpassA*intoNut+w.allpassB*previousAp;
            w.apState=w.allpassB*intoNut-w.allpassA*previousAp;
            w.ring[size_t(w.cursor)]=afterAp;
            w.cursor=(w.cursor+1)%w.integerDelay;
            w.queueEnergy+=dt*w.z*(afterAp*afterAp-original*original);
            lostToPropagation+=dt*w.z*(reflected*reflected-intoNut*intoNut);
            const double stringVelocity=original+reflected;
            result.stringVelocity+=(p==0?.72:.28)*stringVelocity;
        }
    }
    for(int p=0;p<planes;++p){
        result.left +=frames[size_t(p)].radiatedL*(p==0?1.:.42);
        result.right+=frames[size_t(p)].radiatedR*(p==0?.42:1.);
        result.bridgeVelocity+=.5*(frames[size_t(p)].bridgeVelocity[0]+frames[size_t(p)].bridgeVelocity[1]);
    }
    const double residual=energyResidual();
    if(std::isfinite(residual))maxResidual=std::max(maxResidual,std::abs(residual));
    return result;
}
double PowerWaveStrings::storedStringEnergy() const noexcept {
    double e=0.;for(const auto& both:waves)for(const auto& w:both)
        e+=w.queueEnergy+dt*w.z*w.apState*w.apState;
    return e;
}
double PowerWaveStrings::storedBridgeEnergy() const noexcept {
    return bridges[0].storedEnergy()+bridges[1].storedEnergy();
}
double PowerWaveStrings::bridgeDissipation() const noexcept {
    return bridges[0].dissipatedEnergy()+bridges[1].dissipatedEnergy();
}
double PowerWaveStrings::energyResidual() const noexcept {
    return storedStringEnergy()+storedBridgeEnergy()+bridgeDissipation()
        +lostToPropagation+mutedEnergy-externalEnergy;
}
bool PowerWaveStrings::sane() const noexcept {
    return bridges[0].sane()&&bridges[1].sane()
        &&std::isfinite(energyResidual())&&storedStringEnergy()>=-1e-8
        &&std::isfinite(lostToPropagation)&&lostToPropagation>=-1e-8;
}
double PowerWaveStrings::stringImpedance(int s) const noexcept {
    return s>=0&&s<strings?waves[size_t(s)][0].z:0.;
}
double PowerWaveStrings::stringLength(int s) const noexcept {
    return s>=0&&s<strings?waves[size_t(s)][0].stringLength:0.;
}
} // namespace pmg
