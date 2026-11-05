#include "../DSP/StringEngine.h"
#include <array>
#include <cmath>
#include <cstdio>
#include <memory>
#include <stdexcept>

static void check(bool ok, const char* what) { if (!ok) throw std::runtime_error(what); }
struct Metrics { double energy=0., centroid=0., hash=0., attack=0.; int contacts=0; float peak=0.; };
static Metrics render(int sr, pmg::Model model, bool physics07, int stringNo, int fret,
                      float velocity, bool repeated=false, float clearance=.65f)
{
    auto engine=std::make_unique<pmg::StringEngine>();
    pmg::Controls c;
    c.physics06=true; c.physics07=physics07;
    c.fretClearance=clearance;
    c.body=.4f; c.coupling=.58f;
    engine->prepare(sr,model,c);
    constexpr std::array<int,6> tuning{{40,45,50,55,59,64}};
    const int midi=tuning[size_t(stringNo)]+fret;
    check(engine->noteOnString(midi,velocity,stringNo),"unplayable test note");
    const int frames=sr/3;
    Metrics out;
    for(int i=0;i<frames;++i)
    {
        if(repeated && i==sr/9)
            check(engine->noteOnString(midi,velocity,stringNo),"reattack failed");
        const auto f=engine->nextFrame();
        check(std::isfinite(f.left)&&std::isfinite(f.right),"nonfinite");
        check(std::abs(f.left)<1.00001f && std::abs(f.right)<1.00001f,"peak limit violation");
        const double s=.5*(double(f.left)+double(f.right));
        out.energy+=s*s;
        out.hash+=s*std::sin(.00013*i);
        out.centroid+=std::abs(s)*(double(i)/frames);
        if(i<sr/30)out.attack+=s*s;
        out.peak=std::max(out.peak,float(std::abs(s)));
    }
    out.contacts=engine->contactCount();
    return out;
}
int main()
{
    try
    {
        int impactful=0;
        for (int sr:{44100,48000,96000})
            for(int m=1;m<=3;++m)
                for(int s:{0,2,5})
                {
                    const auto old=render(sr,pmg::Model(m),false,s,3,.8f);
                    const auto a=render(sr,pmg::Model(m),true,s,3,.8f);
                    const auto b=render(sr,pmg::Model(m),true,s,3,.8f);
                    check(a.hash==b.hash && a.energy==b.energy,"non-deterministic");
                    check(a.energy>1.e-11,"silent attack");
                    check(a.peak<.9999,"limiter stuck");
                    if(std::abs(a.energy-old.energy)>1.e-10 || a.contacts!=old.contacts) ++impactful;
                    if(sr==48000)std::printf("model=%d string=%d old_contacts=%d new_contacts=%d oldEnergy=%g newEnergy=%g\n",m,s,old.contacts,a.contacts,old.energy,a.energy);
                }
        check(impactful>=3,"new contact solver never affects sound");
        const auto play=render(48000,pmg::Model::steel,true,0,3,1.f,true,.12f);
        check(play.contacts>0,"no fret contact under close action");
        const auto old=render(48000,pmg::Model::steel,false,0,3,1.f,true,.12f);
        check(std::abs(play.hash-old.hash)>1.e-8,"reattack energy memory ineffective");
        for(float velocity:{.15f,.50f,1.f})
            for(int m=1;m<=3;++m)
            {
                const auto high=render(48000,pmg::Model(m),true,5,9,velocity,false,.95f);
                check(high.energy>0.,"silence under extreme conditions");
            }
        std::puts("Physics 0.7 tests OK");
    }
    catch(const std::exception& e)
    {
        std::fprintf(stderr,"Physics 0.7 test failed: %s\n",e.what());
        return 1;
    }
}
