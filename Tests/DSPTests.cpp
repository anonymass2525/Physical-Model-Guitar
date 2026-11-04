#include "ModelRack.h"
#include <atomic>
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <new>
#include <stdexcept>
#include <string>
#include <vector>
static thread_local bool watch = false;
static std::atomic<int> allocations{0};
void *operator new(std::size_t n)
{
    if (watch)
        ++allocations;
    if (auto *p = std::malloc(n ? n : 1))
        return p;
    throw std::bad_alloc();
}
void *operator new[](std::size_t n)
{
    return ::operator new(n);
}
void operator delete(void *p) noexcept
{
    std::free(p);
}
void operator delete[](void *p) noexcept
{
    std::free(p);
}
void operator delete(void *p, std::size_t) noexcept
{
    std::free(p);
}
void operator delete[](void *p, std::size_t) noexcept
{
    std::free(p);
}
void require(bool ok, const char *s)
{
    if (!ok)
        throw std::runtime_error(s);
}
int main(int argc, char **argv)
{
    try
    {
        const std::string test = argc > 1 ? argv[1] : "models";
        if (test == "models")
        {
            std::array<std::vector<float>, 4> waves;
            for (int model = 0; model < 4; ++model)
                for (int sr : {44100, 48000, 96000})
                {
                    auto e = std::make_unique<pmg::StringEngine>();
                    pmg::Controls c;
                    e->prepare(sr, pmg::Model(model), c);
                    for (int i = 0; i < 300; ++i)
                        require(e->nextSample() == 0, "Initial silence");
                    e->noteOn(52, 100.f / 127.f);
                    double sum = 0;
                    float peak = 0;
                    for (int i = 0; i < sr; ++i)
                    {
                        float s = e->nextSample();
                        require(std::isfinite(s), "Finite audio");
                        sum += s * s;
                        peak = std::max(peak, std::abs(s));
                        if (sr == 48000)
                            waves[size_t(model)].push_back(s);
                    }
                    require(sum > 1e-8 && peak < 1, "Model sounds without clipping");
                    e->noteOff(52);
                    double tail = 0;
                    for (int i = 0; i < sr * 2; ++i)
                    {
                        const float s = e->nextSample();
                        if (i > sr)
                            tail += s * s;
                    }
                    require(tail < 1e-6, "Note-off decay");
                    c.decay = 12;
                    c.brightness = 1;
                    c.body = .8f;
                    c.output = 6;
                    c.material = 1;
                    e->setControls(c);
                    for (int n = 28; n < 60; ++n)
                        e->noteOn(n, 1);
                    require(e->activeVoices() == 16, "Bounded polyphony");
                    for (int i = 0; i < sr; ++i)
                        require(std::isfinite(e->nextSample()), "Dense chord safety");
                    std::cout << "model=" << model << " rate=" << sr << " energy=" << sum << " peak=" << peak
                              << "\n";
                }
            for (int a = 0; a < 4; ++a)
                for (int b = a + 1; b < 4; ++b)
                {
                    double d = 0;
                    for (size_t i = 0; i < waves[0].size(); ++i)
                    {
                        double x = waves[size_t(a)][i] - waves[size_t(b)][i];
                        d += x * x;
                    }
                    require(d > .001, "Distinct model waveforms");
                }
            for (int model = 0; model < 4; ++model)
                for (int note : {40, 64, 88})
                {
                    double energy[2] = {};
                    for (int vel = 0; vel < 2; ++vel)
                    {
                        auto e = std::make_unique<pmg::StringEngine>();
                        pmg::Controls c;
                        e->prepare(48000, pmg::Model(model), c);
                        e->noteOn(note, vel ? 1.f : .25f);
                        for (int i = 0; i < 24000; ++i)
                        {
                            const float s = e->nextSample();
                            require(std::isfinite(s), "Guitar range finite");
                            energy[vel] += s * s;
                        }
                    }
                    require(energy[1] > energy[0] * 1.3 && energy[0] > 1e-10,
                            "Velocity dynamic response in guitar range");
                }
            // Advanced physical parameters alter the signal before the final body output.
            for (int model = 1; model < 4; ++model)
                for (int which = 0; which < 4; ++which)
                {
                    if ((which == 1 && model != 1) || (which == 3 && model != 3))
                        continue;
                    auto a = std::make_unique<pmg::StringEngine>(), b = std::make_unique<pmg::StringEngine>();
                    pmg::Controls x, y;
                    float *xa[] = {&x.material, &x.character, &x.coupling, &x.touch};
                    float *ya[] = {&y.material, &y.character, &y.coupling, &y.touch};
                    *xa[which] = 0;
                    *ya[which] = 1;
                    a->prepare(48000, pmg::Model(model), x);
                    b->prepare(48000, pmg::Model(model), y);
                    a->noteOn(52, .8f);
                    b->noteOn(52, .8f);
                    double diff = 0;
                    for (int i = 0; i < 16000; ++i)
                    {
                        const double d = a->nextSample() - b->nextSample();
                        diff += d * d;
                    }
                    require(diff > 1e-8, "Physical parameter is effective");
                }
        }
        else if (test == "transition")
        {
            for (int from = 0; from < 4; ++from)
                for (int to = 0; to < 4; ++to)
                    if (from != to)
                    {
                        auto rack = std::make_unique<pmg::ModelRack>();
                        pmg::Controls c;
                        rack->prepare(48000, pmg::Model(from), c);
                        rack->noteOn(52, .8f);
                        for (int i = 0; i < 2000; ++i)
                            rack->nextSample();
                        rack->request(pmg::Model(to), c);
                        float prior = rack->nextSample(), step = 0;
                        for (int i = 0; i < 2400; ++i)
                        {
                            const float s = rack->nextSample();
                            step = std::max(step, std::abs(s - prior));
                            prior = s;
                        }
                        require(rack->applied() == pmg::Model(to), "All directed model switches apply");
                        require(step < .2f, "Transition has no large discontinuity");
                    }
            for (int sr : {44100, 48000, 96000})
            {
                auto rack = std::make_unique<pmg::ModelRack>();
                auto old = std::make_unique<pmg::StringEngine>();
                pmg::Controls c;
                rack->prepare(sr, pmg::Model::original, c);
                old->prepare(sr, pmg::Model::original, c);
                rack->noteOn(52, .8f);
                old->noteOn(52, .8f);
                for (int i = 0; i < 5000; ++i)
                    require(rack->nextSample() == old->nextSample(), "Original no-change path");
                rack->request(pmg::Model::electric, c);
                require(rack->transitioning(), "Transition pending");
                require(rack->nextSample() == old->nextSample(), "Crossfade starts exactly on old output");
                rack->request(pmg::Model::nylon,
                              c); // Request during a fade waits for the next block; no reset/click.
                rack->noteOff(52);
                for (int i = 0; i < int(sr * .04) + 1; ++i)
                    require(std::isfinite(rack->nextSample()), "Transition finite");
                require(!rack->transitioning() && rack->applied() == pmg::Model::electric,
                        "First transition completes");
                rack->request(pmg::Model::nylon, c);
                for (int i = 0; i < int(sr * .04) + 1; ++i)
                    rack->nextSample();
                require(rack->applied() == pmg::Model::nylon, "Latest request then applies");
                rack->noteOn(64, .5f);
                rack->allNotesOff();
                for (int i = 0; i < sr * 3; ++i)
                    rack->nextSample();
                require(rack->activeVoices() == 0, "No stuck notes after transition");
            }
        }
        else if (test == "realtime")
        {
            for (int sr : {44100, 48000, 96000})
            {
                auto rack = std::make_unique<pmg::ModelRack>();
                pmg::Controls c;
                rack->prepare(sr, pmg::Model::original, c);
                allocations = 0;
                watch = true;
                double checksum = 0;
                const auto start = std::chrono::steady_clock::now();
                for (int block = 0; block < 1000; ++block)
                {
                    if (block % 200 == 0)
                    {
                        rack->request(pmg::Model((block / 200) % 4), c);
                        for (int n = 40; n < 56; ++n)
                            rack->noteOn(n, .7f);
                    }
                    if (block % 200 == 150)
                        rack->allNotesOff();
                    for (int s = 0; s < 256; ++s)
                        checksum += rack->nextSample();
                }
                const auto stop = std::chrono::steady_clock::now();
                watch = false;
                const double elapsed = std::chrono::duration<double>(stop - start).count();
                require(allocations == 0, "No C++ new in DSP note/switch/render");
                require(std::isfinite(checksum), "Finite performance checksum");
                std::cout << "rate=" << sr << " wall_seconds=" << elapsed << " audio_seconds=" << 256000. / sr
                          << " realtime_ratio=" << elapsed / (256000. / sr) << " new=" << allocations << "\n";
            }
            auto a = std::make_unique<pmg::ModelRack>(), b = std::make_unique<pmg::ModelRack>();
            pmg::Controls c;
            a->prepare(48000, pmg::Model::steel, c);
            b->prepare(44100, pmg::Model::nylon, c);
            a->noteOn(60, .8f);
            b->noteOn(48, .5f);
            a->request(pmg::Model::electric, c);
            for (int i = 0; i < 3000; ++i)
            {
                a->nextSample();
                b->nextSample();
            }
            require(b->applied() == pmg::Model::nylon, "Independent instances");
        }
        else
            return 2;
        std::cout << "PASS " << test << "\n";
        return 0;
    }
    catch (const std::exception &e)
    {
        watch = false;
        std::cerr << "FAIL " << e.what() << "\n";
        return 1;
    }
}
