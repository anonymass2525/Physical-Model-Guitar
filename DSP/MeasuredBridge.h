#pragma once
#include <array>
namespace pmg {
// Collocated 2D passive mechanical mobility, ten damped mass-spring modes.
// Force[N] -> velocity[m/s] ONLY if string force and modal mass are SI-calibrated.
class MeasuredBridge {
public:
 static constexpr unsigned numberOfModes=10;
 struct ModeParams {double frequencyHz=100.,Q=8.,effectiveMass=1.,directionX=1.,directionY=0.,radiationL=1.,radiationR=1.;};
 struct Frame {float velocityX=0,velocityY=0,radiatedLeft=0,radiatedRight=0;};
 void prepare(double rate,const std::array<ModeParams,numberOfModes>& parameters) noexcept;
 Frame tick(double forceX,double forceY) noexcept;
 void reset() noexcept;
 double storedEnergy() const noexcept;
 double integratedInputWork() const noexcept{return totalWork;}
 double integratedDissipation() const noexcept{return totalDissipation;}
 bool sane() const noexcept;
 std::array<ModeParams,numberOfModes> parameters() const noexcept;
 static std::array<ModeParams,numberOfModes> fallbackGuitar(int model) noexcept;
private:
 struct Mode{ModeParams param{};double displacement=0.,velocity=0.,omega=0.,damping=0.;};
 std::array<Mode,numberOfModes> modes{};
 double dt=1./48000.,totalWork=0.,totalDissipation=0.;
};
} // namespace pmg
