#pragma once
#include "StringEngine.h"

namespace pmg
{
// Real-guitar calibration candidates from 89 separately recorded notes.
// Experiment: 18 electric, 17 steel, 18 nylon training notes;
// 12 held-out notes per model (fret 5/9, alternate dynamic/technique where available).
// Six string-model pairs passed limited held-out feature gates. These are NOT
// physical N/m measurements and are NOT claimed perceptually superior.
// Reference metric: GitHub Actions 41929723962, artifact 13370614260.
// Neutral parameter values leave v0.4 sound bit-for-bit unchanged.
inline void applyHeldoutCandidates(Model model, PhysicalCalibration& p) noexcept
{
    switch (model)
    {
    case Model::electric:
        p.contactStiffness[5] = 1.36f; // High E string, reference held-out gate pass
        p.pickupOffset[5] = -.035f;
        break;
    case Model::steel:
        for (int s : {1, 3, 4, 5}) // A, G, B, High E
            p.contactStiffness[static_cast<std::size_t>(s)] = 1.36f;
        break;
    case Model::nylon:
        p.contactStiffness[3] = 1.16f; // G string
        p.spectralLoss[3] = {0.f, .0019f, .0053f, .010f};
        break;
    default:
        break;
    }
}
} // namespace pmg
