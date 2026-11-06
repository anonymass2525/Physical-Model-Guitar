#include "../DSP/TwoPortBridge.h"
#include "../DSP/StringEngine.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <memory>
#include <stdexcept>
using namespace pmg;
static void require(bool ok,const char* msg){if(!ok)throw std::runtime_error(msg);}
static void portIdentity(int sr,int model){
    auto p=MeasuredBridge::fallbackGuitar(model);
    TwoPortBridge bridge;bridge.prepare(sr,p);
    const auto z=TwoPortBridge::normalizedImpedance(model);
    double maxAbsoluteResidual=0.,maximumPerSampleMismatch=0.,maximumPowerMismatch=0.;
    double previousStored=0.,previousDissipated=0.,previousWork=0.;
    double maxForward=0.;
    for(int t=0;t<sr/2;++t){
        std::array<double,6> a{};
        if(t<400){
            a[0]=.07*std::sin(double(t)*.071);a[5]=.03*std::cos(double(t)*.113);
            a[2]=.015*std::sin(double(t)*.33);
        }
        const auto frame=bridge.tick(a,z);
        require(bridge.sane(),"bridge state invalid");
        const double dStored=bridge.storedEnergy()-previousStored;
        const double dDissipated=bridge.dissipatedEnergy()-previousDissipated;
        const double dWork=bridge.couplingWork()-previousWork;
        const double dWave=frame.incomingEnergy-frame.outgoingEnergy;
        maximumPerSampleMismatch=std::max(maximumPerSampleMismatch,std::abs(dStored+dDissipated-dWork));
        maximumPowerMismatch=std::max(maximumPowerMismatch,std::abs(dWave-dWork));
        maxAbsoluteResidual=std::max(maxAbsoluteResidual,std::abs(bridge.energyResidual()));
        maxForward=std::max(maxForward,std::abs(frame.outgoing[5]));
        previousStored=bridge.storedEnergy();
        previousDissipated=bridge.dissipatedEnergy();
        previousWork=bridge.couplingWork();
    }
    // Assertions are proportional to input work: floating-point summation error
    // must remain small even with all six strings and unequal characteristic Z.
    const double scale=std::max(1.,bridge.incomingEnergy());
    require(maxAbsoluteResidual<5.e-9*scale,"total wave/bridge energy mismatch");
    require(maximumPerSampleMismatch<2.e-10*scale,"midpoint mode work mismatch");
    require(maximumPowerMismatch<2.e-10*scale,"wave work != bridge port work");
    require(bridge.outgoingEnergy()>=0. && bridge.dissipatedEnergy()>=0.,"non-passive ledger");
    require(maxForward>1e-7,"no six-string cross-transfer");
    std::printf("energy test sr=%d model=%d maxResidual=%.3g, input=%g output=%g D=%g E=%g\n",
      sr,model,maxAbsoluteResidual,bridge.incomingEnergy(),bridge.outgoingEnergy(),bridge.dissipatedEnergy(),bridge.storedEnergy());
}
static void modelComparison(int sr,int model){
    Controls c; c.physics06=true;c.physics07=true;c.physics08=true;
    c.physics09=false;c.body=.50f;c.coupling=.6f;
    auto a=std::make_unique<StringEngine>(),b=std::make_unique<StringEngine>();
    a->prepare(sr,Model(model),c);c.physics09=true;b->prepare(sr,Model(model),c);
    const int note=(model==3?64:45),physicalString=(model==3?5:1);
    require(a->noteOnString(note,.78f,physicalString),"legacy note failed");
    require(b->noteOnString(note,.78f,physicalString),"bridge09 note failed");
    double powerA=0.,powerB=0.,maxPeak=0.,different=0.;
    for(int i=0;i<sr/3;++i){
        const auto old=a->nextFrame(),newer=b->nextFrame();
        require(std::isfinite(newer.left)&&std::isfinite(newer.right),"NaN in guitar DSP");
        powerA+=double(old.left)*old.left+double(old.right)*old.right;
        powerB+=double(newer.left)*newer.left+double(newer.right)*newer.right;
        different+=std::abs(newer.left-old.left)+std::abs(newer.right-old.right);
        maxPeak=std::max(maxPeak,std::max(std::abs(double(newer.left)),std::abs(double(newer.right))));
    }
    require(powerA>1e-7 && powerB>1e-7,"silent guitar");
    require(different>1e-5,"new bridge did not change sound");
    require(maxPeak<.99,"extreme clipped output");
    require(powerB/powerA>.01 && powerB/powerA<100.,"new bridge path runaway loudness");
    std::printf("guitar sr=%d model=%d eOld=%g eNew=%g peak=%g diff=%g\n",sr,model,powerA,powerB,maxPeak,different);
}
static void legacyExact(int model){
    // When the new boundary is OFF, identical parameter paths must be
    // bit-exact regardless of other 0.9 configuration details.
    auto a=std::make_unique<StringEngine>(),b=std::make_unique<StringEngine>();
    Controls c;c.physics06=true;c.physics07=true;c.physics08=true;c.physics09=false;
    a->prepare(48000.,Model(model),c);b->prepare(48000.,Model(model),c);
    a->noteOn(52,.76f);b->noteOn(52,.76f);
    for(int t=0;t<5000;++t){
        const auto x=a->nextFrame(),y=b->nextFrame();
        require(x.left==y.left && x.right==y.right,"legacy determinism changed");
    }
}
int main(){try{
    for(int sr:{44100,48000,96000})
        for(int model:{1,2,3})portIdentity(sr,model);
    for(int model:{1,2,3}){
        modelComparison(48000,model);
        legacyExact(model);
    }
    std::puts("TwoPortBridge09 tests passed");
}catch(const std::exception&e){std::fprintf(stderr,"TwoPortBridge09 FAILED: %s\n",e.what());return 1;}}
