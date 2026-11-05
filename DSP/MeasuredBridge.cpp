#include "MeasuredBridge.h"
#include <cmath>
#include <algorithm>
namespace pmg {
namespace{constexpr double pi=3.14159265358979323846;double clamp(double x,double a,double b) noexcept{return std::max(a,std::min(b,x));}}
std::array<MeasuredBridge::ModeParams,MeasuredBridge::numberOfModes> MeasuredBridge::fallbackGuitar(int model) noexcept{
 const double freq[]{82,111,182,210,284,397,535,742,1020,1580};
 const double invMass[]{.035,.050,.048,.036,.026,.019,.015,.011,.008,.005};
 std::array<ModeParams,numberOfModes> result{};
 for(unsigned i=0;i<numberOfModes;++i){
  auto&m=result[i]; const double sf=(model==1?1.35:model==3?.93:1.);
  m.frequencyHz=freq[i]*sf;
  m.Q=(model==1?2.5:model==3?5.7:8.1)+i*.48;
  m.effectiveMass=1./invMass[i];
  const double a=i%3==0?.18:i%3==1?.55:.8;
  m.directionX=std::sqrt(1.-a*a);m.directionY=i%2?-a:a;
  m.radiationL=i%2==0?1.:.55;m.radiationR=i%2?1.:.55;
 }return result;
}
void MeasuredBridge::prepare(double sr,const std::array<ModeParams,numberOfModes>& p) noexcept{
 dt=1./clamp(sr,8000.,192000.);
 for(unsigned i=0;i<numberOfModes;++i){
  auto&m=modes[i];m.param=p[i];auto&x=m.param;
  x.frequencyHz=clamp(std::isfinite(x.frequencyHz)?x.frequencyHz:100.,30.,.40/dt);
  x.Q=clamp(std::isfinite(x.Q)?x.Q:7.,.55,120.);
  x.effectiveMass=clamp(std::isfinite(x.effectiveMass)?x.effectiveMass:1.,.001,10000.);
  x.directionX=clamp(std::isfinite(x.directionX)?x.directionX:1.,-1.,1.);
  x.directionY=clamp(std::isfinite(x.directionY)?x.directionY:0.,-1.,1.);
  const double length=std::hypot(x.directionX,x.directionY);
  if(length>1.){x.directionX/=length;x.directionY/=length;}
  x.radiationL=clamp(std::isfinite(x.radiationL)?x.radiationL:1.,-2.,2.);
  x.radiationR=clamp(std::isfinite(x.radiationR)?x.radiationR:1.,-2.,2.);
  m.omega=2.*pi*x.frequencyHz;m.damping=m.omega/x.Q;
 }reset();
}
void MeasuredBridge::reset() noexcept{
 for(auto&m:modes){m.displacement=0.;m.velocity=0.;}totalWork=totalDissipation=0.;
}
MeasuredBridge::Frame MeasuredBridge::tick(double fx,double fy) noexcept{
 fx=clamp(std::isfinite(fx)?fx:0.,-200.,200.);fy=clamp(std::isfinite(fy)?fy:0.,-200.,200.);
 double vx=0.,vy=0.,left=0.,right=0.,loss=0.;
 for(auto&m:modes){
  const double h=dt,w=m.omega,g=m.damping,a=w*w*h*h*.25,b=g*h*.5;
  const double force=(m.param.directionX*fx+m.param.directionY*fy)/m.param.effectiveMass;
  const double nextV=((1.-b-a)*m.velocity-h*w*w*m.displacement+h*force)/(1.+b+a);
  const double mid=(m.velocity+nextV)*.5;
  m.displacement+=h*mid;m.velocity=nextV;
  vx+=m.param.directionX*mid;vy+=m.param.directionY*mid;
  left+=m.param.radiationL*mid;right+=m.param.radiationR*mid;
  loss+=m.param.effectiveMass*g*mid*mid*h;
 }
 totalWork+=dt*(fx*vx+fy*vy);totalDissipation+=loss;
 return {float(vx),float(vy),float(left),float(right)};
}
double MeasuredBridge::storedEnergy()const noexcept{
 double e=0.;for(const auto&m:modes)e+=.5*m.param.effectiveMass*(m.velocity*m.velocity+m.omega*m.omega*m.displacement*m.displacement);
 return e;
}
bool MeasuredBridge::sane()const noexcept{
 return std::isfinite(storedEnergy())&&std::isfinite(totalWork)&&std::isfinite(totalDissipation)&&storedEnergy()>=0.;
}
std::array<MeasuredBridge::ModeParams,MeasuredBridge::numberOfModes> MeasuredBridge::parameters()const noexcept{
 std::array<ModeParams,numberOfModes> p{};for(unsigned i=0;i<numberOfModes;++i)p[i]=modes[i].param;return p;
}
} // namespace pmg
