#pragma once
#include "../DSP/ModelRack.h"
#include <JuceHeader.h>
#include <array>
#include <atomic>
class PhysicalModelGuitarAudioProcessor final : public juce::AudioProcessor
{
  public:
    PhysicalModelGuitarAudioProcessor();
    ~PhysicalModelGuitarAudioProcessor() override = default;
    void prepareToPlay(double, int) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported(const BusesLayout &) const override;
    void processBlock(juce::AudioBuffer<float> &, juce::MidiBuffer &) override;
    juce::AudioProcessorEditor *createEditor() override;
    bool hasEditor() const override
    {
        return true;
    }
    const juce::String getName() const override
    {
        return JucePlugin_Name;
    }
    bool acceptsMidi() const override
    {
        return true;
    }
    bool producesMidi() const override
    {
        return false;
    }
    bool isMidiEffect() const override
    {
        return false;
    }
    double getTailLengthSeconds() const override
    {
        return 12.;
    }
    int getNumPrograms() override
    {
        return 1;
    }
    int getCurrentProgram() override
    {
        return 0;
    }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override
    {
        return {};
    }
    void changeProgramName(int, const juce::String &) override {}
    void getStateInformation(juce::MemoryBlock &) override;
    void setStateInformation(const void *, int) override;
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    void loadComparisonPreset(int model); // Message-thread UI action, not an audio callback.
    juce::UndoManager undo;
    juce::AudioProcessorValueTreeState parameters;
    static constexpr std::array<const char *, 17> ids{
        "decay",           "brightness",     "pluckPosition", "pickupPosition", "pickHardness",
        "bodyResonance",   "bodySize",       "outputLevel",   "model",          "material",
        "pickupCharacter", "bridgeCoupling", "touch", "pickAngle", "fretClearance",
        "pickupAperture", "bridgeMobility"};
    std::atomic<int> appliedModel{0}, voiceCount{0}, editorWidth{1000}, editorHeight{980};
    std::atomic<bool> modelTransition{false};
    std::atomic<float> peak{0};

  private:
    std::array<std::atomic<float> *, 17> raw{};
    std::atomic<float> *rawRealCalibration = nullptr, *rawPhysics06 = nullptr, *rawPhysics07 = nullptr, *rawPhysics08 = nullptr, *rawMeasuredBridge08 = nullptr, *rawPhysics09 = nullptr, *rawPhysics10 = nullptr, *rawPhysics11 = nullptr, *rawPhysics12 = nullptr, *rawMagnetic13 = nullptr, *rawRadiation13 = nullptr, *rawMagnetic14 = nullptr, *rawRadiation14 = nullptr, *rawMagneticAttack14 = nullptr, *rawNylonTreble14 = nullptr, *rawMagnetic15 = nullptr, *rawRadiation15 = nullptr, *rawDirectPhase15 = nullptr, *rawRadiation16 = nullptr, *rawLateBody16 = nullptr, *rawRadiation17 = nullptr, *rawDirectBodyPhase17 = nullptr, *rawDirectBodyMix17 = nullptr, *rawDiagnostic171 = nullptr, *rawDiagnosticDirect171 = nullptr, *rawDiagnosticBridge171 = nullptr, *rawDiagnosticBody171 = nullptr;
    pmg::ModelRack rack;
    pmg::Controls readControls() const noexcept;
    pmg::Model requestedModel() const noexcept;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PhysicalModelGuitarAudioProcessor)
};
