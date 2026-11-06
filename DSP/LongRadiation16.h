#pragma once
#include "LongRadiation16Coeffs.h"
#include <array>
#include <cstddef>
namespace pmg {
// Two bridge ports -> two nearfield observation microphones. The stable
// complex-pole bank synthesizes late soundboard tails with measured/estimated
// relative phase. Early phase-correct 96-tap response stays in Observer15.
// This observer only reads bridge velocities; never feeds force into strings.
class LongRadiation16 {
public:
    struct Stereo { double left=0.,right=0.; };
    void prepare(double sampleRate) noexcept;
    void reset() noexcept;
    Stereo tick(double bridgeBass,double bridgeTreble) noexcept;
    double maximumPoleMagnitude() const noexcept;
    static constexpr int earlySamples48k=96;
private:
    struct State {double re=0.,im=0.;};
    std::array<std::array<State,4>,kLongModeCount16> states{};
    std::array<double,kLongModeCount16> rotRe{},rotIm{};
    std::array<std::array<double,256>,2> delayed{};
    int cursor=0,delaySamples=96;
};
}
