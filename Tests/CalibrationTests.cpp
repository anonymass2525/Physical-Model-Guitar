#include "../DSP/StringEngine.h"
#include <cmath>
#include <iostream>
#include <iomanip>
#include <memory>
#include <stdexcept>
#include <string>

static void require(bool v,const char* what){if(!v) throw std::runtime_error(what);}
static double run(int stringNo,int model,const pmg::Controls& c,int len=22000){
    auto e=std::make_unique<pmg::StringEngine>();
    e->prepare(48000.0,pmg::Model(model),c);
    const int tunings[]={40,45,50,55,59,64};
    require(e->noteOnString(tunings[stringNo]+3,.74f,stringNo),"force assigned note");
    double energy=0, checksum=0;
    for(int i=0;i<len;++i){const auto v=e->nextFrame();
        require(std::isfinite(v.left)&&std::isfinite(v.right),"finite audio");
        require(std::abs(v.left)<=1.00001f&&std::abs(v.right)<=1.00001f,"bounded audio");
        energy+=double(v.left)*v.left+double(v.right)*v.right;
        checksum+=double(v.left)*std::cos(.002*i);
    }
    require(energy>1e-10,"sustained energy");
    return checksum;
}
int main(){
try{
    pmg::Controls neutral;
    neutral.body=.45f;
    // Keep bridge coupling for physically faithful body-mode sensitivity.
    // Disable it only for the independent-pickup isolation check.
    neutral.calibration.contactStiffness[0]=1.f;
    // Changing only this string's contact is observable at fixed fret, velocity.
    const auto baseline=run(0,2,neutral);
    auto variant=neutral;variant.calibration.contactStiffness[0]=1.6f;
    require(std::abs(run(0,2,variant)-baseline)>1e-6,"individual contact does nothing");
    auto other=neutral;other.calibration.contactStiffness[1]=1.6f;
    require(std::abs(run(0,2,other)-baseline)<1e-7,"contact leaks to other string");
    variant=neutral;variant.calibration.spectralLoss[0]={0.f,.01f,.04f,.10f};
    require(std::abs(run(0,2,variant)-baseline)>1e-6,"partial-band damping does nothing");
    variant=neutral;variant.calibration.modeFrequency[0]=1.15f;
    require(std::abs(run(0,2,variant)-baseline)>1e-10,"body mode frequency does nothing");
    variant=neutral;variant.calibration.modeQ[1]=1.7f;
    std::cout<<std::setprecision(16)<<"baseline="<<baseline<<" q="<<run(0,2,variant)<<"\n";
    require(std::abs(run(0,2,variant)-baseline)>1e-10,"body mode q does nothing");
    variant=neutral;variant.calibration.modeGain[2]=1.7f;
    require(std::abs(run(0,2,variant)-baseline)>1e-10,"body mode strength does nothing");
    const auto elec=run(0,1,neutral);
    variant=neutral;variant.calibration.pickupOffset[0]=.035f;
    require(std::abs(run(0,1,variant)-elec)>1e-6,"per-string pickup does nothing");
    other=neutral;other.coupling=0.f;other.calibration.pickupOffset[1]=.035f;
    auto coupledOff=neutral;coupledOff.coupling=0.f;
    require(std::abs(run(0,1,other)-run(0,1,coupledOff))<1e-7,"pickup leaks without coupling");
    // Impossible string/fret combinations must be rejected, not silently remapped.
    auto engine=std::make_unique<pmg::StringEngine>();engine->prepare(48000.,pmg::Model::steel,neutral);
    require(!engine->noteOnString(40,.8f,5),"low E erroneously assignable to high E");
    require(engine->noteOnString(52,.8f,0),"E3 should be playable on low E fret 12");
    require(engine->assignedString(52)==0,"string assignment ignored");
    for(int m=1;m<=3;++m)for(int s=0;s<6;++s){
        variant=neutral;
        variant.calibration.contactStiffness[size_t(s)]=2.5f;
        variant.calibration.spectralLoss[size_t(s)]={.03f,.12f,.22f,.34f};
        variant.calibration.modeFrequency[1]=1.4f;
        variant.calibration.modeQ[1]=2.5f;
        variant.calibration.modeGain[1]=2.f;
        run(s,m,variant,6000);
    }
    std::cout<<"PASS: physically assigned 6 strings, 4 calibrated domains, passivity bounds, per-string isolation\n";
    return 0;
}catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<"\n";return 1;}
}
