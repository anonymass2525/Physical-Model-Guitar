#include "../DSP/ModelRack.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <vector>
namespace {
void le16(std::ofstream& f, std::uint16_t v) {char b[2]={char(v),char(v>>8)};f.write(b,2);}
void le32(std::ofstream& f, std::uint32_t v) {for(int i=0;i<4;++i){char b=char(v>>(8*i));f.write(&b,1);}}
void save(const std::string& path,const std::vector<float>& stereo,int sr)
{
    std::ofstream f(path,std::ios::binary);
    const auto size=std::uint32_t(stereo.size()*2);
    f.write("RIFF",4);le32(f,36+size);f.write("WAVEfmt ",8);
    le32(f,16);le16(f,1);le16(f,2);le32(f,std::uint32_t(sr));le32(f,std::uint32_t(sr*4));
    le16(f,4);le16(f,16);f.write("data",4);le32(f,size);
    for(float v:stereo){float value=std::max(-1.f,std::min(1.f,v));const auto s=std::int16_t(std::lround(value*32767.f));le16(f,std::uint16_t(s));}
}
}
int main(int argc,char**argv)
{
    const std::string base=argc>1?argv[1]:"PMG03";
    constexpr int sr=48000;
    const int notes[]={52,55,59,64,57,52,50,55};
    for(int model=0;model<4;++model)
    {
        auto e=std::make_unique<pmg::ModelRack>();pmg::Controls c;
        if(model==1){c.output=-2;c.body=.13f;c.pickup=.23f;c.pickupAperture=.35f;}
        if(model==2){c.body=.6f;c.coupling=.65f;c.bridgeMobility=.6f;c.hardness=.7f;}
        if(model==3){c.body=.55f;c.hardness=.3f;c.pickAngle=.7f;c.touch=.65f;}
        e->prepare(sr,pmg::Model(model),c);
        std::vector<float> audio;audio.reserve(size_t(sr*9*2));
        for(int i=0;i<sr*9;++i)
        {
            for(int n=0;n<8;++n)
            {
                const int begin=int((.24 + .95*n)*sr);
                if(i==begin)e->noteOn(notes[n],.72f + .025f*(n%3));
                if(i==begin+int(sr*.70))e->noteOff(notes[n]);
            }
            const auto frame=e->nextFrame();
            audio.push_back(frame.left);audio.push_back(frame.right);
        }
        const std::string file=base+"_"+std::to_string(model)+"_"+
             pmg::modelNames[model]+".wav";
        save(file,audio,sr);
        std::cout<<file<<" (9 sec 48kHz PCM16 stereo)\n";
    }
}
