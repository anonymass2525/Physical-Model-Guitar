#include "../DSP/StringEngine.h"
#include "../DSP/MeasuredObserver15Coeffs.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <memory>
#include <stdexcept>
static void require(bool b,const char* text){if(!b)throw std::runtime_error(text);}
static pmg::Controls preset(int type,bool m15,bool n15){
    pmg::Controls c;c.physics10=c.physics11=c.physics12=true;
    c.magnetic14=type==1;c.radiation14=type==3;
    c.magnetic15=m15;c.radiation15=n15;
    return c;
}
static void run(double rate,int type){
    auto old=std::make_unique<pmg::StringEngine>();
    auto now=std::make_unique<pmg::StringEngine>();
    auto a=preset(type,false,false),b=preset(type,type==1,type==3);
    old->prepare(rate,pmg::Model(type),a);now->prepare(rate,pmg::Model(type),b);
    require(old->noteOnString(76,.8f,5)&&now->noteOnString(76,.8f,5),"cannot begin note");
    double delta=0.,energy=0.,peak=0.;
    for(int i=0;i<int(.13*rate);++i){
        auto x=old->nextFrame(),y=now->nextFrame();
        require(std::isfinite(y.left)&&std::isfinite(y.right),"nonfinite output");
        require(old->powerWaveStringJoules10()==now->powerWaveStringJoules10(),"observer perturbed string energy");
        require(old->unifiedBridgeEnergy12()==now->unifiedBridgeEnergy12(),"observer perturbed bridge energy");
        require(old->powerWaveResidual10()==now->powerWaveResidual10(),"observer perturbed energy ledger");
        delta+=std::abs(y.left-x.left)+std::abs(y.right-x.right);
        energy+=y.left*y.left+y.right*y.right;
        peak=std::max(peak,double(std::max(std::abs(y.left),std::abs(y.right))));
        if(type==2) require(y.left==x.left&&y.right==x.right,"steel changed");
    }
    require(energy>1.e-12,"silent measured observer");
    require(peak<=1.0001,"clip");
    if(type!=2) require(delta>1.e-6,"observer did not affect signal");
}
static void phaseDifference(){
    auto x=std::make_unique<pmg::StringEngine>();
    auto y=std::make_unique<pmg::StringEngine>();
    auto a=preset(3,false,true),b=a;a.directPhase15=0;b.directPhase15=1;
    x->prepare(48000,pmg::Model::nylon,a);y->prepare(48000,pmg::Model::nylon,b);
    require(x->noteOnString(76,.83f,5)&&y->noteOnString(76,.83f,5),"onset");
    double diff=0;
    for(int i=0;i<8000;++i){
        auto s=x->nextFrame(),t=y->nextFrame();
        diff+=std::abs(s.left-t.left)+std::abs(s.right-t.right);
        require(x->powerWaveStringJoules10()==y->powerWaveStringJoules10(),"delay modifies strings");
    }
    require(diff>1e-9,"relative direct-air path phase inert");
}
static void bypass(){
    for(int model:{1,2,3}){
        auto a=std::make_unique<pmg::StringEngine>(),b=std::make_unique<pmg::StringEngine>();
        auto old=preset(model,false,false),now=old;now.magnetic15=now.radiation15=true;
        old.physics12=now.physics12=false;
        a->prepare(48000,pmg::Model(model),old);b->prepare(48000,pmg::Model(model),now);
        a->noteOnString(64,.75f,5);b->noteOnString(64,.75f,5);
        for(int i=0;i<2048;++i){auto x=a->nextFrame(),y=b->nextFrame();require(x.left==y.left&&x.right==y.right,"legacy regression");}
    }
}
int main(){try{
    require(pmg::kMeasuredFIR15==96,"calibration coefficient width wrong");
    for(double sr:{44100.,48000.,96000.})for(int m:{1,2,3})run(sr,m);
    phaseDifference();bypass();
    std::puts("PMG1.5: complex FIR observer independence and direct-air phase all passed");
    return 0;
}catch(const std::exception& e){std::fprintf(stderr,"PMG15 FAIL: %s\n",e.what());return 1;}}
