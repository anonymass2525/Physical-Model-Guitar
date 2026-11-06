#include "../DSP/StringEngine.h"
#include "../DSP/CoherentRadiation17.h"
#include <cmath>
#include <cstdio>
#include <memory>
#include <stdexcept>
static void require(bool v,const char* message){if(!v)throw std::runtime_error(message);}
static pmg::Controls controls(int model,bool on,float phase=.5f,float mix=.5f){
    pmg::Controls c;c.physics10=c.physics11=c.physics12=true;
    c.radiation15=c.radiation16=(model==3);
    c.lateBody16=.55f;c.radiation17=on;
    c.directBodyPhase17=phase;c.directBodyMix17=mix;return c;
}
static void trial(double fs,int model){
    auto base=std::make_unique<pmg::StringEngine>();
    auto current=std::make_unique<pmg::StringEngine>();
    auto alt=std::make_unique<pmg::StringEngine>();
    base->prepare(fs,pmg::Model(model),controls(model,false));
    current->prepare(fs,pmg::Model(model),controls(model,true,.16f,1.f));
    alt->prepare(fs,pmg::Model(model),controls(model,true,.84f,1.f));
    require(base->noteOnString(64,.75f,5),"cannot play old");
    require(current->noteOnString(64,.75f,5),"cannot play current");
    require(alt->noteOnString(64,.75f,5),"cannot play alt");
    double diff=0.,phaseDiff=0.,output=0.;
    for(int i=0;i<int(.25*fs);++i){
        auto a=base->nextFrame(),b=current->nextFrame(),c=alt->nextFrame();
        require(std::isfinite(b.left)&&std::isfinite(b.right),"NaN readout");
        require(base->powerWaveStringJoules10()==current->powerWaveStringJoules10(),"string energy changed");
        require(base->unifiedBridgeEnergy12()==current->unifiedBridgeEnergy12(),"bridge energy changed");
        require(base->powerWaveResidual10()==current->powerWaveResidual10(),"energy ledger changed");
        require(current->powerWaveResidual10()==alt->powerWaveResidual10(),"delay changed energy ledger");
        diff+=std::abs(a.left-b.left)+std::abs(a.right-b.right);
        phaseDiff+=std::abs(c.left-b.left)+std::abs(c.right-b.right);
        output+=b.left*b.left+b.right*b.right;
        if(model!=3)require(a.left==b.left && a.right==b.right,"non-Nylon affected");
    }
    require(output>1e-10,"silent observer");
    if(model==3)require(diff>1e-6 && phaseDiff>1e-6,"coherent observer no impact");
}
int main(){try {
    for(double fs:{44100.,48000.,96000.,192000.})
        for(int m:{1,2,3})trial(fs,m);
    // bypass must equal old path exactly, including original legacy DSP.
    auto a=std::make_unique<pmg::StringEngine>();
    auto b=std::make_unique<pmg::StringEngine>();
    auto ca=controls(3,false),cb=controls(3,true);ca.physics12=cb.physics12=false;
    a->prepare(48000,pmg::Model::nylon,ca);b->prepare(48000,pmg::Model::nylon,cb);
    a->noteOnString(64,.65f,5);b->noteOnString(64,.65f,5);
    for(int i=0;i<3072;++i){auto x=a->nextFrame(),y=b->nextFrame();require(x.left==y.left&&x.right==y.right,"legacy output changed");}
    std::puts("PMG17 coherent source/bridge, delay-phase and no feedback PASS");
    return 0;
}catch(const std::exception& e){std::fprintf(stderr,"PMG17 FAIL %s\n",e.what());return 1;}}
