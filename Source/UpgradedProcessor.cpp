#include "UpgradedProcessor.h"
#include "UpgradedEditor.h"
#include "../DSP/CalibrationProfiles.h"
#include <cmath>
PhysicalModelGuitarAudioProcessor::PhysicalModelGuitarAudioProcessor()
    : AudioProcessor(BusesProperties().withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      parameters(*this, &undo, "PARAMETERS", createParameterLayout())
{
    for (size_t i = 0; i < ids.size(); ++i)
        raw[i] = parameters.getRawParameterValue(ids[i]);
    rawRealCalibration = parameters.getRawParameterValue("realCalibration");
    rawPhysics06 = parameters.getRawParameterValue("physics06");
    rawPhysics07 = parameters.getRawParameterValue("physics07");
    rawPhysics08 = parameters.getRawParameterValue("physics08");
    rawMeasuredBridge08 = parameters.getRawParameterValue("measuredBridge08");
    rawPhysics09 = parameters.getRawParameterValue("physics09");
    rawPhysics10 = parameters.getRawParameterValue("physics10");
    rawPhysics11 = parameters.getRawParameterValue("physics11");
    rawPhysics12 = parameters.getRawParameterValue("physics12");
    rawMagnetic13 = parameters.getRawParameterValue("magnetic13");
    rawRadiation13 = parameters.getRawParameterValue("radiation13");
    rawMagnetic14 = parameters.getRawParameterValue("magnetic14");
    rawRadiation14 = parameters.getRawParameterValue("radiation14");
    rawMagneticAttack14 = parameters.getRawParameterValue("magneticAttack14");
    rawNylonTreble14 = parameters.getRawParameterValue("nylonTreble14");
    rawMagnetic15 = parameters.getRawParameterValue("magnetic15");
    rawRadiation15 = parameters.getRawParameterValue("radiation15");
    rawRadiation16 = parameters.getRawParameterValue("radiation16");
    rawRadiation17 = parameters.getRawParameterValue("radiation17");
    rawDirectBodyPhase17 = parameters.getRawParameterValue("directBodyPhase17");
    rawDirectBodyMix17 = parameters.getRawParameterValue("directBodyMix17");
    rawLateBody16 = parameters.getRawParameterValue("lateBody16");
    rawDirectPhase15 = parameters.getRawParameterValue("directPhase15");
}
juce::AudioProcessorValueTreeState::ParameterLayout PhysicalModelGuitarAudioProcessor::createParameterLayout()
{
    using P = juce::AudioParameterFloat;
    juce::AudioProcessorValueTreeState::ParameterLayout l;
    // The existing eight IDs, versions, ranges, order and defaults are preserved.
    l.add(std::make_unique<P>(juce::ParameterID{"decay", 1}, "String Decay",
                              juce::NormalisableRange<float>(.5f, 12.f, .01f, .45f), 4.f,
                              juce::AudioParameterFloatAttributes().withLabel("s")));
    l.add(std::make_unique<P>(juce::ParameterID{"brightness", 1}, "String Brightness",
                              juce::NormalisableRange<float>(0.f, 1.f, .001f), .72f));
    l.add(std::make_unique<P>(juce::ParameterID{"pluckPosition", 1}, "Pluck Position",
                              juce::NormalisableRange<float>(.03f, .49f, .001f), .18f));
    l.add(std::make_unique<P>(juce::ParameterID{"pickupPosition", 1}, "Pickup Position",
                              juce::NormalisableRange<float>(.03f, .49f, .001f), .15f));
    l.add(std::make_unique<P>(juce::ParameterID{"pickHardness", 1}, "Pick Hardness",
                              juce::NormalisableRange<float>(0.f, 1.f, .001f), .6f));
    l.add(std::make_unique<P>(juce::ParameterID{"bodyResonance", 1}, "Body Resonance",
                              juce::NormalisableRange<float>(0.f, .8f, .001f), .24f));
    l.add(std::make_unique<P>(juce::ParameterID{"bodySize", 1}, "Body Size",
                              juce::NormalisableRange<float>(.7f, 1.4f, .001f), 1.f));
    l.add(std::make_unique<P>(juce::ParameterID{"outputLevel", 1}, "Output",
                              juce::NormalisableRange<float>(-24.f, 6.f, .1f), -3.f,
                              juce::AudioParameterFloatAttributes().withLabel("dB")));
    l.add(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID{"model", 1}, "Guitar Model",
        juce::StringArray{"Original", "Electric", "Steel Acoustic", "Classical Nylon"}, 0));
    l.add(std::make_unique<P>(juce::ParameterID{"material", 1}, "String Material",
                              juce::NormalisableRange<float>(0.f, 1.f, .001f), .5f));
    l.add(std::make_unique<P>(juce::ParameterID{"pickupCharacter", 1}, "Pickup Character",
                              juce::NormalisableRange<float>(0.f, 1.f, .001f), .5f));
    l.add(std::make_unique<P>(juce::ParameterID{"bridgeCoupling", 1}, "Bridge Coupling",
                              juce::NormalisableRange<float>(0.f, 1.f, .001f), .45f));
    l.add(std::make_unique<P>(juce::ParameterID{"touch", 1}, "Finger Contact",
                              juce::NormalisableRange<float>(0.f, 1.f, .001f), .5f));
    l.add(std::make_unique<P>(juce::ParameterID{"pickAngle", 1}, "Pick Angle",
                              juce::NormalisableRange<float>(0.f, 1.f, .001f), .4f));
    l.add(std::make_unique<P>(juce::ParameterID{"fretClearance", 1}, "Fret Clearance",
                              juce::NormalisableRange<float>(0.f, 1.f, .001f), .65f));
    l.add(std::make_unique<P>(juce::ParameterID{"pickupAperture", 1}, "Pickup Aperture",
                              juce::NormalisableRange<float>(0.f, 1.f, .001f), .5f));
    l.add(std::make_unique<P>(juce::ParameterID{"bridgeMobility", 1}, "Bridge Mobility",
                              juce::NormalisableRange<float>(0.f, 1.f, .001f), .5f));
    l.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{"realCalibration", 1},
          "Real Recording Calibration (A/B Experimental)", false));
    l.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{"physics06", 1},
          "Physics 0.6 / 0.5 Blind A/B", true));
    l.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{"physics07", 1},
          "Physics 0.7 Distributed Frets / 0.6 A/B", true));
    l.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{"physics08", 1},
          "Physics 0.8 Bridge Mobility / 0.7 A/B", true));
    l.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{"physics09", 1},
          "Physics 0.9 Passive Two-Location Bridge / 0.8 A/B", true));
    l.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{"physics10", 1},
          "Physics 1.0 Physical Velocity-Wave String / 0.9 A/B Research", true));
    l.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{"physics11", 1},
          "Physics 1.1 Integrated Pick/Fret/Longitudinal/Pickup A/B", true));
    l.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{"physics12", 1},
          "Physics 1.2 Unified Wave / 1.1 Hybrid A/B", false));
    l.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{"magnetic13", 1},
          "1.3 Electric Magnetic Flux / 1.2 Coil A/B", false));
    l.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{"radiation13", 1},
          "1.3 Nylon Modal Pressure / 1.2 Acoustic A/B", false));
    l.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{"magnetic14", 1},
          "1.4 Transient Magnetic / 1.3 A/B", false));
    l.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{"radiation14", 1},
          "1.4 Upper Soundboard Radiation / 1.3 A/B", false));
    l.add(std::make_unique<P>(juce::ParameterID{"magneticAttack14",1},
          "1.4 Magnetic Transient Detail",juce::NormalisableRange<float>(0.f,1.f,.001f),.5f));
    l.add(std::make_unique<P>(juce::ParameterID{"nylonTreble14",1},
          "1.4 Nylon High-Register Radiation",juce::NormalisableRange<float>(0.f,1.f,.001f),.5f));
    l.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{"magnetic15", 1},
          "1.5 Measured Coil Electrical Load (A/B)", false));
    l.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{"radiation15", 1},
          "1.5 Measured Nylon Nearfield Complex Radiation (A/B)", false));
    l.add(std::make_unique<P>(juce::ParameterID{"directPhase15",1},
          "1.5 String/Body Air Delay Hypothesis",juce::NormalisableRange<float>(0.f,1.f,.001f),.5f));
    l.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{"radiation16",1},
          "1.6 Long Nylon Body Modes / 1.5 A/B",false));
    l.add(std::make_unique<P>(juce::ParameterID{"lateBody16",1},
          "1.6 Long Body Tail Mix",juce::NormalisableRange<float>(0.f,1.f,.001f),.5f));
    l.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{"radiation17",1},
          "1.7 Coherent Direct/Body Radiation A/B",false));
    l.add(std::make_unique<P>(juce::ParameterID{"directBodyPhase17",1},
          "1.7 Signed Direct/Body Air Delay",juce::NormalisableRange<float>(0.f,1.f,.001f),.5f));
    l.add(std::make_unique<P>(juce::ParameterID{"directBodyMix17",1},
          "1.7 Direct String Radiation Mix",juce::NormalisableRange<float>(0.f,1.f,.001f),.5f));
    l.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{"measuredBridge08", 1},
          "Zenodo Classical Bridge (Experimental Scalar)", false));
    return l;
}
void PhysicalModelGuitarAudioProcessor::loadComparisonPreset(int model)
{
    model = juce::jlimit(0, 3, model);
    // Preset now restores model defaults. v0.2 RMS calibration does not apply to the new DSP.
    undo.beginNewTransaction("Comparison preset");
    for (size_t i = 0; i < ids.size(); ++i)
    {
        auto *p = parameters.getParameter(ids[i]);
        float v = p->getDefaultValue();
        if (i == 8)
            v = p->convertTo0to1(float(model));
        p->beginChangeGesture();
        p->setValueNotifyingHost(v);
        p->endChangeGesture();
    }
    if (auto* cal = parameters.getParameter("realCalibration")) {
        cal->beginChangeGesture();
        cal->setValueNotifyingHost(0.f);
        cal->endChangeGesture();
    }
}
pmg::Controls PhysicalModelGuitarAudioProcessor::readControls() const noexcept
{
    pmg::Controls c;
    float *dst[] = {&c.decay, &c.brightness, &c.pluck,    &c.pickup,    &c.hardness, &c.body,
                    &c.size,  &c.output,     &c.material, &c.character, &c.coupling, &c.touch,
                    &c.pickAngle, &c.fretClearance, &c.pickupAperture, &c.bridgeMobility};
    const int src[] = {0, 1, 2, 3, 4, 5, 6, 7, 9, 10, 11, 12, 13, 14, 15, 16};
    for (int i = 0; i < 16; ++i)
    {
        const float v = raw[size_t(src[i])]->load(std::memory_order_relaxed);
        if (std::isfinite(v))
            *dst[i] = v;
    }
    if (rawPhysics06 != nullptr)
        c.physics06 = rawPhysics06->load(std::memory_order_relaxed) > .5f;
    if (rawPhysics07 != nullptr)
        c.physics07 = rawPhysics07->load(std::memory_order_relaxed) > .5f;
    if (rawPhysics08 != nullptr)
        c.physics08 = rawPhysics08->load(std::memory_order_relaxed) > .5f;
    if (rawPhysics09 != nullptr)
        c.physics09 = rawPhysics09->load(std::memory_order_relaxed) > .5f;
    if (rawPhysics10 != nullptr)
        c.physics10 = rawPhysics10->load(std::memory_order_relaxed) > .5f;
    if (rawPhysics11 != nullptr)
        c.physics11 = rawPhysics11->load(std::memory_order_relaxed) > .5f;
    if (rawPhysics12 != nullptr)
        c.physics12 = rawPhysics12->load(std::memory_order_relaxed) > .5f;
    if (rawMagnetic13 != nullptr) c.magnetic13 = rawMagnetic13->load(std::memory_order_relaxed) > .5f;
    if (rawRadiation13 != nullptr) c.radiation13 = rawRadiation13->load(std::memory_order_relaxed) > .5f;
    if (rawMagnetic14 != nullptr) c.magnetic14 = rawMagnetic14->load(std::memory_order_relaxed) > .5f;
    if (rawRadiation14 != nullptr) c.radiation14 = rawRadiation14->load(std::memory_order_relaxed) > .5f;
    if (rawMagneticAttack14 != nullptr) c.magneticAttack14 = rawMagneticAttack14->load(std::memory_order_relaxed);
    if (rawNylonTreble14 != nullptr) c.nylonTreble14 = rawNylonTreble14->load(std::memory_order_relaxed);
    if (rawMagnetic15 != nullptr) c.magnetic15 = rawMagnetic15->load(std::memory_order_relaxed) > .5f;
    if (rawRadiation15 != nullptr) c.radiation15 = rawRadiation15->load(std::memory_order_relaxed) > .5f;
    if (rawRadiation16 != nullptr) c.radiation16 = rawRadiation16->load(std::memory_order_relaxed) > .5f;
    if (rawRadiation17 != nullptr) c.radiation17 = rawRadiation17->load(std::memory_order_relaxed) > .5f;
    if (rawDirectBodyPhase17 != nullptr) c.directBodyPhase17 = rawDirectBodyPhase17->load(std::memory_order_relaxed);
    if (rawDirectBodyMix17 != nullptr) c.directBodyMix17 = rawDirectBodyMix17->load(std::memory_order_relaxed);
    if (rawLateBody16 != nullptr) c.lateBody16 = rawLateBody16->load(std::memory_order_relaxed);
    if (rawDirectPhase15 != nullptr) c.directPhase15 = rawDirectPhase15->load(std::memory_order_relaxed);
    if (rawMeasuredBridge08 != nullptr)
        c.measuredBridge08 = rawMeasuredBridge08->load(std::memory_order_relaxed) > .5f;
    if (rawRealCalibration != nullptr && rawRealCalibration->load(std::memory_order_relaxed) > .5f)
        pmg::applyHeldoutCandidates(requestedModel(), c.calibration);
    return c;
}
pmg::Model PhysicalModelGuitarAudioProcessor::requestedModel() const noexcept
{
    const float v = raw[8]->load(std::memory_order_relaxed);
    return static_cast<pmg::Model>(std::isfinite(v) ? juce::jlimit(0, 3, int(std::round(v))) : 0);
}
void PhysicalModelGuitarAudioProcessor::prepareToPlay(double sr, int)
{
    rack.prepare(sr, requestedModel(), readControls());
    appliedModel.store(int(rack.applied()));
    modelTransition.store(false);
    peak.store(0);
}
bool PhysicalModelGuitarAudioProcessor::isBusesLayoutSupported(const BusesLayout &l) const
{
    return l.getMainInputChannelSet().isDisabled() &&
           (l.getMainOutputChannelSet() == juce::AudioChannelSet::mono() ||
            l.getMainOutputChannelSet() == juce::AudioChannelSet::stereo());
}
void PhysicalModelGuitarAudioProcessor::processBlock(juce::AudioBuffer<float> &b, juce::MidiBuffer &midi)
{
    juce::ScopedNoDenormals noDenormals;
    b.clear();
    rack.request(requestedModel(), readControls());
    int pos = 0;
    float maxPeak = 0;
    auto render = [&](int end)
    {
        for (; pos < end; ++pos)
        {
            const auto frame = rack.nextFrame();
            maxPeak = std::max(maxPeak, std::max(std::abs(frame.left), std::abs(frame.right)));
            if (b.getNumChannels() == 1)
                b.setSample(0, pos, .5f*(frame.left + frame.right));
            else
            {
                b.setSample(0, pos, frame.left);
                b.setSample(1, pos, frame.right);
            }
        }
    };
    for (const auto e : midi)
    {
        render(juce::jlimit(pos, b.getNumSamples(), e.samplePosition));
        // Inspect bounded short messages directly: ignored SysEx never constructs an allocating MidiMessage.
        if (e.numBytes < 3)
            continue;
        const auto *d = e.data;
        const int type = d[0] & 0xf0, n = d[1] & 127, v = d[2] & 127;
        if (type == 0x90 && v)
            rack.noteOn(n, float(v) / 127.f);
        else if (type == 0x80 || (type == 0x90 && !v))
            rack.noteOff(n);
        else if (type == 0xb0 && (n == 120 || n == 123))
            rack.allNotesOff();
        else if (type == 0xe0)
        {
            const int value14 = int(d[1]) + 128*int(d[2]);
            rack.pitchBend(2.f*(float(value14)-8192.f)/8192.f);
        }
    }
    render(b.getNumSamples());
    appliedModel.store(int(rack.applied()), std::memory_order_relaxed);
    modelTransition.store(rack.transitioning(), std::memory_order_relaxed);
    voiceCount.store(rack.activeVoices(), std::memory_order_relaxed);
    peak.store(maxPeak, std::memory_order_relaxed);
}
juce::AudioProcessorEditor *PhysicalModelGuitarAudioProcessor::createEditor()
{
    return new PhysicalModelGuitarAudioProcessorEditor(*this);
}
void PhysicalModelGuitarAudioProcessor::getStateInformation(juce::MemoryBlock &data)
{
    auto s = parameters.copyState();
    s.setProperty("schema", 14, nullptr);
    s.setProperty("editorWidth", editorWidth.load(), nullptr);
    s.setProperty("editorHeight", editorHeight.load(), nullptr);
    auto xml = s.createXml();
    copyXmlToBinary(*xml, data);
}
void PhysicalModelGuitarAudioProcessor::setStateInformation(const void *data, int bytes)
{
    auto xml = getXmlFromBinary(data, bytes);
    if (!xml || !xml->hasTagName("PARAMETERS"))
        return;
    auto state = juce::ValueTree::fromXml(*xml);
    if (!state.isValid())
        return;
    // Old eight-parameter sessions must restore Original even when the current instance was another model.
    for (size_t i = 8; i < ids.size(); ++i)
    {
        bool found = false;
        for (auto c : state)
            if (c.getProperty("id").toString() == ids[i])
                found = true;
        if (!found)
        {
            juce::ValueTree c("PARAM");
            c.setProperty("id", ids[i], nullptr);
            auto *p = parameters.getParameter(ids[i]);
            c.setProperty("value", p->convertFrom0to1(p->getDefaultValue()), nullptr);
            state.addChild(c, -1, nullptr);
        }
    }
    editorWidth.store(juce::jlimit(900, 1800, int(state.getProperty("editorWidth", 1040))));
    editorHeight.store(juce::jlimit(792, 1584, int(state.getProperty("editorHeight", 880))));
    parameters.replaceState(state);
}
juce::AudioProcessor *JUCE_CALLTYPE createPluginFilter()
{
    return new PhysicalModelGuitarAudioProcessor();
}
