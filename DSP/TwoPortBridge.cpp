#include "TwoPortBridge.h"
#include <algorithm>
#include <cmath>
#include <limits>
namespace pmg {
namespace {
constexpr double pi=3.14159265358979323846264338327950288;
double bounded(double x,double low,double high,double fallback) noexcept {
    return std::clamp(std::isfinite(x)?x:fallback,low,high);
}
// Each string's force acts on bass and treble at the same displacement field
// as its observed bridge endpoint velocity. This transpose relation is key to
// mechanical power conservation: sum_i F_i v_i = F_bridge^T v_bridge.
constexpr std::array<double,TwoPortBridge::nStrings> bassWeights{{.94,.82,.66,.42,.22,.06}};
}
std::array<double,TwoPortBridge::nStrings> TwoPortBridge::normalizedImpedance(int model) noexcept {
    // Normalized signal-domain impedances, NOT measured SI-string impedances.
    // Accurate absolute string impedance is future work with measured tensions
    // and linear densities. Positive values make the junction scattering passive.
    const double modelScale=(model==3?.72:model==2?1.1:model==1?1.:.88);
    return {{1750.*modelScale,1420.*modelScale,1160.*modelScale,
             970.*modelScale,810.*modelScale,650.*modelScale}};
}
void TwoPortBridge::prepare(double sr,const Params& params) noexcept {
    h=1./bounded(sr,8000.,192000.,48000.);
    for(unsigned i=0;i<nModes;++i) {
        auto& m=modes[i]; const auto& q=params[i];
        const double hz=bounded(q.frequencyHz,25.,.4/h,100.);
        const double Q=bounded(q.Q,.55,120.,7.);
        m.omega=2.*pi*hz;
        m.gamma=m.omega/Q;
        m.mass=bounded(q.effectiveMass,.001,10000.,1.);
        m.bx=bounded(q.directionX,-1.,1.,1.);
        m.by=bounded(q.directionY,-1.,1.,0.);
        const double length=std::hypot(m.bx,m.by);
        if(length>1.) {m.bx/=length;m.by/=length;}
        m.left=bounded(q.radiationL,-2.,2.,1.);
        m.right=bounded(q.radiationR,-2.,2.,1.);
    }
    reset();
}
void TwoPortBridge::reset() noexcept {
    for(auto& m:modes) m.p=m.v=0.;
    incomingWork=outgoingWork=workToBridge=dissipated=0.;
}
TwoPortBridge::Frame TwoPortBridge::tick(
        const std::array<double,nStrings>& incident,
        const std::array<double,nStrings>& impedances) noexcept {
    Frame result{};
    std::array<double,nStrings> a{},z{};
    // Force into two locations produced by incident waves, and weighted
    // impedance Gram matrix G=W Z W^T, guaranteed positive semidefinite.
    double drive[2]={0.,0.};
    double G00=0.,G01=0.,G11=0.;
    for(int i=0;i<nStrings;++i) {
        const double wb=bassWeights[size_t(i)],wt=1.-wb;
        a[size_t(i)]=bounded(incident[size_t(i)],-2.,2.,0.);
        z[size_t(i)]=bounded(impedances[size_t(i)],1e-4,10000.,1.);
        const double zi=z[size_t(i)];
        drive[0]+=2.*zi*a[size_t(i)]*wb;
        drive[1]+=2.*zi*a[size_t(i)]*wt;
        G00+=zi*wb*wb; G01+=zi*wb*wt; G11+=zi*wt*wt;
    }
    // Implicit midpoint bridge operator: V = V_free + Yd F, with Yd PSD.
    // No zero-delay feedback iteration. 2x2 direct solve is unconditionally
    // algebraically stable for positive Z and positive-real body modes.
    double free0=0.,free1=0.,Y00=0.,Y01=0.,Y11=0.;
    std::array<double,nModes> freeV{},compliance{};
    for(unsigned k=0;k<nModes;++k) {
        const auto& m=modes[k];
        const double d=1.+.5*h*m.gamma+.25*h*h*m.omega*m.omega;
        const double nextV0=((1.-.5*h*m.gamma-.25*h*h*m.omega*m.omega)*m.v
                            -h*m.omega*m.omega*m.p)/d;
        const double v0=.5*(m.v+nextV0);
        const double y=h/(2.*m.mass*d);
        freeV[k]=v0;compliance[k]=y;
        free0+=m.bx*v0;free1+=m.by*v0;
        Y00+=y*m.bx*m.bx;Y01+=y*m.bx*m.by;Y11+=y*m.by*m.by;
    }
    // Solve (I + Y G) v = v_free + Y drive (Y and G are symmetric PSD).
    const double m00=1.+Y00*G00+Y01*G01;
    const double m01=Y00*G01+Y01*G11;
    const double m10=Y01*G00+Y11*G01;
    const double m11=1.+Y01*G01+Y11*G11;
    const double rhs0=free0+Y00*drive[0]+Y01*drive[1];
    const double rhs1=free1+Y01*drive[0]+Y11*drive[1];
    const double det=m00*m11-m01*m10;
    // det is mathematically >=1; clamping protects against corrupt input only.
    const double safeDet=std::max(1e-30,det);
    const double velocity0=(m11*rhs0-m01*rhs1)/safeDet;
    const double velocity1=(m00*rhs1-m10*rhs0)/safeDet;
    const double force0=drive[0]-G00*velocity0-G01*velocity1;
    const double force1=drive[1]-G01*velocity0-G11*velocity1;
    result.bridgeVelocity={{velocity0,velocity1}};
    result.bridgeForce={{force0,force1}};
    double incomingPower=0.,outgoingPower=0.;
    for(int i=0;i<nStrings;++i) {
        const double vb=bassWeights[size_t(i)]*velocity0+(1.-bassWeights[size_t(i)])*velocity1;
        const double reflected=vb-a[size_t(i)];
        result.stringEndpointVelocity[size_t(i)]=vb;
        result.outgoing[size_t(i)]=reflected;
        incomingPower+=z[size_t(i)]*a[size_t(i)]*a[size_t(i)];
        outgoingPower+=z[size_t(i)]*reflected*reflected;
    }
    for(unsigned k=0;k<nModes;++k) {
        auto& m=modes[k];
        const double projection=m.bx*force0+m.by*force1;
        // The free midpoint velocity plus the forcing susceptibility.
        const double mid=freeV[k]+compliance[k]*projection;
        const double nextV=2.*mid-m.v;
        m.p+=h*mid; m.v=nextV;
        dissipated+=h*m.mass*m.gamma*mid*mid;
        result.radiatedL+=m.left*mid;
        result.radiatedR+=m.right*mid;
    }
    result.incomingEnergy=h*incomingPower;
    result.outgoingEnergy=h*outgoingPower;
    incomingWork+=result.incomingEnergy;
    outgoingWork+=result.outgoingEnergy;
    workToBridge+=h*(force0*velocity0+force1*velocity1);
    return result;
}
double TwoPortBridge::storedEnergy() const noexcept {
    double E=0.;
    for(const auto& m:modes)
        E+=.5*m.mass*(m.v*m.v+m.omega*m.omega*m.p*m.p);
    return E;
}
bool TwoPortBridge::sane() const noexcept {
    return std::isfinite(storedEnergy()) && std::isfinite(incomingWork)
      && std::isfinite(outgoingWork) && std::isfinite(dissipated)
      && std::isfinite(workToBridge) && storedEnergy()>=0.;
}
} // namespace pmg
