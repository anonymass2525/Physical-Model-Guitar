#include "../DSP/StringEngine.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <memory>
#include <stdexcept>
namespace {
void require(bool value,const char* reason){if(!value)throw std::runtime_error(reason);}
pmg::Controls base() {
    pmg::Controls c;
    c.physics06=c.physics07=c.physics08=c.physics09=c.physics10=c.physics11=c.physics12=true;
    c.body=.45f;
    return c;
}
double energy(const pmg::StereoFrame& f) {
    return double(f.left)*f.left+double(f.right)*f.right;
}
std::unique_ptr<pmg::StringEngine> instrument(pmg::Model m,const pmg::Controls& c) {
    auto e=std::make_unique<pmg::StringEngine>();
    e->prepare(48000,m,c);
    require(e->noteOnString(55,.8f,3),"noteOnString failed");
    return e;
}
void defaultPathUnchanged() {
    for(int m=0;m<4;++m) {
        auto a=base(), b=base();
        b.diagnosticDirect171=b.diagnosticBody171=b.diagnosticBridge171=false;
        // Diagnostic master OFF: no bridge or readout change whatsoever.
        auto x=instrument(pmg::Model(m),a), y=instrument(pmg::Model(m),b);
        for(int n=0;n<5000;++n) {
            const auto p=x->nextFrame(), q=y->nextFrame();
            require(p.left==q.left && p.right==q.right,"OFF changed original v1.7 audio");
        }
    }
}
void readoutIsolation() {
    for(int m: {0,2,3}) {
        auto c=base();c.diagnostic171=true;
        c.diagnosticDirect171=false;c.diagnosticBody171=false;
        auto silent=instrument(pmg::Model(m),c);
        for(int i=0;i<5000;++i){
            const auto f=silent->nextFrame();
            require(f.left==0.f && f.right==0.f,"muted acoustic taps still audible");
        }
        c.diagnosticDirect171=true;
        auto direct=instrument(pmg::Model(m),c);
        double directEnergy=0.;
        for(int i=0;i<5000;++i)directEnergy+=energy(direct->nextFrame());
        require(directEnergy>1e-15,"string-only tap silent");

        c.diagnosticDirect171=false;c.diagnosticBody171=true;
        auto body=instrument(pmg::Model(m),c);
        double bodyEnergy=0.;
        for(int i=0;i<5000;++i)bodyEnergy+=energy(body->nextFrame());
        require(bodyEnergy>1e-15,"body-only tap silent");

        c.diagnosticBridge171=false;
        auto rigid=instrument(pmg::Model(m),c);
        for(int i=0;i<5000;++i) {
            const auto f=rigid->nextFrame();
            require(f.left==0.f&&f.right==0.f,"rigid bridge radiated sound");
            const double e=rigid->unifiedEnergyInput12();
            require(std::abs(rigid->powerWaveResidual10())<3e-8*std::max(1e-6,e),
                    "rigid bridge energy residual");
        }
    }
}
void toggleDoesNotCreateEnergy() {
    auto c=base();c.diagnostic171=true;
    auto e=instrument(pmg::Model::nylon,c);
    for(int i=0;i<12000;++i) {
        if(i==2500) { c.diagnosticBridge171=false; e->setControls(c); }
        if(i==7000) { c.diagnosticBridge171=true; e->setControls(c); }
        const auto f=e->nextFrame();
        require(std::isfinite(f.left)&&std::isfinite(f.right),"nonfinite after toggle");
        const double injected=e->unifiedEnergyInput12();
        require(std::abs(e->powerWaveResidual10())<3e-8*std::max(1e-6,injected),
                "energy residual after bridge toggle");
    }
}
void electricNeverSilent() {
    auto c=base();
    for(bool diagnostic:{false,true}) {
        c.diagnostic171=diagnostic;
        auto e=instrument(pmg::Model::electric,c);
        double power=0.;
        for(int i=0;i<10000;++i)power+=energy(e->nextFrame());
        require(power>1e-9,"Electric Unified pickup path silent");
    }
}
} // namespace
int main() {
    try {
        defaultPathUnchanged();
        readoutIsolation();
        toggleDoesNotCreateEnergy();
        electricNeverSilent();
        std::puts("PMG 1.7.1 diagnostic: default preserved; readouts and bridge isolated; Electric nonzero");
        return 0;
    } catch(const std::exception& e) {
        std::fprintf(stderr,"PMG171 FAIL: %s\n",e.what());
        return 1;
    }
}
