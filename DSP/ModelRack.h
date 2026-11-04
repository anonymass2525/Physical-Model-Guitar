#pragma once
#include "StringEngine.h"
#include <algorithm>
#include <array>
#include <cmath>
namespace pmg
{
class ModelRack
{
public:
    void prepare(double sr, Model m, const Controls& c) noexcept
    {
        rate=std::max(8000.,sr); active=0;remaining=0;
        fadeLength=std::max(1,int(rate*.040)); held.fill(0.f);
        engines[0].prepare(rate,m,c);engines[1].prepare(rate,m,c);
    }
    void request(Model m,const Controls& c) noexcept
    {
        for(auto& e:engines) e.setControls(c);
        if(remaining || m==engines[active].getModel()) return;
        const int next=1-active;
        engines[next].prepare(rate,m,c);
        for(int n=40;n<=88;++n)
            if(held[size_t(n)]>0.f) engines[next].noteOn(n,held[size_t(n)]);
        active=next;remaining=fadeLength;
    }
    void noteOn(int note,float velocity) noexcept
    {
        if(note<0 ||note>127)return;
        held[size_t(note)]=velocity;
        engines[active].noteOn(note,velocity);
        if(remaining) engines[1-active].noteOn(note,velocity);
    }
    void noteOff(int note) noexcept
    {
        if(note<0||note>127)return;
        held[size_t(note)]=0;
        engines[active].noteOff(note);
        if(remaining)engines[1-active].noteOff(note);
    }
    void pitchBend(float semitones) noexcept
    {
        for(auto& e:engines)e.setPitchBend(semitones);
    }
    void allNotesOff() noexcept
    {
        held.fill(0.f);for(auto& e:engines)e.allNotesOff();
    }
    StereoFrame nextFrame() noexcept
    {
        const auto now=engines[active].nextFrame();
        if(!remaining)return now;
        const auto old=engines[1-active].nextFrame();
        const float t=float(fadeLength-remaining)/float(fadeLength);
        const float blend=.5f-.5f*std::cos(t*3.14159265358979323846f);
        --remaining;
        return {old.left+(now.left-old.left)*blend,old.right+(now.right-old.right)*blend};
    }
    float nextSample() noexcept
    {
        const auto f=nextFrame();return .5f*(f.left+f.right);
    }
    Model applied()const noexcept {return engines[remaining?1-active:active].getModel();}
    bool transitioning()const noexcept {return remaining!=0;}
    int activeVoices()const noexcept {return engines[active].activeVoices();}
    int contactCount()const noexcept {return engines[active].contactCount();}
private:
    std::array<StringEngine,2> engines{};
    std::array<float,128> held{};
    double rate=48000;
    int active=0, remaining=0,fadeLength=1920;
};
} // namespace pmg
