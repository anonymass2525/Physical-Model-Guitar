#pragma once
#include <array>
#include <cstdint>
#include "MeasuredBridge.h"
#include "TwoPortBridge.h"

namespace pmg
{
enum class Model : int { original, electric, steel, nylon };
inline constexpr const char *modelNames[] = {"Hybrid Guitar", "Electric", "Steel Acoustic", "Classical Nylon"};

// All parameter fields are fixed-capacity POD. No allocation or locks on the audio thread.
// Research-only per-string/structural inverse-model coefficients. Neutral values
// reproduce the v0.4 DSP; all are fixed-size POD, safe to copy on audio thread.
struct PhysicalCalibration
{
    std::array<float, 6> contactStiffness{{1.f, 1.f, 1.f, 1.f, 1.f, 1.f}};
    std::array<float, 6> pickupOffset{{0.f, 0.f, 0.f, 0.f, 0.f, 0.f}};
    // Four ordered frequency bands per string, extra per-roundtrip amplitude loss.
    // A constrained positive lowpass mixture (rather than free 8-peak EQ) remains passive.
    std::array<std::array<float, 4>, 6> spectralLoss{};
    std::array<float, 10> modeFrequency{{1.f,1.f,1.f,1.f,1.f,1.f,1.f,1.f,1.f,1.f}};
    std::array<float, 10> modeQ{{1.f,1.f,1.f,1.f,1.f,1.f,1.f,1.f,1.f,1.f}};
    std::array<float, 10> modeGain{{1.f,1.f,1.f,1.f,1.f,1.f,1.f,1.f,1.f,1.f}};
};

struct Controls
{
    float decay = 4.f, brightness = .72f, pluck = .18f, pickup = .15f;
    float hardness = .6f, body = .24f, size = 1.f, output = -3.f;
    float material = .5f, character = .5f, coupling = .45f, touch = .5f;
    float pickAngle = .4f, fretClearance = .65f, pickupAperture = .5f, bridgeMobility = .5f;
    // Opt-in numerical mechanics extension. False preserves v0.5 reference.
    bool physics06 = false;
    // Experimental v0.7: distributed unilateral fret contacts and energy-preserving reattack.
    // False is a bit-exact v0.6 code path.
    bool physics07 = false;
    // Alternate bridge boundary + explicitly passive collocated mobility network.
    bool physics08 = false;
    // Optional scalar vertical mobility identified from ONE external archival
    // classical-guitar bridge. Research only; never default-on.
    bool measuredBridge08 = false;
    // 0.9: 6-port passive wave-scattering termination on two spatial bridge points.
    // Its exact energy ledger is local to the added boundary ports; the older
    // displacement waveguides are not full SI-calibrated power-wave states.
    bool physics09 = false;
    PhysicalCalibration calibration{};
};

struct StereoFrame { float left = 0.f, right = 0.f; };

// Six persistent strings. Two orthogonally polarized waveguides per string, nonlinear
// contact scattering and a shared bridge/modal soundboard. MIDI is mapped to a
// physically playable string/fret; not a general-purpose 16-voice piano synthesizer.
class StringEngine
{
public:
    static constexpr int maxVoices = 6;
    static constexpr int delayBufferSize = 8192;
    static constexpr int delayBufferMask = delayBufferSize - 1;
    void prepare(double rate, Model model, const Controls& controls) noexcept;
    void setControls(const Controls& controls) noexcept;
    void noteOn(int note, float velocity) noexcept;
    // For calibration/playability constraints: 0=low E through 5=high E.
    bool noteOnString(int note, float velocity, int physicalString) noexcept;
    void noteOff(int note) noexcept;
    void allNotesOff() noexcept;
    void setPitchBend(float semitones) noexcept;
    StereoFrame nextFrame() noexcept;
    float nextSample() noexcept;  // left/right average, useful for deterministic tests
    int activeVoices() const noexcept;
    Model getModel() const noexcept { return model; }
    int contactCount() const noexcept { return contacts; }
    float bodyMotion() const noexcept { return lastBodyMotion; }
    int assignedString(int midiNote) const noexcept;
    float stringEnergy(int physicalString) const noexcept;

private:
    struct Waveguide
    {
        std::array<float, delayBufferSize> history{};
        int index = 0;
        float previous = 0, dispersionState = 0, previousPickup = 0;
        std::array<float,3> spectralState{};
        float read(float delay) const noexcept;
        void clear() noexcept;
        float propagate(float delay, float gain, float loss, float dispersion, float injected,
                        const std::array<float,4>& spectralLoss,
                        const std::array<float,3>& spectralAlpha) noexcept;
        // Two-sided perturbation of displacement-wave history at a local contact.
        void scatter(float location, float force, float delay) noexcept;
    };
    struct String
    {
        Waveguide transverse[2];
        bool active = false, releasing = false;
        int midiNote = -1, fret = 0, noteAge = 0, contactLength = 1;
        int stringNumber = 0;
        float noteVelocity = 0, rms = 0, targetDelay = 0, baseFrequency = 110.f;
        float desiredDelay = 0, cachedBend = 1000.f, cachedDecay = -1.f, loopGain = .999f;
        float cachedCharacter = -1.f, circuitAlpha = .5f;
        float pickSlip = .7f, pickDepth = 0, prevPickDisplacement = 0;
        float lastContact = 0, fretDisplacement = 0;
        float previousBridge = 0, previousBridgeX = 0, previousBridgeY = 0, lastMagnetic = 0, circuitLo = 0, circuitMid = 0;
        float pickupPrevious = 0, rolloffState = 0;
        float bodyHighpass = 0;
        float dispersion = .5f, planeDetune = .0007f;
        // Acoustic Realism II: passive polarization exchange, amplitude-dependent tension,
        // and independent longitudinal/transverse losses. The coefficients are physical
        // approximations and require validation against matched clean recordings.
        float strainEnergy = 0.f, waveExchange = 0.f;
        // Single longitudinal modal degree of freedom, coupled through geometric
        // stretching of the pair of transverse waveguides. This is an effective
        // reduced-order longitudinal mode, not a discretised longitudinal PDE.
        float longitudinalDisplacement = 0.f, longitudinalVelocity = 0.f;
        float longitudinalBridge = 0.f, longitudinalFrequency = .0f;
        // Compliant pick tangential stick / slip state. Zero at a new note.
        float tangentialDeflection = 0.f, previousPickTip = 0.f;
        std::array<float, 5> fretContactHistory{};
        std::array<float, 5> fretLocation{};
        std::array<float, 5> fretHeightMultiplier{};
        int availableContactFrets = 0;
        float fretContactMemory = 0.f;
        float pickStiffness = 1.f, strainSensitivity = 0.f;
        float transverseLossRatio = 1.3f, polarizationExchange = .0002f;
        float attackTimeScale = 1.f;
        std::array<float,4> perBandLoss{};
        std::array<float,3> perBandAlpha{};
        void start(int n, int stringNo, int fretNo, float velocity,
                   float rate, Model m, const Controls& controls) noexcept;
        struct Output { float bridge = 0, electric = 0, direct = 0, fret = 0; float bridgeX=0, bridgeY=0; };
        Output tick(float rate, Model m, const Controls& c, float bridgeReaction,
                    float pitchBend, int& contactCounter) noexcept;
    };
    struct Mode
    {
        float y1 = 0, y2 = 0, a1 = 0, a2 = 0, b = 0;
        float targetA1 = 0, targetA2 = 0, targetB = 0, smoothing = .001f;
        void configure(float sampleRate, float frequency, float q, float response, bool reset) noexcept;
        float tick(float force) noexcept;
    };
    struct Body
    {
        std::array<Mode, 10> modes{};
        float feedback = 0, lowpass = 0;
        StereoFrame radiated{};
        void configure(float sr, Model model, float size, const PhysicalCalibration& calibration, bool reset) noexcept;
        void clear() noexcept;
        void tick(float bridgeForce, float coupling, float mobility) noexcept;
    };
    static constexpr std::array<int, 6> tuning{{40, 45, 50, 55, 59, 64}};
    std::array<String, maxVoices> strings{};
    Body acousticBody{};
    MeasuredBridge measuredBridge{};
    TwoPortBridge twoPortBridge{};
    std::array<float, maxVoices> bridgeReflections09{};
    std::array<double,maxVoices> stringPortZ09{};
    float bridgePortFeedback = 0;
    Model model = Model::original;
    Controls controls{};
    float rate = 48000.f, bend = 0.f, gain = 1.f, smoothAlpha = .003f;
    float smoothedOutput = 1.f, smoothedBody = .24f, smoothedCoupling = .45f;
    float lastBodyMotion = 0.f;
    int contacts = 0;
};
} // namespace pmg
