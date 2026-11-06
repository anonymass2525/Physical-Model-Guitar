#include "../DSP/StringEngine.h"
#include <cmath>
#include <memory>
#include <stdexcept>
#include <cstdio>
static void need(bool b,const char* reason){if(!b)throw std::runtime_error(reason);}
static pmg::Controls cfg(bool unified=true){
    pmg::Controls c;
    c.physics06=c.physics07=c.physics08=c.physics09=c.physics10=c.physics11=true;
    c.physics12=unified;c.body=.45f;
    return c;
}
static void noParallelDryRenderer(){
    auto a=std::make_unique<pmg::StringEngine>();
    auto b=std::make_unique<pmg::StringEngine>();
    auto ca=cfg(),cb=cfg();
    // Legacy DSP control flags cannot influence audible unified synthesis.
    cb.physics06=cb.physics07=cb.physics08=cb.physics09=false;
    for(int m=1;m<=3;++m){
        a->prepare(48000,pmg::Model(m),ca);
        b->prepare(48000,pmg::Model(m),cb);
        need(a->noteOnString(55,.78f,3)&&b->noteOnString(55,.78f,3),"note onset");
        double power=0.;
        for(int i=0;i<2500;++i){
            const auto x=a->nextFrame(),y=b->nextFrame();
            need(x.left==y.left && x.right==y.right,"old DSP contaminates unified output");
            power+=double(x.left)*x.left+double(x.right)*x.right;
        }
        need(power>1e-9,"unified output silent");
        need(std::abs(a->powerWaveResidual10())<1e-9,"unified energy ledger discrepancy");
        need(a->unifiedEnergyInput12()>=a->powerWaveStringJoules10()+a->unifiedBridgeEnergy12(),
            "energy growth without input");
    }
}
static void ablationsArePassive(){
    for(int m=1;m<=3;++m){
        auto a=std::make_unique<pmg::StringEngine>();
        auto c=cfg();
        a->prepare(48000,pmg::Model(m),c);
        need(a->noteOnString(55,.8f,3),"strike");
        for(int i=0;i<8000;++i){
            if(i==1000){c.axial12=false;a->setControls(c);}
            if(i==2000){c.pick12=false;a->setControls(c);}
            if(i==3000){c.fret12=false;a->setControls(c);}
            if(i==4000){c.pickup12=false;a->setControls(c);}
            const auto f=a->nextFrame();
            need(std::isfinite(f.left)&&std::isfinite(f.right),"nonfinite audio");
            const double e=a->unifiedEnergyInput12();
            need(std::abs(a->powerWaveResidual10())<2e-8*std::max(1e-6,e),"lost energy on module toggle");
            need(a->unifiedAllDissipations12()>=-1e-10,"negative dissipation");
        }
    }
}
static void sensorHasNoFeedback(){
    auto a=std::make_unique<pmg::StringEngine>(),b=std::make_unique<pmg::StringEngine>();
    auto ca=cfg(),cb=cfg();ca.pickup=.1f;cb.pickup=.3f;
    a->prepare(48000,pmg::Model::electric,ca);b->prepare(48000,pmg::Model::electric,cb);
    a->noteOnString(64,.8f,5);b->noteOnString(64,.8f,5);
    double audioDifference=0.;
    for(int i=0;i<4800;++i){
        const auto x=a->nextFrame(),y=b->nextFrame();
        audioDifference+=std::abs(double(x.left)-y.left);
        need(a->powerWaveStringJoules10()==b->powerWaveStringJoules10(),"sensor loaded the strings");
        need(a->unifiedBridgeEnergy12()==b->unifiedBridgeEnergy12(),"sensor loaded the bridge");
    }
    need(audioDifference>1e-5,"electric pickup relocation made no sound difference");
}
int main(){try{noParallelDryRenderer();ablationsArePassive();sensorHasNoFeedback();
    std::puts("PMG 1.2 integrated model: legacy output isolated, ablation passive, pickup no feedback");
    return 0;}catch(const std::exception&e){std::fprintf(stderr,"PMG12 FAIL %s\n",e.what());return 1;}}
