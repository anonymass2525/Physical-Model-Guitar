#include "../DSP/StringEngine.h"
#include <cmath>
#include <cstdio>
#include <memory>
#include <stdexcept>
#include <algorithm>
static void need(bool v,const char* what) {if(!v)throw std::runtime_error(what);}
static pmg::Controls configuration(bool magnetic,bool nylon,bool unified=true) {
    pmg::Controls c;
    c.physics06=c.physics07=c.physics08=c.physics09=true;
    c.physics10=c.physics11=true;c.physics12=unified;
    c.magnetic13=magnetic;c.radiation13=nylon;
    return c;
}
static void verify(int model,double sr){
    auto base=std::make_unique<pmg::StringEngine>();
    auto upgraded=std::make_unique<pmg::StringEngine>();
    auto ca=configuration(false,false),cb=configuration(model==1,model==3);
    base->prepare(sr,pmg::Model(model),ca);
    upgraded->prepare(sr,pmg::Model(model),cb);
    need(base->noteOnString(55,.78f,3) && upgraded->noteOnString(55,.78f,3),"strike");
    double difference=0.,power=0.,maxAmplitude=0.;
    for(int i=0;i<int(sr*.13);++i){
        const auto a=base->nextFrame(),b=upgraded->nextFrame();
        need(std::isfinite(a.left)&&std::isfinite(b.left)&&std::isfinite(b.right),"nonfinite output");
        maxAmplitude=std::max(maxAmplitude,double(std::max(std::abs(b.left),std::abs(b.right))));
        power+=b.left*b.left+b.right*b.right;
        difference+=std::abs(a.left-b.left)+std::abs(a.right-b.right);
        // Strict sensor noninvasiveness: even a newly tuned coil cannot
        // change the wave, bridge, or its cumulative energy ledger.
        need(base->powerWaveStringJoules10()==upgraded->powerWaveStringJoules10(),"observer changed string energy");
        need(base->unifiedBridgeEnergy12()==upgraded->unifiedBridgeEnergy12(),"observer changed bridge energy");
        need(base->powerWaveResidual10()==upgraded->powerWaveResidual10(),"observer changed energy residual");
    }
    need(power>1e-10,"silent instrument");
    if(model==1||model==3)need(difference>1e-4,"updated observer had no audible effect");
    if(model==2)need(difference==0.,"Electric/Nylon switches affected Steel");
    need(maxAmplitude<=1.0001,"unbounded digital output");
    need(std::abs(upgraded->powerWaveResidual10())<1e-8,"ledger residual too high");
}
static void legacyReproduction(){
    auto a=std::make_unique<pmg::StringEngine>(),b=std::make_unique<pmg::StringEngine>();
    auto c=configuration(false,false,false),d=configuration(true,true,false);
    a->prepare(48000,pmg::Model::electric,c);b->prepare(48000,pmg::Model::electric,d);
    a->noteOnString(55,.75f,3);b->noteOnString(55,.75f,3);
    for(int i=0;i<4096;++i){auto x=a->nextFrame(),y=b->nextFrame();
        need(x.left==y.left && x.right==y.right,"legacy hybrid sound altered by 1.3 flag");}
}
static void pickupPositionChangesSignalOnly(){
    auto a=std::make_unique<pmg::StringEngine>(),b=std::make_unique<pmg::StringEngine>();
    auto c=configuration(true,false),d=c;c.pickup=.09f;d.pickup=.27f;
    a->prepare(48000,pmg::Model::electric,c);b->prepare(48000,pmg::Model::electric,d);
    a->noteOnString(64,.75f,5);b->noteOnString(64,.75f,5);
    double diff=0.;
    for(int i=0;i<3000;++i){auto x=a->nextFrame(),y=b->nextFrame();
        diff+=std::abs(x.left-y.left);
        need(a->powerWaveStringJoules10()==b->powerWaveStringJoules10(),"pickup location affected physics");}
    need(diff>1e-4,"pickup position had no effect");
}
int main(){try{
    for(double sr:{44100.,48000.,96000.})for(int m=1;m<=3;++m)verify(m,sr);
    legacyReproduction();pickupPositionChangesSignalOnly();
    std::puts("PMG 1.3 observer tests: model isolation, no feedback, legacy regression, sample-rate finite");
    return 0;
}catch(const std::exception& e){std::fprintf(stderr,"PMG13 FAIL %s\n",e.what());return 1;}}
