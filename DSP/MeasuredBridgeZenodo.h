#pragma once
#include "MeasuredBridge.h"
namespace pmg {
// EXPERIMENTAL Zenodo 4604577, classical guitar #1, bass-side VERTICAL driving point.
// Not a 2-axis measurement; not perceptually validated; spectral fitting error ~53%.
inline std::array<MeasuredBridge::ModeParams,10> zenodoMeasuredBridgeCandidate() noexcept{
 auto p=MeasuredBridge::fallbackGuitar(3);
 constexpr double hz[]{117,243,357,414,537,561,936,1371,1437,1521};
 constexpr double q[]{6.5,11.571,1.725,19.714,9.421,26.714,39,41.545,43.545,39};
 constexpr double invmass[]{2.97941737,21.58886308,0.,1.51323736,6.13415308,1.41997829,6.34579046,6.05437677,6.58041037,6.65155428};
 for(unsigned i=0;i<10;++i){p[i].frequencyHz=hz[i];p[i].Q=q[i];p[i].effectiveMass=1./(invmass[i]>.0001?invmass[i]:.0001);p[i].directionX=1.;p[i].directionY=0.;}
 return p;
}
}
