#include "../DSP/PowerWaveStrings.h"
#include "../DSP/StringEngine.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <memory>
#include <stdexcept>
namespace {
void require(bool good,const char* what){if(!good)throw std::runtime_error(what);}
void waveTest(int rate,int model){
    pmg::PowerWaveStrings waves;
    waves.prepare(rate,model);
    const std::array<int,6> tuning{{40,45,50,55,59,64}};
    for(int i=0;i<6;++i){
        require(waves.stringImpedance(i)>0.,"nonpositive characteristic impedance");
        require(std::abs(waves.stringLength(i)-.65)<1e-12,"invalid open string length");
        require(waves.strike(i,tuning[size_t(i)],.6+.05*i,.15+.03*i),"strike failed");
    }
    const double initial=waves.injectedEnergy();
    require(initial>1e-10,"missing initial excitation energy");
    double maxResidual=0.,maxBridge=0.,maxString=0.;
    const int samples=rate/3;
    for(int t=0;t<samples;++t){
        if(t==samples/3){
            waves.release(1);
            require(waves.strike(0,43,.7,.21),"re-fretted note failed");
        }
        auto sample=waves.tick();
        require(std::isfinite(sample.left)&&std::isfinite(sample.right),"invalid radiation");
        require(waves.sane(),"non-finite wave system");
        maxBridge=std::max(maxBridge,std::abs(sample.bridgeVelocity));
        maxString=std::max(maxString,std::abs(sample.stringVelocity));
        maxResidual=std::max(maxResidual,std::abs(waves.energyResidual()));
        const double e=waves.injectedEnergy();
        require(waves.storedStringEnergy()+waves.storedBridgeEnergy()<e+1e-8,"passive system energy growth");
    }
    const double scale=std::max(1e-14,waves.injectedEnergy());
    require(maxResidual<3e-9*scale,"wave/bridge/propagation energy ledger error");
    require(waves.propagationLoss()>=0.&&waves.bridgeDissipation()>=0.,"negative energy dissipation");
    require(maxBridge>1e-12&&maxString>1e-6,"no response from coupled string/bridge");
    std::printf("PMG1.0 sr=%d model=%d E_in=%.7g E_string=%.7g E_bridge=%.7g L=%.7g D=%.7g residual=%.3g bridge=%g\n",
      rate,model,waves.injectedEnergy(),waves.storedStringEnergy(),waves.storedBridgeEnergy(),
      waves.propagationLoss(),waves.bridgeDissipation(),maxResidual,maxBridge);
}
void guitarTest(int rate,int model){
    auto guitar=std::make_unique<pmg::StringEngine>();
    pmg::Controls c;
    c.physics06=c.physics07=c.physics08=c.physics09=true;
    c.physics10=true;
    guitar->prepare(rate,pmg::Model(model),c);
    require(guitar->noteOnString(model==3?64:45,.8f,model==3?5:1),"failed note allocation");
    double rms=0.,peak=0.;
    for(int t=0;t<rate/5;++t){
        const auto f=guitar->nextFrame();
        require(std::isfinite(f.left)&&std::isfinite(f.right),"nonfinite guitar output");
        rms+=double(f.left)*f.left+double(f.right)*f.right;
        peak=std::max(peak,std::max(std::abs(double(f.left)),std::abs(double(f.right))));
        require(std::abs(guitar->powerWaveResidual10())<1e-8,"wave ledger drift while playing guitar");
    }
    require(rms>1e-7&&peak<.999,"silent or clipped output");
    std::printf("guitar %d %d rms=%g peak=%g\n",rate,model,rms,peak);
}
}
int main(){try{
    for(int sr:{44100,48000,96000})for(int m:{1,2,3})waveTest(sr,m);
    for(int m:{1,2,3})guitarTest(48000,m);
    std::puts("PMG v1.0 travelling-wave and bridge energy tests passed");
}catch(const std::exception&e){std::fprintf(stderr,"PMG10 FAILURE: %s\n",e.what());return 1;}}
