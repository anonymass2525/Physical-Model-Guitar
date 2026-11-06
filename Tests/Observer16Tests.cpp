#include "../DSP/StringEngine.h"
#include "../DSP/LongRadiation16.h"
#include <array>
#include <cmath>
#include <cstdio>
#include <memory>
#include <stdexcept>
#include <algorithm>
static void require(bool v,const char* msg){if(!v)throw std::runtime_error(msg);}
static pmg::Controls config(int model,bool on){
    pmg::Controls c;c.physics10=c.physics11=c.physics12=true;
    c.radiation15=model==3; c.radiation16=on;
    c.lateBody16=.65f;
    return c;
}
static void impulse(double rate){
    pmg::LongRadiation16 late;late.prepare(rate);
    require(late.maximumPoleMagnitude() <1. && late.maximumPoleMagnitude()>.0001,"unstable modal poles");
    double initial=0.,lateEnergy=0.,twoSecond=0.;
    const int n=int(rate*2.0);
    for(int i=0;i<n;++i){
        auto f=late.tick(i==0?1.:0.,0.);
        require(std::isfinite(f.left)&&std::isfinite(f.right),"nonfinite impulse tail");
        if(i<96*rate/48000.-1)require(f.left==0.&&f.right==0.,"pre-response noncausal");
        if(i>int(.13*rate)&&i<int(.36*rate)) lateEnergy+=f.left*f.left+f.right*f.right;
        if(i>int(1.8*rate))twoSecond+=f.left*f.left+f.right*f.right;
        if(i>=int(.005*rate)&&i<int(.03*rate))initial+=f.left*f.left+f.right*f.right;
    }
    require(initial>1e-15 && lateEnergy>1e-16,"missing late or early body response");
    require(twoSecond < lateEnergy*.05,"unstable decay, tail does not dissipate");
}
static void models(double sr,int model){
    auto old=std::make_unique<pmg::StringEngine>();
    auto newer=std::make_unique<pmg::StringEngine>();
    auto baseline=config(model,false),modified=config(model,true);
    old->prepare(sr,pmg::Model(model),baseline);
    newer->prepare(sr,pmg::Model(model),modified);
    require(old->noteOnString(64,.70f,5)&&newer->noteOnString(64,.70f,5),"noteOn");
    double diff=0.,signal=0.;
    for(int i=0;i<int(sr*.35);++i){
        auto a=old->nextFrame(),b=newer->nextFrame();
        require(std::isfinite(b.left)&&std::isfinite(b.right),"nonfinite observation");
        require(old->powerWaveStringJoules10()==newer->powerWaveStringJoules10(),"readout alters string energy");
        require(old->unifiedBridgeEnergy12()==newer->unifiedBridgeEnergy12(),"readout alters bridge energy");
        require(old->powerWaveResidual10()==newer->powerWaveResidual10(),"readout alters ledger");
        diff+=std::abs(a.left-b.left)+std::abs(a.right-b.right);
        signal+=b.left*b.left+b.right*b.right;
        if(model!=3)require(a.left==b.left && a.right==b.right,"non-nylon affected");
    }
    require(signal>1e-12,"silent sound");
    if(model==3)require(diff>1e-6,"long observer inactive");
}
int main(){try{
    for(double sr:{44100.,48000.,96000.}){
        impulse(sr);
        for(int model:{1,2,3})models(sr,model);
    }
    auto a=std::make_unique<pmg::StringEngine>();
    auto b=std::make_unique<pmg::StringEngine>();
    auto ca=config(3,false),cb=config(3,true);ca.physics12=cb.physics12=false;
    a->prepare(48000,pmg::Model::nylon,ca);b->prepare(48000,pmg::Model::nylon,cb);
    a->noteOnString(64,.72f,5);b->noteOnString(64,.72f,5);
    for(int i=0;i<2048;++i){auto x=a->nextFrame(),y=b->nextFrame();require(x.left==y.left && x.right==y.right,"legacy regression");}
    std::puts("PMG1.6 long modal response, old-mode bypass and non-backreaction verified");
    return 0;
}catch(const std::exception& e){std::fprintf(stderr,"PMG16 FAIL %s\n",e.what());return 1;}}
