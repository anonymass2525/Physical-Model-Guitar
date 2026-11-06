#include "../DSP/StringEngine.h"
#include <cmath>
#include <cstdio>
#include <memory>
#include <stdexcept>
#include <algorithm>
#include <cstdint>
static void need(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
static pmg::Controls preset(int model,bool v14,bool unified=true){
    pmg::Controls c;
    c.physics10=c.physics11=true;c.physics12=unified;
    c.magnetic13=model==1;c.radiation13=model==3;
    c.magnetic14=v14 && model==1;c.radiation14=v14 && model==3;
    return c;
}
static void testNoninvasiveness(int model,double sr){
    auto old=std::make_unique<pmg::StringEngine>();
    auto now=std::make_unique<pmg::StringEngine>();
    old->prepare(sr,pmg::Model(model),preset(model,false));
    now->prepare(sr,pmg::Model(model),preset(model,true));
    const int note=model==3?76:64;
    const int stringIndex=5;
    need(old->noteOnString(note,.78f,stringIndex) && now->noteOnString(note,.78f,stringIndex),"note onset");
    double difference=0.,energy=0.,peak=0.;
    for(int i=0;i<int(sr*.16);++i){
        const auto a=old->nextFrame(),b=now->nextFrame();
        need(std::isfinite(a.left)&&std::isfinite(b.left)&&std::isfinite(b.right),"finite sound");
        peak=std::max(peak,double(std::max(std::abs(b.left),std::abs(b.right))));
        difference+=std::abs(a.left-b.left)+std::abs(a.right-b.right);
        energy+=b.left*b.left+b.right*b.right;
        need(old->powerWaveStringJoules10()==now->powerWaveStringJoules10(),"sensor modifies string energy");
        need(old->unifiedBridgeEnergy12()==now->unifiedBridgeEnergy12(),"sensor modifies bridge energy");
        need(old->powerWaveResidual10()==now->powerWaveResidual10(),"sensor modifies passivity ledger");
    }
    need(energy>1e-11,"output silent");need(peak<=1.00001,"digital clipping or NaN");
    if(model==1||model==3) need(difference>1e-4,"new observation has no effect");
    if(model==2) need(difference==0.,"electric or nylon update changed steel");
}
static void testIndividualTuning(int model){
    auto lo=std::make_unique<pmg::StringEngine>();
    auto hi=std::make_unique<pmg::StringEngine>();
    auto a=preset(model,true),b=a;
    if(model==1){a.magneticAttack14=.10f;b.magneticAttack14=.90f;}
    else {a.nylonTreble14=.10f;b.nylonTreble14=.90f;}
    lo->prepare(48000,pmg::Model(model),a);hi->prepare(48000,pmg::Model(model),b);
    need(lo->noteOnString(76,.8f,5) && hi->noteOnString(76,.8f,5),"upper string");
    double delta=0.;
    for(int i=0;i<9000;++i){
        const auto x=lo->nextFrame(),y=hi->nextFrame();
        delta+=std::abs(x.left-y.left)+std::abs(x.right-y.right);
        need(lo->powerWaveStringJoules10()==hi->powerWaveStringJoules10(),"parameter changed motion");
    }
    need(delta>1e-4,"tuning parameter had no effect");
}
static void testLegacyIsolation(){
    for(int model:{1,2,3}){
        auto old=std::make_unique<pmg::StringEngine>();
        auto now=std::make_unique<pmg::StringEngine>();
        auto a=preset(model,false,false),b=a;b.magnetic14=b.radiation14=true;
        old->prepare(48000,pmg::Model(model),a);now->prepare(48000,pmg::Model(model),b);
        old->noteOnString(64,.72f,5);now->noteOnString(64,.72f,5);
        for(int i=0;i<4096;++i){
            const auto x=old->nextFrame(),y=now->nextFrame();
            need(x.left==y.left && x.right==y.right,"v1.3 hybrid regression");
        }
    }
}
int main(){try{
    for(double sr:{44100.,48000.,96000.})for(int model:{1,2,3})testNoninvasiveness(model,sr);
    testIndividualTuning(1);testIndividualTuning(3);testLegacyIsolation();
    std::puts("PMG1.4 independent magnetic attack and high-band nylon radiation: passed");
    return 0;
}catch(const std::exception& e){std::fprintf(stderr,"PMG14 FAIL: %s\n",e.what());return 1;}}
