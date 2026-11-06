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
    pickEnergyWave=axialQ=axialP=0.;fretDisplacement.fill(0.);
    fretCooldown.fill(0);pickSamples=0;
}
void PowerWaveStrings::prepare(double sr,int model) noexcept {
    rate=clip(sr,8000.,192000.);dt=1./rate;
    guitarModel=std::clamp(model,0,3);
    externalEnergy=lostToPropagation=mutedEnergy=maxResidual=0.;
    contactDissipation11=axialDissipation11=0.; collisions11=0;
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
void PowerWaveStrings::setPhysics11(bool enabled) noexcept {
    if(physics11==enabled)return;
    if(!enabled) for(auto& both:waves)for(auto& w:both) {
        // Switching off retires all outstanding local contact state as absorbed energy.
        contactDissipation11+=w.pickEnergyWave*w.pickEnergyWave;
        axialDissipation11+=w.axialQ*w.axialQ+w.axialP*w.axialP;
        w.pickEnergyWave=w.axialQ=w.axialP=0.;w.pickSamples=0;
        w.fretDisplacement.fill(0.);w.fretCooldown.fill(0);
    }
    physics11=enabled;
}
void PowerWaveStrings::setMechanisms(bool pick,bool fret,bool longitudinal,bool sensor) noexcept {
    // The instrument can be ablated while sounding: retire disconnected stored
    // energies through explicit (non-negative) dissipative pathways.
    if(!pick && enablePick)for(auto& both:waves)for(auto& w:both){
        contactDissipation11+=w.pickEnergyWave*w.pickEnergyWave;
        w.pickEnergyWave=0.;w.pickSamples=0;
    }
    if(!longitudinal && enableAxial)for(auto& both:waves)for(auto& w:both){
        axialDissipation11+=w.axialQ*w.axialQ+w.axialP*w.axialP;
        w.axialQ=w.axialP=0.;
    }
    enablePick=pick;enableFret=fret;enableAxial=longitudinal;enableSensor=sensor;
}
void PowerWaveStrings::setContactParameters(double hard,double clearance,double pickup) noexcept {
    pickHardness=clip(hard,0.,1.);fretClearance=clip(clearance,0.,1.);
    pickupLocation=clip(pickup,.03,.49);
}
double PowerWaveStrings::waveStoredEnergy(const Wave& w,double h) noexcept {
    return w.queueEnergy+h*w.z*w.apState*w.apState
           +w.pickEnergyWave*w.pickEnergyWave+w.axialQ*w.axialQ+w.axialP*w.axialP;
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
        mutedEnergy+=waveStoredEnergy(w,dt);
        w.clear();w.nominalHz=f;w.pickPosition=pick;
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
        if(physics11 && enablePick) {
            // Energy supplied at pick contact by the performer: a compliant
            // pick spring, not a free additive output transient.
            const double pickJ=w.queueEnergy*(.045+.055*(1.-pickHardness));
            w.pickEnergyWave=std::sqrt(std::max(0.,pickJ));
            w.pickSamples=std::max(1,int(rate*(.0018+.0028*(1.-pickHardness))));
            externalEnergy+=pickJ;
        }
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
    // Observe travelling waves without loading them. This is an *idealized*
    // velocity-sensitive magnetic/acoustic readout; pickup back-action is zero.
    // Acoustic near-field observation at a fixed physical fraction of each
    // speaking length, separate from the electromagnetic pickup position.
    // Read-only observations never modify the physical energy ledger.
    for(int s=0;s<strings;++s)for(int plane=0;plane<planes;++plane){
        const auto& w=waves[size_t(s)][size_t(plane)];
        const int n=w.integerDelay, k=std::clamp(int(std::lround(n*.27)),2,n/2);
        const double a=w.ring[size_t((w.cursor+k)%n)];
        const double b=w.ring[size_t((w.cursor+n-k)%n)];
        result.stringTapVelocity+=(plane==0?.82:.18)*(.64*a-.36*b);
    }
    if(physics11 && enableSensor) for(int s=0;s<strings;++s){
        const auto& both=waves[size_t(s)];
        for(int p=0;p<planes;++p){
            const auto& w=both[size_t(p)];
            const int n=w.integerDelay;
            const int tap=std::clamp(int(std::lround(n*pickupLocation)),2,n/2);
            const double forward=w.ring[size_t((w.cursor+tap)%n)];
            const double backward=w.ring[size_t((w.cursor+n-tap)%n)];
            const double aperture=(w.ring[size_t((w.cursor+tap+1)%n)]
                +w.ring[size_t((w.cursor+tap+n-1)%n)])*.5;
            result.pickupVelocity+=(p==0?.84:.16)*(.65*forward-.30*backward+.35*aperture);
        }
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
            // Nonlinear tension shortens the effective phase delay at large
            // excursions. Recompute the orthogonal allpass coefficients so
            // the phase change does not create or destroy wave energy.
            const double a=physics11 ? clip(w.allpassA+.028*std::tanh(14.*std::abs(reflected)),-.97,.97)
                                     : w.allpassA;
            const double b=physics11 ? std::sqrt(std::max(0.,1.-a*a)) : w.allpassB;
            const double afterAp=a*intoNut+b*previousAp;
            w.apState=b*intoNut-a*previousAp;
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
    if(physics11)applyLocalContacts11();
    const double residual=energyResidual();
    if(std::isfinite(residual))maxResidual=std::max(maxResidual,std::abs(residual));
    return result;
}
double PowerWaveStrings::storedStringEnergy() const noexcept {
    double e=0.;for(const auto& both:waves)for(const auto& w:both)
        e+=waveStoredEnergy(w,dt);
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
        +lostToPropagation+mutedEnergy+contactDissipation11+axialDissipation11-externalEnergy;
}
bool PowerWaveStrings::sane() const noexcept {
    return bridges[0].sane()&&bridges[1].sane()
        &&std::isfinite(energyResidual())&&storedStringEnergy()>=-1e-8
        &&std::isfinite(lostToPropagation)&&lostToPropagation>=-1e-8;
}
void PowerWaveStrings::applyLocalContacts11() noexcept {
    // All interactions act on real waveguide state, NEVER on the output mix.
    // Wave cell has physical energy h Z v^2. An orthogonal scattering between
    // sqrt(h Z)*v and sqrt(J) contact coordinates is exactly power balanced.
    for(int si=0;si<strings;++si){
        auto& both=waves[size_t(si)];
        for(int p=0;p<planes;++p){
            auto& w=both[size_t(p)];
            const int n=w.integerDelay;
            const double norm=std::sqrt(dt*w.z);
            // Compliant finite-duration pluck: contact spring releases energy
            // into the wave at a physical location. On detachment the remaining
            // spring energy is dissipated by the player's hand/pick.
            if(w.pickSamples>0){
                const int loc=std::clamp(int(std::lround(n*w.pickPosition)),2,n-2);
                const int ix=(w.cursor+loc)%n;
                const double x=norm*w.ring[size_t(ix)];
                const double q=w.pickEnergyWave;
                const double theta=.16+.18*pickHardness;
                const double co=std::cos(theta),sn=std::sin(theta);
                const double xn=co*x+sn*q,qn=co*q-sn*x;
                w.queueEnergy+=xn*xn-x*x;
                w.ring[size_t(ix)]=xn/norm;
                w.pickEnergyWave=qn;
                if(--w.pickSamples==0){
                    contactDissipation11+=qn*qn;
                    w.pickEnergyWave=0.;
                }
            }
            // Unilateral dissipative fret collisions. Contact position and
            // penetration are low-order waveguide proxies, not yet exact
            // string/fret geometric constraints. Reflection cannot gain energy.
            for(int k=0;k<3 && enableFret;++k){
                const int ix=(w.cursor+std::clamp(int(n*(.18+.19*k)),2,n-2))%n;
                const double v=w.ring[size_t(ix)];
                double& displacement=w.fretDisplacement[size_t(k)];
                displacement=.9999*displacement+dt*v;
                int& cool=w.fretCooldown[size_t(k)];
                if(cool>0)--cool;
                const double gap=(.000004+.000014*fretClearance)*(1.+.28*k);
                if(p==0 && cool==0 && displacement<-gap && v<-.0005){
                    const double depth=std::min(.0001,-gap-displacement);
                    const double restitution=clip(.94-.30*std::tanh(15000.*depth),.56,.94);
                    const double updated=-restitution*v;
                    const double oldE=dt*w.z*v*v,newE=dt*w.z*updated*updated;
                    w.ring[size_t(ix)]=updated;
                    w.queueEnergy+=newE-oldE;
                    contactDissipation11+=oldE-newE;
                    displacement=-gap;
                    cool=std::max(1,int(rate/1600.));
                    ++collisions11;
                }
            }
        }
        // Geometric nonlinear transverse/longitudinal interaction: orthogonal
        // rotations share stored energy between two transverse components and
        // one axial oscillator. Variable angle depends on excitation amplitude;
        // this is a *reduced-order* nonlinear effect, not a full PDE solver.
        if(!enableAxial) continue;
        auto& a=both[0];auto& b=both[1];
        const int ia=(a.cursor+std::clamp(int(a.integerDelay*.41),2,a.integerDelay-2))%a.integerDelay;
        const int ib=(b.cursor+std::clamp(int(b.integerDelay*.41),2,b.integerDelay-2))%b.integerDelay;
        const double na=std::sqrt(dt*a.z),nb=std::sqrt(dt*b.z);
        double xa=na*a.ring[size_t(ia)],xb=nb*b.ring[size_t(ib)];
        double q=a.axialQ,p=a.axialP;
        const double theta=.032*std::tanh(1200.*std::hypot(xa,xb));
        const double ca=std::cos(theta),sa=std::sin(theta);
        const double nx=ca*xa+sa*q;
        q=ca*q-sa*xa;
        const double ny=ca*xb+sa*q;
        q=ca*q-sa*xb;
        a.queueEnergy+=nx*nx-xa*xa;
        b.queueEnergy+=ny*ny-xb*xb;
        a.ring[size_t(ia)]=nx/na;
        b.ring[size_t(ib)]=ny/nb;
        const double omega=2.*pi*(guitarModel==3?820.:1430.)/rate;
        const double phaseQ=std::cos(omega)*q-std::sin(omega)*p;
        const double phaseP=std::sin(omega)*q+std::cos(omega)*p;
        const double damping=std::exp(-8./rate);
        axialDissipation11+=(1.-damping*damping)*(phaseQ*phaseQ+phaseP*phaseP);
        a.axialQ=damping*phaseQ;
        a.axialP=damping*phaseP;
    }
}
double PowerWaveStrings::stringImpedance(int s) const noexcept {
    return s>=0&&s<strings?waves[size_t(s)][0].z:0.;
}
double PowerWaveStrings::stringLength(int s) const noexcept {
    return s>=0&&s<strings?waves[size_t(s)][0].stringLength:0.;
}
} // namespace pmg

namespace pmg {
double PowerWaveStrings::sampleTransverseVelocity(int stringIndex,int plane,double fraction) const noexcept {
    if(stringIndex<0||stringIndex>=strings||plane<0||plane>=planes)return 0.;
    const auto& w=waves[size_t(stringIndex)][size_t(plane)];
    const int n=w.integerDelay;
    if(n<4)return 0.;
    fraction=std::clamp(std::isfinite(fraction)?fraction:.15,.003,.497);
    const double sampleOffset=fraction*n;
    const int lo=std::clamp(int(sampleOffset),1,n/2-1);
    const int hi=std::min(n/2,lo+1);
    const double frac=sampleOffset-lo;
    const auto at=[&](int off){
        const double forward=w.ring[size_t((w.cursor+off)%n)];
        const double backward=w.ring[size_t((w.cursor+n-off)%n)];
        return forward-backward;
    };
    return (1.-frac)*at(lo)+frac*at(hi);
}
}
