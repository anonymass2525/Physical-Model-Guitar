#include "../DSP/StringEngine.h"
#include <cmath>
#include <cstdio>
#include <memory>
#include <stdexcept>
static void require(bool b,const char* msg) {if(!b)throw std::runtime_error(msg);}
static double render(pmg::Model m,bool physics,int sr,int stringNo,float vel)
{
    auto e=std::make_unique<pmg::StringEngine>();
    pmg::Controls c;
    c.physics06=physics;c.coupling=.55f;c.body=.45f;
    e->prepare(sr,m,c);
    static constexpr int tuning[]={40,45,50,55,59,64};
    require(e->noteOnString(tuning[stringNo]+3,vel,stringNo),"forced note");
    const int n=sr/4;
    double energy=0,checksum=0,peak=0;
    for(int i=0;i<n;++i)
    {
        if(i==n/2)e->noteOff(tuning[stringNo]+3);
        const auto f=e->nextFrame();
        require(std::isfinite(f.left)&&std::isfinite(f.right),"nonfinite");
        require(std::abs(f.left)<=1.0001f&&std::abs(f.right)<=1.0001f,"output over limit");
        const auto x=.5*(double(f.left)+double(f.right));
        energy+=x*x;checksum+=x*std::sin(.00135*i);peak=std::max(peak,std::abs(x));
    }
    require(energy>1.e-12 && peak<.999,"silence or saturation");
    return checksum;
}
int main()
{
    try {
        for(int sr:{44100,48000,96000})
            for(int m=1;m<=3;++m)
            {
                auto model=pmg::Model(m);
                const int stringNo=m%6;
                const double old=render(model,false,sr,stringNo,.75f);
                const double current=render(model,true,sr,stringNo,.75f);
                require(current==render(model,true,sr,stringNo,.75f),"nondeterministic");
                require(std::abs(current-old)>1.e-8,"new physics has no effect");
            }
        for(int m=1;m<=3;++m)
            for(float vel:{.12f,.50f,1.f})
                for(int stringNo=0;stringNo<6;++stringNo)
                    render(pmg::Model(m),true,48000,stringNo,vel);
        std::puts("physics06 bounded deterministic realtime test: PASS");
    }catch(const std::exception& e){std::fprintf(stderr,"FAIL: %s\n",e.what());return 1;}
}