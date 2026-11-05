#include "../DSP/MeasuredBridge.h"
#include "../DSP/StringEngine.h"
#include <cmath>
#include <cstdio>
#include <memory>
#include <stdexcept>
using namespace pmg;
static void check(bool yes,const char*s){if(!yes)throw std::runtime_error(s);}
int main(){try{
 for(int sr:{44100,48000,96000}){
  MeasuredBridge b;b.prepare(sr,MeasuredBridge::fallbackGuitar(3));
  double err=0.,peak=0.;
  for(int i=0;i<sr/2;++i){double f=i<sr/40?12.*std::sin(i*.02):0.;
   b.tick(f,-f*.35);check(b.sane(),"nonfinite bridge");
   peak=std::max(peak,b.storedEnergy());
   err=std::max(err,std::abs(b.storedEnergy()+b.integratedDissipation()-b.integratedInputWork()));
  }
  check(peak>1.e-12,"bridge never excited");
  check(b.storedEnergy()<peak,"no mechanical dissipation");
  check(err<1.e-8,"energy work balance failed");
  std::printf("sr=%d energyResidual=%g\n",sr,err);
 }
 for(int m=1;m<=3;++m){
  double scores[2]{};
  for(int enabled=0;enabled<2;++enabled){
   auto e=std::make_unique<StringEngine>();Controls c;
   c.physics06=true;c.physics07=true;c.physics08=enabled!=0;c.body=.45f;c.coupling=.6f;
   e->prepare(48000,Model(m),c);check(e->noteOnString(m==3?64:45,.8f,m==3?5:1),"unplayable");
   for(int i=0;i<24000;++i){auto s=e->nextFrame();
    check(std::isfinite(s.left)&&std::isfinite(s.right),"nonfinite output");
    check(std::abs(s.left)<1.001f&&std::abs(s.right)<1.001f,"peak clipped");
    double y=.5*(s.left+s.right);scores[enabled]+=y*y;
   }
  }
  check(scores[0]>1e-12&&scores[1]>1e-12,"silence");
  check(std::abs(scores[1]-scores[0])>1e-7,"bridge coupling not observable");
 }
 puts("Bridge08 PASS");
}catch(const std::exception&e){std::fprintf(stderr,"Bridge08 FAILED: %s\n",e.what());return 1;}}
