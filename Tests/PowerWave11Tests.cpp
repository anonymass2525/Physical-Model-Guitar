#include "../DSP/PowerWaveStrings.h"
#include "../DSP/StringEngine.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <stdexcept>
#include <algorithm>
namespace {
void check(bool b,const char* msg) {if(!b)throw std::runtime_error(msg);}
void testWave(int rate,int model){
 auto w=std::make_unique<pmg::PowerWaveStrings>();
 w->prepare(rate,model);
 w->setPhysics11(true);
 w->setContactParameters(.66,.1,.145);
 const int tuning[6]{40,45,50,55,59,64};
 for(int s=0;s<6;++s)check(w->strike(s,tuning[s],.86,.16),"note strike");
 double maxResidual=0.,maxPickup=0.,maxSound=0.;
 for(int i=0;i<rate/4;++i){
   if(i==rate/10)check(w->strike(2,53,.91,.18),"reattack");
   if(i==rate/6)w->release(1);
   auto f=w->tick();
   check(w->sane(),"physics11 unstable");
   check(std::isfinite(f.pickupVelocity),"nonfinite pickup");
   maxPickup=std::max(maxPickup,std::abs(f.pickupVelocity));
   maxSound=std::max(maxSound,std::abs(f.stringVelocity));
   maxResidual=std::max(maxResidual,std::abs(w->energyResidual()));
   check(w->storedStringEnergy()+w->storedBridgeEnergy()<=w->injectedEnergy()+1e-7,
         "energetic growth without external work");
 }
 const double tol=1e-8*std::max(1e-10,w->injectedEnergy());
 check(maxResidual<tol,"energy budget mismatch");
 check(maxPickup>1e-8&&maxSound>1e-8,"no wave or pickup signal");
 check(w->localContactDissipation11()>0.,"contact not dissipating");
 check(w->longitudinalDissipation11()>0.,"axial modes not damping");
 std::printf("v1.1 sr=%d model=%d input=%g stored=%g contact=%g axial=%g collisions=%d residual=%g pickup=%g\n",
 rate,model,w->injectedEnergy(),w->storedStringEnergy(),w->localContactDissipation11(),
 w->longitudinalDissipation11(),w->fretCollisions11(),maxResidual,maxPickup);
}
void testPickupNoFeedback(){
 auto a=std::make_unique<pmg::PowerWaveStrings>();
 auto b=std::make_unique<pmg::PowerWaveStrings>();
 a->prepare(48000,1);b->prepare(48000,1);
 a->setPhysics11(true);b->setPhysics11(true);
 a->setContactParameters(.6,.4,.12);b->setContactParameters(.6,.4,.38);
 a->strike(5,64,.8,.15);b->strike(5,64,.8,.15);
 double pickupDifference=0.;
 for(int i=0;i<4800;++i){
   const auto x=a->tick(),y=b->tick();
   check(x.stringVelocity==y.stringVelocity,"pickup injected into wave state");
   check(x.bridgeVelocity==y.bridgeVelocity,"pickup back-action not zero");
   pickupDifference+=std::abs(x.pickupVelocity-y.pickupVelocity);
 }
 check(pickupDifference>1e-5,"pickup position has no spectral impact");
 check(a->energyResidual()==b->energyResidual(),"pickup position changed energy ledger");
}
void testBypass(){
 auto a=std::make_unique<pmg::PowerWaveStrings>();
 auto b=std::make_unique<pmg::PowerWaveStrings>();
 a->prepare(48000,2);b->prepare(48000,2);
 a->setPhysics11(false);b->setPhysics11(false);
 a->strike(1,45,.85,.25);b->strike(1,45,.85,.25);
 for(int i=0;i<9600;++i){
  const auto x=a->tick(),y=b->tick();
  check(x.stringVelocity==y.stringVelocity && x.left==y.left && x.right==y.right,
        "off mode is not deterministic");
 }
}
}
int main(){try{
 for(int r:{44100,48000,96000})for(int m:{1,2,3})testWave(r,m);
 testPickupNoFeedback();testBypass();
 std::puts("PMG1.1 multi-contact power-wave tests passed");
 return 0;
}catch(const std::exception& e){std::fprintf(stderr,"PMG11 FAILED: %s\n",e.what());return 1;}}
