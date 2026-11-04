#include "../DSP/ModelRack.h"
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <memory>
#include <new>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
std::atomic<int> allocations{0};
std::atomic<bool> watch{false};
void require(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
struct Metrics {double energy=0, tail=0, peak=0, body=0; int contacts=0;};
Metrics render(pmg::Model model,pmg::Controls c,int rate,int note=52,float vel=.8f,
               int samples=48000, bool release=false)
{
    auto engine=std::make_unique<pmg::StringEngine>();
    engine->prepare(rate,model,c);
    engine->noteOn(note,vel);
    Metrics metrics;
    for(int n=0;n<samples;++n) {
        if(release && n==samples/2)engine->noteOff(note);
        const auto s=engine->nextFrame();
        require(std::isfinite(s.left)&&std::isfinite(s.right),"non-finite sample");
        const double z=.5*(s.left+s.right);
        metrics.energy+=z*z;
        metrics.peak=std::max(metrics.peak,std::max(std::abs(double(s.left)),std::abs(double(s.right))));
        metrics.body+=std::abs(engine->bodyMotion());
        if(n>samples*3/4)metrics.tail+=z*z;
    }
    metrics.contacts=engine->contactCount();
    return metrics;
}
}
void* operator new(std::size_t size) { if(watch.load())++allocations;
    if(void* p=std::malloc(size?size:1))return p;throw std::bad_alloc();}
void* operator new[](std::size_t size) {if(watch.load())++allocations;
    if(void* p=std::malloc(size?size:1))return p;throw std::bad_alloc();}
void operator delete(void*p) noexcept {std::free(p);}
void operator delete[](void*p) noexcept {std::free(p);}
void operator delete(void*p,std::size_t) noexcept {std::free(p);}
void operator delete[](void*p,std::size_t) noexcept {std::free(p);}
int main(int argc,char**argv)
{
    try {
        const std::string test=argc>1?argv[1]:"basic";
        if(test=="basic") {
            for(int sr:{44100,48000,96000}) for(int m=0;m<4;++m) {
                auto e=std::make_unique<pmg::StringEngine>();pmg::Controls c;
                e->prepare(sr,pmg::Model(m),c);
                for(int k=0;k<100;++k) require(e->nextSample()==0,"initial silence");
                e->noteOn(52,.8f);
                require(e->activeVoices()==1,"note on");
                require(e->assignedString(52)>=0,"fret assignment");
                double energy=0, peak=0;
                for(int n=0;n<sr;++n) {
                    const auto f=e->nextFrame();
                    require(std::isfinite(f.left)&&std::isfinite(f.right),"finite");
                    energy+=f.left*f.left+f.right*f.right;
                    peak=std::max(peak,std::max(std::abs(double(f.left)),std::abs(double(f.right))));
                }
                require(energy>1.e-9,"audible synthesis");
                require(peak<1.00001,"bounded peak");
                e->noteOff(52);
                for(int n=0;n<sr*2;++n)e->nextSample();
                require(e->activeVoices()==0,"release settles");
                std::cout<<"model="<<m<<" sr="<<sr<<" energy="<<energy<<" peak="<<peak<<"\n";
            }
            auto g=std::make_unique<pmg::StringEngine>();pmg::Controls c;
            g->prepare(48000,pmg::Model::steel,c);
            for(int n:{40,45,50,55,59,64})g->noteOn(n,1.f);
            require(g->activeVoices()<=6,"max six strings");
            g->noteOn(100,.8f);require(g->activeVoices()<=6,"unplayable range ignored");
        }
        else if(test=="physics") {
            pmg::Controls a,b;
            for(int m=0;m<4;++m) {
                auto standard=render(pmg::Model(m),a,48000);
                require(standard.energy>1.e-9,"nonzero baseline");
                b=a;b.pickAngle=1;b.hardness=1;
                const auto pick=render(pmg::Model(m),b,48000);
                require(std::abs(pick.energy-standard.energy)>1.e-11,"pick contact changes sound");
                b=a;b.material=1.f;
                const auto material=render(pmg::Model(m),b,48000);
                require(std::abs(material.energy-standard.energy)>1.e-11,"polarization/material alters decay");
                b=a;b.body=.8f;
                const auto body=render(pmg::Model(m),b,48000);
                require(std::abs(body.energy-standard.energy)>1.e-11,"modal radiation changes sound");
                b=a;b.bridgeMobility=1.f;b.coupling=1.f;
                const auto bridge=render(pmg::Model(m),b,48000);
                require(bridge.body>0,"feedback from bridge active");
                b=a;b.fretClearance=0.f;b.touch=0.f;
                const auto fret=render(pmg::Model(m),b,48000);
                require(fret.contacts>0,"nonlinear fret collision active");
                b=a;b.fretClearance=1.f;
                const auto high=render(pmg::Model(m),b,48000);
                require(fret.contacts>=high.contacts,"clearance affects collisions");
                if(m==1) {
                    b=a;b.pickupAperture=0.f;b.character=0;
                    const auto pickup=render(pmg::Model(m),b,48000);
                    require(std::abs(pickup.energy-standard.energy)>1.e-11,"spatial pickup effect");
                }
                std::cout<<"model="<<m<<" base="<<standard.energy<<" pick="<<pick.energy
                         <<" contact="<<fret.contacts<<" vs "<<high.contacts<<" bridge="<<bridge.body<<"\n";
            }
        }
        else if(test=="interaction") {
            pmg::Controls c;
            auto a=std::make_unique<pmg::StringEngine>();
            auto b=std::make_unique<pmg::StringEngine>();
            a->prepare(48000,pmg::Model::steel,c);b->prepare(48000,pmg::Model::steel,c);
            a->noteOn(52,.8f);b->noteOn(52,.8f);
            double without=0, with=0;
            for(int i=0;i<36000;++i){ a->nextFrame(); b->nextFrame(); }
            a->noteOn(64,.8f);
            for(int i=0;i<20000;++i){without+=std::abs(b->nextSample());with+=std::abs(a->nextSample());}
            require(without>0 && with>0,"string excitation remains");
            // Drive an unplayed string through the shared mechanical bridge.
            auto coupled=std::make_unique<pmg::StringEngine>();
            auto disconnected=std::make_unique<pmg::StringEngine>();
            pmg::Controls coupledControls, isolatedControls;
            coupledControls.coupling=1.f;coupledControls.bridgeMobility=1.f;
            isolatedControls.coupling=0.f;
            coupled->prepare(48000,pmg::Model::steel,coupledControls);
            disconnected->prepare(48000,pmg::Model::steel,isolatedControls);
            coupled->noteOn(52,.9f);disconnected->noteOn(52,.9f);
            for(int i=0;i<48000;++i){coupled->nextFrame();disconnected->nextFrame();}
            require(coupled->assignedString(52)==2,"physical string placement");
            require(disconnected->stringEnergy(0)==0.f,"decoupled string is silent");
            require(coupled->stringEnergy(0)>1.e-10f,"sympathetic unplayed string rings");
            // A pitch wheel changes the traveling-wave delay, not a post-pitch-shifter.
            a->setPitchBend(2.f);
            double energy=0;
            for(int i=0;i<24000;++i){float v=a->nextSample();require(std::isfinite(v),"bend stable");energy+=v*v;}
            require(energy>1.e-10,"bent note still audible");
            auto rack=std::make_unique<pmg::ModelRack>();
            rack->prepare(48000,pmg::Model::steel,c);
            rack->noteOn(52,.8f);
            for(int i=0;i<5000;++i)rack->nextFrame();
            rack->request(pmg::Model::electric,c);
            for(int i=0;i<2400;++i)rack->nextFrame();
            require(rack->applied()==pmg::Model::electric && !rack->transitioning(),"model crossfade");
        }
        else if(test=="stability") {
            for(int m=0;m<4;++m){
                pmg::Controls c;c.output=6;c.body=.8f;c.coupling=1.f;c.bridgeMobility=1.f;
                c.decay=12;c.fretClearance=0;c.brightness=1;c.material=1;
                auto e=std::make_unique<pmg::StringEngine>();e->prepare(96000,pmg::Model(m),c);
                for(int n:{40,45,50,55,59,64})e->noteOn(n,1.f);
                for(int i=0;i<96000*3;++i){
                    if(i==96000)e->setPitchBend(-2);
                    if(i==96000*2)e->allNotesOff();
                    const auto s=e->nextFrame();
                    require(std::isfinite(s.left)&&std::isfinite(s.right),"dense-note finite");
                    require(std::abs(s.left)<=1.00001f && std::abs(s.right)<=1.00001f,"no rail overflow");
                }
                std::cout<<"stable model="<<m<<"\n";
            }
        }
        else if(test=="realtime") {
            pmg::Controls c;
            auto rack=std::make_unique<pmg::ModelRack>();rack->prepare(48000,pmg::Model::steel,c);
            allocations=0;watch=true;
            double e=0;
            const auto start=std::chrono::steady_clock::now();
            for(int b=0;b<1500;++b) {
                if(b%160==0){rack->noteOn(52,.65f);rack->noteOn(64,.7f);}
                if(b%160==125)rack->allNotesOff();
                if(b==400)rack->request(pmg::Model::nylon,c);
                if(b==800)rack->request(pmg::Model::electric,c);
                if(b==1200)rack->pitchBend(1.2f);
                for(int n=0;n<256;++n) e+=rack->nextSample();
            }
            watch=false;
            const double elapsed=std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count();
            const double ratio=elapsed/(1500.*256/48000.);
            require(allocations==0,"no allocations in audio processing");
            require(std::isfinite(e),"checksum finite");
            std::cout<<"elapsed="<<elapsed<<" ratio="<<ratio<<" allocations="<<allocations<<"\n";
        }
        else throw std::runtime_error("unknown case");
        std::cout<<"PASS "<<test<<"\n";
        return 0;
    } catch(const std::exception&e) {watch=false;std::cerr<<"FAIL "<<e.what()<<"\n";return 1;}
}
